#pragma once
#include <string>
#include <PubSubClient.h>
#include <functional>
#include "config/DeviceConfig.h"
#include "config/StatusConfig.h"
#include "utils/JsonUtils.h"

class NetworkManager;

class MQTTHandler {
public:
    void begin(const MQTTConfig& config, const std::string& deviceId, Client& netClient);
    void loop();

    bool isConnected();
    void publish(const char* payload);
    void publishStatus(const StatusConfig& status);

    using CommandCallback = std::function<void(const char* payload, unsigned int length)>;
    using ConnectedCallback = std::function<void()>;
    void setOnCommand(CommandCallback cb) { _onCommand = cb; }
    void setOnConnected(ConnectedCallback cb) { _onConnected = cb; }

    void disconnect();

private:
    MQTTConfig _config;
    std::string _deviceId;
    PubSubClient _client;
    IPAddress _brokerIP;
    CommandCallback _onCommand = nullptr;
    ConnectedCallback _onConnected = nullptr;

    int _retryCount = 0;
    static constexpr int MAX_RETRIES = 0; // 0 = unlimited retries
    unsigned long _lastRetry = 0;
    static constexpr unsigned long RETRY_INTERVAL = 10000;

    void connect();
    std::string resolveTopic(const std::string& tmpl) const;
    static MQTTHandler* _instance;
    static void _mqttCallback(char* topic, byte* payload, unsigned int length);
};
