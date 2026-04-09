#pragma once
#include <Arduino.h>
#include <string>
#include <functional>
#include <IPAddress.h>
#include <WiFi.h>
#include <ETH.h>
#include <SPI.h>
#include "config/DeviceConfig.h"
#include "config/PinConfig.h"

enum class NetMode { NONE, ETHERNET, WIFI_STA };

class NetManager {
public:
    void begin(NetworkConfig& config);
    void loop();

    bool isConnected() const { return _connected; }
    NetMode activeMode() const { return _mode; }
    IPAddress localIP() const;
    std::string macAddress() const;

    // ETH management IP (always 192.168.1.200 in WiFi mode)
    bool isEthManagementActive() const { return _ethMgmtActive; }
    IPAddress ethManagementIP() const;

    WiFiClient& getNetClient() { return _netClient; }

    using ConnectedCallback = std::function<void()>;
    void setOnConnected(ConnectedCallback cb) { _onConnected = cb; }

private:
    NetworkConfig* _config = nullptr;
    NetMode _mode = NetMode::NONE;
    WiFiClient _netClient;
    bool _connected = false;
    bool _notifiedConnected = false;
    bool _ethMgmtActive = false;  // ETH as management interface in WiFi mode

    ConnectedCallback _onConnected = nullptr;

    void initEthernet(NetworkConfig& config);
    void initEthManagement();
    void initWiFiSTA(NetworkConfig& config);
    static IPAddress strToIP(const std::string& str);

    static void onNetworkEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    static NetManager* _instance;
};
