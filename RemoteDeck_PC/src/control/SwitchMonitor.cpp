#include "SwitchMonitor.h"

// Design Ref: §5.2 — INPUT_PULLUP 로 초기화.
// 광커플러 접점이 GND로 당기면 GPIO2 LOW → active (재실).
// 개방 시 pull-up 으로 HIGH → inactive (부재).
void SwitchMonitor::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);
    _currentState = (digitalRead(_pin) == LOW);
    _lastState = _currentState;
}

// Plan SC-1/2/7: 1s poll + 3x debounce + edge-triggered.
// 상태 유지 중 재발화 없음.
void SwitchMonitor::loop() {
    unsigned long now = millis();
    if (now - _lastPoll < _pollMs) return;
    _lastPoll = now;

    bool reading = (digitalRead(_pin) == LOW);

    if (reading != _lastState) {
        _debounceCount++;
        if (_debounceCount >= DEBOUNCE_THRESHOLD) {
            _lastState = reading;
            _currentState = reading;
            _debounceCount = 0;

            Serial.printf("Switch State: %s\n", _currentState ? "ACTIVE (LOW)" : "INACTIVE (HIGH)");

            if (_onChange) {
                _onChange(_currentState);
            }
        }
    } else {
        _debounceCount = 0;
    }
}
