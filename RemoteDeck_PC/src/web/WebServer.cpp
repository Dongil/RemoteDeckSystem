#include "WebServer.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

void WebServer::begin(uint16_t port, const AuthConfig* auth) {
    _auth = auth;
    _server = new AsyncWebServer(port);

    _ws.begin(_server);
    _ota.setup(_server);

    setupStaticFiles();
    setupAPI();

    _server->begin();
    Serial.printf("WebServer: Started on port %d (auth: %s)\n", port, _auth ? "enabled" : "disabled");
}

bool WebServer::requireAuth(AsyncWebServerRequest* req) {
    if (!_auth) return true;
    if (!req->authenticate(_auth->user.c_str(), _auth->pass.c_str())) {
        req->requestAuthentication("RemoteDeck");
        return false;
    }
    return true;
}

void WebServer::setupStaticFiles() {
    // Static files also require auth
    _server->serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html")
        .setAuthentication(_auth ? _auth->user.c_str() : "admin",
                           _auth ? _auth->pass.c_str() : "12345");
}

void WebServer::setupAPI() {
    // ─── All endpoints require Basic Auth ────────────────

    // GET /api/status
    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (_getStatus) req->send(200, "application/json", _getStatus());
        else req->send(500);
    });

    // POST /api/relay
    _server->on("/api/relay", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!requireAuth(req)) return;
            if (!_handleRelay) { req->send(500); return; }
            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, data, len)) { req->send(400); return; }
            uint8_t relay = doc["relay"] | 1;
            String action = doc["action"] | "toggle";
            uint16_t duration = doc["duration"] | 500;
            _handleRelay(relay, action, duration);
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // POST /api/wol
    _server->on("/api/wol", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!requireAuth(req)) return;
            if (!_handleWOL) { req->send(500); return; }
            StaticJsonDocument<128> doc;
            if (deserializeJson(doc, data, len)) { req->send(400); return; }
            String mac = doc["mac"] | "";
            bool ok = _handleWOL(mac);
            req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
        }
    );

    // GET /api/schedule
    _server->on("/api/schedule", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (_getSchedule) req->send(200, "application/json", _getSchedule());
        else req->send(200, "application/json", "{\"schedules\":[]}");
    });

    // GET /api/config
    _server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (_getConfig) req->send(200, "application/json", _getConfig());
        else req->send(500);
    });

    // GET /api/log
    _server->on("/api/log", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (_getLog) req->send(200, "application/json", _getLog());
        else req->send(200, "application/json", "{\"logs\":[]}");
    });

    // POST /api/config (auth required)
    _server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!requireAuth(req)) return;
            if (!_setConfig) { req->send(500); return; }
            String json((char*)data, len);
            bool ok = _setConfig(json);
            req->send(200, "application/json",
                ok ? "{\"ok\":true,\"reboot_required\":true}" : "{\"ok\":false}");
        }
    );

    // POST /api/schedule (auth required)
    _server->on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!requireAuth(req)) return;
            if (!_setSchedule) { req->send(500); return; }
            String json((char*)data, len);
            bool ok = _setSchedule(json);
            req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
        }
    );

    // DELETE /api/schedule (auth required)
    _server->on("/api/schedule", HTTP_DELETE, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        if (!_deleteSchedule || !req->hasParam("id")) { req->send(400); return; }
        uint8_t id = req->getParam("id")->value().toInt();
        bool ok = _deleteSchedule(id);
        req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
    });

    // POST /api/reboot (auth required)
    _server->on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req)) return;
        req->send(200, "application/json", "{\"ok\":true}");
        if (_handleReboot) _handleReboot();
    });

    // POST /api/auth (password change - auth required)
    _server->on("/api/auth", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!requireAuth(req)) return;
            if (!_changeAuth) { req->send(500); return; }

            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, data, len)) { req->send(400); return; }

            String curPass = doc["current_pass"] | "";
            String newUser = doc["new_user"] | "";
            String newPass = doc["new_pass"] | "";
            String confirmPass = doc["confirm_pass"] | "";

            if (newPass != confirmPass) {
                req->send(200, "application/json", "{\"ok\":false,\"error\":\"패스워드 불일치\"}");
                return;
            }
            if (newUser.isEmpty()) newUser = _auth ? _auth->user.c_str() : "admin";

            bool ok = _changeAuth(curPass, newUser, newPass);
            req->send(200, "application/json",
                ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"현재 패스워드 오류\"}");
        }
    );
}
