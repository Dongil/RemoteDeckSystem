---
template: design
version: 1.3
feature: RemoteDeck_PC_v2.6
date: 2026-07-05
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.5.1 → v2.6.0
architecture: Option C - SwitchMonitor (PCMonitor mirror)
---

# RemoteDeck_PC v2.6.0 Design Document

> **Architecture**: Option C — SwitchMonitor 클래스 (PCMonitor 패턴 미러)
> **Baseline**: v2.5.1 firmware + v2.5.2 SPIFFS
> **Target**: v2.6.0 firmware (SPIFFS 변경 없음)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | GPIO2 상태 변화 이벤트가 자동 발화되지 않는 공백을 채워, 접점 기반 입력(조명 스위치→광커플러→GPIO2) 시나리오에 범용 대응 |
| **WHO** | 재부재 시스템 서버 측(gpio2_high/low URL 수신), 필드 설치 인력 |
| **RISK** | INPUT_PULLUP 도입으로 인한 기존 필드 영향(사용 이력 없음으로 판단) / 광커플러 노이즈 오감지 / v2.5 방어선 훼손 방지 |
| **SUCCESS** | 상태 전이 후 ≤4s에 fire, URL 미설정 skip, pcled 무변화, NetManager diff=0, edge-triggered |
| **SCOPE** | Phase1 SwitchMonitor + fire wiring → Phase2 필드 dogfood 1일 |

---

## 1. Overview

### 1.1 Architecture Summary

- **SwitchMonitor 신규 클래스** (PCMonitor 패턴 100% 미러)
  - `pinMode(pin, INPUT_PULLUP)` in `begin()`
  - 1s poll + 3x debounce, edge-triggered `setOnChange` 콜백
  - `bool isActive()` 노출 (LOW = active = present)
- **main.cpp wiring**
  - `SwitchMonitor switchMonitor;` 인스턴스
  - `switchMonitor.begin(PIN_GPIO2)`
  - `setOnChange` 콜백 → `webRequestHandler.fire("gpio2_low"|"gpio2_high", active ? 1 : 0)`
  - `loop()`에 `switchMonitor.loop()` 추가
- **모든 나머지 자산 무변경**: WebRequestHandler, DeviceConfig, ConfigManager, Web UI, NetManager

### 1.2 Selected Architecture — Option C

- PCMonitor와 대칭적 구조 → 팀 코드 리딩에 유리
- 신규 파일 2개(.h/.cpp) 총 ~60 LOC. main.cpp 추가 ~10 LOC. 총 ~70 LOC.
- GPIO1/GPIO3 확장 필요 시 후속 사이클에서 GpioMonitor(B)로 승격 가능 — v2.6에는 YAGNI

### 1.3 Design Principles

- **v2.4.7 방어선 무결성**: NetManager diff = 0
- **기존 자산 재사용**: URL/UI/스키마/getURL()/syncCurrentStates 모두 무변경
- **Edge-triggered fire**: 상태 유지 중 재발화 없음 (서버 스팸 방지)
- **Empty URL self-skip**: `fire()` 기존 로직 그대로 작동

---

## 2. System Architecture

### 2.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                RemoteDeck_PC v2.6.0                          │
│                                                              │
│  main.cpp                                                    │
│  ├─ pcMonitor (PIN_PCLED=4)  [기존, 무변경]                  │
│  │    onChange → fire("pcled_on"|"pcled_off")                │
│  ├─ switchMonitor (PIN_GPIO2=14, INPUT_PULLUP)  ◄── 신규     │
│  │    onChange → fire("gpio2_low"|"gpio2_high")              │
│  ├─ webRequestHandler [기존, 무변경]                          │
│  │    getURL("gpio2_low")  → config.webRequest.gpio2_low     │
│  │    getURL("gpio2_high") → config.webRequest.gpio2_high    │
│  │    empty URL → self-skip                                  │
│  └─ NetManager [불변, v2.4.7 방어선]                          │
│                                                              │
│  data/www/  [무변경]                                          │
│  └─ 설정 > Web Request 탭 > GPIO 2 HIGH/LOW URL              │
│                                                              │
│  /deviceconfig.json  [스키마 무변경]                          │
│  └─ webRequest.gpio2_high / gpio2_low  [기존 필드]           │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Module Boundaries

| Module | Responsibility | Change |
|--------|----------------|:------:|
| `SwitchMonitor` (신규) | GPIO2 상태 감지·디바운스·콜백 | **New** |
| `main.cpp` | 인스턴스 + wiring + loop | Modify (+~10 LOC) |
| `ConfigManager` | 버전 stamp 2.5.1 → 2.6.0 | Modify (+2) |
| `WebRequestHandler` | fire/getURL | **No change** |
| `PCMonitor` | PIR/PCLED 감지 | **No change** |
| `data/www/*` | Web UI | **No change** |
| `NetManager` | 콜드 부팅 방어선 | **No change (SC-8)** |

---

## 3. Data Model

### 3.1 SwitchMonitor 내부 상태

```cpp
class SwitchMonitor {
public:
    void begin(uint8_t pin);
    void loop();
    bool isActive() const { return _currentState; }  // true = LOW = active

    using Callback = std::function<void(bool active)>;
    void setOnChange(Callback cb) { _onChange = cb; }

    // v2.6 default: PCMonitor와 동일 파라미터
    void setPollInterval(uint16_t ms) { _pollMs = ms; }

private:
    uint8_t _pin = 0;
    bool _lastState = false;
    bool _currentState = false;
    uint16_t _pollMs = 1000;
    unsigned long _lastPoll = 0;
    uint8_t _debounceCount = 0;
    static constexpr uint8_t DEBOUNCE_THRESHOLD = 3;
    Callback _onChange = nullptr;
};
```

### 3.2 상태 전이 정의

| GPIO2 물리 상태 | `digitalRead` | `isActive()` (내부) | fire event |
|---|:-:|:-:|---|
| 스위치 ON (광커플러 도통, GND 연결) | LOW | true (active) | `gpio2_low` |
| 스위치 OFF (개방, INPUT_PULLUP) | HIGH | false | `gpio2_high` |

### 3.3 지속성

- 없음. 모든 상태는 RAM에만 존재. 재부팅 시 초기화 → 첫 poll에서 현재 상태로 재판정.
- 부팅 sync (`syncCurrentStates`) 는 v2.5.1에서 이미 GPIO2 채널 포함 → 부팅 직후 현재 상태 URL 1회 호출 (v2.6에서 SwitchMonitor 로 채널 감지 시작하기 전에 발생).

---

## 4. API Contract

**변경 없음.** 모든 기존 엔드포인트와 이벤트 이름 유지.

| Endpoint | v2.5.1 | v2.6.0 |
|---|---|---|
| `GET /api/status` | gpio2 스냅숏 필드 | 동일 (SwitchMonitor 활성/비활성 무관 실제 digitalRead) |
| `POST /api/config` | gpio2_high/low URL 저장 | 동일 |
| WebRequest event `gpio2_high` | syncCurrentStates에서만 발화 | + 상태 전이 시에도 발화 |
| WebRequest event `gpio2_low` | 동일 | + 상태 전이 시에도 발화 |

### 4.1 fire() 호출 스펙

```cpp
// 상태 LOW→HIGH 전이 (스위치 OFF)
webRequestHandler.fire("gpio2_high", 0);

// 상태 HIGH→LOW 전이 (스위치 ON)
webRequestHandler.fire("gpio2_low", 1);
```

value 인자는 기존 pcled와 동일 관례(활성=1, 비활성=0).

---

## 5. Detailed Module Design

### 5.1 SwitchMonitor.h

```cpp
#pragma once
#include <Arduino.h>
#include <functional>

// Design Ref: §5.1 — PCMonitor 패턴을 GPIO2 접점 입력용으로 미러링.
// INPUT_PULLUP 사용, LOW = active (스위치 ON / 광커플러 도통).
class SwitchMonitor {
public:
    void begin(uint8_t pin);
    void loop();

    bool isActive() const { return _currentState; }

    void setPollInterval(uint16_t ms) { _pollMs = ms; }

    using Callback = std::function<void(bool active)>;
    void setOnChange(Callback cb) { _onChange = cb; }

private:
    uint8_t _pin = 0;
    bool _lastState = false;
    bool _currentState = false;
    uint16_t _pollMs = 1000;
    unsigned long _lastPoll = 0;
    uint8_t _debounceCount = 0;
    static constexpr uint8_t DEBOUNCE_THRESHOLD = 3;
    Callback _onChange = nullptr;
};
```

### 5.2 SwitchMonitor.cpp

```cpp
#include "SwitchMonitor.h"

// Design Ref: §5.2 — INPUT_PULLUP 로 초기화.
// 광커플러 접점이 GND로 당기면 LOW → active.
void SwitchMonitor::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);
    // LOW = active
    _currentState = (digitalRead(_pin) == LOW);
    _lastState = _currentState;
}

void SwitchMonitor::loop() {
    unsigned long now = millis();
    if (now - _lastPoll < _pollMs) return;
    _lastPoll = now;

    bool reading = (digitalRead(_pin) == LOW);

    if (reading != _lastState) {
        _debounceCount++;
        if (_debounceCount >= DEBOUNCE_THRESHOLD) {
            _lastState = reading;
            _currentState = reading;
            _debounceCount = 0;

            Serial.printf("Switch State: %s\n", _currentState ? "ACTIVE (LOW)" : "INACTIVE (HIGH)");

            if (_onChange) {
                _onChange(_currentState);
            }
        }
    } else {
        _debounceCount = 0;
    }
}
```

### 5.3 main.cpp 통합

**추가 위치 1: 인스턴스 선언 (기존 pcMonitor 근처)**

```cpp
#include "control/SwitchMonitor.h"
// ...
SwitchMonitor switchMonitor;
```

**추가 위치 2: setup() 초기화 (기존 pcMonitor.begin 근처)**

```cpp
// 기존 (참고)
// pinMode(PIN_GPIO2, INPUT);   ← 이 줄 제거 (SwitchMonitor.begin이 pinMode 설정)
// ...

// Design Ref: §5.3 — SwitchMonitor 초기화 (GPIO2 INPUT_PULLUP)
switchMonitor.begin(PIN_GPIO2);
switchMonitor.setPollInterval(config.monitor.pcledPollMs);  // pcled와 동일 주기 재사용
switchMonitor.setOnChange([](bool active) {
    webRequestHandler.fire(active ? "gpio2_low" : "gpio2_high", active ? 1 : 0);
});
```

**추가 위치 3: loop() 폴링**

```cpp
void loop() {
    networkManager.loop();
    mqttHandler.loop();
    rs485Handler.loop();
    relayController.loop();
    pcMonitor.loop();
    switchMonitor.loop();   // ← 추가
    scheduleManager.loop();
    // ...
}
```

**변경 사항 최소화**:
- `pinMode(PIN_GPIO2, INPUT);` 제거 (SwitchMonitor::begin이 INPUT_PULLUP으로 설정)
- 순서: setup에서 `switchMonitor.begin` 은 `PIN_GPIO1/3 pinMode` 근처(기존 라인 454 부근)
- `syncCurrentStates`의 gpio2 reader lambda는 그대로 유지 (`digitalRead(PIN_GPIO2) == HIGH ? 1 : 0`) — INPUT_PULLUP 이후에도 정상 판독

### 5.4 ConfigManager 버전 stamp

- `"2.5.1"` → `"2.6.0"` (2곳: load() default, loadDefaults())
- 날짜 `"2026-07-03"` → `"2026-07-05"` (또는 실제 빌드일)

---

## 6. Sequence Diagrams

### 6.1 스위치 ON (재실 감지)

```
Field         GPIO2 pin      SwitchMonitor          WebRequestHandler          Server
  │              │                │                       │                       │
  ├─ Switch ON  ─┤ LOW            │                       │                       │
  │              │                │                       │                       │
  │              │(1s)  loop() ──▶│ reading=true          │                       │
  │              │                │ _lastState(false)≠reading→debounce=1          │
  │              │(2s)  loop() ──▶│ debounce=2                                    │
  │              │(3s)  loop() ──▶│ debounce=3 ≥ threshold                       │
  │              │                │ _currentState=true                            │
  │              │                │ onChange(true) ──────▶│                       │
  │              │                │                       ├─ fire("gpio2_low",1)  │
  │              │                │                       │  queue push           │
  │              │                │                       │  worker task ────────▶│ HTTP GET
  │              │                │                       │                       │ (재실 이벤트)
```

**Latency**: 최대 4초 (poll 1s × 3 debounce + fire→HTTP 처리)

### 6.2 스위치 OFF (부재 판정)

```
Field         GPIO2         SwitchMonitor         WebRequestHandler         Server
  │              │                │                       │                    │
  ├─ Switch OFF ─┤ HIGH           │                       │                    │
  │              │(1~3s)    loop() ──▶ debounce 3회       │                    │
  │              │                │ _currentState=false                        │
  │              │                │ onChange(false) ──────▶ fire("gpio2_high",0)│
  │              │                │                       │ HTTP GET ─────────▶│ (부재)
```

### 6.3 상태 유지 중 (재발화 없음)

```
Field         GPIO2         SwitchMonitor
  │              │                │
  ├─ Switch ON (계속) ─┤ LOW      │
  │              │(1s)   loop() ──▶ reading=true, _lastState=true → debounce=0
  │              │(2s)   loop() ──▶ 동일
  │              │ ...            │  (fire 호출 없음, edge-triggered)
```

---

## 7. State Machine

### 7.1 SwitchMonitor 내부 상태

```
[UNINITIALIZED]
      │
      ├─ begin(pin)
      │
      ▼
[INIT] (INPUT_PULLUP, 초기 read → _currentState)
      │
      ▼
[STABLE_HIGH] ◄────┐          ┌──▶ [STABLE_LOW]
      │            │          │            │
      │ read=LOW   │ 3회      │ read=HIGH  │
      │            │ 미달성   │            │
      ▼            │          │            ▼
[DEBOUNCE_TO_LOW] ─┘          └── [DEBOUNCE_TO_HIGH]
      │                                    │
      │ 3회 연속 LOW                       │ 3회 연속 HIGH
      │                                    │
      ├──▶ onChange(true) → STABLE_LOW    ├──▶ onChange(false) → STABLE_HIGH
```

---

## 8. Test Plan

### 8.1 Unit Tests (L1)

- [ ] `SwitchMonitor::begin` 이 `pinMode(_, INPUT_PULLUP)` 호출하는지 (mock 불가하므로 실기 검증)
- [ ] 초기 read 값이 `_currentState` 에 반영되는지
- [ ] 상태 변화 3회 연속 일치 시 콜백 호출
- [ ] 상태 유지 중 재호출 없음
- [ ] Poll 주기 조정 setPollInterval 반영

### 8.2 Integration Tests (L2)

- [ ] PIN_GPIO2 를 실제 3.3V → GND 로 수동 토글 → 4초 이내 콜백 발화
- [ ] `webRequestHandler.fire` 호출 확인 (Serial 로그 `WebRequest [200|...]`)
- [ ] URL 미설정 시 fire 내부 skip 확인 (WebRequest 로그 없음)
- [ ] 상태 유지 30초 이상 재발화 없음

### 8.3 E2E (L3) — 필드 검증

- [ ] SC-1/2: GPIO2 물리 전이 후 ≤4초 서버 도달
- [ ] SC-3/4: 조명 스위치 배선 기기에서 서버 액세스 로그 GET 도달
- [ ] SC-6: PCLED 흐름 무변화 (14대 필드 회귀 없음)
- [ ] SC-10: 하루 이상 dogfood, 오탐/미탐 각 ≤ 5%

### 8.4 Regression

- [ ] `/api/status` gpio2 필드 shape 무변화
- [ ] `POST /api/config` gpio2_high/low URL 저장/불러오기
- [ ] `NetManager.h/.cpp` diff = 0
- [ ] `syncCurrentStates` 부팅 sync 동작 무변화

---

## 9. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| GPIO2 INPUT_PULLUP 이 기존 pcled sync와 충돌 | v2.5.1의 syncCurrentStates gpio2 reader는 `digitalRead(HIGH)?1:0` 방식이라 pull 방향에 상관없이 정상 판독. |
| 광커플러 노이즈로 오감지 | 3x debounce = 최소 2초 안정 필요. 추가 튜닝 필요 시 setPollInterval / DEBOUNCE_THRESHOLD 상향. |
| loop() 처리 시간 증가 | `if (now-_lastPoll<_pollMs) return;` 초기 조건으로 대부분 즉시 반환. 무시할 수준. |
| 상태 초기값과 첫 콜백 관계 | begin() 은 콜백 발화하지 않음. 최초 상태는 부팅 sync가 담당 (v2.5.1 syncCurrentStates). |
| 필드 배포 도중 오배선으로 fire 폭주 | Edge-triggered라 자연 방지. 다만 접촉 불량으로 채터링 시 debounce가 걸러줌. |

---

## 10. Non-Functional Verification

| NFR | Method |
|-----|--------|
| Flash ≤ 3KB 증가 | `pio run` 로그의 Program 크기 비교 |
| Heap ≥ 100KB | 부팅 후 `ESP.getFreeHeap()` |
| Latency ≤ 4s | 실기 스톱워치 or Serial timestamp |
| API/UI shape 무변화 | 기존 IntegrateController 정상 동작 확인 |
| NetManager diff=0 | `git diff v2.5.1 -- src/network/NetManager.*` empty |

---

## 11. Implementation Guide

### 11.1 File Change Summary

| File | Change | Lines (est.) |
|------|:------:|:------------:|
| `RemoteDeck_PC/src/control/SwitchMonitor.h` | **New** | ~30 |
| `RemoteDeck_PC/src/control/SwitchMonitor.cpp` | **New** | ~35 |
| `RemoteDeck_PC/src/main.cpp` | Modify | ~10 (include + 인스턴스 + begin + setOnChange + loop 호출), `pinMode(PIN_GPIO2, INPUT)` 삭제 1줄 |
| `RemoteDeck_PC/src/config/ConfigManager.cpp` | Modify | 4 (버전+날짜 2곳) |
| **NetManager.h/cpp** | **No change** | **0 (SC-8)** |
| **Web UI (data/www/*)** | **No change** | **0** |
| **DeviceConfig.h / WebRequestHandler.{h,cpp}** | **No change** | **0** |

**총계**: 신규 2 파일 (~65 LOC), 수정 2 파일 (~14 LOC). 예상 diff ≈ **+80 LOC**.

### 11.2 Implementation Order

1. `SwitchMonitor.h/.cpp` 신규 파일 작성 (PCMonitor 복사 + 특화)
2. `main.cpp`:
   - `#include "control/SwitchMonitor.h"`
   - `SwitchMonitor switchMonitor;` 전역
   - setup에서 기존 `pinMode(PIN_GPIO2, INPUT)` 제거
   - `switchMonitor.begin(PIN_GPIO2)` + `setPollInterval` + `setOnChange` wiring
   - loop에 `switchMonitor.loop()` 추가
3. `ConfigManager.cpp` 버전/날짜 스탬프
4. 로컬 빌드 (`pio run`) → Program 크기 비교 (v2.5.1 대비 ≤ +3KB)
5. Serial 모니터로 GPIO2 수동 토글하며 상태 로그 및 fire 확인
6. `RemoteDeck_PC_V2.6.0_OTA_20260705.bin` firmware/ 배치
7. SPIFFS 변경 없음 (v2.5.2 spiffs 그대로 사용)
8. 실제 광커플러 배선 기기 1대에 배포 → dogfood

### 11.3 Session Guide

**Module Map**:

| Module Key | Files | 예상 시간 |
|------------|-------|-----------|
| `switch-monitor` | SwitchMonitor.{h,cpp} | 15분 |
| `main-wiring` | main.cpp | 15분 |
| `version-stamp` | ConfigManager.cpp | 5분 |
| `build-verify` | pio run + Serial 검증 | 15분 |

**Recommended Session Plan**:

- **Session 1** (통합 세션, 1시간 이하): 전 4개 모듈 순차 수행. 코드 소량이라 세션 분할 불필요.

---

## 12. Open Items (Resolution before Do phase)

| Item | Decision |
|------|----------|
| SwitchMonitor 폴 주기: config로 노출? | **재사용**: `config.monitor.pcledPollMs` 값 그대로 setPollInterval에 넘김. 필드에서 다른 값 필요하면 향후 별도 필드 (v2.6.1+) |
| Debounce N=3 상수 vs 설정 노출 | 상수 유지 (필드 관찰 후 필요 시 v2.6.1+에서 조정) |
| GPIO1/GPIO3 동일 확장 | 이번 스코프 아님, GpioMonitor(B) 승격은 후속 사이클 |
| 부팅 시 SwitchMonitor의 첫 콜백 발화 안 함 | v2.5.1 syncCurrentStates가 gpio2 채널을 이미 부팅 시 호출하므로 이중 발화 방지 |

---

**Next Step**: `/pdca do RemoteDeck_PC_v2.6`
