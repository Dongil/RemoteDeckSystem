// Design Ref: §4.1, §12.1 — sync WebServer (PoC 기반 + Phase 2 정적 파일 + Auth 헬퍼)

#include "WebServer.h"
#include <SPIFFS.h>

void TouchWebServer::begin(uint16_t port, const TouchAuth* auth) {
    _auth = auth;
    if (port != 80) {
        // ::WebServer 는 ctor port 만 받음 — port 변경 시 인스턴스 재생성 필요
        // PoC 는 80 고정. v2.2 Phase 2 에서 동적 port 검토.
    }
    setupRoutes();
    _server.begin();
    Serial.printf("TouchWebServer: started (port %u, auth=%s)\n", (unsigned)port, _auth ? "on" : "off");
}

void TouchWebServer::handleClient() {
    _server.handleClient();
}

bool TouchWebServer::requireAuth() {
    if (!_auth) return true;
    if (!_server.authenticate(_auth->user, _auth->pass)) {
        _server.requestAuthentication();
        return false;
    }
    return true;
}

void TouchWebServer::logEvent(const char* event, const char* detail) {
    if (_onLog) _onLog(event, detail);
}

void TouchWebServer::setupRoutes() {
    // GET /api/status — Phase 1 PoC 핵심 endpoint
    _server.on("/api/status", HTTP_GET, [this]() {
        if (!requireAuth()) return;
        String body = _getStatus ? _getStatus() : "{\"ok\":true,\"poc\":true}";
        _server.send(200, "application/json", body);
        logEvent("API", "/api/status");
    });

    // Phase 2: SPIFFS /www/ 정적 파일 — sync WebServer 는 chaining 미지원, 명시적 핸들러
    auto serveStaticFile = [this](const char* path, const char* spiffsPath, const char* mime) {
        _server.on(path, HTTP_GET, [this, spiffsPath, mime]() {
            if (!requireAuth()) return;
            File f = SPIFFS.open(spiffsPath, "r");
            if (!f) { _server.send(404, "text/plain", "Not found"); return; }
            _server.streamFile(f, mime);
            f.close();
        });
    };
    serveStaticFile("/",          "/www/index.html", "text/html");
    serveStaticFile("/index.html","/www/index.html", "text/html");
    serveStaticFile("/style.css", "/www/style.css",  "text/css");
    serveStaticFile("/app.js",    "/www/app.js",     "application/javascript");

    // 404 fallback — ImageApi 가 onNotFound 등록하므로 여기는 미설정
}
