#pragma once
#include <Arduino.h>
#include <string>
#include <time.h>

class NTPSync {
public:
    void begin(const char* server, const char* timezone);
    bool isSynced() const;

    uint8_t getHour() const;
    uint8_t getMinute() const;
    uint8_t getSecond() const;
    uint8_t getWeekday() const;

    std::string getTimeString() const;
    std::string getDateString() const;
    unsigned long getUptime() const { return millis() / 1000; }

private:
    bool _synced = false;
    bool _getTime(struct tm& t) const;
};
