#ifndef ETHERNET_MQTT_H
#define ETHERNET_MQTT_H

// Design Ref: §2.3, §11.2 C2 — ETH.h + ETH_PHY_W5500 (PC v2.3.0 NetManager 패턴)
// Plan SC: FR-01, FR-02 (AsyncWebServer Ethernet 동작), FR-07 (HTTPClient 호환)

#include <SPI.h>
#include <PubSubClient.h>
#include <ETH.h>           // ESP32 내장 - W5500을 lwIP PHY로 등록
#include <WiFi.h>          // WiFiClient/NetworkClient는 ETH/WiFi 공용 (lwIP socket)
#include <ArduinoJson.h>

#include "config/ConfigManager.h"
#include "config/DeviceConfig.h"
#include "config/ServerConfig.h"

// W5500 핀 - RemoteDeck_PC PinConfig 와 동일
#define INT_GPIO        17
#define MISO_GPIO       19
#define MOSI_GPIO       23
#define SCK_GPIO        18
#define W5500_CS_GPIO    5

void mqttEthernet_init();
void mqttEthernet_connect();
void mqttEthernet_reconnect();
void mqttEthernet_loop();
void mqttEthernet_setCallback(MQTT_CALLBACK_SIGNATURE);
void mqttEthernet_publish(const char* msg);
void mqttEthernet_macToStr(const byte* mac, char* strMAC);
void mqttEthernet_disconnect();
bool mqttEthernet_connected();
std::string mqttEthernet_replaceID(const std::string& template_str, const std::string& device_id);
IPAddress stringToIPAddress(const std::string& str);

#endif // ETHERNET_MQTT_H
