# PC-원격전원관리 Plan Document

## Executive Summary

| Item | Detail |
|------|--------|
| **Feature** | PC-원격전원관리 (RemoteDeck_PC 리뉴얼 + IPSetupTool) |
| **Start Date** | 2026-04-06 |
| **Estimated Duration** | 3~4 weeks |
| **Level** | Dynamic |

### Value Delivered

| Perspective | Description |
|-------------|-------------|
| **Problem** | 기존 RemoteDeck_PC 펌웨어가 TV 제어(CEC/IR) 등 불필요한 기능이 혼재되어 있고, IP 설정을 위해 매번 시리얼 접속이 필요하여 현장 배포/관리가 비효율적 |
| **Solution** | PC 전원관리 전용 펌웨어로 리뉴얼하고, UDP Discovery 기반 IPSetupTool로 네트워크 설정을 간소화 |
| **Function UX Effect** | Web 대시보드에서 실시간 PC 상태 모니터링, 릴레이 제어, 스케줄 관리, OTA 업데이트까지 원스톱 관리 |
| **Core Value** | 원격에서 PC 전원을 안정적으로 관리하고, 현장 방문 없이 기기 설정/업데이트가 가능한 무인 운영 시스템 |

---

## 1. Background & Objectives

### 1.1 Background

현재 RemoteDeck_PC는 ESP32 기반의 범용 IoT 제어 펌웨어로, HDMI CEC, IR 리모컨, 릴레이, GPIO 모니터링 등 다양한 기능이 하나의 펌웨어에 혼재되어 있다. PC 원격 전원관리 용도로 사용 시 불필요한 코드가 많고, Web 인터페이스가 비활성화 상태이며, IP 설정을 위한 전용 도구도 없다.

### 1.2 Objectives

1. **전용 펌웨어**: CEC/IR 코드를 완전 제거하고, PC 전원관리에 최적화된 경량 펌웨어로 리뉴얼
2. **Web 대시보드**: 포트 5050에서 실시간 상태 모니터링, 릴레이 제어, 설정 변경, OTA 업데이트 제공
3. **IPSetupTool**: UDP 브로드캐스트 기반 기기 검색 및 네트워크 설정 유틸리티 (Win11 단일 실행파일)
4. **확장 기능**: PC 상태 자동 감시, 스케줄 전원관리, WOL, 펌웨어 OTA 업데이트

---

## 2. Scope

### 2.1 In Scope

#### Sub-Project 1: RemoteDeck_PC 펌웨어 리뉴얼

| Category | Items |
|----------|-------|
| **제거** | HDMI CEC 핸들러, IR 리모컨 코드, esp-cec-esp8266 라이브러리, IRremote 라이브러리 |
| **유지/개선** | MQTT 통신, RS485 시리얼 통신, 릴레이 제어 (Relay1, Relay2), GPIO 모니터링, PCLED 입력 감지, W5500 이더넷, WiFi, SPIFFS 설정 관리 |
| **신규 추가** | Web 서버 (포트 5050), UDP Discovery 서버, PC 상태 자동 감시 (폴링+이벤트), 스케줄 전원관리 (NTP 동기화), WOL 매직패킷 전송, 펌웨어 OTA 업데이트, WebSocket 실시간 통신 |

#### Sub-Project 2: IPSetupTool (Win11 유틸리티)

| Category | Items |
|----------|-------|
| **기기 검색** | UDP 브로드캐스트 Discovery (포트 고정, 예: 5051) |
| **설정 항목** | Device ID, 네트워크 설정 (IP/Gateway/Subnet/DNS), MQTT 브로커 설정, Ethernet/WiFi 선택 |
| **동작** | 설정 저장 후 기기 재부팅 명령 전송, 변경된 IP로 재연결 확인 |
| **배포** | 단일 실행파일 (.exe), 설치 불필요 |

### 2.2 Out of Scope

- 멀티 디바이스 허브 관리 (RemoteDeck_Hub) - 향후 별도 프로젝트
- 모바일 앱
- 클라우드 대시보드/모니터링
- 하드웨어 회로 변경 (기존 PCB 그대로 사용)

---

## 3. Architecture Overview

### 3.1 System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    RemoteDeck_PC (ESP32)                     │
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌───────────┐  │
│  │ Web UI   │  │ MQTT     │  │ RS485     │  │ UDP       │  │
│  │ :5050    │  │ Client   │  │ UART2     │  │ Discovery │  │
│  │ +WebSocket│ │          │  │           │  │ :5051     │  │
│  └────┬─────┘  └────┬─────┘  └─────┬─────┘  └─────┬─────┘  │
│       │             │              │               │        │
│  ┌────┴─────────────┴──────────────┴───────────────┴─────┐  │
│  │              Core Controller                          │  │
│  │  ┌─────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │  │
│  │  │ Relay   │ │ PC State │ │ Schedule │ │ WOL      │  │  │
│  │  │ Control │ │ Monitor  │ │ Manager  │ │ Sender   │  │  │
│  │  └─────────┘ └──────────┘ └──────────┘ └──────────┘  │  │
│  │  ┌─────────┐ ┌──────────┐ ┌──────────┐               │  │
│  │  │ Config  │ │ OTA      │ │ NTP      │               │  │
│  │  │ Manager │ │ Updater  │ │ Sync     │               │  │
│  │  └─────────┘ └──────────┘ └──────────┘               │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                             │
│  Hardware I/O:                                              │
│  [RELAY1:GPIO25] [RELAY2:GPIO26] [PCLED:GPIO4]              │
│  [GPIO1:12] [GPIO2:14] [GPIO3:15]                           │
│  [STATUS1:GPIO32] [STATUS2:GPIO33]                          │
│  [W5500:SPI] [RS485:UART2(21,22)]                           │
└─────────────────────────────────────────────────────────────┘
         │                    │
         ▼                    ▼
┌─────────────────┐  ┌─────────────────┐
│  IPSetupTool    │  │  MQTT Broker    │
│  (Win11 .exe)   │  │  / Master PC    │
│  UDP Discovery  │  │  RS485 Serial   │
└─────────────────┘  └─────────────────┘
```

### 3.2 Pin Configuration (변경 없음)

| Function | GPIO | Direction | Note |
|----------|------|-----------|------|
| RELAY1 | 25 | OUTPUT | PC 전원 스위치 연결 |
| RELAY2 | 26 | OUTPUT | 보조 릴레이 |
| PCLED | 4 | INPUT | PC 전원 LED 감지 (반전 로직) |
| GPIO1/2/3 | 12/14/15 | INPUT | 범용 디지털 입력 |
| STATUS1 | 32 | OUTPUT | 시스템 상태 LED |
| STATUS2 | 33 | OUTPUT | 네트워크 상태 LED |
| RS485 RX/TX | 21/22 | UART2 | 9600bps |
| W5500 | 5,18,23,19,17 | SPI | 이더넷 |

---

## 4. Feature Specifications

### 4.1 Core: 릴레이 제어 및 PC 전원 관리

**기존 유지 (개선)**
- RELAY1: PC 전원 스위치 (짧은 펄스 = 전원 토글, 긴 펄스 = 강제 종료)
- RELAY2: 보조 릴레이 (사용자 정의 용도)
- PCLED 입력: PC 전원 ON/OFF 상태 감지

**명령 프로토콜 (유지)**
```json
// Command (수신)
{"command":"RELAY","sequence":1,"data":1,"message":"null"}

// Status (송신)
{"device_id":"node_1","status":[{"type":"RELAY","sequence":1,"data":1}],"message":"null"}
```

**개선 사항**
- 릴레이 펄스 시간 설정 가능 (짧은 펄스: 500ms, 긴 펄스: 5000ms)
- 릴레이 동작 시 디바운스 처리
- 릴레이 상태와 PCLED 상태를 조합한 PC 전원 상태 판단 로직

### 4.2 PC 상태 자동 감시

| Item | Detail |
|------|--------|
| **폴링 주기** | 1초 (설정 가능) |
| **감지 방식** | PCLED(GPIO4) digitalRead, 반전 로직 |
| **이벤트 발생** | 상태 변화 감지 시 즉시 MQTT/RS485로 알림 전송 |
| **상태 값** | 0=PC OFF, 1=PC ON |
| **WebSocket** | 실시간 상태 변경 push |

### 4.3 스케줄 전원관리

| Item | Detail |
|------|--------|
| **NTP 서버** | pool.ntp.org (설정 가능) |
| **Timezone** | Asia/Seoul (UTC+9, 설정 가능) |
| **스케줄 수** | 최대 8개 |
| **스케줄 항목** | 시간(HH:MM), 요일(일~토 비트마스크), 동작(ON/OFF/TOGGLE), 활성화 여부 |
| **저장** | SPIFFS에 JSON 파일로 영구 저장 |
| **Web UI** | 스케줄 추가/수정/삭제/활성화 토글 |

### 4.4 WOL (Wake-on-LAN)

| Item | Detail |
|------|--------|
| **매직 패킷** | 0xFF x6 + MAC x16, UDP 포트 9 |
| **대상 MAC** | Web UI 또는 MQTT 명령으로 설정 |
| **동작** | RELAY 물리 스위치와 WOL 중 선택 가능 |
| **명령** | `{"command":"WOL","sequence":1,"data":0,"message":"AA:BB:CC:DD:EE:FF"}` |

### 4.5 Web 대시보드 (포트 5050)

| Page | Features |
|------|----------|
| **Dashboard** | PC 상태 (ON/OFF), Relay1/2 상태, GPIO 상태, 가동 시간, 네트워크 정보, 실시간 WebSocket 업데이트 |
| **Control** | Relay1/2 ON/OFF/Pulse 버튼, WOL 전송 버튼 |
| **Schedule** | 스케줄 목록, 추가/수정/삭제, 활성화 토글 |
| **Settings** | Device ID, 네트워크 설정, MQTT 설정, 폴링 주기, 릴레이 펄스 시간, NTP/Timezone |
| **OTA** | 펌웨어 파일 업로드, 현재 버전 표시, 업데이트 진행률 |
| **Log** | 최근 이벤트 로그 (메모리 기반, 최대 100건) |

**기술 스택**
- ESP32 AsyncWebServer + WebSocket
- HTML/CSS/JS를 SPIFFS에 저장 (또는 PROGMEM 임베딩)
- REST API: `/api/status`, `/api/relay`, `/api/schedule`, `/api/config`, `/api/wol`, `/api/ota`
- WebSocket: `/ws` (실시간 상태 push)

### 4.6 UDP Discovery 프로토콜

| Item | Detail |
|------|--------|
| **포트** | 5051 (UDP) |
| **Discovery 요청** | `{"cmd":"DISCOVER","product":"RemoteDeck"}` |
| **Discovery 응답** | `{"device_id":"...","ip":"...","mac":"...","fw_ver":"...","product":"RemoteDeck_PC"}` |
| **설정 명령** | `{"cmd":"SET_CONFIG","device_id":"...","config":{...}}` |
| **재부팅 명령** | `{"cmd":"REBOOT","device_id":"..."}` |
| **브로드캐스트** | 255.255.255.255:5051로 전송, 같은 네트워크 내 모든 기기 응답 |

### 4.7 펌웨어 OTA 업데이트

| Item | Detail |
|------|--------|
| **방식** | HTTP 업로드 (Web UI에서 .bin 파일 업로드) |
| **라이브러리** | ESP32 Update 내장 라이브러리 |
| **진행률** | WebSocket으로 실시간 진행률 전송 |
| **롤백** | ESP32 파티션 테이블 dual OTA (app0/app1) |
| **검증** | 업로드 후 MD5 체크섬 검증 |

---

## 5. Communication Protocol (개선)

### 5.1 MQTT (유지 + WOL 추가)

**기존 명령 유지:**
- RELAY, PCLED, GETGPIO, GETSTATUS, WIFION, WIFIOFF

**제거:**
- IR, HDMI (CEC 관련 명령 모두 제거)

**신규 추가:**
| Command | Sequence | Data | Message | Description |
|---------|----------|------|---------|-------------|
| WOL | 1 | 0 | MAC 주소 | WOL 매직패킷 전송 |
| SCHEDULE | 1 | 0~7 | JSON | 스케줄 조회/설정 |
| PULSE | 1 | duration_ms | null | 릴레이1 펄스 (지정 시간) |
| PULSE | 2 | duration_ms | null | 릴레이2 펄스 (지정 시간) |
| REBOOT | 0 | 0 | null | 기기 재부팅 |

### 5.2 RS485 (유지)

- 기존 JSON 프로토콜 그대로 유지
- 신규 명령(WOL, SCHEDULE, PULSE, REBOOT)도 동일 형식으로 지원
- 9600bps, 8N1

### 5.3 Web API (신규)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | 전체 상태 조회 |
| POST | `/api/relay` | 릴레이 제어 `{relay:1, action:"on/off/pulse", duration:500}` |
| GET | `/api/schedule` | 스케줄 목록 조회 |
| POST | `/api/schedule` | 스케줄 추가/수정 |
| DELETE | `/api/schedule/:id` | 스케줄 삭제 |
| GET | `/api/config` | 설정 조회 |
| POST | `/api/config` | 설정 변경 |
| POST | `/api/wol` | WOL 전송 `{mac:"AA:BB:CC:DD:EE:FF"}` |
| POST | `/api/ota` | 펌웨어 업로드 (multipart) |
| POST | `/api/reboot` | 기기 재부팅 |
| WS | `/ws` | WebSocket 실시간 통신 |

---

## 6. IPSetupTool Specifications

### 6.1 기술 스택

| Item | Choice | Reason |
|------|--------|--------|
| **언어** | C# (.NET 8) | 기존 APITestUtility와 동일 스택, WinForms 경험 있음 |
| **UI** | WinForms | 단순한 설정 도구에 적합, 빠른 개발 |
| **배포** | Single-file publish | `dotnet publish -r win-x64 --self-contained -p:PublishSingleFile=true` |
| **네트워크** | System.Net.Sockets (UDP) | .NET 기본 라이브러리, 외부 의존성 없음 |

### 6.2 UI Layout

```
┌─────────────────────────────────────────────┐
│  RemoteDeck IP Setup Tool           [v1.0]  │
├─────────────────────────────────────────────┤
│                                             │
│  [🔍 기기 검색]              [🔄 새로고침]    │
│                                             │
│  ┌─────────────────────────────────────────┐│
│  │ Device ID  │ IP Address    │ MAC        ││
│  │────────────┼───────────────┼────────────││
│  │ node_1     │ 192.168.1.200 │ AA:BB:CC.. ││
│  │ node_2     │ 192.168.1.201 │ DD:EE:FF.. ││
│  └─────────────────────────────────────────┘│
│                                             │
│  ── 선택된 기기 설정 ──────────────────────── │
│  Device ID:    [node_1          ]           │
│  IP Address:   [192.168.1.100   ]           │
│  Subnet Mask:  [255.255.255.0   ]           │
│  Gateway:      [192.168.1.1     ]           │
│  DNS:          [8.8.8.8         ]           │
│  DHCP:         [ ] 사용                      │
│                                             │
│  ── MQTT 설정 ─────────────────────────────  │
│  Broker:       [mqtt.example.com]           │
│  Port:         [1883            ]           │
│  User:         [user            ]           │
│  Password:     [****            ]           │
│                                             │
│  [💾 설정 저장 & 재부팅]    [📡 연결 테스트]   │
│                                             │
│  Status: 기기 검색 완료 (2대 발견)             │
└─────────────────────────────────────────────┘
```

### 6.3 동작 흐름

```
1. 유틸리티 실행
2. [기기 검색] 클릭
   → UDP 브로드캐스트 (255.255.255.255:5051) DISCOVER 전송
   → 2초 대기, 응답 수집
   → 기기 목록 표시
3. 기기 선택
   → 현재 설정값 자동 로드
4. 설정 수정
5. [설정 저장 & 재부팅] 클릭
   → SET_CONFIG 명령 전송 (유니캐스트)
   → 기기 재부팅 대기 (5초)
   → 변경된 IP로 DISCOVER 재시도하여 확인
6. [연결 테스트]
   → 해당 IP로 HTTP GET /api/status 호출하여 응답 확인
```

---

## 7. Implementation Plan

### Phase 1: 펌웨어 코드 정리 (3일)

| # | Task | Priority |
|---|------|----------|
| 1.1 | CEC 관련 코드 제거 (CECHandler.cpp/h, esp-cec-esp8266 lib) | HIGH |
| 1.2 | IR 관련 코드 제거 (IR 명령 처리, IRremote lib, PinDefinitionsAndMore.h) | HIGH |
| 1.3 | main.cpp에서 CEC/IR 관련 로직 제거 및 정리 | HIGH |
| 1.4 | platformio.ini에서 불필요한 라이브러리 제거 | HIGH |
| 1.5 | 명령 프로토콜에서 IR/HDMI 명령 제거 | MEDIUM |
| 1.6 | 코드 구조 정리 및 컴파일 확인 | HIGH |

### Phase 2: 핵심 기능 개선 (3일)

| # | Task | Priority |
|---|------|----------|
| 2.1 | PC 상태 자동 감시 모듈 (PCLED 폴링 + 이벤트 발생) | HIGH |
| 2.2 | 릴레이 펄스 제어 (짧은/긴 펄스, 설정 가능 duration) | HIGH |
| 2.3 | MQTT 신규 명령 추가 (WOL, PULSE, SCHEDULE, REBOOT) | HIGH |
| 2.4 | RS485 신규 명령 추가 | MEDIUM |
| 2.5 | 상태 메시지 포맷 정리 | MEDIUM |

### Phase 3: 네트워크 서비스 (4일)

| # | Task | Priority |
|---|------|----------|
| 3.1 | UDP Discovery 서버 구현 (포트 5051) | HIGH |
| 3.2 | NTP 시간 동기화 모듈 | HIGH |
| 3.3 | WOL 매직패킷 전송 모듈 | MEDIUM |
| 3.4 | AsyncWebServer 기본 구조 (포트 5050) | HIGH |
| 3.5 | REST API 엔드포인트 구현 | HIGH |
| 3.6 | WebSocket 실시간 통신 | HIGH |

### Phase 4: Web 대시보드 UI (4일)

| # | Task | Priority |
|---|------|----------|
| 4.1 | Dashboard 페이지 (상태 모니터링) | HIGH |
| 4.2 | Control 페이지 (릴레이/WOL 제어) | HIGH |
| 4.3 | Schedule 페이지 (스케줄 관리) | MEDIUM |
| 4.4 | Settings 페이지 (설정 변경) | HIGH |
| 4.5 | OTA 페이지 (펌웨어 업데이트) | MEDIUM |
| 4.6 | Log 페이지 (이벤트 로그) | LOW |
| 4.7 | SPIFFS 파일 업로드 또는 PROGMEM 임베딩 | MEDIUM |

### Phase 5: 스케줄 전원관리 (2일)

| # | Task | Priority |
|---|------|----------|
| 5.1 | 스케줄 데이터 구조 및 SPIFFS 저장 | HIGH |
| 5.2 | 스케줄 실행 엔진 (NTP 기반 시간 체크) | HIGH |
| 5.3 | 스케줄 CRUD API | MEDIUM |

### Phase 6: OTA 업데이트 (2일)

| # | Task | Priority |
|---|------|----------|
| 6.1 | OTA 업로드 핸들러 (multipart 처리) | HIGH |
| 6.2 | 업데이트 진행률 WebSocket push | MEDIUM |
| 6.3 | MD5 체크섬 검증 | MEDIUM |
| 6.4 | 파티션 테이블 dual OTA 설정 | HIGH |

### Phase 7: IPSetupTool 개발 (3일)

| # | Task | Priority |
|---|------|----------|
| 7.1 | C# WinForms 프로젝트 생성 | HIGH |
| 7.2 | UDP Discovery 클라이언트 구현 | HIGH |
| 7.3 | 기기 목록 UI 및 설정 폼 | HIGH |
| 7.4 | SET_CONFIG / REBOOT 명령 전송 | HIGH |
| 7.5 | 연결 테스트 (HTTP) | MEDIUM |
| 7.6 | 단일 실행파일 빌드 설정 | MEDIUM |

### Phase 8: 통합 테스트 및 마무리 (2일)

| # | Task | Priority |
|---|------|----------|
| 8.1 | 펌웨어 전체 기능 통합 테스트 | HIGH |
| 8.2 | IPSetupTool ↔ 펌웨어 연동 테스트 | HIGH |
| 8.3 | MQTT/RS485 기존 호환성 테스트 | HIGH |
| 8.4 | Web UI 크로스 브라우저 테스트 | MEDIUM |
| 8.5 | 문서 정리 (README, 배선도) | LOW |

---

## 8. Technology Stack

### 8.1 RemoteDeck_PC 펌웨어

| Category | Technology |
|----------|-----------|
| **Platform** | ESP32 (esp32dev) |
| **Framework** | Arduino (PlatformIO) |
| **Web Server** | ESPAsyncWebServer + AsyncTCP |
| **WebSocket** | ESPAsyncWebServer 내장 |
| **MQTT** | PubSubClient 2.8 |
| **JSON** | ArduinoJson 6.x |
| **Ethernet** | Ethernet2 (W5500) |
| **NTP** | ESP32 configTime (내장) |
| **OTA** | ESP32 Update (내장) |
| **파일시스템** | SPIFFS (설정, 웹 파일) |

### 8.2 IPSetupTool

| Category | Technology |
|----------|-----------|
| **Language** | C# (.NET 8) |
| **UI Framework** | WinForms |
| **Network** | System.Net.Sockets (UDP), System.Net.Http |
| **JSON** | System.Text.Json |
| **Build** | Single-file self-contained publish |

---

## 9. Configuration Structure (개선)

### 9.1 deviceconfig.json (개선)

```json
{
  "device_id": "node_1",
  "product": "RemoteDeck_PC",
  "network": {
    "mode": "ethernet",
    "ethernet": {
      "dhcp": false,
      "ip": "192.168.1.200",
      "gateway": "192.168.1.1",
      "subnet": "255.255.255.0",
      "dns1": "8.8.8.8",
      "dns2": "8.8.4.4"
    },
    "wifi": {
      "ssid": "",
      "password": "",
      "dhcp": true
    }
  },
  "mqtt": {
    "broker": "mqtt.example.com",
    "port": 1883,
    "user": "",
    "password": "",
    "keepalive": 120,
    "topic_pub": "RemoteDeck/server",
    "topic_sub": "RemoteDeck/node_1",
    "topic_ping": "RemoteDeck/ping"
  },
  "relay": {
    "pulse_short_ms": 500,
    "pulse_long_ms": 5000
  },
  "monitor": {
    "pcled_poll_interval_ms": 1000,
    "auto_notify": true
  },
  "wol": {
    "target_mac": ""
  },
  "ntp": {
    "server": "pool.ntp.org",
    "timezone": "KST-9"
  },
  "firmware": {
    "version": "2.0.0",
    "date": "2026-04-06"
  }
}
```

### 9.2 schedule.json (신규)

```json
{
  "schedules": [
    {
      "id": 0,
      "enabled": true,
      "time": "09:00",
      "days": 62,
      "action": "on",
      "relay": 1
    }
  ]
}
```
> `days`: 비트마스크 (일=1, 월=2, 화=4, 수=8, 목=16, 금=32, 토=64). 62 = 월~금

---

## 10. Risk & Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| ESP32 메모리 부족 (Web UI + MQTT + WebSocket) | HIGH | MEDIUM | PROGMEM 사용, 웹 파일 gzip 압축, AsyncWebServer 사용 |
| SPIFFS 용량 부족 (Web 파일) | MEDIUM | LOW | LittleFS로 전환 고려, 파일 minify/gzip |
| OTA 중 전원 끊김 | HIGH | LOW | Dual OTA 파티션으로 롤백 보장 |
| UDP Discovery 보안 (인증 없음) | MEDIUM | LOW | 로컬 네트워크 전용, 설정 변경 시 기기 물리 버튼 확인 고려 |
| WOL 대상 MAC 잘못 설정 | LOW | MEDIUM | Web UI에서 MAC 형식 검증 |

---

## 11. Success Criteria

| Criteria | Metric |
|----------|--------|
| PC 전원 ON/OFF | RELAY1으로 PC 전원 토글 성공률 100% |
| PC 상태 감지 | PCLED 상태 변화 감지 후 1초 이내 알림 |
| Web 대시보드 | 모든 페이지 정상 동작, 실시간 상태 업데이트 |
| MQTT 호환성 | 기존 RELAY, PCLED, GETGPIO, GETSTATUS 명령 정상 동작 |
| RS485 호환성 | 기존 프로토콜과 100% 호환 |
| IPSetupTool | 기기 검색 → 설정 변경 → 재부팅 → 재연결 전체 흐름 성공 |
| 스케줄 동작 | NTP 동기화 후 설정 시간에 정확히 릴레이 동작 |
| OTA 업데이트 | Web UI에서 펌웨어 업로드 후 정상 재부팅 |
| 단일 실행파일 | IPSetupTool이 설치 없이 Win11에서 실행 |

---

## 12. File Structure (Target)

```
RemoteDeckSystem/
├── RemoteDeck_PC/
│   ├── platformio.ini
│   ├── partitions.csv              (dual OTA 파티션)
│   ├── src/
│   │   ├── main.cpp                (메인 루프, 초기화)
│   │   ├── config/
│   │   │   ├── ConfigManager.cpp/h (설정 로드/저장)
│   │   │   ├── DeviceConfig.h      (설정 구조체)
│   │   │   └── PinConfig.h         (GPIO 핀 정의)
│   │   ├── control/
│   │   │   ├── RelayController.cpp/h   (릴레이 제어 + 펄스)
│   │   │   ├── PCMonitor.cpp/h         (PC 상태 감시)
│   │   │   ├── ScheduleManager.cpp/h   (스케줄 관리)
│   │   │   └── WOLSender.cpp/h         (WOL 매직패킷)
│   │   ├── network/
│   │   │   ├── MQTTHandler.cpp/h       (MQTT 클라이언트)
│   │   │   ├── EthernetManager.cpp/h   (W5500 이더넷)
│   │   │   ├── WiFiManager.cpp/h       (WiFi 관리)
│   │   │   ├── UDPDiscovery.cpp/h      (UDP Discovery 서버)
│   │   │   └── NTPSync.cpp/h           (NTP 동기화)
│   │   ├── web/
│   │   │   ├── WebServer.cpp/h         (AsyncWebServer + API)
│   │   │   ├── WebSocketHandler.cpp/h  (WebSocket 이벤트)
│   │   │   └── OTAHandler.cpp/h        (OTA 업데이트)
│   │   ├── serial/
│   │   │   └── RS485Handler.cpp/h      (RS485 통신)
│   │   └── utils/
│   │       ├── JsonUtils.cpp/h         (JSON 유틸)
│   │       └── Logger.cpp/h            (이벤트 로그)
│   └── data/                       (SPIFFS)
│       ├── www/                    (Web UI 파일)
│       │   ├── index.html
│       │   ├── style.css
│       │   └── app.js
│       ├── deviceconfig.json
│       └── schedule.json
│
├── IPSetupTool/
│   ├── IPSetupTool.sln
│   ├── IPSetupTool/
│   │   ├── Program.cs
│   │   ├── MainForm.cs/Designer.cs
│   │   ├── Services/
│   │   │   ├── UDPDiscoveryService.cs
│   │   │   └── DeviceConfigService.cs
│   │   ├── Models/
│   │   │   └── DeviceInfo.cs
│   │   └── IPSetupTool.csproj
│   └── publish.bat                 (단일 실행파일 빌드)
│
└── docs/
    ├── 01-plan/features/
    │   └── PC-원격전원관리.plan.md
    └── ...
```

---

*Document created: 2026-04-06*
*PDCA Phase: Plan*
