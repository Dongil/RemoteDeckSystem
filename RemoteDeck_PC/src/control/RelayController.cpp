#include "RelayController.h"

void RelayController::begin(uint8_t pin1, uint8_t pin2) {
    _pins[0] = pin1;
    _pins[1] = pin2;
    pinMode(pin1, OUTPUT);
    pinMode(pin2, OUTPUT);
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
}

void RelayController::loop() {
    unsigned long now = millis();
    for (int i = 0; i < 2; i++) {
        if (_pulseActive[i] && now >= _pulseEndTime[i]) {
            _pulseActive[i] = false;
            _applyState(i, false);
        }
    }
}

void RelayController::setRelay(uint8_t relay, bool state) {
    if (relay < 1 || relay > 2) return;
    uint8_t idx = relay - 1;
    _pulseActive[idx] = false;
    _applyState(idx, state);
}

bool RelayController::getRelay(uint8_t relay) const {
    if (relay < 1 || relay > 2) return false;
    return _states[relay - 1];
}

void RelayController::pulse(uint8_t relay, uint16_t durationMs) {
    if (relay < 1 || relay > 2) return;
    uint8_t idx = relay - 1;
    _applyState(idx, true);
    _pulseActive[idx] = true;
    _pulseEndTime[idx] = millis() + durationMs;
}

void RelayController::setPulseConfig(uint16_t shortMs, uint16_t longMs) {
    _pulseShortMs = shortMs;
    _pulseLongMs = longMs;
}

void RelayController::_applyState(uint8_t idx, bool state) {
    if (_states[idx] == state) return;
    _states[idx] = state;
    digitalWrite(_pins[idx], state ? HIGH : LOW);
    Serial.printf("Relay%d: %s\n", idx + 1, state ? "ON" : "OFF");
    if (_onChange) {
        _onChange(idx + 1, state);
    }
}
