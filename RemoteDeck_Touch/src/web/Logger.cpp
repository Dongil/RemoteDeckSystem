// Design Ref: §4.1 #11 — Ring buffer 50건 + WebServer setLogger 통합
// Plan SC: FR-03

#include "Logger.h"
#include "WebServer.h"
#include <ArduinoJson.h>
#include <string.h>

void Logger::attach(WebServer* ws) {
    if (!ws) return;
    ws->setLogger([this](const char* ev, const char* dt) { log(ev, dt); });
    ws->setLogJsonGetter([this]() { return buildJson(); });
}

void Logger::log(const char* event, const char* detail) {
    Entry& e = _ring[_head];
    e.ts = millis();
    strncpy(e.event,  event  ? event  : "?", EVENT_MAX  - 1); e.event [EVENT_MAX  - 1] = 0;
    strncpy(e.detail, detail ? detail : "",  DETAIL_MAX - 1); e.detail[DETAIL_MAX - 1] = 0;
    _head = (_head + 1) % MAX_ENTRIES;
    if (_count < MAX_ENTRIES) _count++;
}

String Logger::buildJson() const {
    // 최신 순으로 직렬화 (head-1 부터 역순)
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.createNestedArray("entries");
    if (_count > 0) {
        size_t idx = (_head == 0 ? MAX_ENTRIES : _head) - 1;
        for (size_t i = 0; i < _count; ++i) {
            const Entry& e = _ring[idx];
            JsonObject o = arr.createNestedObject();
            o["ts"]     = e.ts;
            o["event"]  = e.event;
            o["detail"] = e.detail;
            idx = (idx == 0 ? MAX_ENTRIES - 1 : idx - 1);
        }
    }
    doc["count"] = _count;
    String out;
    serializeJson(doc, out);
    return out;
}
