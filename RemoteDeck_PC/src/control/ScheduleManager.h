#pragma once
#include <Arduino.h>
#include <vector>
#include <string>
#include <functional>

struct Schedule {
    uint8_t id;
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days;       // bitmask: Sun=0x01, Mon=0x02, Tue=0x04, Wed=0x08, Thu=0x10, Fri=0x20, Sat=0x40
    std::string action; // "on" | "off" | "toggle"
    uint8_t relay;      // 1 or 2
};

class ScheduleManager {
public:
    void begin(const char* schedulePath);
    void loop();

    bool addSchedule(const Schedule& s);
    bool updateSchedule(const Schedule& s);
    bool deleteSchedule(uint8_t id);
    const std::vector<Schedule>& getSchedules() const { return _schedules; }

    using ActionCallback = std::function<void(uint8_t relay, const std::string& action)>;
    void setOnAction(ActionCallback cb) { _onAction = cb; }

    // Design Ref: §5.1 — reboot action은 relay와 관심사 분리
    using RebootCallback = std::function<void()>;
    void setOnReboot(RebootCallback cb) { _onReboot = cb; }

    bool loadFromFile();
    bool saveToFile();

    String toJson() const;
    bool fromJson(const String& json);

    static constexpr uint8_t MAX_SCHEDULES = 8;

private:
    std::vector<Schedule> _schedules;
    const char* _filePath = "/schedule.json";
    uint8_t _lastCheckedMinute = 255;
    ActionCallback _onAction = nullptr;
    RebootCallback _onReboot = nullptr;

    bool shouldExecute(const Schedule& s, uint8_t weekday, uint8_t hour, uint8_t minute) const;
    uint8_t nextId() const;
};
