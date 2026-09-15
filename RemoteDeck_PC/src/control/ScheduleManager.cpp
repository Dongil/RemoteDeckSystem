#include "ScheduleManager.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <time.h>

void ScheduleManager::begin(const char* schedulePath) {
    _filePath = schedulePath;
    loadFromFile();
}

void ScheduleManager::loop() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    uint8_t currentMinute = timeinfo.tm_min;
    if (currentMinute == _lastCheckedMinute) return;
    _lastCheckedMinute = currentMinute;

    uint8_t weekday = timeinfo.tm_wday;  // 0=Sun
    uint8_t hour = timeinfo.tm_hour;

    for (const auto& s : _schedules) {
        if (s.enabled && shouldExecute(s, weekday, hour, currentMinute)) {
            // Design Ref: §5.1 — reboot은 별도 콜백으로 dispatch
            if (s.action == "reboot") {
                Serial.printf("Schedule #%d triggered: reboot\n", s.id);
                if (_onReboot) _onReboot();
            } else {
                Serial.printf("Schedule #%d triggered: relay%d %s\n", s.id, s.relay, s.action.c_str());
                if (_onAction) _onAction(s.relay, s.action);
            }
        }
    }
}

bool ScheduleManager::shouldExecute(const Schedule& s, uint8_t weekday, uint8_t hour, uint8_t minute) const {
    uint8_t dayBit = 1 << weekday;
    return (s.days & dayBit) && s.hour == hour && s.minute == minute;
}

bool ScheduleManager::addSchedule(const Schedule& s) {
    if (_schedules.size() >= MAX_SCHEDULES) return false;
    Schedule newS = s;
    newS.id = nextId();
    _schedules.push_back(newS);
    saveToFile();
    return true;
}

bool ScheduleManager::updateSchedule(const Schedule& s) {
    for (auto& existing : _schedules) {
        if (existing.id == s.id) {
            existing = s;
            saveToFile();
            return true;
        }
    }
    return false;
}

bool ScheduleManager::deleteSchedule(uint8_t id) {
    for (auto it = _schedules.begin(); it != _schedules.end(); ++it) {
        if (it->id == id) {
            _schedules.erase(it);
            saveToFile();
            return true;
        }
    }
    return false;
}

uint8_t ScheduleManager::nextId() const {
    uint8_t maxId = 0;
    for (const auto& s : _schedules) {
        if (s.id > maxId) maxId = s.id;
    }
    return maxId + 1;
}

bool ScheduleManager::loadFromFile() {
    File file = SPIFFS.open(_filePath, "r");
    if (!file) {
        Serial.println("Schedule: No schedule file found");
        return false;
    }
    String json = file.readString();
    file.close();
    return fromJson(json);
}

bool ScheduleManager::saveToFile() {
    String json = toJson();
    File file = SPIFFS.open(_filePath, "w");
    if (!file) return false;
    file.print(json);
    file.close();
    return true;
}

String ScheduleManager::toJson() const {
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.createNestedArray("schedules");
    for (const auto& s : _schedules) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = s.id;
        obj["enabled"] = s.enabled;
        obj["hour"] = s.hour;
        obj["minute"] = s.minute;
        obj["days"] = s.days;
        obj["action"] = s.action;
        obj["relay"] = s.relay;
    }
    String output;
    serializeJson(doc, output);
    return output;
}

bool ScheduleManager::fromJson(const String& json) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, json)) return false;

    _schedules.clear();
    JsonArray arr = doc["schedules"].as<JsonArray>();
    for (JsonVariant v : arr) {
        Schedule s;
        s.id = v["id"];
        s.enabled = v["enabled"];
        s.hour = v["hour"];
        s.minute = v["minute"];
        s.days = v["days"];
        s.action = v["action"].as<std::string>();
        s.relay = v["relay"];
        // Design Ref: §5.1 하위호환 — 알 수 없는 action은 skip
        if (s.action != "on" && s.action != "off" && s.action != "toggle" && s.action != "reboot") {
            Serial.printf("Schedule: unknown action '%s', skipping id=%u\n", s.action.c_str(), s.id);
            continue;
        }
        _schedules.push_back(s);
    }
    return true;
}
