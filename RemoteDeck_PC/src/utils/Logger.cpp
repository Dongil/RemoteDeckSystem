#include "Logger.h"
#include <ArduinoJson.h>

Logger::Logger() {
    _mutex = xSemaphoreCreateMutex();
}

void Logger::log(const char* evt, const char* det) {
    LogEntry entry;
    entry.ts = millis();
    entry.timeStr = _getTime ? _getTime() : "";
    entry.eventStr = evt;
    entry.detailStr = det;

    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (_entries.size() >= MAX_ENTRIES) {
            _entries.erase(_entries.begin());
        }
        _entries.push_back(entry);
        xSemaphoreGive(_mutex);
    }

    Serial.printf("[LOG] %s: %s\n", evt, det);
}

String Logger::toJson() const {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.createNestedArray("logs");

    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (const auto& e : _entries) {
            JsonObject obj = arr.createNestedObject();
            obj["timestamp"] = e.ts;
            obj["time"] = e.timeStr;
            obj["event"] = e.eventStr;
            obj["detail"] = e.detailStr;
        }
        xSemaphoreGive(_mutex);
    }

    String output;
    serializeJson(doc, output);
    return output;
}

void Logger::clear() {
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        _entries.clear();
        xSemaphoreGive(_mutex);
    }
}
