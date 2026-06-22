// Design Ref: §2.2, §12.2 — Upload + Hot Reload + race-free 동기화
// Plan SC: FR-02, FR-03, FR-11

#include "ImageApi.h"
#include "WebServer.h"
#include "../images/images.h"
#include <ArduinoJson.h>

void ImageApi::attach(WebServer* ws) {
    if (!ws) return;
    ws->setStatusGetter([this]() { return buildStatusJson(); });
    ws->setImagesListGetter([this]() { return buildImagesListJson(); });
    ws->setImagesConfigGetter([this]() { return readImagesConfigJson(); });
    ws->setImageDeleteHandler([this](const String& n) { return onImageDelete(n); });
    ws->setImageUploadStarter([this](const String& f, size_t t) { onUploadStart(f, t); });
    ws->setImageUploadChunk([this](uint8_t* d, size_t l, bool f) { return onUploadChunk(d, l, f); });
}

void ImageApi::loop() {
    // Design Ref: §12.2 - LVGL race 회피
    // upload 완료 후 main loop 에서 images_update() 호출
    if (_pendingReload) {
        _pendingReload = false;
        Serial.println("ImageApi: triggering images_update() from main loop");
        images_update();
    }
}

bool ImageApi::sanitizeFilename(const String& in, String& out) const {
    // basename 추출 (경로 분리자 제거)
    int slash1 = in.lastIndexOf('/');
    int slash2 = in.lastIndexOf('\\');
    int sep = slash1 > slash2 ? slash1 : slash2;
    String base = (sep >= 0) ? in.substring(sep + 1) : in;
    base.trim();
    if (base.length() == 0 || base.indexOf("..") >= 0) return false;

    // 확장자 화이트리스트
    String lower = base; lower.toLowerCase();
    if (!lower.endsWith(".png") && !lower.endsWith(".bmp")) return false;

    out = base;
    return true;
}

void ImageApi::onUploadStart(const String& filename, size_t total) {
    String safe;
    if (!sanitizeFilename(filename, safe)) {
        Serial.printf("Upload reject: invalid filename '%s'\n", filename.c_str());
        _uploadOk = false;
        return;
    }
    if (total > IMAGE_MAX_BYTES) {
        Serial.printf("Upload reject: %u > %u bytes\n", (unsigned)total, (unsigned)IMAGE_MAX_BYTES);
        _uploadOk = false;
        return;
    }

    // /images/ 폴더 보장
    if (!SPIFFS.exists("/images")) {
        // SPIFFS는 디렉토리 개념 없음 (경로 prefix만) - 파일 쓰기로 자동 생성됨
    }

    _uploadName = safe;
    _uploadExpected = total;
    _uploadWritten = 0;
    _uploadOk = true;

    String path = String("/images/") + _uploadName;
    _uploadFile = SPIFFS.open(path, FILE_WRITE);
    if (!_uploadFile) {
        Serial.printf("SPIFFS open failed: %s\n", path.c_str());
        _uploadOpen = false;
        _uploadOk = false;
        return;
    }
    _uploadOpen = true;
    Serial.printf("Upload start: %s (expected=%u)\n", path.c_str(), (unsigned)total);
}

bool ImageApi::onUploadChunk(uint8_t* data, size_t len, bool isFinal) {
    if (!_uploadOk) {
        // 이미 실패 상태 — 잔여 청크 폐기
        if (isFinal && _uploadOpen) {
            _uploadFile.close();
            _uploadOpen = false;
            SPIFFS.remove(String("/images/") + _uploadName);
        }
        return false;
    }
    if (!_uploadOpen) return false;

    if (_uploadWritten + len > IMAGE_MAX_BYTES) {
        Serial.println("Upload exceeded size limit mid-stream");
        _uploadFile.close();
        _uploadOpen = false;
        SPIFFS.remove(String("/images/") + _uploadName);
        _uploadOk = false;
        return false;
    }

    size_t w = _uploadFile.write(data, len);
    if (w != len) {
        Serial.printf("SPIFFS short write: %u/%u\n", (unsigned)w, (unsigned)len);
        _uploadFile.close();
        _uploadOpen = false;
        SPIFFS.remove(String("/images/") + _uploadName);
        _uploadOk = false;
        return false;
    }
    _uploadWritten += len;

    if (isFinal) {
        _uploadFile.flush();
        _uploadFile.close();
        _uploadOpen = false;
        Serial.printf("Upload complete: /images/%s (%u bytes)\n", _uploadName.c_str(), (unsigned)_uploadWritten);

        // Design Ref: §12.2 - main loop 에서 images_update() 처리
        _pendingReload = true;
    }
    return true;
}

bool ImageApi::onImageDelete(const String& name) {
    // 삭제 후 fallback 이미지로 즉시 갱신
    _pendingReload = true;
    return true;
}

String ImageApi::buildStatusJson() const {
    // Design Ref: §4.2 GET /api/status
    StaticJsonDocument<512> doc;
    doc["uptime_sec"] = (uint32_t)(millis() / 1000);
    doc["heap_free"]  = ESP.getFreeHeap();
    doc["heap_min"]   = ESP.getMinFreeHeap();

    size_t spiffsTotal = SPIFFS.totalBytes();
    size_t spiffsUsed  = SPIFFS.usedBytes();
    doc["spiffs_used"]  = spiffsUsed;
    doc["spiffs_total"] = spiffsTotal;

    JsonObject net = doc.createNestedObject("network");
    net["iface"] = _iface;
    net["ip"]    = _ip;

    doc["fw_version"] = _fwVersion;
    doc["fw_date"]    = _fwDate;

    String out;
    serializeJson(doc, out);
    return out;
}

String ImageApi::buildImagesListJson() const {
    // SPIFFS 디렉토리 순회 - /images/* 만 반환
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("images");

    File root = SPIFFS.open("/images");
    if (root && root.isDirectory()) {
        File f = root.openNextFile();
        while (f) {
            String name = f.name();
            // SPIFFS 에서 name() 은 절대경로 또는 상대경로 — basename 만 추출
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

    String out;
    serializeJson(doc, out);
    return out;
}

String ImageApi::readImagesConfigJson() const {
    File f = SPIFFS.open("/imagesconfig.json", "r");
    if (!f) return "{}";
    String s;
    while (f.available()) s += (char)f.read();
    f.close();
    return s;
}
