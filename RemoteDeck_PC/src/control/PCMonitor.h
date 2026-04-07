#pragma once
#include <Arduino.h>
#include <functional>

class PCMonitor {
public:
    void begin(uint8_t pcledPin);
    void loop();

    bool isPCOn() const { return _currentState; }

    void setPollInterval(uint16_t ms) { _pollMs = ms; }
    void setAutoNotify(bool enabled) { _autoNotify = enabled; }

    using Callback = std::function<void(bool pcOn)>;
    void setOnChange(Callback cb) { _onChange = cb; }

private:
    uint8_t _pin = 0;
    bool _lastState = false;
    bool _currentState = false;
    bool _autoNotify = true;
    uint16_t _pollMs = 1000;
    unsigned long _lastPoll = 0;
    uint8_t _debounceCount = 0;
    static constexpr uint8_t DEBOUNCE_THRESHOLD = 3;
    Callback _onChange = nullptr;
};
