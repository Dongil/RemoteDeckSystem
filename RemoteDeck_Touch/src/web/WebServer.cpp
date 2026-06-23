// Design Ref: §4.1, §12.1 — Phase 1 PoC (sync WebServer, /api/status 만 노출)

#include "WebServer.h"

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

    // 간단한 root - Phase 2 에서 SPIFFS /www/ 정적 서빙으로 교체
    _server.on("/", HTTP_GET, [this]() {
        if (!requireAuth()) return;
        _server.send(200, "text/plain",
            "RemoteDeck_Touch v2.2 PoC\n"
            "sync WebServer + W5500 + MQTT 검증 단계\n"
            "API: GET /api/status (Basic Auth)\n");
    });

    _server.onNotFound([this]() {
        if (_server.uri().startsWith("/api/")) {
            _server.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
        } else {
            _server.send(404, "text/plain", "Not found");
        }
    });
}
