#include "ConfigApi.h"
#include "WebServer.h"
#include "Logger.h"
#include <SPIFFS.h>

void ConfigApi::attach(TouchWebServer* ws, Logger* logger) {
    _ws = ws;
    _logger = logger;
    if (!_ws) return;

    ::WebServer& s = _ws->server();
    s.on("/api/config", HTTP_GET, [this]() { handleGetDeviceConfig(); });
    s.on("/api/config", HTTP_POST, [this]() { handlePostDeviceConfig(); });
    s.on("/api/serverconfig", HTTP_GET, [this]() { handleGetServerConfig(); });
    s.on("/api/serverconfig", HTTP_POST, [this]() { handlePostServerConfig(); });
    s.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
}

void ConfigApi::log(const char* event, const char* detail) {
    extern void logger_add(Logger* l, const char* e, const char* d);
    logger_add(_logger, event, detail);
}

void ConfigApi::streamSpiffsJson(const char* path) {
    File f = SPIFFS.open(path, "r");
    if (!f) { _ws->server().send(404); return; }
    _ws->server().streamFile(f, "application/json");
    f.close();
}

// POST body 를 받아서 SPIFFS 파일에 저장
bool ConfigApi::acceptUploadAsJson(const char* targetPath, const char* logEvent) {
    if (!_ws) return false;
    if (!_ws->server().hasArg("plain")) {
        _ws->server().send(400, "application/json",
            "{\"ok\":false,\"error\":\"missing JSON body\"}");
        return false;
    }
    String body = _ws->server().arg("plain");
    if (body.length() == 0 || body.length() > 4096) {
        _ws->server().send(400, "application/json", "{\"ok\":false,\"error\":\"bad size\"}");
        return false;
    }
    File f = SPIFFS.open(targetPath, FILE_WRITE);
    if (!f) {
        _ws->server().send(500, "application/json", "{\"ok\":false,\"error\":\"spiffs write\"}");
        return false;
    }
    size_t w = f.write((const uint8_t*)body.c_str(), body.length());
    f.close();
    if (w != body.length()) {
        _ws->server().send(500, "application/json", "{\"ok\":false,\"error\":\"short write\"}");
        return false;
    }
    log(logEvent, targetPath);
    String resp = "{\"ok\":true,\"bytes\":" + String((unsigned)w) +
                  ",\"reboot_required\":true}";
    _ws->server().send(200, "application/json", resp);
    return true;
}

void ConfigApi::handleGetDeviceConfig()  { streamSpiffsJson("/deviceconfig.json"); }
void ConfigApi::handleGetServerConfig()  { streamSpiffsJson("/serverconfig.json"); }
void ConfigApi::handlePostDeviceConfig() { acceptUploadAsJson("/deviceconfig.json", "CFG_DEVICE"); }
void ConfigApi::handlePostServerConfig() { acceptUploadAsJson("/serverconfig.json", "CFG_SERVER"); }

void ConfigApi::handleReboot() {
    if (!_ws) return;
    _ws->server().send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
    log("REBOOT", "via /api/reboot");
    delay(500);
    ESP.restart();
}
