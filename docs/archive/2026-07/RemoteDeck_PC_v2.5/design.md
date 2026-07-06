---
template: design
version: 1.3
feature: RemoteDeck_PC_v2.5
date: 2026-07-03
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.4.7 → v2.5.0
architecture: Option C - Pragmatic Balance
---

# RemoteDeck_PC v2.5 Design Document

> **Architecture**: Option C - Pragmatic Balance
> **Baseline**: v2.4.7 (2026-07-01, minimal defensive)
> **Target**: v2.5.0 (관리 탭 재편성 + IntegrateController 로그 뷰 + 부팅 sync)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.4.7 콜드 부팅 마감 후 필드 확인된 3가지 편의성·통합성 gap 해소 |
| **WHO** | RemoteDeck_PC 필드 운영자, IntegrateController 통합 감시자, 외부 자동화 시스템(홈어시스턴트/Node-RED) |
| **RISK** | schedule.json 하위호환 파싱, `/api/log` 폴링 부하, sync 실패의 부팅 격리, 상태 mismatch, v2.4.7 방어선 훼손 |
| **SUCCESS** | 관리 탭 3기능 완결 · 자동 재부팅 ±1분 · 로그 latency ≤5s · sync ≤15s · 하위호환 · v2.4.7 방어선 diff 0 |
| **SCOPE** | Phase1 FR1 UI+ScheduleManager → Phase2 FR3 부팅 sync → Phase3 FR2 IC 로그 뷰 → Phase4 통합 검증·v2.5.0 |

---

## 1. Overview

### 1.1 Architecture Summary

Option C (Pragmatic Balance)를 채택한다. 기존 모듈 확장 + 최소 신규 경계 원칙:

- **FR1**: `ScheduleManager`의 `Schedule.action` 문자열에 `"reboot"` 추가, callback 분기에서 reboot dispatch. Web UI는 기존 `sub-tabs` 대신 카드 스택으로 단순화.
- **FR2**: IntegrateController에 `Services/LogPoller.cs` + `UI/LogViewControl.cs` 신규 추가. 기존 `DevicePoller`와 독립 실행(다른 주기·다른 관심사).
- **FR3**: `WebRequestHandler::syncCurrentStates()` 신규 메서드. GPIO/PCLED 상태 getter를 callback DI로 받아 부팅 완료 후 1회 `fire()`. 기존 이벤트 이름 규칙(`gpio1_high` 등) 재사용.

### 1.2 Selected Architecture

**Option C - Pragmatic Balance**

핵심 원칙:
- 기존 자산 재사용 극대: ScheduleManager, WebRequestHandler.fire, /api/log, DevicePoller
- 신규 경계는 관심사 분리가 명확한 곳만 (LogPoller ≠ DevicePoller)
- callback DI 로 헤더 의존성 최소화 (main.cpp만 wiring)

### 1.3 Design Principles

- **v2.4.7 방어선 무결성**: NetManager.h/cpp diff = 0
- **하위호환**: 기존 `/schedule.json` 파일 무손실 로드
- **비동기 격리**: sync 호출 실패해도 부팅·loop() 정상 진행
- **명명 규칙 준수**: 이벤트 이름은 기존 규칙(`{channel}_{state}`) 그대로

---

## 2. System Architecture

### 2.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    RemoteDeck_PC (v2.5.0)                        │
│                                                                  │
│  main.cpp                                                        │
│  ├─ NetManager (v2.4.7 원형 유지)                                │
│  ├─ ConfigManager                                                │
│  ├─ RelayController                                              │
│  ├─ PCMonitor                          [GPIO/PCLED state getter] │
│  ├─ ScheduleManager  ◄── action:"reboot" 확장                    │
│  │    └─ onAction(relay, action)                                 │
│  │        ├─ relay=1|2, action=on|off|toggle → RelayController   │
│  │        └─ action=reboot           → ESP.restart()             │
│  ├─ WebRequestHandler ◄── syncCurrentStates() 신규               │
│  │    ├─ fire(event, value)  [existing]                          │
│  │    └─ syncCurrentStates(readers)  [new, one-shot]             │
│  └─ WebServer                                                    │
│       ├─ GET /api/log        [existing]                          │
│       ├─ POST /api/reboot    [existing]                          │
│       ├─ GET/POST /api/schedule (reboot action 포함)              │
│       └─ static /www         [index.html/app.js 재편성]          │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│                    Web UI (data/www/)                            │
│  index.html  ─ 탭: 홈 | 제어 | 스케줄 | 설정 | [관리] | 로그    │
│  app.js      ─ 관리 탭 카드 스택 (기기 관리 / 펌웨어 업데이트)     │
│  style.css   ─ .admin-card, .reboot-schedule-row 스타일 추가     │
└─────────────────────────────────────────────────────────────────┘

               HTTP GET /api/log  (5s poll, stagger 300ms/device)
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              IntegrateController (C# WinForms)                   │
│                                                                  │
│  MainForm.cs                                                     │
│  ├─ DevicePoller  [existing, status 3s poll]                     │
│  ├─ LogPoller     [new, log 5s poll staggered]                   │
│  └─ LogViewControl (UserControl) [new]                           │
│       ├─ 기기 선택 드롭다운                                        │
│       └─ 로그 그리드 (시간 역순, 최근 100건)                       │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Module Boundaries

| Module | Responsibility | Change Type |
|--------|----------------|-------------|
| `ScheduleManager` | 스케줄 저장·시각 매칭·action dispatch | Extend (action="reboot" + rebootCallback) |
| `WebRequestHandler` | URL fire + 부팅 sync | Extend (syncCurrentStates + StateReaders) |
| `WebServer` | HTTP 엔드포인트 | Extend (schedule POST에 reboot action validation) |
| `data/www/index.html` | 웹 UI 마크업 | Modify (탭 이름 + 관리 섹션 재구성) |
| `data/www/app.js` | 웹 UI 로직 | Modify (관리 탭 handler + reboot schedule CRUD) |
| `data/www/style.css` | 스타일 | Extend (admin card 스타일) |
| `main.cpp` | 부트 시퀀스·wiring | Modify (reboot callback wiring + syncCurrentStates one-shot) |
| `NetManager` | v2.4.7 최소 방어선 | **NO CHANGE** (diff 0 목표) |
| `Services/LogPoller.cs` (C#) | 기기 로그 폴링 | New |
| `UI/LogViewControl.cs` (C#) | 로그 뷰 UserControl | New |
| `UI/MainForm.cs` (C#) | 뷰 통합 | Modify (LogViewControl 배치) |

---

## 3. Data Model

### 3.1 Schedule (extended)

```cpp
// ScheduleManager.h - 기존 구조 유지, action 문자열만 확장
struct Schedule {
    uint8_t id;
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days;       // bitmask (변경 없음)
    std::string action; // "on" | "off" | "toggle" | "reboot"  ← 확장
    uint8_t relay;      // 1 or 2 (reboot 시 0 or 미사용)
};
```

**하위호환 규칙**:
- 기존 파일 로드 시 unknown action 나오면 skip (경고 로그만 남김)
- reboot 스케줄의 `relay` 필드는 저장 0, UI에서 안 보임
- `nextId()`는 relay/reboot 모두 통합 카운터

### 3.2 Persisted JSON schema (`/schedule.json`)

```json
{
  "schedules": [
    { "id": 1, "enabled": true, "hour": 9, "minute": 0, "days": 62, "action": "on", "relay": 1 },
    { "id": 2, "enabled": true, "hour": 3, "minute": 30, "days": 127, "action": "reboot", "relay": 0 }
  ]
}
```

### 3.3 LogEntry (C# side)

```csharp
public sealed record LogEntry(
    DateTime Timestamp,       // parsed from ISO8601 string
    string   Category,        // "BOOT" | "RELAY" | "WEBREQ" | "MQTT" | ...
    string   Detail,          // free text
    string   DeviceId         // for multi-device view
);
```

---

## 4. API Contract

### 4.1 Existing endpoints (재사용, 변경 없음)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/log` | 이벤트 로그 배열 반환 (IntegrateController가 폴링) |
| POST | `/api/reboot` | 즉시 재부팅 (v2.4.7 응답 flush 전 restart 동작 유지, [[project_rdpc_reboot_flush]]) |
| GET | `/api/schedule` | 스케줄 배열 반환 (reboot action 포함) |
| POST | `/api/schedule` | 스케줄 추가 (relay + reboot 통합) |
| DELETE | `/api/schedule?id={n}` | 스케줄 삭제 |

### 4.2 Payload extension

**POST /api/schedule** — `action` 필드 값에 `"reboot"` 추가 허용:

```json
{ "enabled": true, "hour": 3, "minute": 30, "days": 127, "action": "reboot", "relay": 0 }
```

**Validation**:
- `action="reboot"` 이면 `relay` 는 무시 (0 강제 저장)
- `action ∈ {on,off,toggle,reboot}` 외의 값은 400 응답
- MAX_SCHEDULES=8 초과 시 409 응답 (기존 동작 유지)

### 4.3 GET /api/log response shape

```json
[
  { "ts": "2026-07-03T04:12:33Z", "cat": "BOOT", "msg": "GOT_IP 192.168.1.42" },
  { "ts": "2026-07-03T04:12:35Z", "cat": "BOOT_SYNC", "msg": "gpio1_high fired" }
]
```

**변경 없음** — 기존 shape를 IntegrateController가 파싱만 함.

---

## 5. Detailed Module Design

### 5.1 ScheduleManager 확장

```cpp
// ScheduleManager.h - 신규 콜백 타입 및 setter 추가
class ScheduleManager {
public:
    // 기존 유지
    using ActionCallback = std::function<void(uint8_t relay, const std::string& action)>;
    void setOnAction(ActionCallback cb) { _onAction = cb; }

    // 신규: reboot action 전용 콜백
    using RebootCallback = std::function<void()>;
    void setOnReboot(RebootCallback cb) { _onReboot = cb; }

private:
    RebootCallback _onReboot = nullptr;
    // ...
};

// ScheduleManager.cpp - loop() 안에서 dispatch 분기
void ScheduleManager::loop() {
    // (기존 시각 계산 로직 유지)
    for (auto& s : _schedules) {
        if (shouldExecute(s, weekday, hour, minute)) {
            if (s.action == "reboot") {
                if (_onReboot) _onReboot();
            } else {
                if (_onAction) _onAction(s.relay, s.action);
            }
        }
    }
}

// fromJson() - 하위호환 로딩
bool ScheduleManager::fromJson(const String& json) {
    // parse; unknown action은 skip + Serial.printf 경고
    // "reboot" 도 valid
}
```

**Rationale**: 별도 콜백 분리로 relay와 reboot 관심사가 명확히 분리. main.cpp 에서 `setOnReboot([]{ ESP.restart(); })` 한 줄로 wiring.

### 5.2 WebRequestHandler::syncCurrentStates 확장

```cpp
// WebRequestHandler.h - 신규 API
class WebRequestHandler {
public:
    // 기존 유지
    void fire(const char* event, int value);

    // 신규: 부팅 후 1회 호출
    // reader 는 채널별 현재 상태 반환 (nullptr 이면 해당 채널 skip)
    struct StateReaders {
        std::function<int()> gpio1;   // 0=LOW, 1=HIGH, -1=unavailable
        std::function<int()> gpio2;
        std::function<int()> gpio3;
        std::function<int()> pcled;   // 0=OFF, 1=ON, -1=unavailable
    };
    void syncCurrentStates(const StateReaders& r);
};

// WebRequestHandler.cpp
void WebRequestHandler::syncCurrentStates(const StateReaders& r) {
    auto tryFire = [&](std::function<int()>& reader, const char* on, const char* off) {
        if (!reader) return;
        int v = reader();
        if (v < 0) return;  // unavailable
        const char* ev = (v == 1) ? on : off;
        // URL 이 비어 있으면 fire() 내부에서 skip 됨
        fire(ev, v);
        if (_onLog) _onLog("BOOT_SYNC", ev);
    };
    tryFire(r.gpio1, "gpio1_high", "gpio1_low");
    tryFire(r.gpio2, "gpio2_high", "gpio2_low");
    tryFire(r.gpio3, "gpio3_high", "gpio3_low");
    tryFire(r.pcled, "pcled_on",   "pcled_off");
}
```

**Rationale**: 기존 `fire()` 파이프라인(queue + worker task + timeout)을 그대로 재사용. URL 미설정 채널은 `getURL()` 이 empty string 반환하므로 자연스레 skip. Best-effort.

### 5.3 main.cpp Boot Sync 시점

```cpp
// main.cpp - 신규 flag
static bool _bootSyncedOnce = false;
static unsigned long _bootReadyAt = 0;

void setup() {
    // (기존 v2.4.7 로직 원형 유지)
    scheduleManager.setOnAction([](uint8_t r, const std::string& a){ /* 기존 */ });
    scheduleManager.setOnReboot([]{
        Serial.println("Schedule: reboot triggered");
        delay(200);
        ESP.restart();
    });
    // syncCurrentStates 대상 등록은 loop()에서 실행
}

void loop() {
    // (기존 로직 유지)

    // 부팅 sync — GOT_IP 후 5초 지나면 1회 실행
    if (!_bootSyncedOnce && netManager.hasIP()) {
        if (_bootReadyAt == 0) _bootReadyAt = millis();
        if (millis() - _bootReadyAt >= 5000) {  // 5s stabilization
            WebRequestHandler::StateReaders readers;
            readers.gpio1 = []{ return relayController.getGpioState(1); };  // TODO: 실제 게터
            readers.gpio2 = []{ return relayController.getGpioState(2); };
            readers.gpio3 = []{ return relayController.getGpioState(3); };
            readers.pcled = []{ return pcMonitor.isPCOn() ? 1 : 0; };
            webRequestHandler.syncCurrentStates(readers);
            _bootSyncedOnce = true;
        }
    }
}
```

**타이밍 결정**: GOT_IP 후 5초 stabilization 대기 → NFR-5 (부팅 후 15초 이내) 달성 여유(GOT_IP typical ≤ 10s).

**GPIO/PCLED getter**: 현재 `RelayController`/`PCMonitor`에 존재하는 read API 재사용. 없다면 Design Do 단계에서 최소 게터 추가 (읽기 전용).

### 5.4 Web UI (index.html + app.js)

**index.html — 관리 탭**:

```html
<nav>
  <button class="tab active" data-tab="home">홈</button>
  <button class="tab" data-tab="control">제어</button>
  <button class="tab" data-tab="schedule">스케줄</button>
  <button class="tab" data-tab="settings">설정</button>
  <button class="tab" data-tab="admin">관리</button>   <!-- 변경: ota → admin -->
  <button class="tab" data-tab="log">로그</button>
</nav>

<section id="admin" class="page">
  <div class="card admin-card">
    <h2>기기 관리</h2>

    <h3>즉시 재부팅</h3>
    <button onclick="confirmReboot()">재부팅</button>

    <h3>재부팅 스케줄</h3>
    <div id="reboot-sched-list"></div>
    <div class="form-row">
      <label>시간: <input type="time" id="rb-time" value="03:00"></label>
    </div>
    <div class="form-row">
      <label><input type="checkbox" id="rb-d-mon" checked> 월</label>
      <label><input type="checkbox" id="rb-d-tue" checked> 화</label>
      <label><input type="checkbox" id="rb-d-wed" checked> 수</label>
      <label><input type="checkbox" id="rb-d-thu" checked> 목</label>
      <label><input type="checkbox" id="rb-d-fri" checked> 금</label>
      <label><input type="checkbox" id="rb-d-sat"> 토</label>
      <label><input type="checkbox" id="rb-d-sun"> 일</label>
    </div>
    <button onclick="addRebootSchedule()">추가</button>
  </div>

  <div class="card admin-card">
    <h2>펌웨어 업데이트</h2>
    <p>현재 버전: <span id="ota-ver">--</span></p>
    <input type="file" id="ota-file" accept=".bin">
    <button onclick="uploadOTA()">업로드</button>
    <div class="progress-bar"><div id="ota-progress" class="progress-fill"></div></div>
    <span id="ota-pct">0%</span>
  </div>
</section>
```

**app.js — 신규 함수**:

```javascript
function confirmReboot() {
  if (confirm('기기를 지금 재부팅합니다. 계속하시겠습니까?')) {
    fetch('/api/reboot', { method: 'POST' })
      .catch(() => {/* v2.4.7 flush 정책: timeout/ConnectionReset 은 성공 처리 */});
  }
}

function addRebootSchedule() {
  const time = document.getElementById('rb-time').value.split(':');
  const days = ['sun','mon','tue','wed','thu','fri','sat'].reduce((mask, d, i) =>
    document.getElementById('rb-d-' + d)?.checked ? mask | (1 << i) : mask, 0);
  fetch('/api/schedule', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      enabled: true,
      hour: +time[0], minute: +time[1],
      days: days,
      action: 'reboot',
      relay: 0
    })
  }).then(loadRebootSchedules);
}

function loadRebootSchedules() {
  fetch('/api/schedule').then(r => r.json()).then(list => {
    const rebootOnly = list.filter(s => s.action === 'reboot');
    // render to #reboot-sched-list
  });
}
document.querySelector('[data-tab="admin"]').addEventListener('click', loadRebootSchedules);
```

**스케줄 탭 표시**: 기존 loadSchedules() 에 `s.action === 'reboot'` 인 항목은 라벨 앞에 `🔁` 아이콘 표시.

### 5.5 IntegrateController LogPoller (C#)

```csharp
// Services/LogPoller.cs
public sealed class LogPoller : IDisposable
{
    private readonly HttpClient _http;
    private readonly ConcurrentDictionary<string, List<LogEntry>> _logs = new();
    private readonly Timer _timer;
    private int _deviceStaggerIdx = 0;

    public event Action<string>? LogsUpdated;  // deviceId 인자
    public int MaxEntriesPerDevice { get; set; } = 500;
    public int PollIntervalMs { get; set; } = 5000;
    public int StaggerMs { get; set; } = 300;

    public LogPoller(HttpClient http)
    {
        _http = http;
        _timer = new Timer(_ => _ = PollNextAsync(), null, 0, StaggerMs);
    }

    private async Task PollNextAsync()
    {
        var device = DeviceStore.GetEnabledDevices();
        if (device.Count == 0) return;
        var idx = Interlocked.Increment(ref _deviceStaggerIdx) % device.Count;
        var d = device[idx];
        try
        {
            var json = await _http.GetStringAsync($"http://{d.IpAddress}/api/log");
            var fresh = JsonSerializer.Deserialize<LogEntry[]>(json) ?? [];
            MergeAndTrim(d.DeviceId, fresh);
            LogsUpdated?.Invoke(d.DeviceId);
        }
        catch { /* best-effort */ }
    }

    public IReadOnlyList<LogEntry> GetLogs(string deviceId) =>
        _logs.TryGetValue(deviceId, out var list) ? list : Array.Empty<LogEntry>();

    private void MergeAndTrim(string deviceId, LogEntry[] fresh)
    {
        var list = _logs.GetOrAdd(deviceId, _ => new List<LogEntry>());
        lock (list)
        {
            var existing = list.Select(e => (e.Timestamp, e.Category, e.Detail)).ToHashSet();
            foreach (var e in fresh)
                if (!existing.Contains((e.Timestamp, e.Category, e.Detail)))
                    list.Add(e);
            list.Sort((a, b) => b.Timestamp.CompareTo(a.Timestamp));
            if (list.Count > MaxEntriesPerDevice)
                list.RemoveRange(MaxEntriesPerDevice, list.Count - MaxEntriesPerDevice);
        }
    }

    public void Dispose() => _timer.Dispose();
}
```

**stagger 로직**: 14대 기기가 있을 때 5000ms 주기로 전체 폴링하려면 stagger 300ms × 14 = 4200ms → 5000ms 주기 안에 all 완료. 초당 부하 균등화.

```csharp
// UI/LogViewControl.cs (WinForms UserControl)
public partial class LogViewControl : UserControl
{
    private readonly LogPoller _poller;
    private readonly DataGridView _grid;
    private readonly ComboBox _deviceSelect;

    public LogViewControl(LogPoller poller)
    {
        _poller = poller;
        _poller.LogsUpdated += OnLogsUpdated;
        // Designer 대신 코드로 배치 ([[project-winforms-designer]] 안정화 3중 방어선)
        InitLayout();
        RefreshDeviceList();
    }

    private void OnLogsUpdated(string deviceId)
    {
        if (_deviceSelect.SelectedValue as string == deviceId)
            BeginInvoke(() => RefreshGrid(deviceId));
    }

    private void RefreshGrid(string deviceId)
    {
        var logs = _poller.GetLogs(deviceId);
        _grid.DataSource = logs.Take(100).ToList();  // 상위 100건만
    }
}
```

---

## 6. Sequence Diagrams

### 6.1 부팅 sync 시퀀스

```
setup()             loop()                   NetManager   WebRequestHandler   External
  │                   │                          │              │                 │
  ├─ init all         │                          │              │                 │
  │                   │                          │              │                 │
  │──── setup end ───▶│                          │              │                 │
  │                   ├─ NetManager.loop()      ─┤              │                 │
  │                   │                    GOT_IP                                 │
  │                   ├─ hasIP()? YES ────────▶  │              │                 │
  │                   ├─ millis()-t >= 5000?     │              │                 │
  │                   │   YES ──────────────────────────▶ syncCurrentStates()     │
  │                   │                          │              ├─ fire(gpio1_high) ▶ HTTP GET
  │                   │                          │              ├─ fire(pcled_on)   ▶ HTTP GET
  │                   │                          │              └─ _onLog("BOOT_SYNC")
  │                   ├─ _bootSyncedOnce=true    │              │                 │
```

### 6.2 재부팅 스케줄 실행 시퀀스

```
ScheduleManager.loop()    Callback           ESP
  │                           │                │
  ├─ time match: 03:00 화     │                │
  ├─ action=="reboot"         │                │
  ├─ _onReboot() ───────────▶ │                │
  │                           ├─ Serial.println
  │                           ├─ delay(200)   │
  │                           ├─ ESP.restart() ▶ (reboot)
```

### 6.3 IntegrateController 로그 폴링

```
LogPoller.Timer(300ms)   RD_PC1     RD_PC2     ...    RD_PCn
      │                    │           │                 │
      ├─ idx=0 → RD_PC1    │           │                 │
      │           GET /api/log ────▶ │                   │
      │           ◀── JSON logs      │                   │
      │  merge, LogsUpdated event    │                   │
      ├─ +300ms idx=1 → RD_PC2       │                   │
      │                              GET /api/log ────▶  │
      │                              ◀── JSON logs      │
      │  ...                                             │
      ├─ +300ms idx=n → RD_PCn                          GET
      ├─ +300ms idx=0 → RD_PC1 (cycle)
```

---

## 7. State Machine

### 7.1 부팅 sync one-shot flag

```
[POWER ON]
    │
    ▼
[SETUP] ─────▶ [WAIT_GOT_IP]
                    │
              GOT_IP=true
                    │
                    ▼
              [STABILIZE_5s]
                    │
             elapsed>=5000ms
                    │
                    ▼
              [SYNC_ONCE] ── syncCurrentStates(readers)
                    │
                    ▼
              [BOOT_DONE] ── _bootSyncedOnce = true
                    │
                    └── (never re-enter until reboot)
```

---

## 8. Test Plan

### 8.1 Unit Tests (L1)

- [ ] `ScheduleManager::fromJson` 로 relay + reboot 혼합 JSON 파싱 성공
- [ ] `ScheduleManager::fromJson` 로 unknown action 포함 시 skip + 나머지 유지
- [ ] `ScheduleManager::loop()` reboot 실행 시 `_onReboot()` 호출 1회
- [ ] `WebRequestHandler::syncCurrentStates` — reader nullptr 인 채널 skip 확인
- [ ] `WebRequestHandler::syncCurrentStates` — reader=-1(unavailable) 채널 skip 확인
- [ ] `WebRequestHandler::syncCurrentStates` — URL 미설정 채널 fire 호출은 되지만 실제 HTTP 없음(getURL 빈 문자열)

### 8.2 Integration Tests (L2)

- [ ] Web UI: 관리 탭 클릭 → 재부팅 스케줄 목록 로드
- [ ] Web UI: 재부팅 스케줄 추가 → GET /api/schedule 에 reboot action 포함 확인
- [ ] Web UI: 스케줄 탭에서 reboot 항목 아이콘(🔁) 표시 확인
- [ ] IntegrateController: 대상 기기 선택 → 로그 뷰 5초 이내 갱신
- [ ] IntegrateController: 14대 폴링 stagger 검증(Wireshark or 서버 액세스 로그)

### 8.3 E2E (L3) — 필드 dogfood

- [ ] SC-2: 재부팅 스케줄 "화 09:00" 등록 → 다음 화 09:00±1분 자동 재부팅
- [ ] SC-5: RD_PC 로그 발생 → IntegrateController 5초 이내 반영
- [ ] SC-6: GPIO1 HIGH 상태로 부팅 → 15초 이내 gpio1_high URL 호출

### 8.4 Regression

- [ ] 기존 릴레이 스케줄(v2.4.7 이하 저장) 로드 무손실
- [ ] `/api/log`, `/api/reboot`, `/api/schedule` 응답 shape 하위호환
- [ ] `POST /api/reboot` 이 v2.4.7 flush 전 restart 동작 유지 ([[project_rdpc_reboot_flush]])
- [ ] NetManager.h/cpp diff = 0

---

## 9. Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Schedule action="reboot" 이 relay 콜백에 잘못 전달됨 | loop() 안에서 명시적 if/else 분기, callback 함수 자체 분리 |
| syncCurrentStates 가 GOT_IP 직후 DNS/route 미완료 시 실패 | 5초 stabilization 대기 + fire() 내부 queue timeout 5초 |
| IntegrateController가 오프라인 기기 폴링으로 UI freeze | Timer 콜백 안 try/catch + best-effort (예외 삼킴), UI 스레드는 BeginInvoke |
| reboot 스케줄 다수 등록으로 매 분마다 재부팅 | UI에서 중복 확인 dialog, MAX_SCHEDULES=8 상한 유지 |
| 스케줄 탭에서 reboot 항목을 릴레이로 오인 편집 | 스케줄 탭 편집 UI에서 action="reboot" 시 relay 필드 숨김·비활성 |

---

## 10. Non-Functional Verification

| NFR | Method |
|-----|--------|
| Flash ≤ 10KB 증가 | `pio run` 결과의 Program 섹션 diff |
| Heap free ≥ 100KB | 부팅 후 `ESP.getFreeHeap()` 로그 확인 |
| 재부팅 스케줄 정확도 ±1분 | ScheduleManager 는 분 단위 스캔이므로 자연 만족 |
| 로그 latency ≤ 5s | LogPoller PollIntervalMs=5000 + 이벤트 발생 timing 검증 |
| Sync ≤ 15s | GOT_IP typical ≤ 10s + 5s stabilization = ≤ 15s |
| v2.4.7 방어선 무결성 | `git diff v2.4.7 -- RemoteDeck_PC/src/network/NetManager.*` 결과 = 0 |

---

## 11. Implementation Guide

### 11.1 File Change Summary

| File | Change | Lines (est.) |
|------|:------:|:------------:|
| `RemoteDeck_PC/src/control/ScheduleManager.h` | Modify | +5 |
| `RemoteDeck_PC/src/control/ScheduleManager.cpp` | Modify | +15 |
| `RemoteDeck_PC/src/network/WebRequestHandler.h` | Modify | +12 |
| `RemoteDeck_PC/src/network/WebRequestHandler.cpp` | Modify | +25 |
| `RemoteDeck_PC/src/web/WebServer.cpp` | Modify | +8 (action validation) |
| `RemoteDeck_PC/src/main.cpp` | Modify | +30 (wiring + sync flag) |
| `RemoteDeck_PC/data/www/index.html` | Modify | +40 (관리 탭) |
| `RemoteDeck_PC/data/www/app.js` | Modify | +60 (관리 handler) |
| `RemoteDeck_PC/data/www/style.css` | Modify | +15 (admin card) |
| `RemoteDeck_PC/src/config/ConfigManager.cpp` | Modify | +2 (버전 2.4.7 → 2.5.0) |
| `APITestUtility_v2/integrate_controller/.../Services/LogPoller.cs` | New | ~100 |
| `APITestUtility_v2/integrate_controller/.../UI/LogViewControl.cs` | New | ~120 |
| `APITestUtility_v2/integrate_controller/.../UI/MainForm.cs` | Modify | +20 (place control) |
| `RemoteDeck_PC/src/network/NetManager.{h,cpp}` | **NO CHANGE** | 0 |

**총계**: 신규 2 파일(C#), 수정 11 파일, 방어선 무변경 2 파일. 예상 diff ≈ +450 LOC.

### 11.2 Implementation Order

1. **ScheduleManager 확장** — action="reboot" 지원 + rebootCallback + JSON 하위호환
2. **WebServer schedule 엔드포인트** — reboot action validation
3. **main.cpp wiring** — setOnReboot + boot sync one-shot flag
4. **WebRequestHandler::syncCurrentStates** — StateReaders 구조 + fire dispatch
5. **Web UI 재편성** — index.html/app.js/style.css (관리 탭 카드 스택)
6. **버전 스탬프** — ConfigManager.cpp v2.5.0
7. **펌웨어 빌드 + 로컬 검증** — L1/L2 통과
8. **IntegrateController LogPoller** — Timer + merge + stagger
9. **LogViewControl UI 통합** — MainForm에 배치
10. **필드 dogfood 1주** — SC-2/5/6 검증

### 11.3 Session Guide

**Module Map**:

| Module Key | Files | 예상 시간 |
|------------|-------|-----------|
| `sched-ext` | ScheduleManager.{h,cpp} + WebServer.cpp | 45분 |
| `boot-sync` | WebRequestHandler.{h,cpp} + main.cpp | 45분 |
| `admin-ui` | data/www/{index.html,app.js,style.css} + ConfigManager.cpp | 60분 |
| `ic-logpoller` | Services/LogPoller.cs + UI/LogViewControl.cs + MainForm.cs | 60분 |
| `verify` | build + OTA + field | 실측 |

**Recommended Session Plan**:

- **Session 1** (RemoteDeck_PC 펌웨어): `--scope sched-ext,boot-sync,admin-ui` — 펌웨어 완결 + OTA bin 생성
- **Session 2** (IntegrateController): `--scope ic-logpoller` — C# 신규 파일 + MainForm 통합 + 단독 빌드/실행 검증
- **Session 3** (검증): `--scope verify` — 필드 dogfood + SC 체크

---

## 12. Open Items (Resolution before Do phase)

| Item | Decision |
|------|----------|
| 재부팅 스케줄 초 단위 지원 | **분 단위 유지** (기존 ScheduleManager 그대로), 초 단위 불필요 |
| 로그 뷰 최대 건수 | **표시 100건, 저장 500건** (MaxEntriesPerDevice=500, DataSource=100) |
| 부팅 sync 실행 시점 | **GOT_IP + 5초 stabilization 후 1회** (NFR-5 15초 만족) |
| GPIO 상태 게터 소재 | Do 단계 초입에서 RelayController/PCMonitor 게터 존재 확인, 없으면 최소 추가 |

---

**Next Step**: `/pdca do RemoteDeck_PC_v2.5 --scope sched-ext,boot-sync,admin-ui` (Session 1)
