#include "MQTTHandler.h"

MQTTHandler* MQTTHandler::_instance = nullptr;

void MQTTHandler::begin(const MQTTConfig& config, const std::string& deviceId, Client& netClient) {
    _config = config;
    _deviceId = deviceId;
    _instance = this;

    _client.setClient(netClient);
    _client.setServer(_config.broker.c_str(), _config.port);
    _client.setCallback(_mqttCallback);
    _client.setKeepAlive(_config.keepalive);

    if (!_config.broker.empty()) {
        connect();
    }
}

void MQTTHandler::loop() {
    if (_config.broker.empty()) return;

    if (!_client.connected()) {
        unsigned long now = millis();
        if (now - _lastRetry >= RETRY_INTERVAL && _retryCount < MAX_RETRIES) {
            _lastRetry = now;
            connect();
        }
    } else {
        _client.loop();
    }
}

bool MQTTHandler::isConnected() {
    return _client.connected();
}

void MQTTHandler::connect() {
    String clientId = "RD-" + String(_deviceId.c_str()) + "-" + String(random(0xffff), HEX);

    Serial.printf("MQTT: Connecting to %s:%d...", _config.broker.c_str(), _config.port);

    bool connected = false;
    if (!_config.user.empty()) {
        connected = _client.connect(clientId.c_str(), _config.user.c_str(), _config.password.c_str());
    } else {
        connected = _client.connect(clientId.c_str());
    }

    if (connected) {
        Serial.println("connected");
        _retryCount = 0;

        std::string subTopic = resolveTopic(_config.topicSub);
        _client.subscribe(subTopic.c_str());
        Serial.printf("MQTT: Subscribed to %s\n", subTopic.c_str());
    } else {
        Serial.printf("failed (rc=%d)\n", _client.state());
        _retryCount++;
    }
}

void MQTTHandler::publish(const char* payload) {
    if (!_client.connected()) return;
    std::string topic = resolveTopic(_config.topicPub);
    _client.publish(topic.c_str(), payload);
}

void MQTTHandler::publishStatus(const StatusConfig& status) {
    String json;
    if (JsonUtils::serializeStatusConfig(status, json)) {
        publish(json.c_str());
        Serial.printf("MQTT pub: %s\n", json.c_str());
    }
}

void MQTTHandler::disconnect() {
    _client.disconnect();
    _retryCount = 0;
}

std::string MQTTHandler::resolveTopic(const std::string& tmpl) const {
    std::string result = tmpl;
    size_t pos = result.find("[device_id]");
    if (pos != std::string::npos) {
        result.replace(pos, 11, _deviceId);
    }
    return result;
}

void MQTTHandler::_mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance && _instance->_onCommand) {
        // Null-terminate payload
        char* buf = new char[length + 1];
        memcpy(buf, payload, length);
        buf[length] = '\0';

        Serial.printf("MQTT recv [%s]: %s\n", topic, buf);
        _instance->_onCommand(buf, length);

        delete[] buf;
    }
}
