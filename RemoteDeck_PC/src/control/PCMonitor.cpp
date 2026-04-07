#include "PCMonitor.h"

void PCMonitor::begin(uint8_t pcledPin) {
    _pin = pcledPin;
    pinMode(_pin, INPUT);
    _currentState = !digitalRead(_pin);  // inverted logic
    _lastState = _currentState;
}

void PCMonitor::loop() {
    unsigned long now = millis();
    if (now - _lastPoll < _pollMs) return;
    _lastPoll = now;

    bool reading = !digitalRead(_pin);  // inverted: LOW = PC ON

    if (reading != _lastState) {
        _debounceCount++;
        if (_debounceCount >= DEBOUNCE_THRESHOLD) {
            _lastState = reading;
            _currentState = reading;
            _debounceCount = 0;

            Serial.printf("PC State: %s\n", _currentState ? "ON" : "OFF");

            if (_autoNotify && _onChange) {
                _onChange(_currentState);
            }
        }
    } else {
        _debounceCount = 0;
    }
}
