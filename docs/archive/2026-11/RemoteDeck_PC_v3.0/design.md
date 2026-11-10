---
template: design
version: 1.3
feature: rcm-hub-v3.0
date: 2026-11-09
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.6.2 → v3.0.0 (target)
architecture: Option C — Pragmatic Balance
---

# rcm-hub-v3.0 Design Document

> **Summary**: `rcm-api` 참조 라이브러리(RcmHub/RcmDeck, v1.0.2)를 `lib/`로 반입하고, 새 서브시스템 `HubBridge`가 소유·배선한다. 허브와 충돌하는 기존 전원 경로는 main.cpp의 가드 헬퍼 하나로 막고, OTA 업로드에 인증을 붙인다. 웹 UI에 설정 카드와 홈 카드를 추가한다.
>
> **Project**: RemoteDeckSystem
> **Version**: 3.0.0 (target)
> **Author**: KDI
> **Date**: 2026-11-09
> **Status**: Draft
> **Planning Doc**: [rcm-hub-v3.0.plan.md](plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | RCM 허브 통합 운영을 위해 SmartDeck이 허브 `deck`으로 붙어야 하고, LED 미확인 전원 조작(현장 오조작 원인)을 제거해야 함 |
| **WHO** | RCM 서버/허브(명령 발신), 설치 운영자(웹 UI 설정·상태 확인), 기존 MQTT/RS485/재부재 사용 현장(회귀 없어야 함) |
| **RISK** | (1) loop() 블로킹 → heartbeat 끊김(15~30s). (2) 설정 키 4곳 중 하나 누락 → UDP/HTTP 설정 변경 시 hub 값 소실. (3) 허브 미사용 현장 회귀. (4) 이더넷 GOT_IP 감시 재부팅(20s)과 허브 재접속 겹침. (5) 실기 시험 중 전원 불안정을 S/W 문제로 오인 |
| **SUCCESS** | `pio run` 성공 · conformance 기본 그룹 + `--with power,led,reboot` PASS · 수동 4항목 통과 · hub.enabled=false에서 기존 기능 회귀 0 |
| **SCOPE** | Phase1 펌웨어(라이브러리·설정·연결·/api/status) / Phase2 충돌 정리·보안·버전 / Phase3 Web UI / Phase4 실기 conformance + dogfood |

---

## 1. Overview

### 1.1 Design Goals

- 참조 라이브러리는 **수정하지 않고** 반입한다(수정이 필요하면 rcm-api에 PR, 같은 수정 반영).
- 허브 관련 코드를 `HubBridge` 한 곳에 모아 main.cpp 증가를 약 50줄로 제한한다.
- 허브 사용 여부(`config.hub.enabled`)가 **유일한 분기점**이다. false이면 v2.6.2와 동작이 같다.
- `loop()`를 막는 새 코드를 넣지 않는다.

### 1.2 Design Principles

- 기존 패턴 유지: 서브시스템 클래스 + main.cpp 배선(AttendanceHandler와 같은 방식).
- 하위호환: 설정·API는 **추가만** 한다. 없는 키는 기본값, 모르는 키는 무시.
- 방어선 보존: `NetManager`는 diff=0.
- 설정 변경은 **재부팅 후 반영**한다(기존 설정 카드와 같은 동작). 런타임 재시작 로직을 만들지 않는다.

---

## 2. Architecture

### 2.0 Architecture Comparison

| Criteria | Option A: Minimal | Option B: Clean | Option C: Pragmatic |
|----------|:-:|:-:|:-:|
| **Approach** | main.cpp에 직접 배선 | HubBridge + PowerGuard + 인증 미들웨어 분리 | HubBridge 1개 + main.cpp 가드 헬퍼 |
| **New Files (lib 제외)** | 0 | 4 | 2 |
| **Modified Files** | ~11 | ~12 | ~11 |
| **main.cpp 증가** | +120줄 | +30줄 | +50줄 |
| **Complexity** | Low | High | Medium |
| **Maintainability** | Medium | High | High |
| **Risk** | Low | Medium(기존 코드 리팩터링) | Low |

**Selected**: **Option C — Pragmatic Balance** — **Rationale**: AttendanceHandler와 같은 서브시스템 패턴이고, 허브 코드가 한 파일에 모여 Touch 등 다른 펌웨어로 옮기기도 쉽다. 가드는 세 곳(릴레이 명령 2경로 + 스케줄)에서 같은 헬퍼를 쓰면 충분하다.

### 2.1 Component Diagram

```
                 ┌──────────────── RCM Hub (ws://host:port/v1/ws) ────────────────┐
                 │  challenge → hello(HMAC) → welcome → req/res, deck.state      │
                 └──────────────────────────────▲─────────────────────────────────┘
                                                │ links2004/WebSockets (WiFiClient, ETH/WiFi 공통 lwIP)
┌───────────────────────── RemoteDeck_PC ───────┼─────────────────────────────────────┐
│ lib/RcmHub (원본 그대로)                        │                                     │
│   RcmHub ── RcmDeck ◀── RcmDeckIo(함수 묶음) ────┘                                     │
│                ▲                                                                      │
│ src/network/HubBridge  (신규)                                                          │
│   begin(cfg, ioDeps)  loop()  fillStatus(doc)  enabled()                              │
│                ▲ ioDeps: PCMonitor.isPCOn / RelayController / NetManager / version    │
│ src/main.cpp                                                                          │
│   setup: hubBridge.selfTestLog()                                                     │
│   onConnected: [MQTT: hub이면 skip] → NTP → hubBridge.begin() (1회)                   │
│   loop: ... hubBridge.loop()                                                          │
│   blockPowerRelayLatch(relay,on,source) ← processCommand(cmd:relay), /api/relay       │
│   onScheduleAction: hub이면 relay1 skip                                               │
│   buildStatusJson: hubBridge.fillStatus(doc)                                          │
│   onUDPConfig / buildConfigJson: hub 블록 (device_key 가림)                            │
│ src/web/OTAHandler: setAuthCheck(fn) — index==0에서 인증, 실패 시 조각 폐기 + 401        │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

```
[허브 → 전원]  pc.powerOn{pressMs,waitMs} → RcmDeck: io.pcOn()?
                 ├ 이미 on → res{pc:"on",pressed:false}
                 └ off → io.pulse(1, pressMs) → loop마다 LED 확인 → res{pressed:true} | DEVICE_ERROR
[LED → 허브]  PCMonitor(1s×3 디바운스) → RcmDeck.loop()가 io.pcOn() 비교 → deck.state{pc}
[상태 → UI]   RcmHub.ready/stopped/stopReason → HubBridge.fillStatus → /api/status → 홈 카드(5s 폴링)
[설정]        웹 saveEtc / UDP SET_CONFIG → onUDPConfig → ConfigManager::save → (재부팅) → hubBridge.begin
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| `lib/RcmHub` | `links2004/WebSockets@^2.6.1`, ArduinoJson 6, mbedtls(HMAC) | 허브 프로토콜 |
| `HubBridge` | `RcmDeck`, `HubConfig`, 함수 주입(io) | 서브시스템 래퍼 |
| main.cpp | `HubBridge`, PCMonitor, RelayController, NetManager | 배선·가드 |
| `OTAHandler` | 인증 함수(WebServer가 주입) | OTA 인증 |

---

## 3. Data Model

### 3.1 `HubConfig` (DeviceConfig.h)

```cpp
// Design Ref: rcm-hub-v3.0 §3.1 — RCM 허브 연동 설정 (handoff/SMARTDECK.md §3)
struct HubConfig {
    bool        enabled    = false;
    std::string host       = "";        // IP 권장 (W5500 DNS 불가)
    uint16_t    port       = 8891;
    std::string path       = "/v1/ws";
    bool        tls        = false;
    std::string productId  = "";        // 짝 녹화장비와 같은 제품번호
    std::string deviceKey  = "";        // tools/device-key.js --role deck
};
// DeviceConfig 에 HubConfig hub; 추가
```

### 3.2 deviceconfig.json `hub` 블록

```json
"hub": {
  "enabled": false,
  "host": "",
  "port": 8891,
  "path": "/v1/ws",
  "tls": false,
  "product_id": "",
  "device_key": ""
}
```
> `hub` 블록에는 `"version"` 키를 두지 않는다(`build_firmware.bat`이 첫 `"version"`을 파일명에 씀).

### 3.3 설정 키 반영 체크리스트 (4곳)

| # | 위치 | 작업 |
|---|------|------|
| 1 | `src/config/DeviceConfig.h` | `HubConfig` 구조체 + `DeviceConfig::hub` |
| 2 | `src/config/ConfigManager.cpp` load | `doc["hub"]` 읽기, 없으면 기본값(하위호환) |
| 2 | `ConfigManager.cpp` save | `hub` 블록 쓰기(device_key 평문 저장 — SPIFFS 내부) |
| 2 | `ConfigManager.cpp` defaults | 기본값 설정 |
| 3 | `main.cpp onUDPConfig` (HTTP POST `/api/config` 공용) | `hub` 키별 `containsKey` 갱신. `device_key == "****"` 또는 빈 키 없음이면 기존 값 유지 |
| 4 | `main.cpp buildConfigJson` (GET `/api/config`, UDP `GET_CONFIG` 공용) | `hub` 블록 출력, `device_key`는 값이 있으면 `"****"`, 없으면 `""` |

> 저장 → 재부팅 → `/api/config` 조회 → UDP `GET_CONFIG` 조회 → hub 블록 없는 `SET_CONFIG`(IPSetupTool) 저장 → 조회 순서로 왕복 시험(§8 T-05, T-06).

### 3.4 버전 통일

| 위치 | 변경 |
|------|------|
| `data/deviceconfig.json` `firmware.version` | `"3.0.0"`, `date` 갱신 (**단일 기준**) |
| `ConfigManager.cpp` load 기본값 / defaults | `"3.0.0"` |
| `main.cpp` 부팅 배너 | `"=== RemoteDeck PC Power Manager v%s ==="`에 `config.firmware.version` (config load 뒤로 이동) |

> 현장 장비는 SPIFFS OTA 시 deviceconfig.json을 보존하므로 `firmware.version`은 펌웨어 OTA 파일명 파싱(`RemoteDeck_PC_V3.0.0_*.bin`)으로 갱신된다. 허브에 보고하는 `fw`도 이 값.

---

## 4. API Specification

### 4.1 `/api/status` 추가 필드 (기존 필드 변경 없음)

| Field | Type | 값 |
|-------|------|----|
| `hub_enabled` | bool | `config.hub.enabled` |
| `hub_connected` | bool | `deck.hub().ready()` (비활성 시 false) |
| `hub_stopped` | bool | `deck.hub().stopped()` (4001/4009/4010) |
| `hub_stop_reason` | string | `deck.hub().stopReason()` (없으면 `""`) |
| `hub_host` | string | `"host:port"` (홈 카드 표시용, 비활성 시 `""`) |
| `hub_product_id` | string | 제품번호 |

- `buildStatusJson`의 `StaticJsonDocument<512>` → `<768>`.
- WebSocket status 브로드캐스트도 같은 함수를 쓰므로 자동 반영.
- IntegrateController는 모르는 필드를 무시(tolerant) → 회귀 없음.

### 4.2 `/api/config` (GET/POST), UDP `GET_CONFIG`/`SET_CONFIG`

- GET 응답에 `hub` 블록(§3.2) 포함, `device_key`는 `"****"` 또는 `""`.
- POST/SET_CONFIG: `hub` 블록이 없으면 hub 값 그대로. `device_key`가 `"****"`면 그대로.

### 4.3 릴레이1 latch 거부 (hub.enabled일 때만)

| 경로 | 거부 조건 | 응답 |
|------|----------|------|
| MQTT/RS485 `processCommand` `{cmd:"relay", relay:1, state:"on"}` | hub.enabled && relay==1 && on | 실행 안 함, `logger.log("GUARD", "relay1 latch blocked (hub) from <src>")` |
| `POST /api/relay` `relay:1, action/state:"on"` | 동일 | `409 {"ok":false,"error":"relay1 latch blocked: RCM hub enabled (use pulse)"}` |
| `off`, `pulse` | — | 기존과 동일 허용 |

- `WebServer` relay 핸들러 시그니처를 `bool(uint8_t, const String&, uint16_t)`로 바꿔 false면 409 응답(WebServer.cpp 한 곳).

### 4.4 OTA 업로드 인증 (`POST /api/ota`, 펌웨어·SPIFFS 공용)

- `OTAHandler::setAuthCheck(std::function<bool(AsyncWebServerRequest*)>)` 추가. WebServer가 `requireAuth`와 같은 판정(`req->authenticate(user, pass)`)을 넘김. 인증 비활성(`_auth == nullptr`)이면 항상 true.
- 업로드 콜백 `index == 0`에서 판정 → 실패 시 `_rejected = true`, `Update.begin`/백업/`_onFilename` 호출 안 함, 이후 조각 무시.
- 응답 콜백: `_rejected`면 `request->requestAuthentication("RemoteDeck")`(401) 후 플래그 초기화, 재부팅 안 함.
- 웹 UI XHR은 같은 origin(정적 파일과 같은 realm) 인증이 자동 첨부되므로 수정 불필요. (.NET 클라이언트에서 `/api/ota` 사용처 없음 확인)

---

## 5. Firmware Detail

### 5.1 `HubBridge` (src/network/HubBridge.h/.cpp)

```cpp
// Design Ref: rcm-hub-v3.0 §5.1 — RCM 허브 deck 서브시스템 래퍼
#pragma once
#include "RcmDeck.h"
#include "../config/DeviceConfig.h"

class HubBridge {
public:
    struct Deps {                       // main.cpp가 채움
        std::function<bool()> pcOn;
        std::function<bool(uint8_t)> getRelay;
        std::function<void(uint8_t, bool)> setRelay;
        std::function<void(uint8_t, uint16_t)> pulse;
        std::function<void(JsonArray)> inputs;
        std::function<String()> ip;
        std::function<String()> net;    // "eth" | "wifi"
    };
    static void logSelfTest();          // 부팅 시 HMAC KAT 결과 Serial + logger
    void begin(const HubConfig& cfg, const std::string& fw, const std::string& deviceName, const Deps& d);
    void loop();                        // 시작 전이면 no-op
    bool started() const { return _started; }
    void fillStatus(JsonDocument& doc, const HubConfig& cfg);  // §4.1 필드
private:
    RcmDeck _deck;
    bool _started = false;
};
```

- `begin`: 이미 시작했거나 `cfg.enabled == false` 또는 `host`/`productId`/`deviceKey`가 비어 있으면 시작하지 않고 로그만 남김(`HUB`, "config incomplete"). `RcmHubConfig{host, port, path, tls, role="deck", productId, key, clientName="RemoteDeck_PC", clientVersion=fw}`. `RcmDeckIo`에 Deps를 옮기고 `relayCount=2, powerRelay=1, reboot=ESP.restart, fw`.
- `host`가 IP 형식이 아니면 경고 로그(`HUB`, "host is not an IP — DNS may fail on Ethernet") 후 그대로 시도.
- 상태 변화 로그: `loop()`에서 `ready()`/`stopped()` 변화를 감지해 `logger.log("HUB", "connected" | "disconnected" | "stopped: <reason>")` 1회씩.

### 5.2 main.cpp 배선

| 위치 | 변경 |
|------|------|
| 전역 | `HubBridge hubBridge;` |
| `setup()` config load 뒤 | `HubBridge::logSelfTest();`, 배너를 config 버전으로 |
| `onConnected` MQTT 블록 | `if (!config.mqtt.broker.empty() && !config.hub.enabled) mqttHandler.begin(...)`; hub이면 최초 1회 `logger.log("MQTT", "disabled (RCM hub enabled)")` |
| `onConnected` NTP 뒤 | `if (config.hub.enabled && !hubBridge.started()) hubBridge.begin(config.hub, config.firmware.version, config.deviceName, deps);` |
| `loop()` | `hubBridge.loop();` (webServer.ws().loop() 다음) |
| `buildStatusJson` | `<768>` + `hubBridge.fillStatus(doc, config.hub);` |

`Deps` 바인딩:

```cpp
deps.pcOn     = [] { return pcMonitor.isPCOn(); };
deps.getRelay = [](uint8_t r) { return relayController.getRelay(r); };   // 1 기준 확인됨
deps.setRelay = [](uint8_t r, bool on) { relayController.setRelay(r, on); };
deps.pulse    = [](uint8_t r, uint16_t ms) { relayController.pulse(r, ms); };  // 비블로킹 확인됨
deps.inputs   = [](JsonArray a) { a.add(digitalRead(PIN_GPIO1)); a.add(digitalRead(PIN_GPIO2)); a.add(digitalRead(PIN_GPIO3)); };
deps.ip       = [] { return networkManager.localIP().toString(); };
deps.net      = [] { return String(networkManager.activeMode() == NetMode::ETHERNET ? "eth" : "wifi"); };
```

> `RcmDeck`은 허브 명령을 `io.setRelay`/`io.pulse`로 직접 수행하므로 §5.3 가드를 거치지 않는다(라이브러리 자체가 전원 릴레이 latch를 막음). 가드는 **기존 명령 입구**에만 둔다.

### 5.3 가드 헬퍼

```cpp
// Design Ref: rcm-hub-v3.0 §5.3 — 허브 사용 시 전원 릴레이(1) latch 금지
static bool blockPowerRelayLatch(uint8_t relay, bool on, const char* source) {
    if (!config.hub.enabled || relay != 1 || !on) return false;
    logger.log("GUARD", (String("relay1 latch blocked (hub) from ") + source).c_str());
    return true;
}
```

| 호출 위치 | 처리 |
|----------|------|
| `processCommand` `cmd:"relay"` | `if (blockPowerRelayLatch(relay, on, source)) return;` |
| `webServer.setRelayHandler` `action=="on"` | `if (blockPowerRelayLatch(relay, true, "HTTP")) return false;` |
| `onScheduleAction` | `if (config.hub.enabled && relay == 1) { logger.log("SCHEDULE", "Relay1 <action> skipped (RCM hub)"); return; }` (릴레이2 스케줄은 그대로) |

### 5.4 블로킹 점검 결과

| 항목 | 결과 | 조치 |
|------|------|------|
| WebRequest(재부재 포함) | 별도 FreeRTOS 태스크 + 큐 | 없음 |
| MQTT 테스트 | 별도 태스크 | 없음 |
| MQTT connect | loop 블로킹 | hub 사용 시 begin 안 함 |
| NTP begin | onConnected에서 최대 10s | hub begin을 NTP 뒤에 둠. 랜선 재연결 시 재접속이 최대 10s 늦어지는 것은 허용(그동안 링크 없음) |
| `onUDPReboot` `delay(1000)` | 재부팅 직전 | 없음 |

---

## 6. UI/UX Design (data/www)

### 6.1 Screen Layout

```
설정 > 기타 (stab-etc)
┌ 재부재 시스템 (기존) ───────────────────────┐
└────────────────────────────────────────────┘
┌ RCM-API 연동 (신규) ────────────────────────┐
│ ☐ RCM 허브 연동하기                          │
│ 허브 주소 [192.168.10.5   ]  포트 [8891]     │
│ 경로 [/v1/ws]   ☐ TLS(wss)                   │
│ 제품번호 [XHR131258]                          │
│ 장비 키 [••••••••] (변경 시에만 입력)          │
│ ⚠ 사용 시: 스케줄 릴레이1 전원 동작·MQTT 비활성, │
│   릴레이1 ON 유지 명령 거부. 저장 후 재부팅 필요 │
└────────────────────────────────────────────┘
[저장] [재부팅]   ← 기존 saveEtc()

홈 (최상단, 재부재 카드 위)
┌ 🔗 RCM 허브 ─────────────── [● 연결됨] ─────┐
│ 허브 192.168.10.5:8891 · 제품번호 XHR131258  │
│ (정지 시) ⛔ 장비 키 또는 제품번호 오류(4001)   │
└────────────────────────────────────────────┘
```

### 6.2 User Flow

```
설정 > 기타 → RCM-API 카드 입력 → 저장(hostname이면 DNS→IP 변환 확인) → 재부팅 → 홈 허브 카드 '연결됨'
```

### 6.3 Component List

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `#hub-card` | index.html 홈, 재부재 카드 위 | 허브 상태 카드(hub_enabled일 때만) |
| RCM-API 설정 카드 | index.html `stab-etc`, 재부재 카드 아래 | 입력 7개 + 안내 |
| `updateStatus` 확장 | app.js | `hub_*` 필드로 카드 갱신 |
| `loadConfig` 확장 | app.js | `d.hub` → 입력란 채움(device_key는 `****`면 placeholder) |
| `saveEtc` 확장 | app.js, async로 변경 | `hub` 블록 포함, host DNS→IP 변환(saveMQTT 패턴), device_key 빈 입력이면 `"****"` 전송 |
| `resolveHostToIp(host)` | app.js | saveMQTT의 dns.google 변환 로직 추출·공유 |
| `.hub-card` 스타일 | style.css | 재부재 카드 스타일 재사용 + 배지 색 |

### 6.4 Page UI Checklist

#### 설정 > 기타 — RCM-API 연동 카드

- [ ] Card 제목: "RCM-API 연동", 재부재 시스템 카드 바로 아래
- [ ] Checkbox `#cfg-hub-enabled`: "RCM 허브 연동하기"
- [ ] Input `#cfg-hub-host` (text, placeholder "192.168.10.5", IP 권장 안내)
- [ ] Input `#cfg-hub-port` (number, 기본 8891)
- [ ] Input `#cfg-hub-path` (text, 기본 "/v1/ws")
- [ ] Checkbox `#cfg-hub-tls`: "TLS (wss)"
- [ ] Input `#cfg-hub-pid` (text): 제품번호
- [ ] Input `#cfg-hub-key` (password): 장비 키, 저장된 키가 있으면 placeholder "저장됨 (변경 시에만 입력)"
- [ ] 안내 문구: 스케줄 릴레이1·MQTT 비활성, 릴레이1 latch 거부, 재부팅 필요
- [ ] 저장: 기존 `저장` 버튼(`saveEtc`)에 포함

#### 홈 — RCM 허브 카드

- [ ] Card `#hub-card`: `hub_enabled`일 때만 표시, 재부재 카드 위
- [ ] Badge `#hub-badge`: 연결됨(초록) / 연결 중(노랑) / 정지(빨강)
- [ ] Text `#hub-addr`: `hub_host`
- [ ] Text `#hub-pid`: `hub_product_id`
- [ ] Text `#hub-reason`: 정지 시에만 표시, 사유 한글 매핑 4001 "장비 키 또는 제품번호 오류", 4009 "같은 제품번호 장비가 이미 접속 중", 4010 "버전 불일치", 그 외 원문

---

## 7. Error Handling & Security

| 상황 | 처리 |
|------|------|
| 설정 불완전(host/product_id/device_key 빈 값) | begin 안 함, `HUB` 로그, 홈 카드 "연결 중"이 계속 → 사유로 "설정 불완전" 표시(`hub_stop_reason="config incomplete"`, `hub_stopped=false`) |
| HMAC selfTest 실패 | 부팅 로그 `HUB selfTest FAILED`, 허브는 그대로 시도(인증 실패 시 4001) |
| 4001/4009/4010 | 라이브러리가 재접속 중단, 사유 노출. 설정 수정 후 재부팅으로 재시도 |
| 허브 미도달 | 라이브러리 지수 백오프 0.5s→10s 재접속 |
| device_key 노출 | GET 응답 가림, 로그에 키 출력 금지 |
| OTA 무인증 | 401, Update 미시작 |

- device_key는 SPIFFS `deviceconfig.json`에 평문 저장(장비 내부). 물리 접근 보호는 범위 밖.
- UDP `SET_CONFIG`/`REBOOT` 무인증은 carry(Plan §2.2).

---

## 8. Test Plan

> 자동 테스트 하네스 없음. 빌드 + curl/UDP + conformance + 실기 수동 시험.

### 8.1 Test Scope

| Type | Target | Tool | Phase |
|------|--------|------|-------|
| L0 Build | 컴파일, Flash/RAM | `pio run` | Do |
| L1 API | /api/status, /api/config, /api/relay, /api/ota | curl, IPSetupTool | Do/Check |
| L2 UI | 설정 카드, 홈 카드 | 브라우저 | Do/Check |
| L3 Protocol | 허브 deck 규격 | `rcm-api/tools/conformance.js` | Check |
| L4 Field | 재접속·정지·회귀 | 실기 수동 | Check |

### 8.2 L1 API

| # | 대상 | 시험 | 기대 |
|---|------|------|------|
| T-01 | GET /api/status | hub 비활성 | `hub_enabled:false, hub_connected:false`, 기존 필드 그대로 |
| T-02 | GET /api/config | 키 저장 후 | `hub.device_key == "****"` |
| T-03 | UDP GET_CONFIG | 키 저장 후 | `device_key` 평문 없음 |
| T-04 | POST /api/config `hub.device_key:"****"` | 저장 | 기존 키 유지(허브 인증 성공으로 확인) |
| T-05 | POST /api/config hub 블록 없음 | 저장 후 조회 | hub 값 유지 |
| T-06 | IPSetupTool SET_CONFIG | 저장 후 조회 | hub 값 유지 |
| T-07 | POST /api/relay `{relay:1,state:"on"}` hub on | | 409, 릴레이1 off 유지 |
| T-08 | POST /api/relay `{relay:1,action:"pulse"}` hub on | | 200, 펄스 동작 |
| T-09 | POST /api/relay `{relay:1,state:"on"}` hub off | | 200(기존 동작) |
| T-10 | MQTT/RS485 `{cmd:"relay",relay:1,state:"on"}` hub on | | 미실행, GUARD 로그 (RS485로 시험, MQTT는 hub 시 비활성) |
| T-11 | POST /api/ota 무인증 `curl -F file=@x.bin` | | 401, 재부팅 없음, Update 미시작 |
| T-12 | 웹 UI 관리 탭 펌웨어/SPIFFS 업로드 | | 정상 + deviceconfig(hub 포함)·schedule 보존 |

### 8.3 L2 UI

| # | 화면 | 동작 | 기대 |
|---|------|------|------|
| U-01 | 설정 > 기타 | 로드 | §6.4 요소 전부, 저장값 반영, 키는 placeholder |
| U-02 | 설정 > 기타 | hostname 입력 후 저장 | DNS→IP 변환 확인창 |
| U-03 | 홈 | hub off | 허브 카드 숨김 |
| U-04 | 홈 | hub on, 연결 | 초록 "연결됨", 주소·제품번호 |
| U-05 | 홈 | 잘못된 키 | 빨강 "정지" + 4001 한글 사유 |

### 8.4 L3 Protocol (conformance)

| # | 명령 | 기대 |
|---|------|------|
| C-01 | `node tools/conformance.js --role deck --productid <id> --ip <ip> --port 8899` | 기본 그룹 PASS |
| C-02 | `... --with power,led,reboot` | 전부 PASS (power: PC off에서 시작) |

### 8.5 L4 Field (수동)

| # | 시나리오 | 기대 |
|---|---------|------|
| F-00 | **사전**: 아답터·전원 안정성 점검(정격, 콜드부팅 3회) | 기록 |
| F-01 | 허브 껐다 켜기 | 10s 이내 재접속, full 상태 |
| F-02 | 랜선 뽑았다 꽂기 | 재접속, GOT_IP 20s 재부팅과 충돌 없음 |
| F-03 | 잘못된 키 | 4001 정지, 재접속 없음, 홈 표시 |
| F-04 | 같은 제품번호 2대 | 선행 장비 4009 정지 |
| F-05 | hub off 회귀: MQTT 연결·명령, RS485, 스케줄 on/off, 재부재 URL, Web Request | v2.6.2와 동일 |
| F-06 | hub on: 스케줄 릴레이1 시각 도달 | 동작 안 함 + SCHEDULE skipped 로그 |

---

## 9. Clean Architecture (Firmware Layering)

| Layer | Responsibility | Location |
|-------|---------------|----------|
| Protocol | 허브 프로토콜, 전원 상태 머신 | `lib/RcmHub` (외부 원본) |
| Subsystem | 허브 서브시스템 수명·상태 노출 | `src/network/HubBridge` |
| Orchestration | 배선, 정책(가드) | `src/main.cpp` |
| Config | 영속 설정 | `src/config` |
| Presentation | 웹 UI | `data/www` |

규칙: `lib/RcmHub`는 RemoteDeck_PC 코드를 include하지 않는다(Io 주입). `HubBridge`는 전역 객체를 직접 참조하지 않는다(Deps 주입).

---

## 10. Coding Convention Reference

- 주석: `// Design Ref: rcm-hub-v3.0 §n — …`, 성공 기준 관련은 `// Plan SC: …`
- 로그 카테고리: `HUB`, `GUARD`, 기존 `SCHEDULE`/`MQTT`
- JSON 키: snake_case(기존 `/api/config`와 동일), C++ 필드: camelCase
- 커밋: `feat(RemoteDeck_PC): v3.0.0 — RCM 허브 deck 연동 …`

---

## 11. Implementation Guide

### 11.1 Implementation Order

1. [ ] `lib/RcmHub` 복사(rcm-api v1.0.2 / 3c1791b), `platformio.ini` lib_deps 추가 → `pio run`
2. [ ] `HubConfig` + ConfigManager load/save/defaults + 버전 3.0.0(deviceconfig.json, 기본값)
3. [ ] `HubBridge` 작성
4. [ ] main.cpp 배선(selfTest, 배너, onConnected MQTT/hub, loop, buildStatusJson) → `pio run`
5. [ ] onUDPConfig/buildConfigJson hub 블록 + 키 가림
6. [ ] 가드 헬퍼 + processCommand / setRelayHandler(시그니처 bool) / onScheduleAction
7. [ ] OTAHandler 인증 → `pio run`
8. [ ] 웹 UI: index.html 카드 2개, app.js(loadConfig/saveEtc/updateStatus/resolveHostToIp), style.css
9. [ ] Manual 갱신
10. [ ] 실기: L1 → L2 → L3 conformance → L4

### 11.2 Files

| 구분 | 파일 |
|------|------|
| 신규 | `lib/RcmHub/{RcmHub.h,RcmHub.cpp,RcmDeck.h,RcmDeck.cpp}`(복사), `src/network/HubBridge.h`, `src/network/HubBridge.cpp` |
| 수정 | `platformio.ini`, `src/config/DeviceConfig.h`, `src/config/ConfigManager.cpp`, `src/main.cpp`, `src/web/WebServer.h/.cpp`, `src/web/OTAHandler.h/.cpp`, `data/deviceconfig.json`, `data/www/index.html`, `data/www/app.js`, `data/www/style.css`, `docs/RemoteDeck_PC_Manual.md` |

### 11.3 Session Guide

| Module | Scope Key | 내용 | 검증 |
|--------|-----------|------|------|
| M1 | `module-1` | 라이브러리 반입 + 설정 모델 + 버전 (§11.1 1-2) | `pio run` |
| M2 | `module-2` | HubBridge + main.cpp 배선 + /api/config (§11.1 3-5) | `pio run`, T-01~06 |
| M3 | `module-3` | 가드 + OTA 인증 (§11.1 6-7) | `pio run`, T-07~12 |
| M4 | `module-4` | 웹 UI + Manual (§11.1 8-9) | U-01~05 |
| M5 | `module-5` | 실기 conformance + 필드 (§11.1 10) | C-01~02, F-00~06 |

권장: M1~M3을 한 세션(펌웨어), M4 한 세션, M5는 실기 환경에서. 예: `/pdca do rcm-hub-v3.0 --scope module-1,module-2,module-3`.

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-09 | Initial draft — Option C 선택 | KDI |
