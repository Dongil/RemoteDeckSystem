#pragma once
#include <Arduino.h>
#include <Ethernet2.h>
#include <functional>
#include <string>
#include "config/DeviceConfig.h"

class UDPDiscovery {
public:
    void begin(uint16_t port, const DeviceConfig* config);
    void loop();

    using ConfigCallback = std::function<bool(const char* jsonConfig)>;
    void setOnConfigRequest(ConfigCallback cb) { _onConfig = cb; }

    using RebootCallback = std::function<void()>;
    void setOnRebootRequest(RebootCallback cb) { _onReboot = cb; }

    using ConfigGetter = std::function<String()>;
    void setOnGetConfig(ConfigGetter cb) { _onGetConfig = cb; }

private:
    EthernetUDP _udp;
    uint16_t _port = 5051;
    const DeviceConfig* _config = nullptr;
    char _buffer[1460];
    bool _started = false;

    void startUDP();
    void handlePacket(int len, IPAddress remoteIP, uint16_t remotePort);
    void sendResponse(const char* json, IPAddress ip, uint16_t port);

    ConfigCallback _onConfig = nullptr;
    RebootCallback _onReboot = nullptr;
    ConfigGetter _onGetConfig = nullptr;
};
