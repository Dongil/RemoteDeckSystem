#include "WOLSender.h"
#include <NetworkUdp.h>
#include "config/PinConfig.h"

bool WOLSender::send(const char* macStr) {
    uint8_t mac[6];
    if (!parseMac(macStr, mac)) {
        Serial.println("WOL: Invalid MAC format");
        return false;
    }
    return sendPacket(mac);
}

bool WOLSender::sendDefault() {
    if (!_hasDefault) {
        Serial.println("WOL: No default MAC set");
        return false;
    }
    return sendPacket(_defaultMac);
}

void WOLSender::setDefaultMac(const std::string& macStr) {
    if (macStr.empty()) {
        _hasDefault = false;
        return;
    }
    _hasDefault = parseMac(macStr.c_str(), _defaultMac);
}

bool WOLSender::parseMac(const char* macStr, uint8_t mac[6]) {
    int vals[6];
    if (sscanf(macStr, "%x:%x:%x:%x:%x:%x",
               &vals[0], &vals[1], &vals[2],
               &vals[3], &vals[4], &vals[5]) != 6) {
        // Try without colons
        if (sscanf(macStr, "%2x%2x%2x%2x%2x%2x",
                   &vals[0], &vals[1], &vals[2],
                   &vals[3], &vals[4], &vals[5]) != 6) {
            return false;
        }
    }
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)vals[i];
    return true;
}

bool WOLSender::sendPacket(const uint8_t mac[6]) {
    // Magic packet: 6 bytes of 0xFF + 16 repetitions of target MAC
    uint8_t packet[102];
    memset(packet, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(&packet[6 + i * 6], mac, 6);
    }

    NetworkUDP udp;
    udp.begin(0);
    udp.beginPacket(IPAddress(255, 255, 255, 255), WOL_PORT);
    udp.write(packet, 102);
    udp.endPacket();
    udp.stop();

    Serial.printf("WOL: Sent magic packet to %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}
