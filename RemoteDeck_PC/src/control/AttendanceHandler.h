#pragma once
#include <Arduino.h>
#include "config/DeviceConfig.h"

class WebRequestHandler;  // fwd

// Design Ref: RemoteDeck_PC_v2.6.1 §5.1 — 얕은 stateless dispatcher.
// pcMonitor / switchMonitor onChange 콜백에서 호출됨.
// config.enabled + source 매칭 시에만 fire("attendance_on"|"attendance_off").
class AttendanceHandler {
public:
    void begin(const AttendanceConfig* cfg, WebRequestHandler* wr);
    void onSourceStateChange(const char* sourceKey, bool active);

private:
    const AttendanceConfig* _cfg = nullptr;
    WebRequestHandler*      _wr  = nullptr;
};
