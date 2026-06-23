// Design Ref: §4 API + §12.2 협력적 yield
// Plan SC: FR-02, FR-03 (이미지 업로드 + 핫리로드)

#include "ImageApi.h"
#include "WebServer.h"
#include "Logger.h"
#include "../images/images.h"
#include <lvgl.h>
#include <ArduinoJson.h>

// 협력적 yield helper (Design §12.2)
static inline void yieldToLvgl() {
    lv_timer_handler();
}

void ImageApi::attach(TouchWebServer* ws, Logger* logger) {
    _ws = ws;
    _logger = logger;
    if (!_ws) return;

    ::WebServer& s = _ws->server();

    s.on("/api/images/list", HTTP_GET, [this]() { handleList(); });
    s.on("/api/imagesconfig", HTTP_GET, [this]() { handleConfigJson(); });

    // /api/images/<name> — sync WebServer 는 명시적 path matching 사용
    s.onNotFound([this]() {
        const String uri = _ws->server().uri();
        if (uri.startsWith("/api/images/") && uri.length() > 12) {
            HTTPMethod method = _ws->server().method();
            if (method == HTTP_GET) { handleGetImage(); return; }
            if (method == HTTP_DELETE) { handleDeleteImage(); return; }
        }
        // fallback (WebServer.cpp 의 onNotFound 와 중복되지만 sync API 는 마지막 등록만 살아남음)
        if (uri.startsWith("/api/")) {
            _ws->server().send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
        } else {
            _ws->server().send(404, "text/plain", "Not found");
        }
    });

    // Upload — sync WebServer 는 2개 핸들러 (response + chunk callback)
    s.on("/api/images/upload", HTTP_POST,
        [this]() { handleUploadResponse(); },
        [this]() { handleUploadChunk(); });
}

void ImageApi::loop() {
    if (_pendingReload) {
        _pendingReload = false;
        Serial.println("ImageApi: triggering images_update()");
        images_update();
    }
}

void ImageApi::log(const char* event, const char* detail) {
    if (_logger) {
        // forward-declared, full call in Logger.h
        extern void logger_add(Logger* l, const char* e, const char* d);
        logger_add(_logger, event, detail);
    }
}

// /api/images/list
void ImageApi::handleList() {
    if (!_ws) return;
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("images");
    File root = SPIFFS.open("/images");
    if (root && root.isDirectory()) {
        File f = root.openNextFile();
        while (f) {
            String name = f.name();
            int sep = name.lastIndexOf('/');
            String base = (sep >= 0) ? name.substring(sep + 1) : name;
            JsonObject obj = arr.createNestedObject();
            obj["name"] = base;
            obj["size"] = f.size();
            f = root.openNextFile();
        }
    }
    doc["spiffs_used"]  = SPIFFS.usedBytes();
    doc["spiffs_total"] = SPIFFS.totalBytes();
    String out; serializeJson(doc, out);
    _ws->server().send(200, "application/json", out);
}

void ImageApi::handleConfigJson() {
    if (!_ws) return;
    File f = SPIFFS.open("/imagesconfig.json", "r");
    if (!f) { _ws->server().send(404); return; }
    _ws->server().streamFile(f, "application/json");
    f.close();
}

void ImageApi::handleGetImage() {
    if (!_ws) return;
    String name = _ws->server().uri().substring(12);  // strip "/api/images/"
    if (name.indexOf("..") >= 0 || name.indexOf('/') >= 0) {
        _ws->server().send(400, "application/json", "{\"ok\":false}");
        return;
    }
    String path = "/images/" + name;
    if (!SPIFFS.exists(path)) {
        _ws->server().send(404);
        return;
    }
    File f = SPIFFS.open(path, "r");
    const char* mime = name.endsWith(".png") ? "image/png" : "image/bmp";
    _ws->server().streamFile(f, mime);
    f.close();
}

void ImageApi::handleDeleteImage() {
    if (!_ws) return;
    String name = _ws->server().uri().substring(12);
    if (name.indexOf("..") >= 0 || name.indexOf('/') >= 0) {
        _ws->server().send(400);
        return;
    }
    String path = "/images/" + name;
    bool removed = SPIFFS.remove(path);
    _pendingReload = true;  // fallback BMP로 자동 복귀
    char detail[64]; snprintf(detail, sizeof(detail), "%s (%s)", name.c_str(), removed ? "ok" : "missing");
    log("IMG_DELETE", detail);
    String body = "{\"ok\":";
    body += (removed ? "true" : "false");
    body += ",\"name\":\""; body += name; body += "\"}";
    _ws->server().send(200, "application/json", body);
}

// Upload response — 청크 처리 후 자동 호출됨
void ImageApi::handleUploadResponse() {
    if (!_ws) return;
    String body = "{\"ok\":";
    body += (_uploadOk ? "true" : "false");
    body += ",\"name\":\""; body += _uploadName; body += "\"";
    body += ",\"size\":"; body += String((unsigned)_uploadWritten);
    body += ",\"reloaded\":"; body += (_uploadOk ? "true" : "false");
    body += "}";
    _ws->server().send(_uploadOk ? 201 : 500, "application/json", body);
}

bool ImageApi::sanitizeFilename(const String& in, String& out) const {
    int sl1 = in.lastIndexOf('/'), sl2 = in.lastIndexOf('\\');
    int sep = sl1 > sl2 ? sl1 : sl2;
    String base = (sep >= 0) ? in.substring(sep + 1) : in;
    base.trim();
    if (base.length() == 0 || base.indexOf("..") >= 0) return false;
    String lower = base; lower.toLowerCase();
    if (!lower.endsWith(".png") && !lower.endsWith(".bmp")) return false;
    out = base;
    return true;
}

// Upload chunk — sync WebServer 는 청크마다 이 콜백 호출
void ImageApi::handleUploadChunk() {
    if (!_ws) return;
    HTTPUpload& upload = _ws->server().upload();

    switch (upload.status) {
        case UPLOAD_FILE_START: {
            String safe;
            if (!sanitizeFilename(upload.filename, safe)) {
                _uploadOk = false;
                Serial.printf("Upload reject: bad filename '%s'\n", upload.filename.c_str());
                return;
            }
            _uploadName = safe;
            _uploadWritten = 0;
            _uploadOk = true;
            String path = "/images/" + _uploadName;
            _uploadFile = SPIFFS.open(path, FILE_WRITE);
            _uploadOpen = (bool)_uploadFile;
            if (!_uploadOpen) {
                _uploadOk = false;
                Serial.printf("SPIFFS open failed: %s\n", path.c_str());
                return;
            }
            Serial.printf("Upload start: %s\n", path.c_str());
            break;
        }
        case UPLOAD_FILE_WRITE: {
            if (!_uploadOk || !_uploadOpen) return;
            if (_uploadWritten + upload.currentSize > IMAGE_MAX_BYTES) {
                Serial.println("Upload exceed size limit");
                _uploadFile.close();
                SPIFFS.remove("/images/" + _uploadName);
                _uploadOpen = false;
                _uploadOk = false;
                return;
            }
            size_t w = _uploadFile.write(upload.buf, upload.currentSize);
            if (w != upload.currentSize) {
                Serial.printf("SPIFFS short write %u/%u\n", (unsigned)w, (unsigned)upload.currentSize);
                _uploadFile.close();
                SPIFFS.remove("/images/" + _uploadName);
                _uploadOpen = false;
                _uploadOk = false;
                return;
            }
            _uploadWritten += upload.currentSize;
            // Design §12.2 협력적 yield — LVGL 갱신 보장
            yieldToLvgl();
            break;
        }
        case UPLOAD_FILE_END: {
            if (_uploadOpen) {
                _uploadFile.flush();
                _uploadFile.close();
                _uploadOpen = false;
            }
            if (_uploadOk) {
                _pendingReload = true;
                Serial.printf("Upload done: /images/%s (%u bytes)\n",
                              _uploadName.c_str(), (unsigned)_uploadWritten);
                char detail[120];
                snprintf(detail, sizeof(detail), "%s (%u bytes, ok)",
                         _uploadName.c_str(), (unsigned)_uploadWritten);
                log("IMG_UPLOAD", detail);
            } else {
                log("IMG_UPLOAD", "failed");
            }
            break;
        }
        case UPLOAD_FILE_ABORTED:
            if (_uploadOpen) {
                _uploadFile.close();
                SPIFFS.remove("/images/" + _uploadName);
                _uploadOpen = false;
            }
            _uploadOk = false;
            Serial.println("Upload aborted");
            break;
    }
}

String ImageApi::buildListJson() const {
    // 미사용 (handleList 가 직접 send) — 추후 setStatusGetter 와 일관성 위해
    return "{\"images\":[]}";
}

String ImageApi::readImagesConfigJson() const {
    File f = SPIFFS.open("/imagesconfig.json", "r");
    if (!f) return "{}";
    String s;
    while (f.available()) s += (char)f.read();
    f.close();
    return s;
}
