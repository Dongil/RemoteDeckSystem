#pragma once
#include <Arduino.h>
#include <functional>
#include "config/DeviceConfig.h"

class WebRequestHandler;  // fwd

// Design Ref: RemoteDeck_PC_v2.6.1 §5.1 — 얕은 stateless dispatcher.
// v2.6.2 §5.1 — 확장: 링버퍼(8) + toJson + syncOnBoot + currentStateText + state getters.
// v2.6.2 fix — Entry.timeStr / Logger bridge / onFireResult (실제 HTTP 결과 반영)
class AttendanceHandler {
public:
    void begin(const AttendanceConfig* cfg, WebRequestHandler* wr);
    void onSourceStateChange(const char* sourceKey, bool active);

    using StateGetter  = std::function<bool()>;
    using TimeGetter   = std::function<String()>;
    using LoggerBridge = std::function<void(const char* cat, const char* det)>;

    void setStateGetters(StateGetter pcled, StateGetter gpio);
    void setTimeGetter(TimeGetter cb)   { _getTime = cb; }
    void setLoggerBridge(LoggerBridge cb) { _log = cb; }

    void syncOnBoot();
    String toJson() const;
    const char* currentStateText() const;   // "present" | "absent" | "unknown"

    // v2.6.2 fix-1: 이벤트 fire의 HTTP 결과가 도착하면 최근 buffer entry의 httpCode 갱신
    void onFireResult(const char* event, int httpCode);

private:
    struct Entry {
        uint32_t ts;         // millis() at push
        char     timeStr[16];// NTP 시각 HH:MM:SS (없으면 빈 문자열)
        bool     active;
        int16_t  httpCode;   // -1 pending, 0+ HTTP response code, -2 begin fail, -11 timeout etc.
    };
    Entry   _history[8];
    uint8_t _head  = 0;
    uint8_t _count = 0;

    const AttendanceConfig* _cfg = nullptr;
    WebRequestHandler*      _wr  = nullptr;
    StateGetter  _getPcled = nullptr;
    StateGetter  _getGpio  = nullptr;
    TimeGetter   _getTime  = nullptr;
    LoggerBridge _log      = nullptr;

    void _push(bool active, int16_t code);
    bool _sourceActive() const;
    void _logAttend(const char* action, bool active);
};
