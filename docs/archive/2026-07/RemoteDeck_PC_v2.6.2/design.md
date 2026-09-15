---
template: design
version: 1.3
feature: RemoteDeck_PC_v2.6.2
date: 2026-07-06
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.6.1 → v2.6.2
architecture: Option C - AttendanceHandler 확장 + 홈 UI 재구성
---

# RemoteDeck_PC v2.6.2 Design Document

> **Architecture**: Option C — AttendanceHandler 확장 (링버퍼+toJson+syncOnBoot) + 홈 Web UI 재구성
> **Baseline**: v2.6.1 firmware + v2.6.1 SPIFFS
> **Target**: v2.6.2 firmware + v2.6.2 SPIFFS

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.6.1 설정 UX 완결 이후 감시·통합 표면(홈 모니터링·부팅 정합성·외부 API) 정비 |
| **WHO** | 재부재 감시자, 외부 자동화 시스템, 설치 운영자 |
| **RISK** | RAM 증가 · WebSocket 부하 · 이중 fire · 홈 UI 회귀 · v2.4.7 방어선 |
| **SUCCESS** | 홈 카드 조건부 표시 · 상태 모니터 순서/이름/dot · GPIO2 실시간 · 부팅 15s sync · /api/status attendance · NetManager diff=0 |
| **SCOPE** | S1 펌웨어(링버퍼+엔드포인트+broadcast+syncOnBoot+status) → S2 홈 Web UI → S3 필드 dogfood |

---

## 1. Overview

### 1.1 Architecture Summary

- **AttendanceHandler 확장**:
  - 내부 링버퍼 `Entry _history[8]` + `head`, 각 fire 시 push
  - `syncOnBoot()` — enabled 시 현재 소스 상태 참조하여 초기 fire 1회
  - `toJson() const` — 응답 JSON 생성
- **WebServer 라우트 신규**: `GET /api/attendance/history` → `attendanceHandler.toJson()`
- **/api/status 확장**: `buildStatusJson()`에 `attendance: {enabled, source, current}` 미니 블록 추가
- **broadcastStatus 강화**: main.cpp `switchMonitor.setOnChange` 콜백에 `webServer.ws().broadcastStatus(buildStatusJson().c_str())` 1줄 추가
- **부팅 sync**: main.cpp의 GOT_IP+5s stabilization one-shot에 `attendanceHandler.syncOnBoot()` 추가
- **홈 Web UI 재구성**:
  - 조건부 `#attendance-card` 홈 최상단 (attendance.enabled 시 표시)
  - `#pc-status` 카드 `상태 모니터`로 개명 + 순서 재정렬 + PC LED dot
  - CSS 통일 (폰트/뱃지/아이콘)

### 1.2 Selected Architecture — Option C

- **AttendanceHandler 확장이 자연스러움**: v2.6.1에서 stateless dispatcher로 시작했으나 링버퍼는 handler와 결합도가 높아 별도 클래스 분리 이득 적음.
- **broadcast는 main.cpp 1줄**: helper 분리 필요 없음.
- **Web UI는 S2 별도 세션**: firmware와 spiffs를 나눠 배포 편의 확보.

### 1.3 Design Principles

- v2.4.7 방어선 무결성 (NetManager diff=0)
- 기존 자산 재사용 (fire pipeline, WebSocket broadcast, buildStatusJson)
- 하위호환 우선 (/api/status 필드 추가만, 기존 파서 무영향)
- 조건부 UI (attendance.enabled=false 시 홈 원본 유지)

---

## 2. System Architecture

### 2.1 Component Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                    RemoteDeck_PC v2.6.2                             │
│                                                                     │
│  main.cpp                                                           │
│  ├─ pcMonitor.setOnChange → fire + attendanceHandler.onSource + ws.broadcastStatus [existing]
│  ├─ switchMonitor.setOnChange → fire + attendance + ws.broadcastStatus ◄ 신규 broadcast
│  ├─ setup loop (GOT_IP+5s one-shot):                                │
│  │    syncCurrentStates(readers)  [existing]                        │
│  │    + attendanceHandler.syncOnBoot()  ◄ 신규                       │
│  └─ NetManager [불변]                                                │
│                                                                     │
│  AttendanceHandler (v2.6.1 base + 확장)                              │
│  ├─ begin(cfg*, wr*) [existing]                                     │
│  ├─ onSourceStateChange(sourceKey, active)                          │
│  │    → fire + push(Entry{ts,active,httpCode}) ◄ 확장                │
│  ├─ syncOnBoot()  ◄ 신규                                             │
│  │    → enabled + source에 따라 fire 1회                              │
│  ├─ toJson() const  ◄ 신규                                           │
│  ├─ currentStateText() const → "present" | "absent"  ◄ 신규          │
│  └─ Entry _history[8] + uint8_t _head  ◄ 신규                        │
│                                                                     │
│  WebServer                                                          │
│  ├─ GET /api/status → buildStatusJson()                             │
│  │    + attendance: {enabled, source, current}  ◄ 확장               │
│  └─ GET /api/attendance/history → attendanceHandler.toJson() ◄ 신규 │
│                                                                     │
│  data/www (Web UI)                                                  │
│  ├─ index.html 홈 섹션                                                │
│  │    #attendance-card [신규, 조건부 표시]                            │
│  │    #pc-status → h2 "상태 모니터" + 순서 재정렬 + PC LED dot         │
│  ├─ app.js                                                          │
│  │    updateStatus(data): attendance 필드로 카드 표시/갱신             │
│  │    loadAttendanceHistory(): /api/attendance/history 폴링 5s       │
│  └─ style.css                                                       │
│       .attendance-card, .presence-badge, .tx-badge, .led-dot         │
└────────────────────────────────────────────────────────────────────┘
```

### 2.2 Module Boundaries

| Module | Change |
|---|:-:|
| `AttendanceHandler.h/.cpp` | Extend (+~50 LOC 링버퍼+toJson+syncOnBoot) |
| `WebServer.cpp` | Extend (+1 라우트) |
| `main.cpp` | Modify (+3 lines — broadcast 1줄, syncOnBoot 1줄, buildStatusJson attendance 4줄) |
| `data/www/index.html` | Modify (홈 재구성 ~35 LOC) |
| `data/www/app.js` | Modify (~60 LOC: attendance 카드 렌더 + polling + 상태 모니터 렌더 순서) |
| `data/www/style.css` | Modify (~40 LOC: 카드/뱃지/dot 스타일) |
| `ConfigManager.cpp` | Modify (버전 스탬프 2.6.1→2.6.2) |
| **NetManager.h/cpp** | **No change (SC-10)** |
| **DeviceConfig / SwitchMonitor / PCMonitor** | **No change** |

---

## 3. Data Model

### 3.1 AttendanceHandler 확장

```cpp
class AttendanceHandler {
public:
    void begin(const AttendanceConfig* cfg, WebRequestHandler* wr);
    void onSourceStateChange(const char* sourceKey, bool active);

    // v2.6.2 신규
    void syncOnBoot();                          // 초기 상태 fire 1회
    String toJson() const;                       // /api/attendance/history 응답
    const char* currentStateText() const;        // "present" | "absent" | "unknown"

    // v2.6.2 부팅 sync에 사용할 소스 상태 게터 연결
    using PcledGetter = std::function<bool()>;
    using GpioGetter  = std::function<bool()>;   // GPIO2 active
    void setStateGetters(PcledGetter p, GpioGetter g);

private:
    struct Entry {
        uint32_t ts;        // millis() at fire time (필드에서 NTP 시각 별도로 시간 문자열 저장하는 방식도 대안)
        String   timeStr;   // NTP 시각 문자열 (있으면)
        bool     active;
        int16_t  httpCode;  // -1 = pending/failed, 200 = ok
    };
    Entry _history[8];
    uint8_t _head = 0;      // 다음 write 위치
    uint8_t _count = 0;     // 최대 8

    const AttendanceConfig* _cfg = nullptr;
    WebRequestHandler*      _wr  = nullptr;
    PcledGetter _getPcled = nullptr;
    GpioGetter  _getGpio  = nullptr;

    void _push(bool active, int16_t code);
    bool _sourceActive() const;                  // enabled + source에 따라 현재 상태 반환
};
```

### 3.2 currentStateText 매핑

| enabled | source active | 반환 |
|:-:|:-:|---|
| false | — | `"unknown"` (또는 그대로 미노출) |
| true | true | `"present"` |
| true | false | `"absent"` |

### 3.3 toJson 스키마 예시

```json
{
  "enabled": true,
  "source": "pcled",
  "current": "present",
  "history": [
    { "ts": 1735000000, "time": "14:10:24", "active": true,  "http": 200 },
    { "ts": 1734999900, "time": "14:08:44", "active": false, "http": 200 }
  ]
}
```

시간 문자열은 Logger가 사용하는 NTP 문자열 게터를 참조하거나, 없으면 millis 표기. Design 단계 결정: NTP 있으면 우선 표시, 없으면 `ts`(millis)만.

### 3.4 /api/status attendance 미니 블록

```json
{
  "pc_on": true,
  "relay1": false,
  "relay2": false,
  "gpio": [1,0,1],
  ...
  "attendance": {
    "enabled": true,
    "source": "pcled",
    "current": "present"
  }
}
```

- `enabled=false`면 여전히 필드 노출(감시 클라이언트가 조건부 표시 판단할 때 필요)
- **하위호환**: 기존 IntegrateController 파서는 unknown 필드 무시 → 회귀 없음

---

## 4. API Contract

### 4.1 기존 엔드포인트 확장

| Endpoint | v2.6.1 | v2.6.2 |
|---|---|---|
| `GET /api/status` | 상태 스냅숏 | + `attendance: {enabled, source, current}` |
| `GET /api/config` | 무변경 | 무변경 |
| `POST /api/config` | attendance 블록 파싱 | 무변경 |

### 4.2 신규 엔드포인트

| Method | Path | Response |
|:---:|---|---|
| GET | `/api/attendance/history` | `{enabled, source, current, history:[{ts,time,active,http}]}` (최대 8건) |

- 인증: 기존 `/api/status`와 동일 (`requireAuth`)

### 4.3 WebSocket 이벤트

- 기존 `broadcastStatus` (JSON status 스냅숏) 재사용
- `switchMonitor.onChange` 시점에도 발화됨 → 클라이언트 status 자동 갱신
- attendance 상태 갱신도 status 안에 포함되므로 별도 WS 이벤트 불필요

---

## 5. Detailed Module Design

### 5.1 AttendanceHandler 확장 로직

**onSourceStateChange 확장**:
```cpp
void AttendanceHandler::onSourceStateChange(const char* sourceKey, bool active) {
    if (!_cfg || !_wr) return;
    if (!_cfg->enabled) return;
    if (strcmp(_cfg->source.c_str(), sourceKey) != 0) return;

    const char* event = active ? "attendance_on" : "attendance_off";
    _wr->fire(event, active ? 1 : 0);
    Serial.printf("Attendance: source=%s active=%d -> %s\n", sourceKey, active ? 1 : 0, event);

    // v2.6.2: 링버퍼 push (httpCode는 초기 -1, 후속 WEBREQ 로그로 업데이트 어려우면 그냥 -1 유지)
    _push(active, -1);
}
```

**_push**:
```cpp
void AttendanceHandler::_push(bool active, int16_t code) {
    Entry& e = _history[_head];
    e.ts       = millis();
    // NTP 시각 문자열은 setStateGetters와 별도로 getter 주입 or Logger 참조.
    // 단순화: main.cpp에서 ntpSync.getTimeString()을 static 함수로 노출 or fwd
    e.active   = active;
    e.httpCode = code;
    _head = (_head + 1) % 8;
    if (_count < 8) _count++;
}
```

**syncOnBoot**:
```cpp
void AttendanceHandler::syncOnBoot() {
    if (!_cfg || !_wr || !_cfg->enabled) return;
    bool active = _sourceActive();
    const char* event = active ? "attendance_on" : "attendance_off";
    _wr->fire(event, active ? 1 : 0);
    Serial.printf("Attendance boot sync: source=%s active=%d\n", _cfg->source.c_str(), active);
    _push(active, -1);
}
```

**_sourceActive**:
```cpp
bool AttendanceHandler::_sourceActive() const {
    if (_cfg->source == "pcled" && _getPcled) return _getPcled();
    if (_cfg->source == "gpio2" && _getGpio)  return _getGpio();
    return false;
}
```

**currentStateText**:
```cpp
const char* AttendanceHandler::currentStateText() const {
    if (!_cfg || !_cfg->enabled) return "unknown";
    return _sourceActive() ? "present" : "absent";
}
```

**toJson**:
```cpp
String AttendanceHandler::toJson() const {
    DynamicJsonDocument doc(1024);
    doc["enabled"] = _cfg ? _cfg->enabled : false;
    doc["source"]  = _cfg ? _cfg->source.c_str() : "";
    doc["current"] = currentStateText();
    JsonArray arr = doc.createNestedArray("history");
    // _head 기준으로 최신순으로 순회
    for (uint8_t i = 0; i < _count; i++) {
        uint8_t idx = (_head + 8 - 1 - i) % 8;   // newest → oldest
        JsonObject o = arr.createNestedObject();
        o["ts"]     = _history[idx].ts;
        o["active"] = _history[idx].active;
        o["http"]   = _history[idx].httpCode;
    }
    String out; serializeJson(doc, out); return out;
}
```

**참고**: `Entry.timeStr`은 초기 설계에는 있었으나 String 8개 저장 시 heap fragmentation 우려 → millis만 저장하고 클라이언트에서 NTP time 별도 표시 가능. Design 최종 결정: `timeStr` 필드 제외, millis(`ts`)만 저장 후 클라이언트에서 현재 NTP 시각 - millis(now)-ts 로 계산.

### 5.2 WebServer 라우트 신규

```cpp
// WebServer.cpp - 기존 라우트 근처
_server->on("/api/attendance/history", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    if (_getAttendance) req->send(200, "application/json", _getAttendance());
    else req->send(200, "application/json", "{\"enabled\":false,\"history\":[]}");
});
```

**Setter API (WebServer.h)**:
```cpp
using AttendanceGetter = std::function<String()>;
void setAttendanceGetter(AttendanceGetter cb) { _getAttendance = cb; }
```

### 5.3 main.cpp wiring

**buildStatusJson 확장** (line ~300 부근):
```cpp
// 기존 필드 후:
JsonObject att = doc.createNestedObject("attendance");
att["enabled"] = config.attendance.enabled;
att["source"]  = config.attendance.source;
att["current"] = attendanceHandler.currentStateText();
```

**setup wiring**:
```cpp
// AttendanceHandler begin 후:
attendanceHandler.setStateGetters(
    []() { return pcMonitor.isPCOn(); },
    []() { return switchMonitor.isActive(); }
);

// WebServer setter:
webServer.setAttendanceGetter([]() { return attendanceHandler.toJson(); });
```

**switchMonitor 콜백 확장** (broadcast 추가):
```cpp
switchMonitor.setOnChange([](bool active) {
    webRequestHandler.fire(active ? "gpio2_low" : "gpio2_high", active ? 1 : 0);
    attendanceHandler.onSourceStateChange("gpio2", active);
    webServer.ws().broadcastStatus(buildStatusJson().c_str());   // 신규
});
```

**loop() 부팅 sync**:
```cpp
if (!_bootSyncedOnce && networkManager.isConnected()) {
    if (_bootReadyAt == 0) _bootReadyAt = millis();
    if (millis() - _bootReadyAt >= 5000) {
        WebRequestHandler::StateReaders readers = { /* 기존 */ };
        webRequestHandler.syncCurrentStates(readers);
        attendanceHandler.syncOnBoot();           // 신규
        Serial.println("BootSync: syncCurrentStates() + attendanceSync() completed");
        _bootSyncedOnce = true;
    }
}
```

### 5.4 Web UI 재구성

**index.html 홈 섹션** (기존 `<section id="home">` 내부):

```html
<section id="home" class="page active">
  <!-- v2.6.2: attendance 카드 (조건부 표시, JS에서 style.display 제어) -->
  <div class="card attendance-card" id="attendance-card" style="display:none">
    <div class="att-header">
      <h2>🚪 재부재 시스템</h2>
      <div class="att-source"><span id="att-source-badge">--</span></div>
    </div>
    <div class="presence-badge" id="att-presence">
      <span class="dot" id="att-dot"></span>
      <span class="text" id="att-text">--</span>
    </div>
    <h3>최근 전송 내역</h3>
    <div class="att-history" id="att-history">
      <p style="color:#666">아직 기록이 없습니다</p>
    </div>
  </div>

  <div class="card-row">
    <div class="card">
      <h2>상태 모니터</h2>   <!-- v2.6.2: PC 상태 → 상태 모니터 -->
      <div class="status-line">릴레이1: <span id="r1">--</span></div>
      <div class="status-line">릴레이2: <span id="r2">--</span></div>
      <div class="status-line pc-led-line">
        <span class="lbl">PC LED :</span>
        <span class="pc-led-value" id="pc-text">--</span>
        <span class="led-dot" id="pc-dot"></span>
      </div>
      <div class="status-line">GPIO: <span id="gpio-val">--</span></div>
    </div>
    <div class="card">
      <h2>시스템 정보</h2>
      <!-- 기존 시스템 정보 필드 유지 -->
    </div>
  </div>
</section>
```

**app.js** — 요지:
```javascript
function updateStatus(data) {
  // ... 기존 릴레이/GPIO/PC 갱신
  // v2.6.2: PC LED dot 색상
  const pcOn = !!data.pc_on;
  document.getElementById('pc-text').textContent = pcOn ? 'ON' : 'OFF';
  document.getElementById('pc-dot').className = pcOn ? 'led-dot on' : 'led-dot off';

  // v2.6.2: attendance 카드 표시/갱신
  const att = data.attendance;
  const card = document.getElementById('attendance-card');
  if (att && att.enabled) {
    card.style.display = '';
    document.getElementById('att-source-badge').textContent =
      att.source === 'pcled' ? 'PC LED (PIR)' : 'SwitchMonitor (GPIO2)';
    const present = att.current === 'present';
    document.getElementById('att-text').textContent = present ? '재실' : '부재';
    document.getElementById('att-dot').className = present ? 'dot present' : 'dot absent';
  } else {
    card.style.display = 'none';
  }
}

function loadAttendanceHistory() {
  fetch('/api/attendance/history').then(r => r.json()).then(d => {
    const box = document.getElementById('att-history');
    if (!d.history || d.history.length === 0) {
      box.innerHTML = '<p style="color:#666">아직 기록이 없습니다</p>';
      return;
    }
    box.innerHTML = d.history.map(e => `
      <div class="att-row">
        <span class="time">${formatTs(e.ts)}</span>
        <span class="state ${e.active ? 'present' : 'absent'}">${e.active ? '재실' : '부재'}</span>
        <span class="tx ${e.http === 200 ? 'ok' : 'fail'}">${e.http === 200 ? '✅ 200' : `❌ ${e.http}`}</span>
      </div>
    `).join('');
  });
}
setInterval(loadAttendanceHistory, 5000);   // 5s 폴링
document.querySelector('[data-tab="home"]').addEventListener('click', loadAttendanceHistory);
```

**style.css** — 신규 클래스:
```css
.attendance-card { margin-bottom: 12px; padding: 20px; }
.attendance-card .att-header { display:flex; justify-content:space-between; align-items:center; }
.attendance-card .att-source { color:#aaa; font-size:13px; }
.presence-badge { display:flex; align-items:center; gap:12px; padding:14px 0; }
.presence-badge .dot { width:18px; height:18px; border-radius:50%; }
.presence-badge .dot.present { background:#f04a4a; box-shadow: 0 0 8px #f04a4a88; }
.presence-badge .dot.absent  { background:#4ac86e; box-shadow: 0 0 8px #4ac86e88; }
.presence-badge .text { font-size:22px; font-weight:bold; }
.att-history .att-row { display:grid; grid-template-columns: 90px 60px 1fr; gap:8px; padding:4px 0; border-bottom:1px solid #222; font-size:13px; }
.att-history .state.present { color:#f04a4a; font-weight:bold; }
.att-history .state.absent  { color:#4ac86e; }
.att-history .tx.ok   { color:#4ac86e; }
.att-history .tx.fail { color:#f04a4a; }

/* PC LED dot */
.pc-led-line { display:flex; align-items:center; gap:6px; }
.pc-led-line .lbl { font-weight:bold; }
.pc-led-line .pc-led-value { font-size:15px; font-weight:bold; }
.led-dot { width:12px; height:12px; border-radius:50%; display:inline-block; }
.led-dot.on  { background:#4ac86e; }
.led-dot.off { background:#f04a4a; }
```

**색상 규칙**:
- 재실(present) = 빨강(활성/주의)
- 부재(absent) = 초록(비활성)
- PC LED ON = 초록 (활성)
- PC LED OFF = 빨강 (비활성)
- 첨부 이미지에 PC LED OFF가 빨강 dot으로 표시되어 있음 준수

---

## 6. Sequence Diagrams

### 6.1 부팅 시 attendance sync

```
Boot   NetManager   main.cpp     WebRequestHandler   AttendanceHandler       Server
  │        │           │                │                    │                  │
  │        ├─ GOT_IP ─▶│                │                    │                  │
  │        │(5s)      │  bootReady      │                    │                  │
  │        │           ├─ syncCurrentStates(readers)          │                  │
  │        │           │   ├─ fire(gpio2_high)  ─────────────────────────────── ▶│
  │        │           │   └─ fire(pcled_on) ────────────────────────────────── ▶│
  │        │           ├─ attendanceHandler.syncOnBoot()      │                  │
  │        │           │                                       ├─ _sourceActive()│
  │        │           │                                       ├─ fire(attendance_on) ▶│
  │        │           │                                       └─ _push(true, -1)│
```

### 6.2 상태 변화 (GPIO2 → 홈 UI 실시간)

```
Field    SwitchMonitor      main.cpp lambda      WebRequestHandler   AttendanceHandler   WebSocket
  │            │                   │                       │                 │                │
  ├─ 상태변화 ─▶│ debounce           │                       │                 │                │
  │            ├─ onChange(true) ──▶│                       │                 │                │
  │            │                   ├─ fire("gpio2_low") ───▶│                 │                │
  │            │                   ├─ attendanceHandler.onSourceStateChange("gpio2", true)     │
  │            │                   │                       │                 ├─ fire(attendance_on) ▶ HTTP
  │            │                   │                       │                 └─ _push(true, -1)│
  │            │                   ├─ ws.broadcastStatus(buildStatusJson()) ──────────────────▶│
  │            │                   │                       │                 │                ├─ 모든 클라이언트로 status 전달
  │                                                                                          │
Browser  ◀── WebSocket status message                                                       │
  ├─ updateStatus(data) → attendance 카드 갱신, GPIO 실시간 반영
```

### 6.3 브라우저 5초 폴링

```
Browser                    Server
  │                          │
  ├─ 5s Timer ──▶ GET /api/attendance/history ──▶│
  │                          ├─ attendanceHandler.toJson()
  │◀── {enabled, source, current, history:[...]} ──
  ├─ 카드 하단 전송 내역 리렌더
```

---

## 7. State Machine

**AttendanceHandler는 여전히 stateless dispatcher** (v2.6.1 특성 유지). 링버퍼는 순수 데이터 저장소로 상태 아님. 부팅 시 syncOnBoot 1회 실행 후 이후는 소스 콜백에 의존.

---

## 8. Test Plan

### 8.1 Unit Tests (L1)

- [ ] AttendanceHandler `_push` — 링버퍼 wrap-around 8회 초과 시 head 순환
- [ ] `toJson` — history newest-first 순서, 8건 초과 저장 시 최근 8건만 반환
- [ ] `syncOnBoot` — enabled=false 시 fire 없음
- [ ] `syncOnBoot` — enabled=true + source=pcled + getter=true → fire("attendance_on")
- [ ] `currentStateText` — enabled/source에 따른 present/absent/unknown 반환
- [ ] `setStateGetters` — null이면 `_sourceActive` false 반환

### 8.2 Integration Tests (L2)

- [ ] `GET /api/attendance/history` 응답 shape 검증 (enabled/source/current/history)
- [ ] `GET /api/status` 응답에 attendance 필드 포함 확인
- [ ] switchMonitor 상태 변화 시 WebSocket status broadcast 발생 확인
- [ ] 부팅 후 15초 이내 attendance URL 호출 로그 확인 (외부 서버)

### 8.3 E2E (L3) — 필드 검증

- [ ] SC-1: attendance.enabled=true로 홈 접속 → 재부재 카드 표시
- [ ] SC-2: enabled=false → 카드 미표시, 나머지 홈 정상
- [ ] SC-3: 상태 변화 → 카드 배지 3초 이내 갱신
- [ ] SC-4: 이벤트 발생 → 카드 하단 내역에 5초 이내 추가
- [ ] SC-5: 상태 모니터 이름/순서/PC LED dot (첨부 이미지 준수)
- [ ] SC-6: GPIO2 상태 변화 → 웹UI 2초 이내 반영
- [ ] SC-7: 부팅 후 15초 이내 attendance URL 도달
- [ ] SC-8/9: /api/status attendance 필드, /api/attendance/history 8건

### 8.4 Regression

- [ ] `/api/status` 기존 필드 shape 유지 → IntegrateController 파서 정상
- [ ] 홈 화면 기존 카드(시스템 정보 등) 표시 정상
- [ ] pcled_on/off, gpio2_low/high 개별 fire 정상 (v2.6.1과 동일)
- [ ] NetManager.h/.cpp diff = 0

---

## 9. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Entry에 String 저장 시 heap fragmentation | timeStr 필드 제외, millis(ts)만 저장. 클라이언트에서 상대 시간 표시. |
| 다중 클라이언트 동시 broadcast 부하 | 기존 PCMonitor onChange도 broadcast 하고 있음. switchMonitor 추가는 유사 수준. |
| 링버퍼 wrap-around 로직 버그 | Unit test로 8회 초과 검증. `(_head + 8 - 1 - i) % 8` newest-first 인덱스 계산 명시. |
| /api/status attendance 필드 추가로 응답 크기 증가 | 3필드 ~50B. 무시할 수준. |
| 부팅 syncOnBoot 이중 fire (개별 채널 + attendance) | 사용자 확정 정책. 서버가 중복 처리. |
| 홈 UI 재구성으로 기존 DOM id 회귀 | 기존 id(pc-status, r1, r2, gpio-val, pc-text, pc-dot 등) 유지. h2/CSS/순서만 변경. |
| v2.4.7 방어선 훼손 | SC-10 (NetManager diff=0) |

---

## 10. Non-Functional Verification

| NFR | Method |
|-----|--------|
| Flash ≤ 8KB 증가 | `pio run` Program 크기 diff |
| Heap ≥ 100KB | `ESP.getFreeHeap()` |
| GPIO2 실시간 ≤ 2s | 웹 UI 시각 관찰 + Serial timestamp |
| 부팅 후 15s | Serial "Attendance boot sync" 확인 |
| /api/status 하위호환 | IntegrateController 정상 파싱 |
| NetManager diff=0 | `git diff` |

---

## 11. Implementation Guide

### 11.1 File Change Summary

| File | Change | LOC (est.) |
|------|:------:|:----------:|
| `src/control/AttendanceHandler.h` | Modify | +30 |
| `src/control/AttendanceHandler.cpp` | Modify | +55 |
| `src/web/WebServer.h` | Modify | +3 |
| `src/web/WebServer.cpp` | Modify | +6 |
| `src/main.cpp` | Modify | +15 (getter set + broadcast + syncOnBoot + attendance getter + status extend) |
| `src/config/ConfigManager.cpp` | Modify | +2 (버전 2.6.1→2.6.2) |
| `data/www/index.html` | Modify | +35 (홈 재구성) |
| `data/www/app.js` | Modify | +60 (attendance card + history poll + status render) |
| `data/www/style.css` | Modify | +40 (카드/뱃지/dot 스타일) |
| **NetManager.h/cpp** | **No change** | 0 |

**총계**: ~246 LOC. Design 예상 ~220 LOC와 부합.

### 11.2 Implementation Order

**Session 1 (Firmware, ~1h)**:
1. AttendanceHandler.h/.cpp 확장 (Entry/링버퍼/toJson/syncOnBoot/StateGetters/currentStateText)
2. WebServer.h/.cpp에 setAttendanceGetter + /api/attendance/history 라우트
3. main.cpp: setStateGetters, setAttendanceGetter, buildStatusJson attendance 추가, switchMonitor broadcast, syncOnBoot 부팅 호출
4. ConfigManager 버전 스탬프 2.6.2
5. `pio run` — Flash 크기 검증
6. curl로 /api/attendance/history, /api/status attendance 필드 확인

**Session 2 (Web UI, ~1h)**:
1. index.html 홈 재구성 (attendance card + 상태 모니터 이름/순서/dot)
2. app.js: updateStatus attendance 처리, loadAttendanceHistory + 5s polling, PC LED dot 색상, tab 리스너
3. style.css: attendance-card, presence-badge, att-history, led-dot 스타일
4. `pio run -t buildfs` — spiffs bin 생성
5. bin 배치

**Session 3 (필드 dogfood)**:
1. 대상 기기 OTA (firmware bin → spiffs bin)
2. 홈 진입 → 카드 표시 확인
3. 상태 변화·재부팅 검증

### 11.3 Session Guide

**Module Map**:
| Module Key | Files | 시간 |
|---|---|---|
| `att-history` | AttendanceHandler.{h,cpp} | 30분 |
| `api-history` | WebServer.{h,cpp} + main.cpp getter | 15분 |
| `broadcast-boot` | main.cpp (switchMonitor + syncOnBoot + status 필드) | 15분 |
| `home-ui` | index.html + app.js + style.css | 45분 |
| `build-verify` | pio run + buildfs + curl + 브라우저 | 20분 |

**Recommended Split**:
- Session 1: `att-history` + `api-history` + `broadcast-boot` + 빌드
- Session 2: `home-ui` + buildfs + 시각 확인
- Session 3: 필드 dogfood

---

## 12. Open Items (Resolution before Do phase)

| Item | Decision |
|------|----------|
| Entry에 timeStr 저장? | **미저장** (millis만) — heap fragmentation 회피, 클라이언트 표시로 처리 |
| /api/attendance/history 응답 device_id 포함? | **미포함** — 클라이언트가 /api/status에서 이미 device_id 획득. 응답 크기 최소화. |
| 재부재 카드 갱신 방식 (폴링 vs WebSocket 전용) | **혼합**: WebSocket status broadcast로 current 실시간 반영, history는 5s 폴링 (단순, 부하 적음). |
| 부팅 syncOnBoot vs syncCurrentStates 통합 | **분리 호출** — 개별 채널은 syncCurrentStates가 처리, attendance는 syncOnBoot로 처리 (사용자 확정 이중 발화 정책) |

---

**Next Step**: `/pdca do RemoteDeck_PC_v2.6.2`
