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

class DeviceConfig {
public:
    std::string deviceID;
    NetworkConfig networkConfig;
    std::string serverURL;
    int rebootTime;    
    int sleepTime;        
    VersionInfo versionInfo;
};