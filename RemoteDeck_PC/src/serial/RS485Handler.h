#pragma once
#include <Arduino.h>
#include <functional>

class RS485Handler {
public:
    void begin(uint8_t rxPin, uint8_t txPin, uint32_t baud);
    void loop();

    void send(const char* json);

    using CommandCallback = std::function<void(const char* json)>;
    void setOnCommand(CommandCallback cb) { _onCommand = cb; }

private:
    HardwareSerial* _serial = nullptr;
    char _buffer[512];
    int _bufPos = 0;
    CommandCallback _onCommand = nullptr;
};
