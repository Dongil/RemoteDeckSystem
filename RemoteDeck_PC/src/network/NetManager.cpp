#include "NetManager.h"

namespace {
    constexpr uint32_t kEthPreInitDelayMs    = 500;    // W5500 POR settle
    constexpr uint8_t  kEthBeginRetryMax     = 3;
    constexpr uint32_t kEthBeginRetryDelayMs = 1000;
    constexpr uint32_t kEthGotIpTimeoutMs    = 20000;  // GOT_IP watchdog
    constexpr uint16_t kW5500ModeRegAddr     = 0x0000;
    constexpr uint8_t  kW5500ResetBit        = 0x80;
}

NetManager* NetManager::_instance = nullptr;

void NetManager::begin(NetworkConfig& config) {
    _config = &config;
    _instance = this;
    _connected = false;
    _notifiedConnected = false;
    _ethMgmtActive = false;

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) { onNetworkEvent(event, info); });

    if (config.mode == "wifi") {
        initEthManagement();
        initWiFiSTA(config);
    } else {
        initEthernet(config);
    }
}

void NetManager::loop() {
    if (_connected && !_notifiedConnected) {
        _notifiedConnected = true;
        digitalWrite(PIN_STATUS1, HIGH);
        Serial.printf("Network: %s connected, IP: %s\n",
                      _mode == NetMode::ETHERNET ? "Ethernet" : "WiFi",
                      localIP().toString().c_str());
        if (_onConnected) _onConnected();
    }

    if (!_connected && _notifiedConnected) {
        _notifiedConnected = false;
        digitalWrite(PIN_STATUS1, LOW);
        Serial.println("Network: Primary disconnected");
    }

    if (_mode == NetMode::WIFI_STA && !_connected) {
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry > 10000) {
            lastRetry = millis();
            Serial.println("Network: WiFi reconnecting...");
            WiFi.reconnect();
        }
    }

    checkGotIpWatchdog();
}

IPAddress NetManager::localIP() const {
    if (_mode == NetMode::ETHERNET) return ETH.localIP();
    if (_mode == NetMode::WIFI_STA) return WiFi.localIP();
    return IPAddress(0, 0, 0, 0);
}

std::string NetManager::macAddress() const {
    if (_mode == NetMode::ETHERNET && _config) return _config->ethMac;
    if (_mode == NetMode::WIFI_STA && _config) return _config->wifiMac;
    return "";
}

IPAddress NetManager::ethManagementIP() const {
    if (_ethMgmtActive) return ETH.localIP();
    return IPAddress(0, 0, 0, 0);
}

// ─── Ethernet primary mode ─────────────────────────────

void NetManager::initEthernet(NetworkConfig& config) {
    delay(kEthPreInitDelayMs);

    SPI.begin(PIN_ETH_SCK, PIN_ETH_MISO, PIN_ETH_MOSI, PIN_ETH_CS);
    Serial.println("Network: SPI bus init");

    w5500SoftReset();
    delay(50);

    bool ok = false;
    for (uint8_t attempt = 1; attempt <= kEthBeginRetryMax; ++attempt) {
        Serial.printf("Network: ETH.begin() attempt %u/%u\n",
                      (unsigned)attempt, (unsigned)kEthBeginRetryMax);
        if (tryEthBegin()) {
            Serial.println("Network: ETH.begin() OK");
            ok = true;
            break;
        }
        if (attempt < kEthBeginRetryMax) {
            w5500SoftReset();
            delay(kEthBeginRetryDelayMs);
        }
    }

    if (!ok) {
        Serial.println("Network: ETH.begin() exhausted, restarting");
        delay(1000);
        ESP.restart();
        return;
    }

    _ethBeginAt = millis();
    _ethWaitingGotIp = true;
    Serial.printf("Network: Awaiting GOT_IP (timeout %lums)\n",
                  (unsigned long)kEthGotIpTimeoutMs);
}

// W5500 software reset: MR (0x0000) bit7 = 1. Recovers from POR garbage.
void NetManager::w5500SoftReset() {
    SPISettings settings(8000000, MSBFIRST, SPI_MODE0);
    SPI.beginTransaction(settings);
    digitalWrite(PIN_ETH_CS, LOW);
    SPI.transfer((uint8_t)(kW5500ModeRegAddr >> 8));
    SPI.transfer((uint8_t)(kW5500ModeRegAddr & 0xFF));
    SPI.transfer(0x04);              // control: BSB=0, RWB=1 (write), OM=0
    SPI.transfer(kW5500ResetBit);
    digitalWrite(PIN_ETH_CS, HIGH);
    SPI.endTransaction();
}

bool NetManager::tryEthBegin() {
    return ETH.begin(ETH_PHY_W5500, 1, PIN_ETH_CS, PIN_ETH_INT, -1, SPI);
}

// Trigger ESP.restart() if GOT_IP not received within timeout.
void NetManager::checkGotIpWatchdog() {
    if (!_ethWaitingGotIp) return;
    if (millis() - _ethBeginAt <= kEthGotIpTimeoutMs) return;

    Serial.printf("Network: GOT_IP watchdog timeout (%lums), restarting\n",
                  (unsigned long)kEthGotIpTimeoutMs);
    delay(500);
    ESP.restart();
}

// ─── Ethernet management mode (WiFi mode, hardcoded 192.168.1.200) ──

void NetManager::initEthManagement() {
    SPI.begin(PIN_ETH_SCK, PIN_ETH_MISO, PIN_ETH_MOSI, PIN_ETH_CS);
    Serial.println("Network: Starting ETH management (192.168.1.200)...");

    if (!ETH.begin(ETH_PHY_W5500, 1, PIN_ETH_CS, PIN_ETH_INT, -1, SPI)) {
        Serial.println("Network: ETH management begin() failed");
        return;
    }
    _ethMgmtActive = true;
}

// ─── WiFi STA mode ─────────────────────────────────────

void NetManager::initWiFiSTA(NetworkConfig& config) {
    Serial.println("Network: === WiFi STA Init ===");
    Serial.printf("  SSID: '%s'\n", config.wifiSsid.c_str());
    Serial.printf("  Password length: %d\n", (int)config.wifiPassword.length());
    Serial.printf("  DHCP: %s\n", config.wifiDhcp ? "true" : "false");

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str(),
               0, nullptr, true);

    if (!config.wifiDhcp) {
        if (config.wifiIp.empty() || config.wifiIp == "0.0.0.0") {
            Serial.println("  WARNING: Static IP empty - falling back to DHCP");
        } else {
            IPAddress ip = strToIP(config.wifiIp);
            IPAddress gw = strToIP(config.wifiGateway);
            IPAddress sn = strToIP(config.wifiSubnet);
            IPAddress dns = strToIP(config.wifiDns1);
            Serial.printf("  Static IP: %s, GW: %s, SN: %s, DNS: %s\n",
                          ip.toString().c_str(), gw.toString().c_str(),
                          sn.toString().c_str(), dns.toString().c_str());
            WiFi.config(ip, gw, sn, dns);
        }
    }

    Serial.println("  WiFi.begin() called, waiting for connection event...");
}

// ─── Helpers ────────────────────────────────────────────

IPAddress NetManager::strToIP(const std::string& str) {
    int p[4] = {0};
    sscanf(str.c_str(), "%d.%d.%d.%d", &p[0], &p[1], &p[2], &p[3]);
    return IPAddress(p[0], p[1], p[2], p[3]);
}

void NetManager::onNetworkEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (!_instance) return;

    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("Network: ETH Started");
            break;

        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("Network: ETH Link Up");
            if (_instance->_ethMgmtActive) {
                Serial.println("Network: Applying ETH management IP 192.168.1.200");
                ETH.config(IPAddress(192,168,1,200),
                           IPAddress(192,168,1,1),
                           IPAddress(255,255,255,0));
            } else if (_instance->_config && !_instance->_config->ethDhcp) {
                Serial.printf("Network: Applying static IP: %s\n",
                              _instance->_config->ethIp.c_str());
                ETH.config(strToIP(_instance->_config->ethIp),
                           strToIP(_instance->_config->ethGateway),
                           strToIP(_instance->_config->ethSubnet),
                           strToIP(_instance->_config->ethDns1));
            }
            break;

        case ARDUINO_EVENT_ETH_GOT_IP:
            if (_instance->_ethMgmtActive) {
                if (_instance->_config) _instance->_config->ethMac = ETH.macAddress().c_str();
                Serial.printf("Network: ETH Management IP: %s (for IPSetupTool)\n",
                              ETH.localIP().toString().c_str());
            } else {
                _instance->_connected = true;
                _instance->_mode = NetMode::ETHERNET;
                _instance->_ethWaitingGotIp = false;
                if (_instance->_config) _instance->_config->ethMac = ETH.macAddress().c_str();
                Serial.printf("Network: ETH Got IP: %s, MAC: %s\n",
                              ETH.localIP().toString().c_str(), ETH.macAddress().c_str());
            }
            break;

        case ARDUINO_EVENT_ETH_DISCONNECTED:
            if (!_instance->_ethMgmtActive) {
                _instance->_connected = false;
            }
            Serial.println("Network: ETH Link Down");
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("Network: WiFi STA Connected to AP");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            _instance->_connected = true;
            _instance->_mode = NetMode::WIFI_STA;
            if (_instance->_config) _instance->_config->wifiMac = WiFi.macAddress().c_str();
            Serial.printf("Network: WiFi Got IP: %s, MAC: %s\n",
                          WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            _instance->_connected = false;
            {
                uint8_t reason = info.wifi_sta_disconnected.reason;
                static unsigned long lastLog = 0;
                static uint8_t lastReason = 0;
                if (millis() - lastLog > 5000 || reason != lastReason) {
                    lastLog = millis();
                    lastReason = reason;
                    Serial.printf("Network: WiFi Disconnected (reason: %d)\n", reason);
                }
            }
            break;

        default:
            break;
    }
}
