#include "Logger.h"
#include "WebServer.h"
#include <ArduinoJson.h>

void Logger::add(const char* event, const char* detail) {
    Entry& e = _buf[_head];
    e.ts = millis();
    strncpy(e.event, event ? event : "", EVENT_LEN - 1);
    e.event[EVENT_LEN - 1] = 0;
    strncpy(e.detail, detail ? detail : "", DETAIL_LEN - 1);
    e.detail[DETAIL_LEN - 1] = 0;
    _head = (_head + 1) % CAPACITY;
    if (_count < CAPACITY) _count++;
}

String Logger::toJson() const {
    StaticJsonDocument<6144> doc;
    JsonArray arr = doc.createNestedArray("logs");
    // 최신 → 오래된 순으로 (head-1 부터 거꾸로 _count 만큼)
    for (uint8_t i = 0; i < _count; ++i) {
        uint8_t idx = (_head + CAPACITY - 1 - i) % CAPACITY;
        const Entry& e = _buf[idx];
        JsonObject obj = arr.createNestedObject();
        obj["ts"] = e.ts;
        obj["event"] = e.event;
        obj["detail"] = e.detail;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void Logger::attach(TouchWebServer* ws) {
    _ws = ws;
    if (!_ws) return;
    _ws->server().on("/api/log", HTTP_GET, [this]() { handleGet(); });
}

void Logger::handleGet() {
    if (!_ws) return;
    _ws->server().send(200, "application/json", toJson());
}

// Free helper for forward-declared usage in ImageApi etc.
void logger_add(Logger* l, const char* event, const char* detail) {
    if (l) l->add(event, detail);
}
