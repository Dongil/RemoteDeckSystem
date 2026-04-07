#include "Logger.h"
#include <ArduinoJson.h>

void Logger::log(const char* evt, const char* det) {
    LogEntry entry;
    entry.ts = millis();
    entry.timeStr = _getTime ? _getTime() : "";
    entry.eventStr = evt;
    entry.detailStr = det;

    if (_entries.size() >= MAX_ENTRIES) {
        _entries.erase(_entries.begin());
    }
    _entries.push_back(entry);

    Serial.printf("[LOG] %s: %s\n", evt, det);
}

String Logger::toJson() const {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.createNestedArray("logs");

    for (const auto& e : _entries) {
        JsonObject obj = arr.createNestedObject();
        obj["timestamp"] = e.ts;
        obj["time"] = e.timeStr;
        obj["event"] = e.eventStr;
        obj["detail"] = e.detailStr;
    }

    String output;
    serializeJson(doc, output);
    return output;
}
