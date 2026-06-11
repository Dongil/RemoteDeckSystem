#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include <string>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
}

struct LogEntry {
    unsigned long ts;
    String timeStr;
    String eventStr;
    String detailStr;
};

class Logger {
public:
    Logger();
    void log(const char* evt, const char* det = "");
    String toJson() const;
    void clear();

    using TimeGetter = String(*)();
    void setNTPTimeGetter(TimeGetter getter) { _getTime = getter; }

    static constexpr size_t MAX_ENTRIES = 100;

private:
    std::vector<LogEntry> _entries;
    TimeGetter _getTime = nullptr;
    mutable SemaphoreHandle_t _mutex = nullptr;
};
