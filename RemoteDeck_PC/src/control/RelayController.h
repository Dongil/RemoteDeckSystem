#pragma once
#include <Arduino.h>
#include <functional>

class RelayController {
public:
    void begin(uint8_t pin1, uint8_t pin2);
    void loop();

    void setRelay(uint8_t relay, bool state);
    bool getRelay(uint8_t relay) const;
    void pulse(uint8_t relay, uint16_t durationMs);

    void setPulseConfig(uint16_t shortMs, uint16_t longMs);
    uint16_t getPulseShort() const { return _pulseShortMs; }
    uint16_t getPulseLong() const { return _pulseLongMs; }

    using Callback = std::function<void(uint8_t relay, bool state)>;
    void setOnChange(Callback cb) { _onChange = cb; }

private:
    uint8_t _pins[2] = {0, 0};
    bool _states[2] = {false, false};
    bool _pulseActive[2] = {false, false};
    unsigned long _pulseEndTime[2] = {0, 0};
    uint16_t _pulseShortMs = 500;
    uint16_t _pulseLongMs = 5000;
    Callback _onChange = nullptr;

    void _applyState(uint8_t idx, bool state);
};
