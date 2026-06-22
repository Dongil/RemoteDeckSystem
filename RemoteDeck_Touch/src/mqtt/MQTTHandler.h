#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "MQTTHandler.h"
#include "config/ConfigManager.h"

class MQTTHandler {
public:
    MQTTHandler(PubSubClient& client, DeviceConfig& deviceConfig, ServerConfig& serverConfig);
    void setup();
    void loop();
    void publish(const char* topic, const char* payload);

    bool isWiFiConnected();   // Wi-Fi 연결 상태를 반환
    bool isMQTTConnected();   // MQTT 연결 상태를 반환
    void retryConnect();    // Wifi, Mqtt 연결 횟수 초기화하고 재연결 시도
    void disConnectToMQTT();    // mqtt, wifi 연결 끊기    

    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void xenoMqttPublish(const char* msg);

private:
    PubSubClient& mqtt_client;
    DeviceConfig& deviceConfig;
    ServerConfig& serverConfig;

    int wifiRetryCount = 0;
    int mqttRetryCount = 0;
    const int maxRetries = 5; // WiFi 및 MQTT 재시도 최대 횟수

    void connectToWiFi();
    void connectToMQTT();
    void reconnect();
    void ensureWiFiConnected();
    std::string replaceID(const std::string& template_str, const std::string& device_id);
};

#endif