#include "AttendanceHandler.h"
#include "network/WebRequestHandler.h"
#include <string.h>

void AttendanceHandler::begin(const AttendanceConfig* cfg, WebRequestHandler* wr) {
    _cfg = cfg;
    _wr  = wr;
}

// Plan SC-1/2/3: config.enabled + source 매칭 확인 후 fire.
// sourceKey: "pcled" or "gpio2" (main.cpp 콜백에서 전달).
void AttendanceHandler::onSourceStateChange(const char* sourceKey, bool active) {
    if (!_cfg || !_wr) return;
    if (!_cfg->enabled) return;
    if (strcmp(_cfg->source.c_str(), sourceKey) != 0) return;

    const char* event = active ? "attendance_on" : "attendance_off";
    _wr->fire(event, active ? 1 : 0);
    Serial.printf("Attendance: source=%s active=%d -> %s\n",
                  sourceKey, active ? 1 : 0, event);
}
