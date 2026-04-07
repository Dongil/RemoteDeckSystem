#include "NTPSync.h"

void NTPSync::begin(const char* server, const char* timezone) {
    configTzTime(timezone, server);
    Serial.printf("NTP: Configured with server=%s, tz=%s\n", server, timezone);

    // Wait briefly for initial sync
    struct tm t;
    for (int i = 0; i < 10; i++) {
        if (getLocalTime(&t, 500)) {
            _synced = true;
            Serial.printf("NTP: Synced - %04d-%02d-%02d %02d:%02d:%02d\n",
                          t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                          t.tm_hour, t.tm_min, t.tm_sec);
            return;
        }
    }
    Serial.println("NTP: Initial sync failed, will retry automatically");
}

bool NTPSync::isSynced() const {
    struct tm t;
    return getLocalTime(&t, 0);
}

bool NTPSync::_getTime(struct tm& t) const {
    return getLocalTime(&t, 0);
}

uint8_t NTPSync::getHour() const {
    struct tm t;
    return _getTime(t) ? t.tm_hour : 0;
}

uint8_t NTPSync::getMinute() const {
    struct tm t;
    return _getTime(t) ? t.tm_min : 0;
}

uint8_t NTPSync::getSecond() const {
    struct tm t;
    return _getTime(t) ? t.tm_sec : 0;
}

uint8_t NTPSync::getWeekday() const {
    struct tm t;
    return _getTime(t) ? t.tm_wday : 0;
}

std::string NTPSync::getTimeString() const {
    struct tm t;
    if (!_getTime(t)) return "--:--:--";
    char buf[9];
    sprintf(buf, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

std::string NTPSync::getDateString() const {
    struct tm t;
    if (!_getTime(t)) return "----/--/--";
    char buf[11];
    sprintf(buf, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return buf;
}
