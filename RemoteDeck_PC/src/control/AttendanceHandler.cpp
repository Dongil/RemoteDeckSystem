#include "AttendanceHandler.h"
#include "network/WebRequestHandler.h"
#include <ArduinoJson.h>
#include <string.h>

void AttendanceHandler::begin(const AttendanceConfig* cfg, WebRequestHandler* wr) {
    _cfg = cfg;
    _wr  = wr;
}

// Plan SC-1/2/3: enabled + source 매칭 확인 후 fire + 링버퍼 push + 시스템 로그.
void AttendanceHandler::onSourceStateChange(const char* sourceKey, bool active) {
    if (!_cfg || !_wr) return;
    if (!_cfg->enabled) return;
    if (strcmp(_cfg->source.c_str(), sourceKey) != 0) return;

    const char* event = active ? "attendance_on" : "attendance_off";
    _wr->fire(event, active ? 1 : 0);
    Serial.printf("Attendance: source=%s active=%d -> %s\n",
                  sourceKey, active ? 1 : 0, event);
    _push(active, -1);
    _logAttend(active ? "STATE" : "STATE", active);
}

void AttendanceHandler::setStateGetters(StateGetter pcled, StateGetter gpio) {
    _getPcled = pcled;
    _getGpio  = gpio;
}

// Plan SC-7: 부팅 후 attendance 초기 상태 1회 fire.
void AttendanceHandler::syncOnBoot() {
    if (!_cfg || !_wr || !_cfg->enabled) return;
    bool active = _sourceActive();
    const char* event = active ? "attendance_on" : "attendance_off";
    _wr->fire(event, active ? 1 : 0);
    Serial.printf("Attendance boot sync: source=%s active=%d -> %s\n",
                  _cfg->source.c_str(), active, event);
    _push(active, -1);
    _logAttend("BOOT", active);
}

const char* AttendanceHandler::currentStateText() const {
    if (!_cfg || !_cfg->enabled) return "unknown";
    return _sourceActive() ? "present" : "absent";
}

// v2.6.2 fix-1: 마지막 fire 이후 HTTP 결과 반영 (attendance_* 이벤트만 필터)
void AttendanceHandler::onFireResult(const char* event, int httpCode) {
    if (!event) return;
    if (strncmp(event, "attendance_", 11) != 0) return;
    if (_count == 0) return;
    // fire→result는 큐 FIFO 순서라 가장 최근 push된 entry가 이 결과의 주인.
    uint8_t idx = (_head + 8 - 1) % 8;
    _history[idx].httpCode = (int16_t)httpCode;

    if (_log) {
        char det[64];
        snprintf(det, sizeof(det), "[%d] %s", httpCode, event);
        _log("ATTEND", det);
    }
}

String AttendanceHandler::toJson() const {
    DynamicJsonDocument doc(1536);
    doc["enabled"] = _cfg ? _cfg->enabled : false;
    doc["source"]  = _cfg ? _cfg->source.c_str() : "";
    doc["current"] = currentStateText();
    JsonArray arr  = doc.createNestedArray("history");
    for (uint8_t i = 0; i < _count; i++) {
        uint8_t idx = (_head + 8 - 1 - i) % 8;
        JsonObject o = arr.createNestedObject();
        o["ts"]     = _history[idx].ts;
        o["time"]   = _history[idx].timeStr;
        o["active"] = _history[idx].active;
        o["http"]   = _history[idx].httpCode;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void AttendanceHandler::_push(bool active, int16_t code) {
    _history[_head].ts       = millis();
    _history[_head].active   = active;
    _history[_head].httpCode = code;
    // v2.6.2 fix-4: 시분초 시각 저장 (NTP getter 있으면 사용)
    if (_getTime) {
        String t = _getTime();
        strlcpy(_history[_head].timeStr, t.c_str(), sizeof(_history[_head].timeStr));
    } else {
        _history[_head].timeStr[0] = '\0';
    }
    _head = (_head + 1) % 8;
    if (_count < 8) _count++;
}

bool AttendanceHandler::_sourceActive() const {
    if (!_cfg) return false;
    if (_cfg->source == "pcled" && _getPcled) return _getPcled();
    if (_cfg->source == "gpio2" && _getGpio)  return _getGpio();
    return false;
}

// v2.6.2 fix-1: 시스템 로그(Logger)에도 재부재 상태 변경 기록
void AttendanceHandler::_logAttend(const char* action, bool active) {
    if (!_log) return;
    char det[64];
    snprintf(det, sizeof(det), "%s %s source=%s",
             action,
             active ? "present" : "absent",
             _cfg ? _cfg->source.c_str() : "");
    _log("ATTEND", det);
}
