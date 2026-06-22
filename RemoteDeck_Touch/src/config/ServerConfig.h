#pragma once

#include <string>

// MQTT 설정 구조체
struct MQTTConfig {
    std::string url;
    std::string user;
    std::string passwd;
    int port;
    int keepalive;
    std::string pubTopic;
    std::string subTopic;
    std::string pingTopic;
};

// 장치 설정 클래스
class ServerConfig {
public:
    std::string version;
    std::string configUrl;
    std::string imageUrl;
    std::string statusUrl;
    MQTTConfig mqttConfig;
    bool usingHttpRequest;
    int sleepTime;
};