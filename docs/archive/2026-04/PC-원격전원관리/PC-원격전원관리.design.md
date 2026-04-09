# PC-원격전원관리 Design Document

## Executive Summary

| Item | Detail |
|------|--------|
| **Feature** | PC-원격전원관리 (RemoteDeck_PC 리뉴얼 + IPSetupTool) |
| **Plan Reference** | `docs/01-plan/features/PC-원격전원관리.plan.md` |
| **Design Date** | 2026-04-06 |

### Value Delivered

| Perspective | Description |
|-------------|-------------|
| **Problem** | 기존 펌웨어에 CEC/IR 등 불필요 코드 혼재, Web 인터페이스 미구현, IP 설정 도구 부재 |
| **Solution** | 모듈화된 PC 전원관리 전용 펌웨어 + AsyncWebServer 대시보드 + UDP Discovery 기반 IPSetupTool |
| **Function UX Effect** | 브라우저에서 실시간 PC 상태 확인/제어/스케줄 설정, 유틸리티로 기기 자동 검색 및 원클릭 설정 |
| **Core Value** | 현장 방문 없는 무인 PC 전원 운영 + 간편한 기기 배포/관리 |

---

## 1. Module Architecture

### 1.1 Module Dependency Diagram

```
main.cpp
  │
  ├── config/
  │   ├── PinConfig.h              (GPIO 핀 상수 정의)
  │   ├── DeviceConfig.h           (설정 구조체)
  │   └── ConfigManager.cpp/h      (SPIFFS 설정 로드/저장)
  │
  ├── control/
  │   ├── RelayController.cpp/h    (릴레이 제어 + 펄스)
  │   ├── PCMonitor.cpp/h          (PC 상태 폴링 + 이벤트)
  │   ├── ScheduleManager.cpp/h    (스케줄 CRUD + 실행)
  │   └── WOLSender.cpp/h          (매직패킷 전송)
  │
  ├── network/
  │   ├── NetworkManager.cpp/h     (Ethernet/WiFi 통합 관리)
  │   ├── MQTTHandler.cpp/h        (MQTT 클라이언트)
  │   ├── UDPDiscovery.cpp/h       (UDP Discovery 서버)
  │   └── NTPSync.cpp/h            (NTP 시간 동기화)
  │
  ├── web/
  │   ├── WebServer.cpp/h          (AsyncWebServer + REST API)
  │   ├── WebSocketHandler.cpp/h   (WebSocket 이벤트 허브)
  │   └── OTAHandler.cpp/h         (펌웨어 OTA 업데이트)
  │
  ├── serial/
  │   └── RS485Handler.cpp/h       (RS485 시리얼 통신)
  │
  └── utils/
      ├── JsonUtils.cpp/h          (JSON 직렬화)
      └── Logger.cpp/h             (이벤트 로그 링버퍼)
```

### 1.2 Module Interaction Flow

```
              ┌──────────────────────────────────────────────┐
              │                 main.cpp                      │
              │  setup() → init all modules                   │
              │  loop()  → tick all modules                   │
              └──────┬───────┬───────┬───────┬───────┬───────┘
                     │       │       │       │       │
          ┌──────────┘       │       │       │       └──────────┐
          ▼                  ▼       ▼       ▼                  ▼
   ┌─────────────┐  ┌──────────┐ ┌─────┐ ┌─────────┐  ┌──────────┐
   │ RS485Handler│  │MQTTHandler│ │WebSrv│ │UDPDiscov│  │PCMonitor │
   │  (receive)  │  │(subscribe)│ │(REST)│ │(listen) │  │(polling) │
   └──────┬──────┘  └────┬─────┘ └──┬──┘ └────┬────┘  └────┬─────┘
          │               │          │          │            │
          └───────┬───────┘          │          │            │
                  ▼                  ▼          │            │
          ┌──────────────┐   ┌─────────────┐   │            │
          │processCommand│   │ REST API    │    │            │
          │  (unified)   │   │ handlers    │    │            │
          └──────┬───────┘   └──────┬──────┘   │            │
                 │                  │           │            │
          ┌──────┴──────────────────┴───────────┘            │
          ▼                                                  │
   ┌─────────────────────────────────────────────────────────┤
   │              Core Controllers                           │
   │  RelayController  ScheduleManager  WOLSender            │
   └─────────────────────┬───────────────────────────────────┘
                         │
                         ▼
                  ┌─────────────┐
                  │ sendStatus()│ → MQTT publish
                  │             │ → RS485 send
                  │             │ → WebSocket push
                  └─────────────┘
```

---

## 2. Detailed Module Design

### 2.1 config/PinConfig.h

기존 main.cpp에 흩어진 `#define`을 하나의 헤더로 통합.

```cpp
#pragma once

// Relay outputs
constexpr uint8_t PIN_RELAY1    = 25;
constexpr uint8_t PIN_RELAY2    = 26;

// PC LED input (inverted logic: LOW = PC ON)
constexpr uint8_t PIN_PCLED     = 4;

// General purpose inputs
constexpr uint8_t PIN_GPIO1     = 12;
constexpr uint8_t PIN_GPIO2     = 14;
constexpr uint8_t PIN_GPIO3     = 15;

// Status LEDs
constexpr uint8_t PIN_STATUS1   = 32;  // System ready
constexpr uint8_t PIN_STATUS2   = 33;  // Network connected

// RS485 UART2
constexpr uint8_t PIN_RS485_RX  = 21;
constexpr uint8_t PIN_RS485_TX  = 22;
constexpr uint32_t RS485_BAUD   = 9600;

// W5500 Ethernet SPI
constexpr uint8_t PIN_ETH_CS    = 5;
constexpr uint8_t PIN_ETH_SCK   = 18;
constexpr uint8_t PIN_ETH_MISO  = 19;
constexpr uint8_t PIN_ETH_MOSI  = 23;
constexpr uint8_t PIN_ETH_INT   = 17;

// Network ports
constexpr uint16_t WEB_PORT       = 5050;
constexpr uint16_t UDP_DISC_PORT  = 5051;
constexpr uint16_t WOL_PORT       = 9;
```

### 2.2 config/DeviceConfig.h

기존 DeviceConfig + ServerConfig를 **하나의 통합 설정 구조체**로 재설계. 기존에는 `deviceconfig.json`과 `serverconfig.json`이 분리되어 있었으나, 관리 편의성을 위해 단일 파일로 통합.

```cpp
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
    std::string version;    // "2.0.0"
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
```

### 2.3 config/ConfigManager.cpp/h

기존 ConfigManager를 리팩터링. 단일 설정 파일 기반으로 변경.

```cpp
#pragma once
#include "DeviceConfig.h"

class ConfigManager {
public:
    static bool load(DeviceConfig& config, const char* path = CONFIG_PATH);
    static bool save(const DeviceConfig& config, const char* path = CONFIG_PATH);
    
    // 개별 섹션만 업데이트 (전체 저장 대신 merge)
    static bool updateNetwork(const NetworkConfig& net);
    static bool updateMQTT(const MQTTConfig& mqtt);
    static bool updateRelay(const RelayConfig& relay);
    static bool updateMonitor(const MonitorConfig& mon);
    static bool updateWOL(const WOLConfig& wol);
    static bool updateNTP(const NTPConfig& ntp);
    
    // 팩토리 리셋 (기본값 복원)
    static void loadDefaults(DeviceConfig& config);
};
```

**구현 핵심:**
- ArduinoJson `DynamicJsonDocument` (2048 bytes)
- `load()`: SPIFFS에서 읽어 구조체로 역직렬화
- `save()`: 구조체를 JSON으로 직렬화하여 SPIFFS에 저장
- `updateXxx()`: load → merge → save 패턴으로 부분 업데이트
- `loadDefaults()`: 하드코딩된 기본값 (IP: 192.168.1.200 등)

### 2.4 control/RelayController.cpp/h

기존 main.cpp에서 `digitalWrite` 직접 호출하던 릴레이 로직을 모듈로 분리.

```cpp
#pragma once
#include <Arduino.h>
#include <functional>

enum class RelayAction { ON, OFF, PULSE_SHORT, PULSE_LONG, PULSE_CUSTOM };

class RelayController {
public:
    void begin(uint8_t pin1, uint8_t pin2);
    void loop();  // 비동기 펄스 완료 체크
    
    void setRelay(uint8_t relay, bool state);
    bool getRelay(uint8_t relay) const;
    void pulse(uint8_t relay, uint16_t durationMs);
    
    void setPulseConfig(uint16_t shortMs, uint16_t longMs);
    
    // 릴레이 상태 변경 시 콜백
    using Callback = std::function<void(uint8_t relay, bool state)>;
    void setOnChange(Callback cb);
    
private:
    uint8_t _pins[2];
    bool _states[2] = {false, false};
    
    // 비동기 펄스 관리
    bool _pulseActive[2] = {false, false};
    unsigned long _pulseEndTime[2] = {0, 0};
    
    uint16_t _pulseShortMs = 500;
    uint16_t _pulseLongMs = 5000;
    
    Callback _onChange = nullptr;
};
```

**구현 핵심:**
- `pulse()`: non-blocking. `_pulseActive` 플래그와 `_pulseEndTime` 설정 후 `loop()`에서 millis() 비교로 자동 OFF
- `loop()`: 매 루프에서 호출, 활성 펄스의 만료 시간 체크
- `setOnChange()`: 릴레이 상태 변경 시 콜백 → main에서 상태 전송 트리거

### 2.5 control/PCMonitor.cpp/h

PCLED 폴링 및 상태 변화 이벤트 감지.

```cpp
#pragma once
#include <Arduino.h>
#include <functional>

class PCMonitor {
public:
    void begin(uint8_t pcledPin);
    void loop();  // 폴링 수행
    
    bool isPCOn() const;
    
    void setPollInterval(uint16_t ms);
    void setAutoNotify(bool enabled);
    
    // 상태 변화 콜백
    using Callback = std::function<void(bool pcOn)>;
    void setOnChange(Callback cb);
    
private:
    uint8_t _pin;
    bool _lastState = false;
    bool _currentState = false;
    bool _autoNotify = true;
    uint16_t _pollMs = 1000;
    unsigned long _lastPoll = 0;
    
    // 디바운스
    uint8_t _debounceCount = 0;
    static constexpr uint8_t DEBOUNCE_THRESHOLD = 3;
    
    Callback _onChange = nullptr;
};
```

**구현 핵심:**
- `loop()`: `millis()` 기반 폴링 주기 체크
- PCLED는 반전 로직: `!digitalRead(pin)` → true = PC ON
- 디바운스: 3회 연속 동일 값 읽어야 상태 변경 확정
- 상태 변경 확정 시 `_onChange` 콜백 호출 → MQTT/RS485/WebSocket으로 알림

### 2.6 control/ScheduleManager.cpp/h

NTP 기반 스케줄 전원관리.

```cpp
#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>

struct Schedule {
    uint8_t id;
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days;       // 비트마스크: Sun=0x01, Mon=0x02, ... Sat=0x40
    std::string action; // "on" | "off" | "toggle"
    uint8_t relay;      // 1 or 2
};

class ScheduleManager {
public:
    void begin(const char* schedulePath);
    void loop();  // 매분 체크
    
    // CRUD
    bool addSchedule(const Schedule& s);
    bool updateSchedule(const Schedule& s);
    bool deleteSchedule(uint8_t id);
    const std::vector<Schedule>& getSchedules() const;
    
    // 실행 콜백
    using ActionCallback = std::function<void(uint8_t relay, const std::string& action)>;
    void setOnAction(ActionCallback cb);
    
    // Persistence
    bool loadFromFile();
    bool saveToFile();
    
    static constexpr uint8_t MAX_SCHEDULES = 8;
    
private:
    std::vector<Schedule> _schedules;
    const char* _filePath;
    uint8_t _lastCheckedMinute = 255;
    
    ActionCallback _onAction = nullptr;
    
    bool shouldExecute(const Schedule& s, uint8_t weekday, uint8_t hour, uint8_t minute) const;
    uint8_t nextId() const;
};
```

**구현 핵심:**
- `loop()`: 분이 변경될 때만 스케줄 체크 (`_lastCheckedMinute` 비교)
- `shouldExecute()`: 현재 요일 비트마스크 & schedule.days, 시간 일치 확인
- `setOnAction()`: 스케줄 발동 시 콜백 → RelayController로 릴레이 제어
- SPIFFS `/schedule.json`에 영구 저장

### 2.7 control/WOLSender.cpp/h

Wake-on-LAN 매직패킷 전송.

```cpp
#pragma once
#include <Arduino.h>

class WOLSender {
public:
    // MAC string "AA:BB:CC:DD:EE:FF" → 매직패킷 전송
    bool send(const char* macStr);
    // 설정된 기본 MAC으로 전송
    bool sendDefault();
    
    void setDefaultMac(const char* macStr);
    
private:
    uint8_t _defaultMac[6] = {0};
    bool _hasDefault = false;
    
    bool parseMac(const char* macStr, uint8_t mac[6]);
    bool sendPacket(const uint8_t mac[6]);
};
```

**구현 핵심:**
- 매직패킷: `0xFF` x 6 + target MAC x 16 = 102 bytes
- UDP 브로드캐스트 `255.255.255.255:9`로 전송
- Ethernet 또는 WiFi UDP 소켓 사용 (현재 활성 인터페이스 감지)

### 2.8 network/NetworkManager.cpp/h

기존 분산된 Ethernet/WiFi 초기화 로직을 통합 관리.

```cpp
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include "config/DeviceConfig.h"

enum class NetInterface { NONE, ETHERNET, WIFI };

class NetworkManager {
public:
    void begin(const NetworkConfig& config);
    void loop();
    
    bool isConnected() const;
    NetInterface activeInterface() const;
    IPAddress localIP() const;
    std::string macAddress() const;
    
    // WiFi 제어 (WIFION/WIFIOFF 명령용)
    std::string connectWiFi(const char* ssid, const char* password);
    void disconnectWiFi();
    
private:
    NetworkConfig _config;
    NetInterface _active = NetInterface::NONE;
    bool _ethConnected = false;
    bool _wifiConnected = false;
    
    bool initEthernet(const NetworkConfig& config);
    bool initWiFi(const NetworkConfig& config);
};
```

**구현 핵심:**
- `begin()`: config.mode에 따라 Ethernet 우선 시도 → 실패 시 WiFi 폴백
- `loop()`: 연결 상태 모니터링, 재연결 시도
- 기존 `ethernet_mqtt.cpp`의 W5500 SPI 초기화 + `MQTTHandler.cpp`의 WiFi 초기화를 통합
- MAC 주소 자동 감지 및 저장

### 2.9 network/MQTTHandler.cpp/h

기존 MQTTHandler + ethernet_mqtt를 **단일 MQTT 핸들러**로 통합. 현재 활성 네트워크 인터페이스에 따라 자동 전환.

```cpp
#pragma once
#include <PubSubClient.h>
#include "config/DeviceConfig.h"
#include "config/StatusConfig.h"

class MQTTHandler {
public:
    void begin(const MQTTConfig& config, const std::string& deviceId);
    void loop();
    
    bool isConnected() const;
    void publish(const char* payload);
    void publishStatus(const StatusConfig& status);
    
    using CommandCallback = std::function<void(const char* payload, unsigned int length)>;
    void setOnCommand(CommandCallback cb);
    
    void disconnect();
    void reconnect();
    
private:
    MQTTConfig _config;
    std::string _deviceId;
    PubSubClient* _client = nullptr;
    Client* _netClient = nullptr;  // EthernetClient or WiFiClient
    
    CommandCallback _onCommand = nullptr;
    
    int _retryCount = 0;
    static constexpr int MAX_RETRIES = 5;
    unsigned long _lastRetry = 0;
    static constexpr unsigned long RETRY_INTERVAL = 5000;
    
    void connect();
    std::string resolveTopicTemplate(const std::string& tmpl) const;
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
};
```

**구현 핵심:**
- `begin()`: NetworkManager의 활성 인터페이스에 따라 EthernetClient 또는 WiFiClient 선택
- `resolveTopicTemplate()`: `[device_id]` → 실제 deviceId 치환 (기존 HelpUtils::replaceID 기능 통합)
- `publishStatus()`: StatusConfig를 JSON으로 직렬화하여 발행
- 재연결: 5초 간격, 최대 5회

### 2.10 network/UDPDiscovery.cpp/h

IPSetupTool과 통신하는 UDP Discovery 서버.

```cpp
#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include "config/DeviceConfig.h"

class UDPDiscovery {
public:
    void begin(uint16_t port = 5051);
    void loop();
    
    // 설정 변경 요청 콜백
    using ConfigCallback = std::function<bool(const char* jsonConfig)>;
    void setOnConfigRequest(ConfigCallback cb);
    
    // 재부팅 요청 콜백
    using RebootCallback = std::function<void()>;
    void setOnRebootRequest(RebootCallback cb);
    
private:
    WiFiUDP _udp;     // WiFiUDP는 Ethernet에서도 동작 (ESP32)
    uint16_t _port;
    char _buffer[512];
    
    ConfigCallback _onConfig = nullptr;
    RebootCallback _onReboot = nullptr;
    
    void handleDiscover(IPAddress remoteIP, uint16_t remotePort);
    void handleSetConfig(const char* json, IPAddress remoteIP, uint16_t remotePort);
    void handleReboot(IPAddress remoteIP, uint16_t remotePort);
    void sendResponse(const char* json, IPAddress ip, uint16_t port);
};
```

**프로토콜 상세:**

| Direction | Message | Format |
|-----------|---------|--------|
| Tool → Device | DISCOVER | `{"cmd":"DISCOVER","product":"RemoteDeck"}` |
| Device → Tool | DISCOVER 응답 | `{"cmd":"DISCOVER_ACK","device_id":"...","ip":"...","mac":"...","fw_ver":"2.0.0","product":"RemoteDeck_PC","web_port":5050}` |
| Tool → Device | SET_CONFIG | `{"cmd":"SET_CONFIG","device_id":"...","config":{...전체 또는 부분 설정...}}` |
| Device → Tool | SET_CONFIG 응답 | `{"cmd":"SET_CONFIG_ACK","device_id":"...","result":"ok"}` |
| Tool → Device | REBOOT | `{"cmd":"REBOOT","device_id":"..."}` |
| Device → Tool | REBOOT 응답 | `{"cmd":"REBOOT_ACK","device_id":"..."}` (전송 후 1초 뒤 ESP.restart()) |

### 2.11 network/NTPSync.cpp/h

```cpp
#pragma once
#include <Arduino.h>

class NTPSync {
public:
    void begin(const char* server, const char* timezone);
    bool isSynced() const;
    
    uint8_t getHour() const;
    uint8_t getMinute() const;
    uint8_t getSecond() const;
    uint8_t getWeekday() const;  // 0=Sun, 1=Mon, ... 6=Sat
    
    std::string getTimeString() const;  // "HH:MM:SS"
    std::string getDateString() const;  // "YYYY-MM-DD"
    unsigned long getUptime() const;    // seconds since boot
    
private:
    bool _synced = false;
    unsigned long _bootTime = 0;
};
```

**구현 핵심:**
- ESP32 내장 `configTime(gmtOffset, daylightOffset, ntpServer)` 사용
- `getLocalTime(&timeinfo)` 으로 현재 시간 조회
- 네트워크 연결 후 자동 동기화, 주기적 재동기화 (1시간)

### 2.12 serial/RS485Handler.cpp/h

기존 main.cpp의 RS485 수신 로직을 모듈로 분리.

```cpp
#pragma once
#include <Arduino.h>
#include <functional>

class RS485Handler {
public:
    void begin(uint8_t rxPin, uint8_t txPin, uint32_t baud);
    void loop();  // 수신 데이터 처리
    
    void send(const char* json);
    
    using CommandCallback = std::function<void(const char* json)>;
    void setOnCommand(CommandCallback cb);
    
private:
    HardwareSerial* _serial = nullptr;
    String _buffer;
    CommandCallback _onCommand = nullptr;
    
    static constexpr size_t MAX_BUFFER = 512;
};
```

**구현 핵심:**
- `_serial = &Serial2` (UART2)
- `loop()`: `Serial2.available()` 체크, `\n` 구분자로 메시지 수신 완료 판단
- 수신 완료 시 `_onCommand` 콜백 호출
- `send()`: `Serial2.println(json)`
- 기존 main.cpp의 `serialEvent` 로직과 동일

### 2.13 web/WebServer.cpp/h

AsyncWebServer 기반 REST API 서버.

```cpp
#pragma once
#include <ESPAsyncWebServer.h>
#include "WebSocketHandler.h"

class WebServer {
public:
    void begin(uint16_t port = 5050);
    
    WebSocketHandler& ws();
    
    // API 핸들러 등록을 위한 콜백 타입
    using StatusGetter = std::function<String()>;
    using RelayHandler = std::function<void(uint8_t relay, const String& action, uint16_t duration)>;
    using ConfigGetter = std::function<String()>;
    using ConfigSetter = std::function<bool(const String& json)>;
    using ScheduleGetter = std::function<String()>;
    using ScheduleSetter = std::function<bool(const String& json)>;
    using ScheduleDeleter = std::function<bool(uint8_t id)>;
    using WOLHandler = std::function<bool(const String& mac)>;
    using RebootHandler = std::function<void()>;
    
    void setStatusGetter(StatusGetter cb);
    void setRelayHandler(RelayHandler cb);
    void setConfigGetter(ConfigGetter cb);
    void setConfigSetter(ConfigSetter cb);
    void setScheduleGetter(ScheduleGetter cb);
    void setScheduleSetter(ScheduleSetter cb);
    void setScheduleDeleter(ScheduleDeleter cb);
    void setWOLHandler(WOLHandler cb);
    void setRebootHandler(RebootHandler cb);
    
private:
    AsyncWebServer* _server = nullptr;
    WebSocketHandler _ws;
    
    void setupRoutes();
    void setupStaticFiles();
    void setupAPI();
    void setupOTA();
};
```

**REST API 상세:**

| Method | Endpoint | Request Body | Response |
|--------|----------|-------------|----------|
| GET | `/api/status` | - | `{"pc_on":true,"relay1":false,"relay2":false,"gpio":[1,0,1],"uptime":3600,"ip":"...","mac":"...","fw_ver":"2.0.0","ntp_synced":true,"time":"14:30:00","mqtt_connected":true}` |
| POST | `/api/relay` | `{"relay":1,"action":"on\|off\|pulse","duration":500}` | `{"ok":true,"relay":1,"state":true}` |
| GET | `/api/schedule` | - | `{"schedules":[...]}` |
| POST | `/api/schedule` | `{"id":0,"enabled":true,"hour":9,"minute":0,"days":62,"action":"on","relay":1}` | `{"ok":true,"id":0}` |
| DELETE | `/api/schedule?id=0` | - | `{"ok":true}` |
| GET | `/api/config` | - | 전체 DeviceConfig JSON (password 마스킹) |
| POST | `/api/config` | 부분 또는 전체 설정 JSON | `{"ok":true,"reboot_required":true}` |
| POST | `/api/wol` | `{"mac":"AA:BB:CC:DD:EE:FF"}` | `{"ok":true}` |
| POST | `/api/reboot` | - | `{"ok":true}` (1초 후 재부팅) |
| POST | `/api/ota` | multipart/form-data (firmware.bin) | `{"ok":true,"progress":100}` |
| GET | `/api/log` | - | `{"logs":[{"time":"...","event":"...","detail":"..."}, ...]}` |

**정적 파일 서빙:**
- `/` → `/www/index.html`
- `/style.css` → `/www/style.css`
- `/app.js` → `/www/app.js`
- SPIFFS에서 서빙, gzip 압축 지원 (`Content-Encoding: gzip`)

### 2.14 web/WebSocketHandler.cpp/h

```cpp
#pragma once
#include <ESPAsyncWebServer.h>

class WebSocketHandler {
public:
    void begin(AsyncWebServer* server, const char* path = "/ws");
    void loop();  // cleanup disconnected clients
    
    // 전체 클라이언트에 브로드캐스트
    void broadcastStatus(const char* json);
    void broadcastLog(const char* json);
    void broadcastOTAProgress(uint8_t percent);
    
private:
    AsyncWebSocket* _ws = nullptr;
    
    void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                 AwsEventType type, void* arg, uint8_t* data, size_t len);
};
```

**WebSocket 메시지 포맷:**

```json
// 서버 → 클라이언트: 상태 업데이트
{"type":"status","data":{"pc_on":true,"relay1":false,...}}

// 서버 → 클라이언트: 이벤트 로그
{"type":"log","data":{"time":"14:30:00","event":"RELAY1_ON","detail":"MQTT command"}}

// 서버 → 클라이언트: OTA 진행률
{"type":"ota","data":{"progress":45,"status":"uploading"}}
```

### 2.15 web/OTAHandler.cpp/h

```cpp
#pragma once
#include <ESPAsyncWebServer.h>
#include <Update.h>

class OTAHandler {
public:
    void setup(AsyncWebServer* server, const char* path = "/api/ota");
    
    using ProgressCallback = std::function<void(uint8_t percent)>;
    void setOnProgress(ProgressCallback cb);
    
private:
    ProgressCallback _onProgress = nullptr;
    size_t _totalSize = 0;
    
    void handleUpload(AsyncWebServerRequest* request, const String& filename,
                      size_t index, uint8_t* data, size_t len, bool final);
};
```

**구현 핵심:**
- `AsyncWebServer`의 `on()` 핸들러에서 multipart upload 처리
- `Update.begin()` → chunk 쓰기 → `Update.end()`
- 각 chunk마다 진행률 계산 → WebSocket으로 push
- 완료 후 MD5 검증, 성공 시 1초 후 `ESP.restart()`

### 2.16 utils/Logger.cpp/h

```cpp
#pragma once
#include <Arduino.h>
#include <vector>

struct LogEntry {
    unsigned long timestamp;  // millis()
    std::string time;         // "HH:MM:SS" (NTP)
    std::string event;        // "RELAY1_ON", "PC_STATE_CHANGE", etc.
    std::string detail;       // "MQTT command", "Schedule #3", etc.
};

class Logger {
public:
    void log(const char* event, const char* detail = "");
    const std::vector<LogEntry>& getAll() const;
    String toJson() const;
    void clear();
    
    void setNTPTimeGetter(std::function<std::string()> getter);
    
    static constexpr size_t MAX_ENTRIES = 100;
    
private:
    std::vector<LogEntry> _entries;
    std::function<std::string()> _getTime = nullptr;
};
```

**구현 핵심:**
- 링버퍼: MAX_ENTRIES 초과 시 가장 오래된 엔트리 제거
- NTP 동기화 전에는 millis() 기반 상대 시간 사용
- Serial 디버그 출력 병행

---

## 3. Command Processing (통합 설계)

### 3.1 Unified Command Processor

기존 main.cpp의 `processCommand()`를 모듈화. MQTT, RS485, Web API 모두 같은 처리 로직 공유.

```cpp
// main.cpp 내 구현

void processCommand(const CommandConfig& cmd, const char* source) {
    logger.log("CMD_RECV", (cmd.command + " from " + source).c_str());
    
    if (cmd.command == "RELAY") {
        relayController.setRelay(cmd.sequence, cmd.data == 1);
    }
    else if (cmd.command == "PULSE") {
        relayController.pulse(cmd.sequence, cmd.data);
    }
    else if (cmd.command == "PCLED") {
        sendPCStatus();
    }
    else if (cmd.command == "GETGPIO") {
        sendGPIOStatus();
    }
    else if (cmd.command == "GETSTATUS") {
        sendFullStatus();
    }
    else if (cmd.command == "WOL") {
        if (cmd.message != "null" && !cmd.message.empty()) {
            wolSender.send(cmd.message.c_str());
        } else {
            wolSender.sendDefault();
        }
    }
    else if (cmd.command == "SCHEDULE") {
        handleScheduleCommand(cmd);
    }
    else if (cmd.command == "WIFION") {
        networkManager.connectWiFi(config.network.wifiSsid.c_str(),
                                   config.network.wifiPassword.c_str());
    }
    else if (cmd.command == "WIFIOFF") {
        networkManager.disconnectWiFi();
    }
    else if (cmd.command == "REBOOT") {
        logger.log("REBOOT", source);
        delay(1000);
        ESP.restart();
    }
}
```

### 3.2 Status Response Format

```json
{
    "device_id": "node_1",
    "status": [
        {"type": "RELAY", "sequence": 1, "data": 0},
        {"type": "RELAY", "sequence": 2, "data": 0},
        {"type": "PCLED", "sequence": 1, "data": 1},
        {"type": "GPIO", "sequence": 1, "data": 0},
        {"type": "GPIO", "sequence": 2, "data": 1},
        {"type": "GPIO", "sequence": 3, "data": 0},
        {"type": "ONLINE", "sequence": 1, "data": 1}
    ],
    "message": "192.168.1.100"
}
```

기존 StatusConfig 구조체 및 JSON 직렬화 유지. 호환성 100%.

---

## 4. main.cpp Design

### 4.1 Global Objects

```cpp
#include "config/PinConfig.h"
#include "config/DeviceConfig.h"
#include "config/ConfigManager.h"
#include "config/CommandConfig.h"
#include "config/StatusConfig.h"
#include "control/RelayController.h"
#include "control/PCMonitor.h"
#include "control/ScheduleManager.h"
#include "control/WOLSender.h"
#include "network/NetworkManager.h"
#include "network/MQTTHandler.h"
#include "network/UDPDiscovery.h"
#include "network/NTPSync.h"
#include "web/WebServer.h"
#include "serial/RS485Handler.h"
#include "utils/JsonUtils.h"
#include "utils/Logger.h"

// Global instances
DeviceConfig config;
RelayController relayController;
PCMonitor pcMonitor;
ScheduleManager scheduleManager;
WOLSender wolSender;
NetworkManager networkManager;
MQTTHandler mqttHandler;
UDPDiscovery udpDiscovery;
NTPSync ntpSync;
WebServer webServer;
RS485Handler rs485Handler;
Logger logger;
```

### 4.2 setup() Flow

```
setup()
├── Serial.begin(115200)             // 디버그 시리얼
├── SPIFFS.begin(true)               // 파일시스템 (auto format)
├── ConfigManager::load(config)      // 설정 로드
│
├── // GPIO 초기화
├── relayController.begin(PIN_RELAY1, PIN_RELAY2)
├── relayController.setPulseConfig(config.relay.pulseShortMs, config.relay.pulseLongMs)
├── pcMonitor.begin(PIN_PCLED)
├── pcMonitor.setPollInterval(config.monitor.pcledPollMs)
├── pinMode(PIN_GPIO1/2/3, INPUT)
├── pinMode(PIN_STATUS1/2, OUTPUT)
│
├── // 네트워크 초기화
├── networkManager.begin(config.network)
├── STATUS2 LED = networkManager.isConnected()
│
├── // MQTT 초기화
├── mqttHandler.begin(config.mqtt, config.deviceId)
│
├── // NTP 동기화
├── ntpSync.begin(config.ntp.server, config.ntp.timezone)
│
├── // 서비스 초기화
├── udpDiscovery.begin(UDP_DISC_PORT)
├── webServer.begin(WEB_PORT)
├── scheduleManager.begin(SCHEDULE_PATH)
├── wolSender.setDefaultMac(config.wol.targetMac)
├── rs485Handler.begin(PIN_RS485_RX, PIN_RS485_TX, RS485_BAUD)
│
├── // 콜백 연결
├── mqttHandler.setOnCommand(onMQTTCommand)
├── rs485Handler.setOnCommand(onRS485Command)
├── pcMonitor.setOnChange(onPCStateChange)
├── relayController.setOnChange(onRelayChange)
├── scheduleManager.setOnAction(onScheduleAction)
├── udpDiscovery.setOnConfigRequest(onUDPConfig)
├── udpDiscovery.setOnRebootRequest(onUDPReboot)
├── // WebServer API 콜백 등록...
│
├── // RS485로 ONLINE 상태 전송
├── sendOnlineStatus()
├── STATUS1 = HIGH                   // 부팅 완료
└── logger.log("SYSTEM", "Boot complete")
```

### 4.3 loop() Flow

```
loop()
├── networkManager.loop()       // 네트워크 상태 체크
├── mqttHandler.loop()          // MQTT keep-alive & 수신
├── rs485Handler.loop()         // RS485 수신 체크
├── relayController.loop()      // 펄스 완료 체크
├── pcMonitor.loop()            // PCLED 폴링
├── scheduleManager.loop()      // 스케줄 체크 (매분)
├── udpDiscovery.loop()         // UDP 수신 체크
└── webServer.ws().loop()       // WebSocket cleanup
```

---

## 5. Web UI Design

### 5.1 Single Page Application

단일 `index.html` 파일에 탭 기반 네비게이션. ESP32 SPIFFS 용량 절약.

```
┌─────────────────────────────────────────────────────────┐
│  RemoteDeck PC Power Manager              v2.0.0        │
├──────┬────────┬──────────┬──────────┬──────┬────────────┤
│ Home │ Control│ Schedule │ Settings │ OTA  │    Log     │
├──────┴────────┴──────────┴──────────┴──────┴────────────┤
│                                                         │
│  ┌─── PC Status ──────────┐  ┌─── System Info ────────┐ │
│  │                        │  │                        │ │
│  │    ●  PC: ON           │  │  Device: node_1        │ │
│  │                        │  │  IP: 192.168.1.100     │ │
│  │  Relay1: OFF           │  │  Uptime: 2h 30m        │ │
│  │  Relay2: OFF           │  │  MQTT: Connected       │ │
│  │                        │  │  NTP: 14:30:00         │ │
│  │  GPIO1: LOW            │  │  FW: 2.0.0             │ │
│  │  GPIO2: HIGH           │  │                        │ │
│  │  GPIO3: LOW            │  │                        │ │
│  └────────────────────────┘  └────────────────────────┘ │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 5.2 파일 구조

```
data/www/
├── index.html      (~8KB) - SPA 전체, 모든 탭 포함
├── style.css       (~3KB) - 다크 테마 기본, 반응형
└── app.js          (~6KB) - API 호출, WebSocket, 탭 제어
```

**총 용량 목표**: ~17KB (gzip 후 ~6KB)

### 5.3 app.js 핵심 로직

```javascript
// WebSocket 연결
const ws = new WebSocket(`ws://${location.host}/ws`);
ws.onmessage = (e) => {
    const msg = JSON.parse(e.data);
    switch(msg.type) {
        case 'status': updateDashboard(msg.data); break;
        case 'log':    appendLog(msg.data); break;
        case 'ota':    updateOTAProgress(msg.data); break;
    }
};

// REST API 호출 예시
async function toggleRelay(relay, action) {
    await fetch('/api/relay', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({relay, action})
    });
}

// 초기 로드 시 상태 fetch
fetch('/api/status').then(r => r.json()).then(updateDashboard);
```

---

## 6. IPSetupTool Design

### 6.1 Class Diagram

```
Program.cs (Entry point)
    └── MainForm.cs
            ├── UDPDiscoveryService.cs
            │   ├── DiscoverDevices() : async Task<List<DeviceInfo>>
            │   ├── SendConfig(DeviceInfo, config) : async Task<bool>
            │   └── SendReboot(DeviceInfo) : async Task<bool>
            │
            ├── DeviceConfigService.cs
            │   ├── ParseConfig(json) : DeviceConfig
            │   ├── BuildConfigJson(form) : string
            │   └── TestConnection(ip, port) : async Task<bool>
            │
            └── Models/
                ├── DeviceInfo.cs       {DeviceId, IP, MAC, FwVer, WebPort}
                └── DeviceConfig.cs     {Network, MQTT, Relay, ...}
```

### 6.2 MainForm 이벤트 흐름

```
[기기 검색] Click
    → UDPDiscoveryService.DiscoverDevices()
        → 브로드캐스트 DISCOVER (255.255.255.255:5051)
        → 2초 대기, 응답 수집
        → DataGridView에 표시

DataGridView Row Select
    → 선택된 기기로 GET /api/config 호출
    → 폼 필드에 현재 설정값 표시

[설정 저장 & 재부팅] Click
    → 입력값 유효성 검사 (IP 형식, MAC 형식 등)
    → UDPDiscoveryService.SendConfig()
    → UDPDiscoveryService.SendReboot()
    → 5초 대기 (ProgressBar 표시)
    → 변경된 IP로 TestConnection()
    → 결과 표시

[연결 테스트] Click
    → DeviceConfigService.TestConnection(ip, 5050)
    → GET /api/status 호출
    → 결과 표시 (OK/실패)
```

### 6.3 프로젝트 설정

```xml
<!-- IPSetupTool.csproj -->
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net8.0-windows</TargetFramework>
    <UseWindowsForms>true</UseWindowsForms>
    <PublishSingleFile>true</PublishSingleFile>
    <SelfContained>true</SelfContained>
    <RuntimeIdentifier>win-x64</RuntimeIdentifier>
    <IncludeNativeLibrariesForSelfExtract>true</IncludeNativeLibrariesForSelfExtract>
  </PropertyGroup>
</Project>
```

---

## 7. platformio.ini (Target)

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = partitions.csv
board_build.filesystem = spiffs
monitor_speed = 115200

lib_deps =
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^6.18.5
    adafruit/Ethernet2@^1.0.4
    me-no-dev/ESPAsyncWebServer@^1.2.6
    me-no-dev/AsyncTCP@^1.1.1

; 제거된 라이브러리:
; arduino-libraries/ArduinoHttpClient (미사용)
; z3t0/IRremote (IR 제거)
```

### 7.1 Partition Table (partitions.csv)

```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
spiffs,   data, spiffs,  0x3D0000,0x30000,
```

- app0/app1: 각 1,920KB (Dual OTA)
- SPIFFS: 192KB (Web UI 파일 + 설정 충분)

---

## 8. Migration Guide (기존 코드 → 신규 코드)

### 8.1 제거 대상 파일

| File | Action | Reason |
|------|--------|--------|
| `io/CECHandler.cpp` | DELETE | CEC 기능 제거 |
| `io/CECHandler.h` | DELETE | CEC 기능 제거 |
| `io/PinDefinitionsAndMore.h` | DELETE | IR 핀 정의 제거 |
| `lib/esp-cec-esp8266/` | DELETE (전체 디렉터리) | CEC 라이브러리 |
| `mqtt/Message.txt` | DELETE | 미사용 파일 |

### 8.2 리팩터링 대상

| 기존 파일 | 신규 파일 | 변경 내용 |
|-----------|----------|-----------|
| `config/DeviceConfig.h` | `config/DeviceConfig.h` | 통합 설정 구조체로 재작성 |
| `config/ServerConfig.h` | (제거) | DeviceConfig에 MQTTConfig로 통합 |
| `config/ConfigManager.cpp/h` | `config/ConfigManager.cpp/h` | 단일 파일 기반으로 리팩터링 |
| `config/CommandConfig.h` | `config/CommandConfig.h` | 유지 (약간의 정리) |
| `config/StatusConfig.h` | `config/StatusConfig.h` | 유지 (호환성) |
| `mqtt/MQTTHandler.cpp/h` | `network/MQTTHandler.cpp/h` | Ethernet/WiFi 통합 핸들러 |
| `mqtt/ethernet_mqtt.cpp/h` | (제거) | MQTTHandler에 통합 |
| `utils/JsonUtils.cpp/h` | `utils/JsonUtils.cpp/h` | 통합 설정 대응 + 불필요 메서드 제거 |
| `utils/FileUtils.cpp/h` | (제거) | ConfigManager에 필요 기능 통합 |
| `utils/HelpUtils.cpp/h` | (제거) | MQTTHandler 내부로 통합 |
| `main.cpp` (865줄) | `main.cpp` (~300줄) | CEC/IR 제거, 모듈 위임, 콜백 연결만 |

### 8.3 신규 생성 파일

| File | Purpose |
|------|---------|
| `config/PinConfig.h` | GPIO 핀 상수 통합 정의 |
| `control/RelayController.cpp/h` | 릴레이 제어 모듈 |
| `control/PCMonitor.cpp/h` | PC 상태 감시 모듈 |
| `control/ScheduleManager.cpp/h` | 스케줄 관리 모듈 |
| `control/WOLSender.cpp/h` | WOL 매직패킷 모듈 |
| `network/NetworkManager.cpp/h` | 네트워크 통합 관리 |
| `network/UDPDiscovery.cpp/h` | UDP Discovery 서버 |
| `network/NTPSync.cpp/h` | NTP 동기화 |
| `web/WebServer.cpp/h` | AsyncWebServer + REST API |
| `web/WebSocketHandler.cpp/h` | WebSocket 이벤트 |
| `web/OTAHandler.cpp/h` | OTA 업데이트 |
| `serial/RS485Handler.cpp/h` | RS485 통신 모듈 |
| `utils/Logger.cpp/h` | 이벤트 로그 |
| `data/www/index.html` | Web UI HTML |
| `data/www/style.css` | Web UI CSS |
| `data/www/app.js` | Web UI JavaScript |
| `partitions.csv` | Dual OTA 파티션 |

---

## 9. Implementation Order

구현 순서는 의존성을 고려하여 bottom-up으로 진행.

```
Phase 1: 기반 정리 (Day 1-3)
  1.1 PinConfig.h 생성
  1.2 DeviceConfig.h 재작성 (통합)
  1.3 ConfigManager 리팩터링
  1.4 CEC/IR 코드 및 라이브러리 제거
  1.5 platformio.ini 업데이트 (ESPAsyncWebServer 추가, IR/CEC 제거)
  1.6 컴파일 확인

Phase 2: Core 모듈 (Day 4-6)
  2.1 RelayController (릴레이 제어 + 펄스)
  2.2 PCMonitor (상태 폴링 + 이벤트)
  2.3 RS485Handler (기존 로직 모듈화)
  2.4 Logger (이벤트 로그)
  2.5 main.cpp 리팩터링 (모듈 연결)
  2.6 RS485 기존 명령 동작 확인

Phase 3: Network 모듈 (Day 7-10)
  3.1 NetworkManager (Ethernet/WiFi 통합)
  3.2 MQTTHandler 통합 리팩터링
  3.3 NTPSync 구현
  3.4 UDPDiscovery 서버
  3.5 WOLSender
  3.6 MQTT + RS485 통합 테스트

Phase 4: Web Server + API (Day 11-14)
  4.1 AsyncWebServer 기본 구조
  4.2 WebSocketHandler
  4.3 REST API 엔드포인트 전체 구현
  4.4 OTAHandler
  4.5 API 동작 테스트

Phase 5: Web UI (Day 15-18)
  5.1 index.html (SPA 구조, 탭 네비게이션)
  5.2 style.css (다크 테마, 반응형)
  5.3 app.js (API 연동, WebSocket, OTA)
  5.4 SPIFFS 업로드 및 동작 확인

Phase 6: Schedule + 통합 (Day 19-20)
  6.1 ScheduleManager 구현
  6.2 스케줄 API + Web UI 연동
  6.3 partitions.csv (Dual OTA)
  6.4 전체 통합 테스트

Phase 7: IPSetupTool (Day 21-23)
  7.1 C# WinForms 프로젝트 생성
  7.2 UDPDiscoveryService
  7.3 MainForm UI
  7.4 DeviceConfigService
  7.5 단일 실행파일 빌드 + 테스트
```

---

## 10. Data Flow Diagrams

### 10.1 PC 전원 ON 시나리오 (MQTT 명령)

```
MQTT Broker                ESP32                    PC
    │                        │                       │
    │ ─RELAY,1,1──────────▶ │                       │
    │                        │ processCommand()      │
    │                        │ relayController.setRelay(1,true)
    │                        │ ─GPIO25=HIGH──────▶  │ (전원 버튼 누름)
    │                        │                       │
    │                        │ (500ms delay)          │
    │                        │ ─GPIO25=LOW───────▶  │ (전원 버튼 해제)
    │                        │                       │
    │                        │ relayController→onChange │
    │ ◀─RELAY,1,0 status─── │                       │
    │                        │                       │
    │                        │ pcMonitor.loop()       │
    │                        │ ◀─PCLED=HIGH──────── │ (PC 부팅됨)
    │                        │ pcMonitor→onChange     │
    │ ◀─PCLED,1,1 status─── │                       │
    │                        │ ws.broadcastStatus()   │
    │                        │                       │
```

### 10.2 IPSetupTool 기기 설정 시나리오

```
IPSetupTool                 ESP32
    │                        │
    │ ─UDP DISCOVER────────▶ │ (브로드캐스트 255.255.255.255:5051)
    │                        │
    │ ◀─DISCOVER_ACK──────── │ (유니캐스트 응답)
    │                        │
    │ (사용자가 설정 수정)     │
    │                        │
    │ ─SET_CONFIG────────▶   │ (유니캐스트)
    │                        │ ConfigManager::save()
    │ ◀─SET_CONFIG_ACK─────  │
    │                        │
    │ ─REBOOT──────────────▶ │
    │ ◀─REBOOT_ACK────────── │
    │                        │ delay(1000) → ESP.restart()
    │                        │
    │ (5초 대기)              │ (재부팅 중...)
    │                        │
    │ ─UDP DISCOVER────────▶ │ (새 IP로 재검색)
    │ ◀─DISCOVER_ACK──────── │ (새 IP로 응답)
    │                        │
    │ ─HTTP GET /api/status─▶│ (연결 테스트)
    │ ◀─200 OK + JSON─────── │
    │                        │
```

---

## 11. Memory Budget (ESP32)

| Component | RAM (approx) | Flash (approx) |
|-----------|-------------|----------------|
| Arduino Core + WiFi | ~50KB | ~800KB |
| AsyncWebServer + AsyncTCP | ~15KB | ~80KB |
| PubSubClient (MQTT) | ~2KB | ~15KB |
| ArduinoJson | ~4KB (dynamic) | ~30KB |
| Ethernet2 (W5500) | ~8KB | ~25KB |
| WebSocket (4 clients max) | ~8KB | ~10KB |
| Application logic | ~10KB | ~60KB |
| SPIFFS cache | ~4KB | - |
| **Total** | **~101KB / 320KB** | **~1020KB / 1920KB** |

여유 메모리: RAM ~219KB, Flash ~900KB. 충분.

---

*Document created: 2026-04-06*
*PDCA Phase: Design*
*Plan Reference: docs/01-plan/features/PC-원격전원관리.plan.md*
