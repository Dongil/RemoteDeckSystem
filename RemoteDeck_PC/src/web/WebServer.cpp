#include "WebServer.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

void WebServer::begin(uint16_t port) {
    _server = new AsyncWebServer(port);

    _ws.begin(_server);
    _ota.setup(_server);

    setupStaticFiles();
    setupAPI();

    _server->begin();
    Serial.printf("WebServer: Started on port %d\n", port);
}

void WebServer::setupStaticFiles() {
    _server->serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");
}

void WebServer::setupAPI() {
    // GET /api/status
    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (_getStatus) {
            req->send(200, "application/json", _getStatus());
        } else {
            req->send(500, "application/json", "{\"error\":\"not configured\"}");
        }
    });

    // POST /api/relay
    _server->on("/api/relay", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
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

    // GET /api/schedule
    _server->on("/api/schedule", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (_getSchedule) {
            req->send(200, "application/json", _getSchedule());
        } else {
            req->send(200, "application/json", "{\"schedules\":[]}");
        }
    });

    // POST /api/schedule
    _server->on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!_setSchedule) { req->send(500); return; }
            String json((char*)data, len);
            bool ok = _setSchedule(json);
            req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
        }
    );

    // DELETE /api/schedule?id=N
    _server->on("/api/schedule", HTTP_DELETE, [this](AsyncWebServerRequest* req) {
        if (!_deleteSchedule || !req->hasParam("id")) { req->send(400); return; }
        uint8_t id = req->getParam("id")->value().toInt();
        bool ok = _deleteSchedule(id);
        req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
    });

    // GET /api/config
    _server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (_getConfig) {
            req->send(200, "application/json", _getConfig());
        } else {
            req->send(500);
        }
    });

    // POST /api/config
    _server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!_setConfig) { req->send(500); return; }
            String json((char*)data, len);
            bool ok = _setConfig(json);
            req->send(200, "application/json",
                ok ? "{\"ok\":true,\"reboot_required\":true}" : "{\"ok\":false}");
        }
    );

    // POST /api/wol
    _server->on("/api/wol", HTTP_POST, [](AsyncWebServerRequest* req) {},
        NULL,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!_handleWOL) { req->send(500); return; }
            StaticJsonDocument<128> doc;
            if (deserializeJson(doc, data, len)) { req->send(400); return; }
            String mac = doc["mac"] | "";
            bool ok = _handleWOL(mac);
            req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
        }
    );

    // POST /api/reboot
    _server->on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "{\"ok\":true}");
        if (_handleReboot) _handleReboot();
    });

    // GET /api/log
    _server->on("/api/log", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (_getLog) {
            req->send(200, "application/json", _getLog());
        } else {
            req->send(200, "application/json", "{\"logs\":[]}");
        }
    });
}
