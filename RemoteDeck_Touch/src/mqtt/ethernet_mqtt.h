#ifndef ETHERNET_MQTT_H
#define ETHERNET_MQTT_H

#include <SPI.h>
#include <PubSubClient.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#include "config/ConfigManager.h"
#include "config/DeviceConfig.h"
#include "config/ServerConfig.h"

// W5500 핀 설정
#define INT_GPIO  17
#define MISO_GPIO 19
#define MOSI_GPIO 23
#define SCK_GPIO  18
#define W5500_CS_GPIO   5

void mqttEthernet_init();
void mqttEthernet_connect();
void mqttEthernet_reconnect();
void mqttEthernet_loop();
void mqttEthernet_setCallback(MQTT_CALLBACK_SIGNATURE);
void mqttEthernet_publish(const char* msg);
void mqttEthernet_macToStr(const byte* mac, char* strMAC);
void mqttEthernet_disconnect();
bool mqttEthernet_connected();  // 연결여부
std::string mqttEthernet_replaceID(const std::string& template_str, const std::string& device_id);
IPAddress stringToIPAddress(const std::string& str);

#endif // ETHERNET_MQTT_H
