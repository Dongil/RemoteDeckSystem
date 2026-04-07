#pragma once
#include <Arduino.h>
#include <string>

class WOLSender {
public:
    bool send(const char* macStr);
    bool sendDefault();
    void setDefaultMac(const std::string& macStr);

private:
    uint8_t _defaultMac[6] = {0};
    bool _hasDefault = false;

    bool parseMac(const char* macStr, uint8_t mac[6]);
    bool sendPacket(const uint8_t mac[6]);
};
