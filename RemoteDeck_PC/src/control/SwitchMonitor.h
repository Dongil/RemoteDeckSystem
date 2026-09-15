#pragma once
#include <Arduino.h>
#include <functional>

// Design Ref: RemoteDeck_PC_v2.6 §5.1 — PCMonitor 패턴을 GPIO2 접점 입력용으로 미러링.
// INPUT_PULLUP 사용, LOW = active (스위치 ON / 광커플러 도통 → GND).
// Edge-triggered onChange: 상태 유지 중 재발화 없음.
class SwitchMonitor {
public:
    void begin(uint8_t pin);
    void loop();

    bool isActive() const { return _currentState; }   // true = LOW = active

    void setPollInterval(uint16_t ms) { _pollMs = ms; }

    using Callback = std::function<void(bool active)>;
    void setOnChange(Callback cb) { _onChange = cb; }

private:
    uint8_t _pin = 0;
    bool _lastState = false;
    bool _currentState = false;
    uint16_t _pollMs = 1000;
    unsigned long _lastPoll = 0;
    uint8_t _debounceCount = 0;
    static constexpr uint8_t DEBOUNCE_THRESHOLD = 3;
    Callback _onChange = nullptr;
};
