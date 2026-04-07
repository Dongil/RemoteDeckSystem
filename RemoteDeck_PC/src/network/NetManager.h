#pragma once
#include <Arduino.h>
#include <string>
#include <functional>
#include <IPAddress.h>
#include <WiFi.h>
#include <SPI.h>
#include <Ethernet2.h>
#include "config/DeviceConfig.h"
#include "config/PinConfig.h"

class NetManager {
public:
    void begin(NetworkConfig& config, const std::string& deviceId);
    void loop();

    bool isEthernetConnected() const;
    IPAddress ethernetIP() const;
    std::string ethernetMAC() const;
    IPAddress wifiAPIP() const;

    // For MQTT (uses Ethernet2's EthernetClient)
    EthernetClient& getEthClient() { return _ethClient; }

    // WiFi AP info
    String getAPSSID() const { return _apSSID; }

private:
    NetworkConfig* _config = nullptr;
    EthernetClient _ethClient;
    bool _ethConnected = false;
    String _apSSID;

    bool initEthernet(NetworkConfig& config);
    void startWiFiAP(const std::string& deviceId);
    static IPAddress strToIP(const std::string& str);
};
