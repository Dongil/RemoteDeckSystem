#pragma once
#include <Arduino.h>
#include <NetworkUdp.h>
#include <functional>
#include <string>
#include "config/DeviceConfig.h"

class UDPDiscovery {
public:
    void begin(uint16_t port, const DeviceConfig* config);
    void setAuth(const AuthConfig* auth) { _auth = auth; }
    void loop();

    using ConfigCallback = std::function<bool(const char* jsonConfig)>;
    void setOnConfigRequest(ConfigCallback cb) { _onConfig = cb; }

    using RebootCallback = std::function<void()>;
    void setOnRebootRequest(RebootCallback cb) { _onReboot = cb; }

    using ConfigGetter = std::function<String()>;
    void setOnGetConfig(ConfigGetter cb) { _onGetConfig = cb; }

    using AuthChanger = std::function<bool(const char* newUser, const char* newPass)>;
    void setOnChangeAuth(AuthChanger cb) { _onChangeAuth = cb; }

private:
    NetworkUDP _udp;
    uint16_t _port = 5051;
    const DeviceConfig* _config = nullptr;
    const AuthConfig* _auth = nullptr;
    char _buffer[1460];
    bool _started = false;

    void startUDP();
    void handlePacket(int len, IPAddress remoteIP, uint16_t remotePort);
    void sendResponse(const char* json, IPAddress ip, uint16_t port);
    bool checkAuth(const char* user, const char* pass);

    ConfigCallback _onConfig = nullptr;
    RebootCallback _onReboot = nullptr;
    ConfigGetter _onGetConfig = nullptr;
    AuthChanger _onChangeAuth = nullptr;
};
