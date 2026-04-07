#include "NetManager.h"
#include <esp_mac.h>

void NetManager::begin(NetworkConfig& config, const std::string& deviceId) {
    _config = &config;

    // 1. Always start WiFi AP (for Web UI - works regardless of Ethernet)
    startWiFiAP(deviceId);

    // 2. Initialize Ethernet via Ethernet2 (reliable on all switches)
    if (config.mode == "ethernet" || config.mode == "") {
        _ethConnected = initEthernet(config);
        if (_ethConnected) {
            Serial.printf("Network: Ethernet connected, IP: %s\n",
                          Ethernet.localIP().toString().c_str());
            digitalWrite(PIN_STATUS1, HIGH);
        } else {
            Serial.println("Network: Ethernet connection failed");
        }
    }
}

void NetManager::loop() {
    // Ethernet2 maintains connection via hardware
    if (_ethConnected) {
        if (Ethernet.localIP() == INADDR_NONE) {
            _ethConnected = false;
            digitalWrite(PIN_STATUS1, LOW);
            Serial.println("Network: Ethernet disconnected");
        }
    }
}

bool NetManager::isEthernetConnected() const {
    return _ethConnected;
}

IPAddress NetManager::ethernetIP() const {
    return _ethConnected ? Ethernet.localIP() : IPAddress(0, 0, 0, 0);
}

std::string NetManager::ethernetMAC() const {
    return _config ? _config->ethMac : "";
}

IPAddress NetManager::wifiAPIP() const {
    return WiFi.softAPIP();
}

void NetManager::startWiFiAP(const std::string& deviceId) {
    _apSSID = "RemoteDeck_" + String(deviceId.c_str());
    String apPassword = "remotedeck";

    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apSSID.c_str(), apPassword.c_str());

    Serial.printf("Network: WiFi AP started, SSID: %s, PW: %s, IP: %s\n",
                  _apSSID.c_str(), apPassword.c_str(),
                  WiFi.softAPIP().toString().c_str());
}

bool NetManager::initEthernet(NetworkConfig& config) {
    SPI.begin(PIN_ETH_SCK, PIN_ETH_MISO, PIN_ETH_MOSI, PIN_ETH_CS);
    Ethernet.init(PIN_ETH_CS);
    delay(100);

    byte mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);

    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    config.ethMac = macStr;

    Serial.printf("Network: ETH initializing (MAC: %s)...\n", macStr);

    unsigned long startTime = millis();

    if (!config.ethDhcp) {
        IPAddress ip = strToIP(config.ethIp);
        IPAddress gw = strToIP(config.ethGateway);
        IPAddress sn = strToIP(config.ethSubnet);
        IPAddress dns = strToIP(config.ethDns1);
        Ethernet.begin(mac, ip, dns, gw, sn);

        // Wait for static IP to be applied
        while (millis() - startTime < 5000) {
            delay(100);
        }
    } else {
        while (Ethernet.begin(mac) == 0 && millis() - startTime < 10000) {
            delay(100);
        }
    }

    return Ethernet.localIP() != INADDR_NONE;
}

IPAddress NetManager::strToIP(const std::string& str) {
    int p[4] = {0};
    sscanf(str.c_str(), "%d.%d.%d.%d", &p[0], &p[1], &p[2], &p[3]);
    return IPAddress(p[0], p[1], p[2], p[3]);
}
