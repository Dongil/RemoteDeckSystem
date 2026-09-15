#pragma once

#include <string>

// 버전 정보 구조체
struct VersionInfo {
    std::string firmwareDate;
    std::string serverConfigVersion;
    std::string imageConfigVersion;
};

// 네트워크 설정 구조체
struct NetworkConfig {
    bool usingEthernet;
    std::string wifiSSID;
    std::string wifiPasswd;
    std::string wifiMAC;
    bool usingStatic;
    std::string staticIP;
    std::string staticGateway;
    std::string staticSubnet;
    std::string staticPrimaryDNS;
    std::string staticSecondaryDNS;
    std::string staticMAC;
};

// v2.7: 야간 화면 끄기 (시간대 기반, 스크린세이버와 별개)
struct NightOffConfig {
    bool enabled = false;
    int startHour = 22;
    int startMinute = 0;
    int endHour = 6;
    int endMinute = 0;
};

// v2.7: 재부팅 스케줄 (단일 주간, NTP 기반 — Design §3.2)
struct RebootScheduleConfig {
    bool enabled = false;
    bool days[7] = { false, false, false, false, false, false, false };  // 0=일 … 6=토
    int hour = 4;
    int minute = 0;
};

class DeviceConfig {
public:
    std::string deviceID;
    NetworkConfig networkConfig;
    std::string serverURL;
    int rebootTime;
    int sleepTime;
    VersionInfo versionInfo;
    bool webConfigMode = false;   // v2.6: 다음 부팅을 웹 설정 모드로 (부팅 시 소비, Design §3.1)
    NightOffConfig nightOff;      // v2.7: 야간 화면 끄기 (Design §3.2b)
    RebootScheduleConfig rebootSchedule;  // v2.7: 재부팅 스케줄 (Design §3.2)
};