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

struct WebRequestConfig {
    bool enabled = false;
    uint16_t timeoutMs = 5000;
    std::string relay1_on;
    std::string relay1_off;
    std::string relay2_on;
    std::string relay2_off;
    std::string pcled_on;
    std::string pcled_off;
    std::string gpio1_high;
    std::string gpio1_low;
    std::string gpio2_high;
    std::string gpio2_low;
    std::string gpio3_high;
    std::string gpio3_low;
    // Design Ref: RemoteDeck_PC_v2.6.1 §3.2 — Attendance URLs (fire pipeline 재사용)
    std::string attendance_on;
    std::string attendance_off;
};

// Design Ref: §3.1 — Attendance 시스템 설정. source = "pcled" | "gpio2"
struct AttendanceConfig {
    bool        enabled = false;
    std::string source  = "pcled";
};

// Design Ref: rcm-hub-v3.0 §3.1 — RCM 허브 연동 설정 (rcm-api handoff/SMARTDECK.md §3)
struct HubConfig {
    bool        enabled   = false;
    std::string host      = "";         // IP 권장 (W5500 Ethernet DNS 불가)
    uint16_t    port      = 8891;
    std::string path      = "/v1/ws";
    bool        tls       = false;
    std::string productId = "";         // 짝 녹화장비와 같은 제품번호
    std::string deviceKey = "";         // tools/device-key.js --role deck
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
    WebRequestConfig webRequest;
    AttendanceConfig attendance;
    HubConfig hub;
};

constexpr const char* CONFIG_PATH    = "/deviceconfig.json";
constexpr const char* SCHEDULE_PATH  = "/schedule.json";
