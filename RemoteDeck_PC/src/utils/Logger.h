#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>
#include <string>

struct LogEntry {
    unsigned long ts;
    String timeStr;
    String eventStr;
    String detailStr;
};

class Logger {
public:
    void log(const char* evt, const char* det = "");
    const std::vector<LogEntry>& getAll() const { return _entries; }
    String toJson() const;
    void clear() { _entries.clear(); }

    using TimeGetter = String(*)();
    void setNTPTimeGetter(TimeGetter getter) { _getTime = getter; }

    static constexpr size_t MAX_ENTRIES = 100;

private:
    std::vector<LogEntry> _entries;
    TimeGetter _getTime = nullptr;
};
