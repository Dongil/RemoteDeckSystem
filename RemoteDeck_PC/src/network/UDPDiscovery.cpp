#include "UDPDiscovery.h"
#include <ArduinoJson.h>
#include "config/PinConfig.h"

void UDPDiscovery::begin(uint16_t port, const DeviceConfig* config) {
    _port = port;
    _config = config;
    _started = false;
    startUDP();
}

void UDPDiscovery::loop() {
    if (!_started) {
        startUDP();
        if (!_started) return;
    }

    int packetSize = _udp.parsePacket();
    if (packetSize <= 0) return;

    int len = _udp.read(_buffer, sizeof(_buffer) - 1);
    if (len <= 0) return;
    _buffer[len] = '\0';

    handlePacket(len, _udp.remoteIP(), _udp.remotePort());
}

void UDPDiscovery::startUDP() {
    if (_started) return;
    if (Ethernet.localIP() == INADDR_NONE) return;

    if (_udp.begin(_port)) {
        _started = true;
        Serial.printf("UDPDiscovery: Listening on port %d\n", _port);
    }
}

void UDPDiscovery::handlePacket(int len, IPAddress remoteIP, uint16_t remotePort) {
    Serial.printf("UDPDiscovery: Packet from %s:%d (%d bytes)\n",
                  remoteIP.toString().c_str(), remotePort, len);

    DynamicJsonDocument doc(2048);
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
        resp["ip"] = Ethernet.localIP().toString();
        resp["mac"] = _config->network.ethMac;
        resp["fw_ver"] = _config->firmware.version;
        resp["product"] = _config->product;
        resp["web_port"] = WEB_PORT;

        char respBuf[512];
        serializeJson(resp, respBuf);
        sendResponse(respBuf, remoteIP, remotePort);
        Serial.printf("UDPDiscovery: DISCOVER_ACK sent to %s\n", remoteIP.toString().c_str());

    } else if (strcmp(cmd, "SET_CONFIG") == 0) {
        const char* targetId = doc["device_id"];
        if (targetId && std::string(targetId) == _config->deviceId) {
            String configJson;
            serializeJson(doc["config"], configJson);

            bool ok = _onConfig ? _onConfig(configJson.c_str()) : false;

            StaticJsonDocument<256> resp;
            resp["cmd"] = "SET_CONFIG_ACK";
            resp["device_id"] = _config->deviceId;
            resp["result"] = ok ? "ok" : "error";

            char respBuf[256];
            serializeJson(resp, respBuf);
            sendResponse(respBuf, remoteIP, remotePort);
        }

    } else if (strcmp(cmd, "GET_CONFIG") == 0) {
        const char* targetId = doc["device_id"];
        // Accept if device_id matches OR is "unknown" (manual IP connection)
        if (targetId && (std::string(targetId) == _config->deviceId ||
                         std::string(targetId) == "unknown")) {
            String configJson = _onGetConfig ? _onGetConfig() : "{}";

            // Send in chunks if needed (UDP max ~1400 bytes)
            String resp = "{\"cmd\":\"GET_CONFIG_ACK\",\"device_id\":\"" +
                          String(_config->deviceId.c_str()) + "\",\"config\":" + configJson + "}";

            sendResponse(resp.c_str(), remoteIP, remotePort);
            Serial.printf("UDPDiscovery: GET_CONFIG_ACK sent to %s (%d bytes)\n",
                          remoteIP.toString().c_str(), resp.length());
        }

    } else if (strcmp(cmd, "REBOOT") == 0) {
        const char* targetId = doc["device_id"];
        if (targetId && std::string(targetId) == _config->deviceId) {
            StaticJsonDocument<128> resp;
            resp["cmd"] = "REBOOT_ACK";
            resp["device_id"] = _config->deviceId;

            char respBuf[128];
            serializeJson(resp, respBuf);
            sendResponse(respBuf, remoteIP, remotePort);

            Serial.println("UDPDiscovery: REBOOT requested");
            if (_onReboot) _onReboot();
        }
    }
}

void UDPDiscovery::sendResponse(const char* json, IPAddress ip, uint16_t port) {
    _udp.beginPacket(ip, port);
    _udp.write((const uint8_t*)json, strlen(json));
    _udp.endPacket();
}
