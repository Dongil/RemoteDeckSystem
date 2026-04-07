#pragma once
#include <string>

struct NetworkConfig {
    std::string mode;       // "ethernet" | "wifi"
    // Ethernet
    bool ethDhcp;
    std::string ethIp;
    std::string ethGateway;
    std::string ethSubnet;
    std::string ethDns1;
    std::string ethDns2;
    std::string ethMac;     // read-only, auto-detected
    // WiFi
    std::string wifiSsid;
    std::string wifiPassword;
    bool wifiDhcp;
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
    uint16_t pulseShortMs;  // default: 500
    uint16_t pulseLongMs;   // default: 5000
};

struct MonitorConfig {
    uint16_t pcledPollMs;   // default: 1000
    bool autoNotify;        // default: true
};

struct WOLConfig {
    std::string targetMac;
};

struct NTPConfig {
    std::string server;     // default: "pool.ntp.org"
    std::string timezone;   // default: "KST-9"
};

struct FirmwareInfo {
    std::string version;
    std::string date;
};

struct DeviceConfig {
    std::string deviceId;
    std::string product;    // "RemoteDeck_PC"
    NetworkConfig network;
    MQTTConfig mqtt;
    RelayConfig relay;
    MonitorConfig monitor;
    WOLConfig wol;
    NTPConfig ntp;
    FirmwareInfo firmware;
};

// SPIFFS paths
constexpr const char* CONFIG_PATH    = "/deviceconfig.json";
constexpr const char* SCHEDULE_PATH  = "/schedule.json";
