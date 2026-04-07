#include "RS485Handler.h"

void RS485Handler::begin(uint8_t rxPin, uint8_t txPin, uint32_t baud) {
    _serial = &Serial2;
    _serial->begin(baud, SERIAL_8N1, rxPin, txPin);
    _bufPos = 0;
    Serial.printf("RS485: Initialized on UART2 (RX=%d, TX=%d, %d bps)\n", rxPin, txPin, baud);
}

void RS485Handler::loop() {
    if (!_serial) return;

    while (_serial->available()) {
        char c = _serial->read();

        if (c == '\n' || c == '\r') {
            if (_bufPos > 0) {
                _buffer[_bufPos] = '\0';

                // Trim trailing CR if present
                if (_bufPos > 0 && _buffer[_bufPos - 1] == '\r') {
                    _buffer[_bufPos - 1] = '\0';
                }

                Serial.printf("RS485 recv: %s\n", _buffer);

                if (_onCommand) {
                    _onCommand(_buffer);
                }

                _bufPos = 0;
            }
        } else {
            if (_bufPos < (int)sizeof(_buffer) - 1) {
                _buffer[_bufPos++] = c;
            }
        }
    }
}

void RS485Handler::send(const char* json) {
    if (!_serial) return;
    _serial->println(json);
    Serial.printf("RS485 send: %s\n", json);
}
