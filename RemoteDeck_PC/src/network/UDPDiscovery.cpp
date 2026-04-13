#include "UDPDiscovery.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ETH.h>
#include "config/PinConfig.h"

void UDPDiscovery::begin(uint16_t port, const DeviceConfig* config) {
    _port = port;
    _config = config;
    _started = false;
    startUDP();
}

void UDPDiscovery::loop() {
    if (!_started) { startUDP(); if (!_started) return; }

    int packetSize = _udp.parsePacket();
    if (packetSize <= 0) return;

    int len = _udp.read(_buffer, sizeof(_buffer) - 1);
    if (len <= 0) return;
    _buffer[len] = '\0';

    handlePacket(len, _udp.remoteIP(), _udp.remotePort());
}

void UDPDiscovery::startUDP() {
    if (_started) return;
    if (_udp.begin(_port)) {
        _started = true;
        Serial.printf("UDPDiscovery: Listening on port %d\n", _port);
    }
}

bool UDPDiscovery::checkAuth(const char* user, const char* pass) {
    if (!_auth) return true;  // No auth configured
    if (!user || !pass) return false;
    return std::string(user) == _auth->user && std::string(pass) == _auth->pass;
}

void UDPDiscovery::handlePacket(int len, IPAddress remoteIP, uint16_t remotePort) {
    Serial.printf("UDPDiscovery: Packet from %s:%d (%d bytes)\n",
                  remoteIP.toString().c_str(), remotePort, len);

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, _buffer)) {
        Serial.printf("UDPDiscovery: Invalid JSON (%d bytes)\n", len);
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "DISCOVER") == 0) {
        StaticJsonDocument<512> resp;
        resp["cmd"] = "DISCOVER_ACK";
        resp["device_id"] = _config->deviceId;
        // Report the best available IP
        IPAddress reportIP;
        if (_config->network.mode == "wifi" && WiFi.localIP() != INADDR_NONE) {
            reportIP = WiFi.localIP();
        } else {
            reportIP = ETH.localIP();  // ETH primary or management
        }
        resp["ip"] = reportIP.toString();
        resp["mac"] = _config->network.mode == "wifi" ?
                      _config->network.wifiMac : _config->network.ethMac;
        resp["fw_ver"] = _config->firmware.version;
        resp["product"] = _config->product;
        resp["web_port"] = WEB_PORT;
        resp["net_mode"] = _config->network.mode;
        if (_config->network.mode == "wifi") {
            resp["eth_ip"] = ETH.localIP().toString();  // management IP for IPSetupTool
        }

        char respBuf[512];
        serializeJson(resp, respBuf);
        sendResponse(respBuf, remoteIP, remotePort);
        Serial.printf("UDPDiscovery: DISCOVER_ACK sent to %s\n", remoteIP.toString().c_str());

    } else if (strcmp(cmd, "GET_CONFIG") == 0) {
        const char* targetId = doc["device_id"];
        if (targetId && (std::string(targetId) == _config->deviceId ||
                         std::string(targetId) == "unknown")) {
            String configJson = _onGetConfig ? _onGetConfig() : "{}";
            String resp = "{\"cmd\":\"GET_CONFIG_ACK\",\"device_id\":\"" +
                          String(_config->deviceId.c_str()) + "\",\"config\":" + configJson + "}";
            sendResponse(resp.c_str(), remoteIP, remotePort);
            Serial.printf("UDPDiscovery: GET_CONFIG_ACK sent (%d bytes)\n", resp.length());
        }

    } else if (strcmp(cmd, "SET_CONFIG") == 0) {
        const char* targetId = doc["device_id"];
        if (targetId && (std::string(targetId) == _config->deviceId ||
                         std::string(targetId) == "unknown")) {
            String configJson;
            serializeJson(doc["config"], configJson);

            // Send ACK first (before save, to avoid timeout if save is slow)
            StaticJsonDocument<256> resp;
            resp["cmd"] = "SET_CONFIG_ACK";
            resp["device_id"] = _config->deviceId;
            resp["result"] = "ok";
            char respBuf[256];
            serializeJson(resp, respBuf);
            sendResponse(respBuf, remoteIP, remotePort);

            // Save config after ACK
            bool ok = _onConfig ? _onConfig(configJson.c_str()) : false;
            Serial.printf("UDPDiscovery: SET_CONFIG %s (%d bytes)\n", ok ? "saved" : "FAILED", configJson.length());
            if (!ok) {
                resp["result"] = "error";
                serializeJson(resp, respBuf);
                sendResponse(respBuf, remoteIP, remotePort);
            }
        }

    } else if (strcmp(cmd, "REBOOT") == 0) {
        const char* targetId = doc["device_id"];
        if (targetId && (std::string(targetId) == _config->deviceId ||
                         std::string(targetId) == "unknown")) {
            StaticJsonDocument<128> resp;
            resp["cmd"] = "REBOOT_ACK";
            resp["device_id"] = _config->deviceId;
            char respBuf[128];
            serializeJson(resp, respBuf);
            sendResponse(respBuf, remoteIP, remotePort);

            Serial.println("UDPDiscovery: REBOOT requested");
            if (_onReboot) _onReboot();
        }

    } else if (strcmp(cmd, "AUTH_CHECK") == 0) {
        const char* authUser = doc["auth"]["user"];
        const char* authPass = doc["auth"]["pass"];
        bool ok = checkAuth(authUser, authPass);
        char respBuf[128];
        snprintf(respBuf, sizeof(respBuf),
                 "{\"cmd\":\"AUTH_CHECK_ACK\",\"result\":\"%s\"}", ok ? "ok" : "auth_failed");
        sendResponse(respBuf, remoteIP, remotePort);
        Serial.printf("UDPDiscovery: AUTH_CHECK %s from %s\n", ok ? "OK" : "FAILED", remoteIP.toString().c_str());

    } else if (strcmp(cmd, "CHANGE_AUTH") == 0) {
        const char* targetId = doc["device_id"];
        if (targetId && (std::string(targetId) == _config->deviceId ||
                         std::string(targetId) == "unknown")) {
            // Current auth check
            const char* authUser = doc["auth"]["user"];
            const char* authPass = doc["auth"]["pass"];
            if (!checkAuth(authUser, authPass)) {
                sendResponse("{\"cmd\":\"CHANGE_AUTH_ACK\",\"result\":\"auth_failed\"}", remoteIP, remotePort);
                return;
            }

            const char* newUser = doc["new_auth"]["user"];
            const char* newPass = doc["new_auth"]["pass"];
            bool ok = _onChangeAuth ? _onChangeAuth(newUser ? newUser : "admin", newPass ? newPass : "") : false;

            char respBuf[128];
            snprintf(respBuf, sizeof(respBuf),
                     "{\"cmd\":\"CHANGE_AUTH_ACK\",\"result\":\"%s\"}", ok ? "ok" : "error");
            sendResponse(respBuf, remoteIP, remotePort);
        }
    }
}

void UDPDiscovery::sendResponse(const char* json, IPAddress ip, uint16_t port) {
    _udp.beginPacket(ip, port);
    _udp.write((const uint8_t*)json, strlen(json));
    _udp.endPacket();
}
