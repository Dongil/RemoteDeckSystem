// Design Ref: §4 API Specification, §5.3 — Web Layer 구현
// Plan SC: FR-01, FR-02, FR-08, FR-09, FR-10, FR-12

#include "WebServer.h"
#include <SPIFFS.h>

void WebServer::begin(uint16_t port, const TouchAuth* auth) {
    _auth = auth;
    _server = new AsyncWebServer(port);

    setupStaticFiles();
    setupAPI();

    _server->begin();
    Serial.printf("WebServer: started on port %u (auth=%s)\n", port, _auth ? "on" : "off");
}

bool WebServer::requireAuth(AsyncWebServerRequest* req) {
    if (!_auth) return true;
    if (!req->authenticate(_auth->user, _auth->pass)) {
        req->requestAuthentication("RemoteDeck_Touch");
        return false;
    }
    return true;
}

void WebServer::setupStaticFiles() {
    // Design Ref: §3.2 SPIFFS Layout — /www/ 가 웹 UI 자산
    auto& h = _server->serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");
    if (_auth) h.setAuthentication(_auth->user, _auth->pass);
}

void WebServer::logEvent(const char* event, const char* detail) {
    if (_onLog) _onLog(event, detail);
}

void WebServer::setupAPI() {
    // GET /api/status — heap, spiffs, uptime, network
    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (_getStatus) req->send(200, "application/json", _getStatus());
        else req->send(500, "application/json", "{\"ok\":false,\"error\":\"no handler\"}");
    });

    // GET /api/images/list
    _server->on("/api/images/list", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (_getImagesList) req->send(200, "application/json", _getImagesList());
        else req->send(500, "application/json", "{\"ok\":false}");
    });

    // GET /api/imagesconfig
    _server->on("/api/imagesconfig", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (_getImagesConfig) req->send(200, "application/json", _getImagesConfig());
        else req->send(404);
    });

    // GET /api/images/<name> — raw image file (browser preview)
    _server->on("^\\/api\\/images\\/(.+)$", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        String name = req->pathArg(0);
        if (name.indexOf("..") >= 0 || name.indexOf('/') >= 0) {
            req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad name\"}");
            return;
        }
        String path = String("/images/") + name;
        if (!SPIFFS.exists(path)) {
            req->send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
            return;
        }
        const char* mime = name.endsWith(".png") ? "image/png" : "image/bmp";
        req->send(SPIFFS, path, mime);
    });

    // DELETE /api/images/<name>
    _server->on("^\\/api\\/images\\/(.+)$", HTTP_DELETE, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        String name = req->pathArg(0);
        if (name.indexOf("..") >= 0 || name.indexOf('/') >= 0) {
            req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad name\"}");
            return;
        }
        String path = String("/images/") + name;
        bool removed = SPIFFS.remove(path);
        if (_deleteImage) _deleteImage(name);
        char detail[80]; snprintf(detail, sizeof(detail), "%s (%s)", name.c_str(), removed ? "ok" : "missing");
        logEvent("IMG_DELETE", detail);
        String body = String("{\"ok\":") + (removed ? "true" : "false") + ",\"name\":\"" + name + "\"}";
        req->send(200, "application/json", body);
    });

    // POST /api/images/upload — multipart/form-data upload
    _server->on("/api/images/upload", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            // upload 완료 후 호출되는 응답 핸들러
            // 실제 파일 처리는 아래 upload handler 에서 수행됨
            if (!requireAuth(req)) return;
            AsyncWebServerResponse* res = req->beginResponse(200, "application/json",
                "{\"ok\":true,\"reloaded\":true}");
            res->addHeader("Connection", "close");
            req->send(res);
        },
        // upload handler — 청크 단위 호출
        [this](AsyncWebServerRequest* req, const String& filename,
               size_t index, uint8_t* data, size_t len, bool isFinal) {
            if (!requireAuth(req)) return;
            if (index == 0) {
                Serial.printf("Upload start: %s (total=%u)\n", filename.c_str(), (unsigned)req->contentLength());
                if (_uploadStart) _uploadStart(filename, req->contentLength());
            }
            bool ok = _uploadChunk ? _uploadChunk(data, len, isFinal) : false;
            if (!ok) {
                Serial.println("Upload chunk failed");
            }
            if (isFinal) {
                char detail[120];
                snprintf(detail, sizeof(detail), "%s (%u bytes, %s)",
                         filename.c_str(), (unsigned)(index + len), ok ? "ok" : "fail");
                logEvent("IMG_UPLOAD", detail);
            }
        }
    );

    // 404 fallback
    _server->onNotFound([this](AsyncWebServerRequest* req) {
        if (req->url().startsWith("/api/")) {
            req->send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });
}
