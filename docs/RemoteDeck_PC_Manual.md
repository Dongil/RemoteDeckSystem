# RemoteDeck PC 원격 전원관리 시스템

> **버전**: v2.2.0
> **최종 수정**: 2026-04-10
> **대상**: 사내 타부서, 개발수행 클라이언트

---

## 목차

1. [시스템 개요](#1-시스템-개요)
2. [하드웨어 설치](#2-하드웨어-설치)
3. [초기 네트워크 설정 (IPSetupTool)](#3-초기-네트워크-설정-ipsetuptool)
4. [웹 인터페이스](#4-웹-인터페이스)
5. [API 문서 (v2.2)](#5-api-문서-v22)
6. [API 테스트 유틸리티 (APITestUtility_v2)](#6-api-테스트-유틸리티)
7. [Web Request (외부 시스템 연동)](#7-web-request-외부-시스템-연동)
8. [공장 초기화 / 펌웨어 업데이트](#8-공장-초기화--펌웨어-업데이트)
9. [하드웨어 사양](#9-하드웨어-사양)
10. [배선 가이드](#10-배선-가이드)
11. [문제 해결](#11-문제-해결)

---

## 1. 시스템 개요

RemoteDeck PC는 ESP32 기반 원격 PC 전원관리 장치입니다.

### 1.1 주요 기능

| 기능 | 설명 |
|------|------|
| PC 전원 제어 | 릴레이 펄스로 전원 켜기/끄기/강제 종료 |
| PC 상태 모니터링 | PC 전원 LED 감지로 ON/OFF 상태 확인 |
| 릴레이 2 | 보조 장비 (조명, 빔프로젝터 등) 제어 |
| GPIO 입력 | 3채널 범용 입력 감지 |
| Wake-on-LAN | 네트워크를 통한 PC 원격 부팅 |
| 스케줄 | 시간/요일 기반 자동 전원 제어 |
| Web Request | IO 상태 변경 시 외부 서버로 HTTP GET 호출 |

### 1.2 지원 인터페이스

| 인터페이스 | 용도 | 포트/프로토콜 |
|-----------|------|-------------|
| **Web UI** | 브라우저 기반 설정/제어 | HTTP :5050 |
| **WebSocket** | 실시간 상태 모니터링 | WS :5050/ws |
| **MQTT** | IoT 플랫폼 연동 | TCP :1883 |
| **RS485** | 유선 시리얼 통신 | 9600bps 8N1 |
| **UDP** | 장치 탐색/초기 설정 | UDP :5051 |
| **Web Request** | 상태 변경 시 외부 HTTP GET 호출 | HTTP GET |

### 1.3 네트워크 모드

| 모드 | 설명 |
|------|------|
| **Ethernet** | W5500 유선 (기본값, 고정 IP 192.168.1.200) |
| **WiFi** | ESP32 내장 WiFi STA 모드 |

---

## 2. 하드웨어 설치

### 2.1 단자 배치

```
┌────────────────────────────────────────────────┐
│  RemoteDeck PC Board                           │
│                                                │
│  [DC 5V]  [ETH]  [RS485]                      │
│                                                │
│  [RELAY1] [RELAY2]                             │
│  COM NO NC  COM NO NC                          │
│                                                │
│  [PC-LED] [GPIO1] [GPIO2] [GPIO3]             │
│                                                │
│  [STATUS1] [STATUS2]                           │
└────────────────────────────────────────────────┘
```

### 2.2 연결 순서

1. **전원**: DC 5V 어댑터 연결 (마이크로 USB 또는 DC 잭)
2. **네트워크**: Ethernet 케이블을 W5500 RJ45 포트에 연결
3. **PC 전원 스위치**: Relay 1의 COM/NO를 메인보드 F_PANEL PWR_BTN(핀3,4)에 병렬 연결
4. **PC 전원 LED**: PC-LED 단자를 메인보드 F_PANEL PWR_LED(핀1,2)에 병렬 연결
5. **보조 장비** (선택): Relay 2에 SSR 또는 스마트 플러그 연결

### 2.3 STATUS LED

| LED | 상태 | 의미 |
|-----|------|------|
| STATUS1 | 켜짐 | 네트워크 연결됨 |
| STATUS1 | 깜빡임 | 공장 초기화 대기 중 |
| STATUS2 | 3초간 점등 | 통신 활동 (MQTT/RS485) |

---

## 3. 초기 네트워크 설정 (IPSetupTool)

### 3.1 개요

IPSetupTool은 장치의 IP 주소 및 네트워크 설정을 구성하는 Windows 유틸리티입니다.

- **실행 파일**: `IPSetupTool.exe` (단독 실행, .NET 설치 불필요)
- **요구 사항**: Windows 10/11, **관리자 권한** 필요

### 3.2 실행 방법

1. `IPSetupTool.exe`를 **관리자 권한으로 실행**
2. PC와 장치를 같은 네트워크(스위치/라우터)에 연결

### 3.3 장치 탐색

1. 네트워크 어댑터가 자동 선택됨 (여러 개인 경우 수동 선택)
2. **[장치 검색]** 버튼 클릭
3. 같은 서브넷의 RemoteDeck 장치가 목록에 표시됨

> **다른 서브넷의 장치를 찾으려면**: IPSetupTool이 자동으로 임시 IP를 추가하여 장치 기본 IP(192.168.1.200)에 접근합니다.

### 3.4 네트워크 설정 변경

1. 탐색된 장치를 선택
2. 네트워크 모드 선택 (Ethernet / WiFi)
3. IP 주소, 게이트웨이, 서브넷, DNS 입력
4. WiFi인 경우 SSID/비밀번호 입력
5. **[저장 및 재부팅]** 클릭

### 3.5 설정 후 확인

- 장치가 재부팅되면 새 IP로 접속 확인
- **[Web UI 열기]** 버튼으로 브라우저에서 바로 확인 가능

---

## 4. 웹 인터페이스

### 4.1 접속

- URL: `http://{장치IP}:5050`
- 인증: Basic Auth (기본값 `admin` / `12345`)

### 4.2 탭 구성

#### 홈

| 항목 | 설명 |
|------|------|
| PC 상태 | ON/OFF 표시 (PC-LED 감지) |
| 릴레이 1/2 상태 | ON/OFF 표시 |
| GPIO 상태 | 3채널 입력 값 |
| 시스템 정보 | 장치 이름, IP, 가동 시간, MQTT, NTP |

#### 제어

- 릴레이 1/2: 펄스 / 켜기 / 끄기
- Wake-on-LAN: MAC 주소 입력 후 전송

#### 스케줄

- 시간, 요일, 릴레이, 동작(켜기/끄기/토글) 설정
- 추가/삭제 관리

#### 설정

5개 서브탭으로 구성:

| 서브탭 | 설정 항목 |
|--------|----------|
| **네트워크** | 장치 ID/이름, 네트워크 모드, 이더넷/WiFi IP 설정 → 저장 시 재부팅 |
| **MQTT** | 브로커 주소, 포트, 인증, 토픽, 연결 테스트 |
| **계정** | 관리자 ID/패스워드 변경 |
| **기타** | 릴레이 펄스 시간, PC 모니터링 주기, WOL MAC, NTP |
| **Web Request** | 활성화 토글, 이벤트별 URL, 플레이스홀더 |

#### 펌웨어

- 현재 버전 표시
- .bin 파일 업로드로 OTA 업데이트

#### 로그

- 이벤트 로그 실시간 조회

---

## 5. API 문서 (v2.2)

### 5.1 HTTP REST API

모든 API는 Basic Auth 필요. 기본 Base URL: `http://{장치IP}:5050`

#### 상태 조회

```
GET /api/status
```

**응답:**
```json
{
  "pc_on": false,
  "relay1": true,
  "relay2": false,
  "gpio": [0, 0, 0],
  "uptime": 12345,
  "ip": "192.168.1.200",
  "mac": "8E:4F:00:A1:E6:14",
  "fw_ver": "2.2.0",
  "device_name": "새기기",
  "mqtt_connected": true,
  "time": "15:30:00"
}
```

#### 릴레이 제어

```
POST /api/relay
Content-Type: application/json
```

| 요청 | 설명 |
|------|------|
| `{"relay":1,"state":"on"}` | 릴레이1 ON |
| `{"relay":1,"state":"off"}` | 릴레이1 OFF |
| `{"relay":2,"state":"on"}` | 릴레이2 ON |
| `{"cmd":"pulse","relay":1}` | 릴레이1 짧은 펄스 (기본 500ms) |
| `{"cmd":"pulse","relay":1,"duration":5000}` | 릴레이1 긴 펄스 (5초) |

**응답:** `{"ok":true}`

#### Wake-on-LAN

```
POST /api/wol
Content-Type: application/json

{"mac":"AA:BB:CC:DD:EE:FF"}
```

#### 설정 조회/저장

```
GET  /api/config          설정 전체 조회
POST /api/config          설정 부분 저장 (전송된 필드만 업데이트)
```

#### MQTT 연결 테스트

```
POST /api/mqtttest        테스트 시작 (JSON: broker, port, user, password)
GET  /api/mqtttest        테스트 결과 폴링 (status: "testing"/"ok"/"fail")
```

#### 스케줄

```
GET    /api/schedule      스케줄 목록 조회
POST   /api/schedule      스케줄 추가/수정
DELETE /api/schedule?id=1  스케줄 삭제
```

#### 기타

```
POST /api/reboot          장치 재부팅
POST /api/auth            계정 변경 (current_pass, new_user, new_pass 필요)
POST /api/ota             펌웨어 업로드 (multipart/form-data)
GET  /api/log             이벤트 로그 조회
```

#### curl 예시

```bash
# 상태 조회
curl -u admin:12345 http://192.168.1.200:5050/api/status

# 릴레이1 ON
curl -u admin:12345 -X POST -H "Content-Type: application/json" \
  -d '{"relay":1,"state":"on"}' http://192.168.1.200:5050/api/relay

# 릴레이1 펄스 (PC 전원 토글)
curl -u admin:12345 -X POST -H "Content-Type: application/json" \
  -d '{"cmd":"pulse","relay":1}' http://192.168.1.200:5050/api/relay

# PC 전원 강제 종료 (5초 길게)
curl -u admin:12345 -X POST -H "Content-Type: application/json" \
  -d '{"cmd":"pulse","relay":1,"duration":5000}' http://192.168.1.200:5050/api/relay
```

---

### 5.2 MQTT API (v2.2)

#### 토픽 구조

| 토픽 | 방향 | 용도 |
|------|------|------|
| `RemoteDeck/PC/{device_id}` | 수신 (Subscribe) | 장치에 명령 전송 |
| `RemoteDeck/PC/server` | 발신 (Publish) | 장치 상태 보고 |
| `RemoteDeck/PC/ping` | 발신 | 장치 존재 알림 |

> `{device_id}`는 장치 설정의 device_id 값 (기본값: `node_1`)

#### Command (장치로 전송)

토픽 `RemoteDeck/PC/node_1`으로 발행:

```json
{"cmd":"relay","relay":1,"state":"on"}     릴레이1 ON
{"cmd":"relay","relay":1,"state":"off"}    릴레이1 OFF
{"cmd":"pulse","relay":1}                  릴레이1 펄스 (짧은)
{"cmd":"pulse","relay":1,"duration":5000}  릴레이1 펄스 (5초)
{"cmd":"status"}                           전체 상태 요청
{"cmd":"wol"}                              WOL 전송 (기본 MAC)
{"cmd":"wol","mac":"AA:BB:CC:DD:EE:FF"}   WOL 전송 (지정 MAC)
{"cmd":"reboot"}                           재부팅
```

#### Status Event (장치에서 수신)

토픽 `RemoteDeck/PC/server`에서 수신:

**online** — 부팅/MQTT 연결 시
```json
{"id":"node_1","event":"online","ip":"192.168.1.200","name":"새기기","fw":"2.2.0"}
```

**relay** — 릴레이 상태 변경 시
```json
{"id":"node_1","event":"relay","relay1":1,"relay2":0}
```

**pcled** — PC 전원 상태 변경 시
```json
{"id":"node_1","event":"pcled","pc_on":true}
```

**full** — status 명령 응답 시 (전체 상태)
```json
{
  "id":"node_1","event":"full",
  "ip":"192.168.1.200","name":"새기기","fw":"2.2.0",
  "relay1":1,"relay2":0,"pc_on":true,
  "gpio1":0,"gpio2":1,"gpio3":0,
  "uptime":12345,"mqtt":true
}
```

---

### 5.3 WebSocket API

#### 연결

```
ws://{장치IP}:5050/ws
Authorization: Basic {base64(user:pass)}
```

#### 수신 메시지

상태가 변경될 때마다 자동 수신:

```json
{
  "type": "status",
  "data": {
    "pc_on": false,
    "relay1": true,
    "relay2": false,
    "gpio": [0, 0, 0],
    "uptime": 12345,
    "ip": "192.168.1.200",
    "fw_ver": "2.2.0",
    "mqtt_connected": true
  }
}
```

---

### 5.4 RS485 API

- **물리**: UART 9600bps 8N1 (GPIO 21 RX, GPIO 22 TX)
- **프로토콜**: JSON 한 줄 + 개행(`\n`) 구분
- **명령/상태 포맷**: MQTT와 동일 (v2.2 JSON)

```
송신: {"cmd":"relay","relay":1,"state":"on"}\n
수신: {"id":"node_1","event":"relay","relay1":1,"relay2":0}\n
```

---

## 6. API 테스트 유틸리티

### 6.1 개요

APITestUtility_v2는 RemoteDeck v2.2 API를 테스트하는 Windows 데스크톱 앱입니다.

- **실행 파일**: `RemoteDeckTest.exe` (단독 실행, .NET 설치 불필요)
- HTTP, MQTT, WebSocket, RS485 모든 인터페이스 테스트 가능

### 6.2 Connection 탭

#### Device (HTTP / WebSocket)

| 필드 | 기본값 | 설명 |
|------|--------|------|
| IP | 192.168.1.200 | 장치 IP |
| Port | 5050 | 웹 포트 |
| User | admin | 인증 ID |
| Pass | 12345 | 인증 PW |

- **[HTTP Status]**: 장치 상태 조회
- **[HTTP Config]**: 장치 설정 조회
- **[WS Connect]**: WebSocket 실시간 연결 (상태 자동 수신)

#### MQTT

| 필드 | 기본값 | 설명 |
|------|--------|------|
| Broker | (입력 필요) | MQTT 브로커 주소 |
| Port | 1883 | MQTT 포트 |
| Pub Topic | RemoteDeck/PC/node_1 | 명령 발행 토픽 |
| Sub Topic | RemoteDeck/PC/server | 상태 수신 토픽 |

#### RS485 (Serial)

| 필드 | 기본값 | 설명 |
|------|--------|------|
| Port | (자동 탐색) | COM 포트 |
| Baud | 9600 | 전송 속도 |

### 6.3 Control & Monitor 탭

1. **Send Interface**: 명령을 보낼 인터페이스 선택 (HTTP / MQTT / RS485)
2. **Relay 1/2**: ON / OFF / Pulse 버튼
3. **Commands**: Get Status, WOL, Reboot
4. **IO Status Monitor**: Relay1, Relay2, PC-LED, GPIO1~3 실시간 상태 표시
5. **Custom JSON**: 직접 JSON 입력하여 명령 전송
6. **Log**: 모든 송수신 로그 표시

### 6.4 사용 흐름

```
1. Connection 탭에서 장치 IP 입력
2. [HTTP Status] 클릭하여 연결 확인
3. Control 탭으로 이동
4. Send Interface에서 원하는 인터페이스 선택
5. 릴레이 ON/OFF 테스트
6. IO Monitor에서 상태 변화 확인
```

---

## 7. Web Request (외부 시스템 연동)

### 7.1 개요

IO 상태(릴레이, GPIO, PC-LED)가 변경될 때 설정된 URL로 HTTP GET을 자동 호출합니다. 기존 웹서비스를 수정하지 않고 상태 알림을 수신할 수 있습니다.

### 7.2 설정 방법

1. 웹 UI → 설정 → **Web Request** 탭
2. **Web Request 사용** 체크
3. 각 이벤트에 대응하는 URL 입력
4. **[저장]** 클릭

### 7.3 지원 이벤트

| 이벤트 | 발생 시점 |
|--------|----------|
| relay1_on / relay1_off | 릴레이1 상태 변경 |
| relay2_on / relay2_off | 릴레이2 상태 변경 |
| pcled_on / pcled_off | PC 전원 상태 변경 |
| gpio1_high / gpio1_low | GPIO1 입력 변경 |
| gpio2_high / gpio2_low | GPIO2 입력 변경 |
| gpio3_high / gpio3_low | GPIO3 입력 변경 |

### 7.4 플레이스홀더

URL에 다음 플레이스홀더를 사용하면 실제 값으로 자동 치환됩니다:

| 플레이스홀더 | 치환값 | 예시 |
|-------------|--------|------|
| `[device_id]` | 장치 ID | node_1 |
| `[device_name]` | 장치 이름 | 새기기 |
| `[ip]` | 장치 IP | 192.168.1.200 |
| `[mac]` | MAC 주소 | 8E:4F:00:A1:E6:14 |
| `[event]` | 이벤트 이름 | relay1_on |
| `[value]` | 상태 값 | 1 또는 0 |

### 7.5 URL 설정 예시

```
http://내부서버.com/api/notify?device=[device_id]&event=[event]&value=[value]&ip=[ip]
```

→ 릴레이1 ON 시 실제 호출:
```
http://내부서버.com/api/notify?device=node_1&event=relay1_on&value=1&ip=192.168.1.200
```

---

## 8. 공장 초기화 / 펌웨어 업데이트

### 8.1 공장 초기화

1. 장치 전원 차단
2. **GPIO1 단자를 GND에 연결** (점퍼 와이어)
3. 전원 인가
4. STATUS1 LED가 깜빡임 → **3초 이상 유지**
5. STATUS1 LED가 2초간 점등되면 초기화 완료
6. 점퍼 와이어 제거 (5초 내)
7. 자동 재부팅 → 기본 설정으로 복원

**초기화 후 기본값:**

| 항목 | 값 |
|------|-----|
| IP | 192.168.1.200 |
| 게이트웨이 | 192.168.1.1 |
| 네트워크 모드 | Ethernet |
| 인증 | admin / 12345 |

### 8.2 펌웨어 업데이트 (OTA)

1. 웹 UI → **펌웨어** 탭
2. `.bin` 파일 선택
3. **[업로드]** 클릭
4. 진행률 표시 → 완료 후 자동 재부팅

---

## 9. 하드웨어 사양

### 9.1 핀 배치

| 기능 | GPIO | 방향 | 설명 |
|------|------|------|------|
| RELAY1 | 25 | 출력 | PC 전원 스위치 |
| RELAY2 | 26 | 출력 | 보조 장비 |
| PC-LED | 4 | 입력 | PC 전원 LED (LOW=ON) |
| GPIO1 | 12 | 입력 | 범용 / 공장 초기화 |
| GPIO2 | 14 | 입력 | 범용 |
| GPIO3 | 15 | 입력 | 범용 |
| STATUS1 | 32 | 출력 | 네트워크 연결 표시 |
| STATUS2 | 33 | 출력 | 통신 활동 표시 |
| RS485 RX | 21 | 입력 | UART2 수신 |
| RS485 TX | 22 | 출력 | UART2 송신 |
| ETH CS | 5 | 출력 | W5500 SPI CS |
| ETH SCK | 18 | 출력 | W5500 SPI CLK |
| ETH MISO | 19 | 입력 | W5500 SPI MISO |
| ETH MOSI | 23 | 출력 | W5500 SPI MOSI |
| ETH INT | 17 | 입력 | W5500 인터럽트 |

### 9.2 릴레이 사양

| 항목 | 값 |
|------|-----|
| 모델 | SONGLE SRD-05VDC-SL-C |
| 코일 전압 | DC 5V |
| 접점 타입 | SPDT (COM / NO / NC) |
| AC 정격 | 10A @ 250VAC |
| DC 정격 | 10A @ 30VDC |

### 9.3 네트워크 포트

| 포트 | 프로토콜 | 용도 |
|------|---------|------|
| 5050 | TCP (HTTP/WS) | 웹 UI + REST API + WebSocket |
| 5051 | UDP | 장치 탐색 (IPSetupTool) |
| 1883 | TCP (MQTT) | MQTT 브로커 연결 (외부) |
| 9 | UDP | Wake-on-LAN |

---

## 10. 배선 가이드

### 10.1 Relay 1 → PC 전원 스위치

PC 케이스 전원 버튼과 **병렬** 연결:

```
  메인보드 F_PANEL        기존 케이스 버튼 (유지)
  PWR_BTN (핀 3,4)  ←──── [PWR BUTTON]
       │    │
       │    └────────── Relay1 NO
       └─────────────── Relay1 COM
```

- **Pulse 500ms** = 전원 켜기/끄기
- **Pulse 5000ms** = 강제 종료

### 10.2 Relay 2 → 220V 기기 (SSR 방식 권장)

```
  Relay2 COM ──── SSR DC+ (3~32V)
  Relay2 NO  ──── SSR DC- 
  5V 전원 ────────┘

  SSR AC 출력 ──── 220V 기기 (조명/빔프로젝터)
```

> 상세 배선도는 `docs/wiring-guide.md` 참조

---

## 11. 문제 해결

| 증상 | 원인 | 해결 |
|------|------|------|
| IPSetupTool에서 장치 검색 안됨 | 서브넷 불일치 또는 방화벽 | 같은 스위치에 연결, Windows 방화벽에서 UDP 5051 허용 |
| 웹 UI 접속 안됨 | IP 설정 오류 | IPSetupTool로 IP 재설정 또는 공장 초기화 |
| MQTT 연결 안됨 | 브로커 주소가 hostname | ESP32 Ethernet은 DNS 미지원. IP 주소 직접 입력 또는 웹 UI MQTT 설정에서 DNS 변환 사용 |
| 릴레이 동작하지만 PC 안 켜짐 | 배선 오류 | F_PANEL PWR_BTN(핀 3,4) 확인, COM/NO 연결 확인 |
| Web Request 호출 안됨 | enabled=false 또는 URL 미입력 | 웹 UI → 설정 → Web Request에서 활성화 및 URL 확인 |
| 펌웨어 업로드 실패 | 파일 크기 초과 | 파티션 크기 확인, .bin 파일이 올바른지 확인 |

---

## 부록: 배포 파일 목록

| 파일 | 용도 |
|------|------|
| `IPSetupTool.exe` | 초기 네트워크 설정 도구 (관리자 권한 필요) |
| `RemoteDeckTest.exe` | API 테스트 유틸리티 (HTTP/MQTT/WS/RS485) |
| `firmware.bin` | ESP32 펌웨어 (OTA 업데이트용) |
| 본 문서 | 사용자 매뉴얼 |
| `wiring-guide.md` | 상세 배선 가이드 |
