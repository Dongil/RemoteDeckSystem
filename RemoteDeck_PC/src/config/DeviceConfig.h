#pragma once
#include <string>

struct NetworkConfig {
    std::string mode;       // "ethernet" | "wifi"
    // Ethernet (ETH.h)
    bool ethDhcp;
    std::string ethIp;
    std::string ethGateway;
    std::string ethSubnet;
    std::string ethDns1;
    std::string ethDns2;
    std::string ethMac;     // read-only, auto-detected
    // WiFi STA
    std::string wifiSsid;
    std::string wifiPassword;
    bool wifiDhcp;
    std::string wifiIp;
    std::string wifiGateway;
    std::string wifiSubnet;
    std::string wifiDns1;
    std::string wifiMac;    // read-only, auto-detected
};

struct MQTTConfig {
    std::string broker;
    uint16_t port;
    std::string user;
    std::string password;
    uint16_t keepalive;
    std::string topicPub;
    std::string topicSub;
    std::string topicPing;
};

struct RelayConfig {
    uint16_t pulseShortMs;
    uint16_t pulseLongMs;
};

struct MonitorConfig {
    uint16_t pcledPollMs;
    bool autoNotify;
};

struct WOLConfig {
    std::string targetMac;
};

struct NTPConfig {
    std::string server;
    std::string timezone;
};

struct FirmwareInfo {
    std::string version;
    std::string date;
};

struct AuthConfig {
    std::string user;   // default: "admin"
    std::string pass;   // default: "12345"
};

struct DeviceConfig {
    std::string deviceId;
    std::string deviceName;  // 사용자가 지정하는 장치 이름
    std::string product;
    NetworkConfig network;
    MQTTConfig mqtt;
    RelayConfig relay;
    MonitorConfig monitor;
    WOLConfig wol;
    NTPConfig ntp;
    FirmwareInfo firmware;
    AuthConfig auth;
};

constexpr const char* CONFIG_PATH    = "/deviceconfig.json";
constexpr const char* SCHEDULE_PATH  = "/schedule.json";
