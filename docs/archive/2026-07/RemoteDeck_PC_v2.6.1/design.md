---
template: design
version: 1.3
feature: RemoteDeck_PC_v2.6.1
date: 2026-07-06
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.6.0 → v2.6.1
architecture: Option C - AttendanceHandler (Pragmatic Balance)
---

# RemoteDeck_PC v2.6.1 Design Document

> **Architecture**: Option C — 얕은 AttendanceHandler 클래스 (config check + fire dispatch)
> **Baseline**: v2.6.0 firmware
> **Target**: v2.6.1 firmware + SPIFFS 갱신 (기타 탭 카드 UI 신규)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.6에서 감지 채널(PIR/스위치) 확보 후, 설치 운영자가 채널명 지식 없이 재부재 연동 완결하도록 통합 UI |
| **WHO** | 재부재 시스템 설치·운영 인력, 서버 담당 |
| **RISK** | attendance 블록 없는 기존 deviceconfig.json 로드 호환 · 이중 발화 서버 부담 · v2.4.7 방어선 훼손 방지 |
| **SUCCESS** | 카드 입력만으로 이벤트 발화 · 기존 Web Request 탭/pcled/gpio2_low 흐름 무변화 · NetManager diff=0 · 14대 필드 무영향 |
| **SCOPE** | Phase1 펌웨어(Handler+config+wiring+WebRequest 이벤트) → Phase2 Web UI 카드 + JS → Phase3 필드 dogfood |

---

## 1. Overview

### 1.1 Architecture Summary

- **AttendanceHandler 신규 (얕은 클래스)**
  - `begin(const AttendanceConfig* cfg, WebRequestHandler* wr)` — 참조 저장만
  - `onSourceStateChange(const char* sourceKey, bool active)` — enabled + source 매칭 시 `wr->fire("attendance_on"|"attendance_off", ...)`
- **DeviceConfig.attendance 블록 신규**: enabled/source (`"pcled"` | `"gpio2"`)
- **WebRequestConfig 확장**: `attendance_on`, `attendance_off` URL 필드 (fire 파이프라인 재사용)
- **WebRequestHandler::getURL()** 케이스 추가
- **main.cpp wiring**: pcMonitor·switchMonitor onChange 콜백 안에 기존 fire 유지 + `attendanceHandler.onSourceStateChange` 호출 1줄 추가
- **ConfigManager** load/save: attendance 블록 하위호환 (`|` 연산자로 기본값)
- **Web UI**: 설정 > 기타 탭 하단에 `재부재 시스템` 카드 신규
- **NetManager**: **No change**

### 1.2 Selected Architecture — Option C

- **Handler 얕게**: 상태·enum 없음. config 참조 + fire 호출만. ~50 LOC.
- **이중 발화 정책**: main.cpp 콜백은 기존 `fire("pcled_on")` 등 유지 + `attendanceHandler.onSourceStateChange` 추가 호출. Attendance는 config 매칭 시에만 별도 fire.
- **UI 신규 카드**: 기존 사용자 학습 부담 최소화. 익숙한 체크박스 + select + URL 2개 패턴.
- **v2.6.2+ 확장 지점**: GPIO3 소스 추가 시 sourceKey 문자열만 추가하면 됨.

### 1.3 Design Principles

- v2.4.7 방어선 무결성 (NetManager diff=0)
- 기존 자산 최대 재사용 (fire pipeline, replacePlaceholders, saveEtc/loadConfig 패턴)
- 하위호환 필수 (기존 14대 필드 무영향)
- 얇은 신규 모듈 (~50 LOC 목표)

---

## 2. System Architecture

### 2.1 Component Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                    RemoteDeck_PC v2.6.1                          │
│                                                                  │
│  main.cpp                                                        │
│  ├─ pcMonitor (PIN_PCLED)                                        │
│  │    setOnChange: [](bool a) {                                  │
│  │       webRequestHandler.fire(a?"pcled_on":"pcled_off",...);   │
│  │       attendanceHandler.onSourceStateChange("pcled", a);◄─新  │
│  │    }                                                          │
│  ├─ switchMonitor (PIN_GPIO2)                                    │
│  │    setOnChange: [](bool a) {                                  │
│  │       webRequestHandler.fire(a?"gpio2_low":"gpio2_high",...); │
│  │       attendanceHandler.onSourceStateChange("gpio2", a);◄─新  │
│  │    }                                                          │
│  ├─ attendanceHandler  ◄── 신규 (얕은 dispatcher)                │
│  │    begin(&config.attendance, &webRequestHandler)              │
│  │    onSourceStateChange("pcled"|"gpio2", active)               │
│  │      → enabled && source_match ? fire(...) : skip             │
│  ├─ webRequestHandler [기존]                                      │
│  │    getURL("attendance_on") → cfg.attendance_on                │
│  │    getURL("attendance_off") → cfg.attendance_off              │
│  └─ NetManager [불변, v2.4.7 방어선]                              │
│                                                                  │
│  data/www/  [기타 탭 하단 카드 신규]                               │
│  └─ 설정 > 기타 > [재부재 시스템 카드]                              │
│       ☑️ 연동 · [PC LED / SwitchMonitor(GPIO2)] · ON/OFF URL     │
│                                                                  │
│  /deviceconfig.json                                              │
│  ├─ webRequest.attendance_on / attendance_off  ← 신규 URL 필드    │
│  └─ attendance.{enabled, source}               ← 신규 블록        │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 Module Boundaries

| Module | Responsibility | Change |
|--------|----------------|:------:|
| `AttendanceHandler` (신규) | config 검사 + fire 호출 dispatcher | **New** |
| `DeviceConfig` | AttendanceConfig 구조체 추가 + WebRequestConfig attendance_on/off | Modify (+~10) |
| `ConfigManager` | attendance 블록 load/save 하위호환 | Modify (+~15) |
| `WebRequestHandler` | getURL() attendance 케이스 추가 | Modify (+~2) |
| `main.cpp` | attendanceHandler 인스턴스·begin + onChange 콜백에 1줄씩 추가 | Modify (+~10) |
| `data/www/index.html` | 기타 탭에 재부재 시스템 카드 추가 | Modify (+~20) |
| `data/www/app.js` | saveEtc/loadConfig 확장 + 소스 select 토글 | Modify (+~30) |
| `NetManager` | v2.4.7 방어선 | **No change (SC-8)** |
| PCMonitor / SwitchMonitor | 무변경 (기존 setOnChange 재사용) | **No change** |

---

## 3. Data Model

### 3.1 AttendanceConfig 신규

```cpp
// DeviceConfig.h
struct AttendanceConfig {
    bool        enabled = false;
    std::string source  = "pcled";   // "pcled" | "gpio2"
};

struct DeviceConfig {
    // ... 기존 필드
    WebRequestConfig  webRequest;
    AttendanceConfig  attendance;   // ← 신규
};
```

### 3.2 WebRequestConfig 확장

```cpp
struct WebRequestConfig {
    // ... 기존 필드 (relay1_on..gpio3_low)
    std::string attendance_on;   // ← 신규
    std::string attendance_off;  // ← 신규
};
```

**Rationale**: attendance URL을 별도 필드로 두지 않고 WebRequestConfig에 두는 이유는 기존 `WebRequestHandler::getURL()` + `fire()` 파이프라인 100% 재사용. URL placeholder 치환도 자동.

### 3.3 /deviceconfig.json 스키마 (하위호환)

```json
{
  "device": { ... },
  "network": { ... },
  "mqtt": { ... },
  "webRequest": {
    "enabled": true,
    "timeoutMs": 5000,
    "relay1_on": "...",
    "...": "...",
    "gpio2_high": "...",
    "gpio2_low": "...",
    "attendance_on":  "http://server/attend?d=[device_id]&s=on",    // ← 신규
    "attendance_off": "http://server/attend?d=[device_id]&s=off"    // ← 신규
  },
  "attendance": {                                                     // ← 신규 블록
    "enabled": false,
    "source":  "pcled"
  }
}
```

**하위호환 규칙**:
- `attendance` 블록 없으면 `{enabled:false, source:"pcled"}` 기본값
- `webRequest.attendance_on/off` 없으면 빈 문자열 (fire self-skip)
- 기존 14대 필드 deviceconfig.json (attendance 블록 없음) → 정상 로드 + attendance disabled

---

## 4. API Contract

### 4.1 신규 이벤트 이름

| Event | Trigger | URL 슬롯 |
|---|---|---|
| `attendance_on` | AttendanceHandler가 활성 상태 감지 후 fire | `webRequest.attendance_on` |
| `attendance_off` | AttendanceHandler가 비활성 상태 감지 후 fire | `webRequest.attendance_off` |

### 4.2 기존 이벤트 무변경

- `pcled_on` / `pcled_off` — PIR 상태 변화 시 (v2.5 이전과 동일)
- `gpio2_low` / `gpio2_high` — SwitchMonitor 상태 변화 시 (v2.6과 동일)
- 릴레이/기타 GPIO 이벤트 — 무변경

### 4.3 /api/status 응답

**변경 없음**. Attendance는 서버로만 URL 호출. status 응답 shape 유지.

### 4.4 /api/config (POST/GET)

- **GET**: attendance 블록 포함해 반환
- **POST**: attendance 블록 저장 (기존 setConfig 흐름)
- 하위호환 유지 (attendance 블록 없어도 정상)

---

## 5. Detailed Module Design

### 5.1 AttendanceHandler.h (신규)

```cpp
#pragma once
#include <Arduino.h>
#include "config/DeviceConfig.h"

class WebRequestHandler;  // fwd

// Design Ref: RemoteDeck_PC_v2.6.1 §5.1 — 얕은 dispatcher.
// pcMonitor/switchMonitor onChange 콜백에서 호출됨.
// config.enabled + source 매칭 시에만 fire.
class AttendanceHandler {
public:
    void begin(const AttendanceConfig* cfg, WebRequestHandler* wr);
    void onSourceStateChange(const char* sourceKey, bool active);

private:
    const AttendanceConfig* _cfg = nullptr;
    WebRequestHandler*      _wr  = nullptr;
};
```

### 5.2 AttendanceHandler.cpp (신규)

```cpp
#include "AttendanceHandler.h"
#include "network/WebRequestHandler.h"
#include <string.h>

void AttendanceHandler::begin(const AttendanceConfig* cfg, WebRequestHandler* wr) {
    _cfg = cfg;
    _wr  = wr;
}

// Plan SC-1/2/3: config.enabled + source 매칭 확인 후 fire.
void AttendanceHandler::onSourceStateChange(const char* sourceKey, bool active) {
    if (!_cfg || !_wr) return;
    if (!_cfg->enabled) return;
    if (strcmp(_cfg->source.c_str(), sourceKey) != 0) return;

    const char* event = active ? "attendance_on" : "attendance_off";
    _wr->fire(event, active ? 1 : 0);
    Serial.printf("Attendance: source=%s active=%d → %s\n",
                  sourceKey, active ? 1 : 0, event);
}
```

### 5.3 WebRequestHandler::getURL() 확장

```cpp
// WebRequestHandler.cpp - getURL()의 기존 switch 끝에 추가
if (strcmp(event, "attendance_on")  == 0) return String(_config->attendance_on.c_str());
if (strcmp(event, "attendance_off") == 0) return String(_config->attendance_off.c_str());
```

### 5.4 main.cpp wiring

```cpp
#include "control/AttendanceHandler.h"
// ...
AttendanceHandler attendanceHandler;

// setup() 안, 기존 pcMonitor/switchMonitor 초기화 이후:
attendanceHandler.begin(&config.attendance, &webRequestHandler);

// pcMonitor onChange 콜백 확장 (기존 fire 유지 + attendance 호출 추가):
pcMonitor.setOnChange([](bool pcOn) {
    webRequestHandler.fire(pcOn ? "pcled_on" : "pcled_off", pcOn ? 1 : 0);
    logger.log("PCLED", pcOn ? "PC ON" : "PC OFF");
    attendanceHandler.onSourceStateChange("pcled", pcOn);  // ← 추가
});

// switchMonitor onChange 콜백 확장:
switchMonitor.setOnChange([](bool active) {
    webRequestHandler.fire(active ? "gpio2_low" : "gpio2_high", active ? 1 : 0);
    attendanceHandler.onSourceStateChange("gpio2", active);  // ← 추가
});
```

**주의**: 기존 pcMonitor onChange는 `onPCStateChange(bool)` 함수 참조로 등록되어 있음. 이 함수 안에서 `attendanceHandler.onSourceStateChange("pcled", pcOn)` 을 추가하거나, 함수 대신 인라인 람다로 변경. Design 관점에서는 후자가 wiring 위치를 한 곳에 모아 두는 장점.

### 5.5 ConfigManager 확장

**load 하위호환**:
```cpp
// ConfigManager.cpp - load() 안, 기존 webRequest 로드 후:
JsonObject att = doc["attendance"].as<JsonObject>();
config.attendance.enabled = att["enabled"] | false;
config.attendance.source  = att["source"]  | "pcled";

// webRequest attendance URL도 로드
if (wr.containsKey("attendance_on"))  config.webRequest.attendance_on  = wr["attendance_on"].as<std::string>();
if (wr.containsKey("attendance_off")) config.webRequest.attendance_off = wr["attendance_off"].as<std::string>();
```

**save**:
```cpp
// save() 안, webRequest 저장 후:
wr["attendance_on"]  = config.webRequest.attendance_on;
wr["attendance_off"] = config.webRequest.attendance_off;

JsonObject att = doc.createNestedObject("attendance");
att["enabled"] = config.attendance.enabled;
att["source"]  = config.attendance.source;
```

### 5.6 Web UI - 설정 > 기타 탭 카드

**index.html** — 기타 탭(`#stab-etc`) 하단, "저장"/"재부팅" 버튼 위에 카드 삽입:

```html
<div class="card">
  <h2>재부재 시스템</h2>
  <p style="color:#888;font-size:12px">선택한 센서의 상태 변화 시 ON/OFF URL이 서버로 호출됩니다.
     기존 Web Request 탭의 개별 URL과 독립적으로 동작합니다.</p>
  <div class="cfg-group">
    <label><input type="checkbox" id="cfg-att-enabled"> 재부재 시스템 연동하기</label>
  </div>
  <div class="cfg-group">
    <label>사용 센서</label>
    <select id="cfg-att-source">
      <option value="pcled">PC LED (PIR)</option>
      <option value="gpio2">SwitchMonitor (GPIO2)</option>
    </select>
  </div>
  <div class="cfg-group">
    <label>ON URL (재실)</label>
    <input type="text" id="cfg-att-on" style="max-width:100%" placeholder="http://server/attend?d=[device_id]&s=on">
  </div>
  <div class="cfg-group">
    <label>OFF URL (부재)</label>
    <input type="text" id="cfg-att-off" style="max-width:100%" placeholder="http://server/attend?d=[device_id]&s=off">
  </div>
  <p style="color:#666;font-size:11px">플레이스홀더: [device_id] [device_name] [ip] [mac] [event] [value]</p>
</div>
```

**app.js** — `saveEtc()` 확장:
```javascript
function saveEtc() {
  // ... 기존 릴레이/모니터/WOL/NTP 값 수집
  cfg.attendance = {
    enabled: document.getElementById('cfg-att-enabled').checked,
    source:  document.getElementById('cfg-att-source').value
  };
  cfg.webRequest = cfg.webRequest || {};
  cfg.webRequest.attendance_on  = document.getElementById('cfg-att-on').value;
  cfg.webRequest.attendance_off = document.getElementById('cfg-att-off').value;
  // 기존 fetch('/api/config', ...) 그대로
}
```

**app.js** — `loadConfig()` 확장:
```javascript
// 기존 값 채우기 로직 안:
if (d.attendance) {
  document.getElementById('cfg-att-enabled').checked = d.attendance.enabled;
  document.getElementById('cfg-att-source').value    = d.attendance.source || 'pcled';
}
if (d.webRequest) {
  document.getElementById('cfg-att-on').value  = d.webRequest.attendance_on  || '';
  document.getElementById('cfg-att-off').value = d.webRequest.attendance_off || '';
}
```

---

## 6. Sequence Diagrams

### 6.1 PIR 감지 + 재부재 활성 (source=pcled)

```
Field       PCMonitor         main.cpp lambda       AttendanceHandler       WebRequestHandler       Server
  │             │                    │                     │                        │                   │
  ├─ 재실 감지 ─▶│ debounce            │                     │                        │                   │
  │             ├─ onChange(true) ───▶│                     │                        │                   │
  │             │                    ├─ fire("pcled_on") ────────────────────────────▶│                   │
  │             │                    │                     │                        ├─ HTTP GET ──────▶│ (기존)
  │             │                    ├─ onSourceStateChange("pcled", true) ▶│         │                   │
  │             │                    │                     ├─ cfg.enabled=T          │                   │
  │             │                    │                     ├─ src match              │                   │
  │             │                    │                     ├─ fire("attendance_on")─▶│                   │
  │             │                    │                     │                        ├─ HTTP GET ──────▶│ (신규)
```

### 6.2 SwitchMonitor 감지 + 재부재 활성 (source=gpio2)

```
Field       SwitchMonitor      main.cpp lambda       AttendanceHandler       WebRequestHandler
  │              │                    │                     │                        │
  ├─ 스위치 OFF ─▶│ HIGH 감지           │                     │                        │
  │              ├─ onChange(false) ─▶│                     │                        │
  │              │                    ├─ fire("gpio2_high") ────────────────────────▶│
  │              │                    ├─ onSourceStateChange("gpio2", false) ▶│      │
  │              │                    │                     ├─ src match ("gpio2")   │
  │              │                    │                     ├─ fire("attendance_off")▶│
```

### 6.3 재부재 비활성 (enabled=false)

```
Field       PCMonitor         main.cpp lambda       AttendanceHandler
  │             │                    │                     │
  ├─ 재실 감지 ─▶│                    │                     │
  │             ├─ onChange(true) ───▶│                     │
  │             │                    ├─ fire("pcled_on")   │
  │             │                    ├─ onSourceStateChange("pcled", true) ▶│
  │             │                    │                     ├─ cfg.enabled=F → return (no fire)
```

### 6.4 소스 불일치 (source=pcled, 이벤트는 gpio2)

```
Field       SwitchMonitor      main.cpp lambda       AttendanceHandler
  │              │                    │                     │
  ├─ 스위치 ON ──▶│                    │                     │
  │              ├─ onChange(true) ──▶│                     │
  │              │                    ├─ fire("gpio2_low")  │
  │              │                    ├─ onSourceStateChange("gpio2", true) ▶│
  │              │                    │                     ├─ enabled=T, but src="pcled" ≠ "gpio2" → return
```

---

## 7. State Machine

**AttendanceHandler는 상태 없음**. `onSourceStateChange`는 매번 config를 확인해 fire 여부 결정 (stateless dispatcher).

**AttendanceConfig 변경 흐름**:
```
[UI 저장]
   │
   ▼
POST /api/config
   │
   ▼
ConfigManager.save (attendance 블록 포함)
   │
   ▼
config.attendance 값 갱신 (in-memory)
   │
   ├─ (자동 재부팅 - saveEtc 기존 정책)
   │
   ▼
재부팅 후 begin() 다시 호출 → 새 config 반영
```

---

## 8. Test Plan

### 8.1 Unit Tests (L1)

- [ ] `AttendanceHandler::begin` — 참조 저장 확인
- [ ] `onSourceStateChange` — enabled=false 시 fire 없음
- [ ] `onSourceStateChange` — source 불일치 시 fire 없음
- [ ] `onSourceStateChange` — enabled + source match + active=true → fire("attendance_on")
- [ ] `onSourceStateChange` — enabled + source match + active=false → fire("attendance_off")
- [ ] `getURL("attendance_on")` — cfg.attendance_on 반환
- [ ] `getURL("attendance_off")` — cfg.attendance_off 반환

### 8.2 Integration Tests (L2)

- [ ] `/api/config` POST에 attendance 블록 포함 → 저장 후 GET에서 반환
- [ ] `/api/config` POST에 attendance 블록 없이 → 기존 값 유지
- [ ] 부팅 시 attendance 블록 없는 deviceconfig.json 로드 → disabled 기본
- [ ] SPIFFS OTA 후 attendance 값 보존 (v2.5.1 backupSpiffsConfigs가 deviceconfig.json 통째 처리)

### 8.3 E2E (L3) — 필드 검증

- [ ] SC-1: 카드 체크 + source=pcled + ON URL 설정 후 PIR 감지 → 서버에 ON URL GET 도달
- [ ] SC-2: 카드 체크 + source=gpio2 + OFF URL 설정 후 GPIO2 HIGH → 서버에 OFF URL GET 도달
- [ ] SC-3: 카드 체크 해제 후 상태 변화 → attendance URL 없음
- [ ] SC-4: 카드 활성 + 기존 pcled_on URL 함께 설정 → 둘 다 도달
- [ ] SC-5: 저장·재부팅 후 값 재로드 유지
- [ ] SC-6: v2.5.x deviceconfig.json (attendance 없음) → v2.6.1 부팅 시 disabled 로 로드, 기존 동작 유지
- [ ] SC-7: Web Request 탭 무변경 확인

### 8.4 Regression

- [ ] pcled_on/off 발화 정상 (기존 flow)
- [ ] gpio2_low/high 발화 정상 (v2.6.0 flow)
- [ ] gpio1_high/low, gpio3_high/low, relay1/2 그대로
- [ ] `NetManager.h/.cpp` diff = 0
- [ ] SPIFFS OTA config preserve 유지

---

## 9. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| attendance 블록 없는 파일 로드 실패 | ArduinoJson `\|` default. 필드 없어도 crash 없음. |
| 이중 발화로 서버 부담 (attendance + pcled_on 동시) | 사용자 확정 A안. 서버가 중복 처리. 릴리스 노트 명시. |
| 소스 값 오타 ("gpio02" 등) | UI에서 select 로 제한. 파일 직접 수정만 방지 안 됨 → 릴리스 노트에 값 명세 |
| SPIFFS OTA 후 attendance 값 소실 | v2.5.1 OTAHandler.backupSpiffsConfigs가 /deviceconfig.json 통째 backup → 자동 유지 |
| v2.4.7 방어선 훼손 | SC-8 (NetManager diff=0) |
| PCMonitor 콜백을 함수 → 람다로 바꾸는 과정에서 로직 유실 | 기존 onPCStateChange 함수 본문을 람다로 이식하며 검증. 코드 리뷰 시 diff 확인. |

---

## 10. Non-Functional Verification

| NFR | Method |
|-----|--------|
| Flash ≤ 5KB 증가 | `pio run` Program 크기 diff |
| Heap ≥ 100KB | `ESP.getFreeHeap()` |
| 상태 감지 → attendance URL 발화 ≤ 4초 | 시리얼 timestamp |
| 기존 pcled/gpio2 발화 무변화 | 시리얼 로그 확인 |
| NetManager diff=0 | git diff |

---

## 11. Implementation Guide

### 11.1 File Change Summary

| File | Change | Lines (est.) |
|------|:------:|:------------:|
| `src/control/AttendanceHandler.h` | **New** | ~15 |
| `src/control/AttendanceHandler.cpp` | **New** | ~25 |
| `src/config/DeviceConfig.h` | Modify | +4 (AttendanceConfig struct + attendance 필드 + attendance_on/off URL) |
| `src/config/ConfigManager.cpp` | Modify | +15 (load/save + 하위호환 + defaults + 버전) |
| `src/network/WebRequestHandler.cpp` | Modify | +2 (getURL 케이스) |
| `src/main.cpp` | Modify | +8 (include + 인스턴스 + begin + 콜백 2곳에 1줄씩 + pcMonitor 콜백 함수→람다 인라인 이식) |
| `data/www/index.html` | Modify | +20 (카드 마크업, 기타 탭 하단) |
| `data/www/app.js` | Modify | +30 (saveEtc/loadConfig 확장) |
| **NetManager.h/cpp** | **No change** | **0** |
| **PCMonitor / SwitchMonitor / WebServer** | **No change** | **0** |

**총계**: 신규 2 파일 (~40 LOC) + 수정 6 파일 (~79 LOC) ≈ **~120 LOC**.

### 11.2 Implementation Order

**Session 1 — Firmware**:
1. `DeviceConfig.h`: AttendanceConfig struct + WebRequestConfig에 attendance_on/off 필드
2. `AttendanceHandler.h/.cpp` 신규
3. `ConfigManager.cpp` load/save/defaults 확장 (하위호환)
4. `WebRequestHandler.cpp` getURL 케이스
5. `main.cpp` include + 인스턴스 + begin + 콜백 wiring (2곳)
6. `ConfigManager.cpp` 버전 2.6.0 → 2.6.1
7. `pio run` 빌드 성공 확인

**Session 2 — Web UI**:
1. `index.html` 기타 탭 하단에 재부재 시스템 카드
2. `app.js` `saveEtc()` / `loadConfig()` 확장
3. `pio run -t buildfs` SPIFFS bin 빌드
4. firmware bin과 spiffs bin `firmware/` 배치

**Session 3 — 필드 dogfood**:
1. 한 사이트에 v2.6.1 OTA (firmware + spiffs 순차)
2. 재부재 카드 입력·저장 → 재부팅 후 재로드 확인
3. 상태 변화 → 서버 도달 확인

### 11.3 Session Guide

**Module Map**:
| Module Key | Files | 예상 시간 |
|------------|-------|-----------|
| `att-config` | DeviceConfig.h + ConfigManager.cpp | 20분 |
| `att-handler` | AttendanceHandler.{h,cpp} + WebRequestHandler.cpp | 20분 |
| `att-wiring` | main.cpp | 15분 |
| `att-ui` | index.html + app.js | 25분 |
| `build-verify` | pio run + buildfs + Serial 테스트 | 15분 |

**Recommended Split**:
- Session 1: `att-config`, `att-handler`, `att-wiring`, firmware 빌드 (~1시간)
- Session 2: `att-ui`, spiffs 빌드 (~30분)
- Session 3: 필드 dogfood (실측)

---

## 12. Open Items (Resolution before Do phase)

| Item | Decision |
|------|----------|
| attendance URL을 WebRequestConfig에 둘지 vs AttendanceConfig에 둘지 | **WebRequestConfig에 두기** — fire pipeline 재사용 (getURL/replacePlaceholders 자동) |
| pcMonitor 콜백을 함수→람다로 변경 | 람다 인라인 (attendance 호출 위치를 switchMonitor와 대칭). 기존 `onPCStateChange` 함수는 필요 시 제거 or 유지 |
| 소스 옵션 값 명세 | `"pcled"` \| `"gpio2"` 두 값만. UI select로 제한. 알 수 없는 값은 사실상 disabled (source match 실패) |
| 부팅 시 attendance 초기 sync | v2.6.1 스코프 밖. `syncCurrentStates`가 개별 채널(pcled/gpio2)은 호출하므로 서버는 부팅 상태 개별 채널로 파악 가능. attendance 초기 호출은 v2.6.2+ 후보 |

---

**Next Step**: `/pdca do RemoteDeck_PC_v2.6.1`
