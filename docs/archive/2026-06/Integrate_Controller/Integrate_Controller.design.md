---
template: design
version: 1.3
feature: Integrate_Controller
date: 2026-06-30
author: KDI
project: RemoteDeckSystem
status: Draft
---

# Integrate_Controller Design Document

> **Summary**: .NET 8 WinForms 통합 컨트롤러. RemoteDeck_PC 단말 다수를 REST 폴링으로 모니터링·재부팅. Models/Services/UI 3-folder Pragmatic 아키텍처.
>
> **Project**: RemoteDeckSystem
> **Version**: RemoteDeck_PC v2.3.x firmware
> **Author**: KDI
> **Date**: 2026-06-30
> **Status**: Draft
> **Planning Doc**: [Integrate_Controller.plan.md](../../01-plan/features/Integrate_Controller.plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 단말 다수화에 따른 개별 웹UI 운영 부담 제거 |
| **WHO** | 사내 RemoteDeck 운영자 |
| **RISK** | (1) 평문 PW 저장 시 유출 (2) 단말 수↑ 폴링 부하 (3) DPAPI PC 종속 |
| **SUCCESS** | N=10대 동시 폴링 안정, reboot RTT < 3s, 등록 ≤ 3 클릭 |
| **SCOPE** | M1 Models/Store → M2 RestClient/Poller → M3 WinForms UI → M4 reboot/에러 |

---

## 1. Overview

### 1.1 Design Goals

- 단일 화면에서 RemoteDeck_PC 단말 N대 상태 동시 표시 (3s 주기 폴링).
- 단말 자격증명(IP/User/Password) 안전 저장 (Windows DPAPI ProtectedData, 사용자 단위).
- 단말당 독립 폴링 Task — 오프라인 단말이 다른 단말 폴링/UI를 막지 않음.
- .NET 8 self-contained single-file exe — 운영 PC에 런타임 설치 불요.

### 1.2 Design Principles

- **단일 책임 분리**: Models(데이터) / Services(비즈니스/IO) / UI(표시).
- **Async-first**: 모든 네트워크 IO `async Task` + `HttpClient` Singleton.
- **펌웨어 0-변경**: 기존 `/api/status` `/api/reboot` Basic Auth만 사용.
- **로컬-only 보안**: DPAPI CurrentUser scope — 단말 자격증명은 해당 Windows 계정에서만 복호화 가능.

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | Option A: Minimal | Option B: Clean | Option C: Pragmatic |
|----------|:-:|:-:|:-:|
| **Approach** | 단일 MainForm.cs | 4-layer + ViewModel | Models/Services/UI 3-folder |
| **New Files** | ~4 | ~18 | ~10 |
| **Complexity** | Low | High | Medium |
| **Maintainability** | Medium | High | High |
| **Effort** | 1세션 | 4세션 | 2~3세션 |
| **Risk** | UI freeze | 과설계 | 균형 |
| **Recommendation** | hotfix | 대형 시스템 | **선택됨** |

**Selected**: **Option C — Pragmatic** — **Rationale**: Plan §7.3 폴더 구조와 일치, 폴링/저장/UI를 각각 단일책임으로 분리하되 ViewModel·DI 같은 과설계 회피. 사용자 채택.

### 2.1 Component Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│  WinForms MainForm (UI thread)                                   │
│   ├─ DataGridView (devices)   ├─ Toolbar (+/-/↑/↓/Reboot/Edit)   │
│   └─ Status detail panel                                         │
└──────────────────────────────────────────────────────────────────┘
        │ events                          │ commands
        ▼                                 ▼
┌──────────────────────┐         ┌──────────────────────┐
│ DevicePoller         │ ──uses─▶│ RemoteDeckClient     │
│  (PeriodicTimer per  │         │  HttpClient Singleton │
│   device + CTS)      │         │  Basic Auth header    │
└──────────────────────┘         └──────────┬───────────┘
        │ status updates                    │ HTTP
        ▼                                   ▼
┌──────────────────────┐         ┌──────────────────────┐
│ DeviceList (BindingL)│         │ RemoteDeck_PC firmware│
│  in-memory state     │         │  GET /api/status      │
└──────────┬───────────┘         │  POST /api/reboot     │
           │ load/save           │  Basic Auth admin/12345│
           ▼                     └──────────────────────┘
┌──────────────────────┐
│ DeviceStore          │
│  devices.json (DPAPI) │
│  %LOCALAPPDATA%\...   │
└──────────────────────┘
```

### 2.2 Data Flow

```
[Add Device]
  User → DeviceEditDialog → DeviceEntry → DeviceList.Add()
                                            ├─ DeviceStore.Save() (DPAPI encrypt PW)
                                            └─ DevicePoller.Start(device, CTS)
                                                  └─ PeriodicTimer 3s
                                                       └─ RemoteDeckClient.GetStatusAsync()
                                                             ├─ 200 OK → parse DeviceStatus
                                                             │            → MainForm.InvokeRequired? BeginInvoke
                                                             │            → DataGridView row update
                                                             └─ timeout/!=200 → fail counter++
                                                                                 → Offline 판정 (≥3회)

[Reboot]
  User → Toolbar/CtxMenu → confirm dialog → RemoteDeckClient.RebootAsync()
                                              ├─ 200 OK → toast "재부팅 요청 전송"
                                              └─ error → toast "실패: {msg}"

[Reorder]
  User → ↑/↓ → DeviceList.Move(idx, newIdx) → DataGridView refresh → DeviceStore.Save()
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| MainForm | DeviceList, DevicePoller, RemoteDeckClient, DeviceStore | UI 조립 |
| DevicePoller | RemoteDeckClient, DeviceList | 단말당 폴링 |
| RemoteDeckClient | `System.Net.Http.HttpClient` (Singleton), `System.Text.Json` | REST 호출 |
| DeviceStore | `System.Security.Cryptography.ProtectedData` | DPAPI 저장 |
| DeviceList | `BindingList<DeviceEntry>` | DataGridView 바인딩 |

외부 NuGet: **없음** (System.Security.Cryptography.ProtectedData 는 .NET 8 — `System.Security.Cryptography.ProtectedData` NuGet 별도 필요. 그 외 모두 BCL).

---

## 3. Data Model

### 3.1 Entity Definition

```csharp
// Models/DeviceEntry.cs — 등록 단말 (영구 저장)
public sealed class DeviceEntry
{
    public string Id { get; set; } = Guid.NewGuid().ToString("N");
    public string Label { get; set; } = "";          // 사용자 표시명
    public string Ip { get; set; } = "";             // 예: 192.168.0.50
    public int Port { get; set; } = 80;
    public string AuthUser { get; set; } = "admin";

    // 직렬화 시 DPAPI ProtectedData(base64). 평문 노출 금지.
    public string AuthPasswordProtected { get; set; } = "";

    public int Order { get; set; }                   // 정렬 순서
    public int PollIntervalSec { get; set; } = 3;    // 1~30
    public int TimeoutMs { get; set; } = 2000;
}

// Models/DeviceStatus.cs — RemoteDeck_PC /api/status 응답 (in-memory only)
public sealed class DeviceStatus
{
    public bool Online { get; set; }
    public DateTime? LastSeen { get; set; }
    public bool PcOn { get; set; }
    public bool Relay1 { get; set; }
    public bool Relay2 { get; set; }
    public int[] Gpio { get; set; } = Array.Empty<int>();
    public long UptimeSec { get; set; }
    public string Ip { get; set; } = "";
    public string FwVer { get; set; } = "";
    public string NetMode { get; set; } = "";
    public bool MqttConnected { get; set; }
    public string? LastError { get; set; }
    public int ConsecutiveFailures { get; set; }
}

// Models/DeviceList.cs — BindingList<DeviceEntry> 래퍼 + 순서 변경 헬퍼
public sealed class DeviceList : BindingList<DeviceEntry>
{
    public void MoveUp(int idx)   { /* swap idx, idx-1; reassign Order */ }
    public void MoveDown(int idx) { /* swap idx, idx+1; reassign Order */ }
    public void ReassignOrder()   { /* 0..N-1 */ }
}
```

### 3.2 Persistence Schema (devices.json)

위치: `%LOCALAPPDATA%\IntegrateController\devices.json`

```json
{
  "version": 1,
  "devices": [
    {
      "Id": "a1b2c3...",
      "Label": "Lab #1",
      "Ip": "192.168.0.50",
      "Port": 80,
      "AuthUser": "admin",
      "AuthPasswordProtected": "AQAAANCMnd8...==",
      "Order": 0,
      "PollIntervalSec": 3,
      "TimeoutMs": 2000
    }
  ]
}
```

- `AuthPasswordProtected` = `Convert.ToBase64String(ProtectedData.Protect(utf8(plain), entropy, CurrentUser))`.
- entropy = 고정 byte[] (앱 상수, 다른 앱이 같은 user scope에서 복호화 불가하도록).

---

## 4. API Specification

### 4.1 Endpoint List (consumer view — 펌웨어는 기존 그대로)

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `http://{ip}:{port}/api/status` | 단말 상태 폴링 | Basic Auth |
| POST | `http://{ip}:{port}/api/reboot` | 단말 재부팅 | Basic Auth |

### 4.2 Detailed Specification

#### `GET /api/status`

**Request Headers**:
```
Authorization: Basic base64(user:password)
```

**Response (200 OK)** — `RemoteDeck_PC/src/main.cpp:295 buildStatusJson()` 와 일치:
```json
{
  "pc_on": true,
  "relay1": false,
  "relay2": false,
  "gpio": [0, 1, 0],
  "uptime": 12345,
  "ip": "192.168.0.50",
  "mac": "AA:BB:CC:DD:EE:FF",
  "net_mode": "ethernet",
  "fw_ver": "2.3.0",
  "device_name": "RemoteDeck-PC-01",
  "ntp_synced": true,
  "time": "2026-06-30 14:00:00",
  "mqtt_connected": true,
  "heap_free": 123456,
  "heap_min": 100000
}
```

**Error Responses**:
- `401 Unauthorized`: Basic Auth 실패 → UI에 "인증 실패" 표시, 폴링 일시 중단(편집 권고).
- timeout/socket error: `Online=false`, `ConsecutiveFailures++`.

#### `POST /api/reboot`

**Request**: empty body.

**Response (200 OK)**:
```json
{"ok": true}
```

UI 동작: 200 수신 즉시 토스트 "재부팅 요청 전송됨". 단말이 곧 재시작되므로 다음 폴링부터 `Offline` → `Online` 회복으로 검증.

---

## 5. UI/UX Design

### 5.1 Screen Layout

```
┌─────────────────────────────────────────────────────────────────────┐
│ Integrate Controller v0.1                                  [_][□][X]│
├─────────────────────────────────────────────────────────────────────┤
│ [+ Add] [Edit] [- Delete] [↑] [↓] | [Reboot]    Poll: [ 3 s] 14:32 │
├─────────────────────────────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────────────────────────────┐ │
│ │ ● │ Label   │ IP             │ PC │GPIO │FW   │Uptime │Last    │ │
│ ├───┼─────────┼────────────────┼────┼─────┼─────┼───────┼────────┤ │
│ │ 🟢│ Lab #1  │ 192.168.0.50   │ ON │010  │2.3.0│12d3h  │14:32:01│ │
│ │ 🟢│ Lab #2  │ 192.168.0.51   │OFF │000  │2.3.0│ 3h12m │14:32:01│ │
│ │ 🔴│ Lobby   │ 192.168.0.52   │ ?  │ ?   │ ?   │ ?     │14:30:55│ │
│ └─────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────┤
│ Detail: Lab #1 — mqtt: ✅, net: ethernet, heap: 121 KB, ntp: synced │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 User Flow

```
앱 시작
  └─ DeviceStore.Load() → DeviceList 채우기 → 각 단말 폴링 자동 시작

[Add] → DeviceEditDialog (Label/IP/Port/User/Pass/Interval) → OK
  └─ DeviceList.Add → Store.Save → Poller.Start

[Edit] → 선택 row → DeviceEditDialog (기존값 prefill) → OK
  └─ Poller.Restart(device)

[↑/↓] → 선택 row → DeviceList.MoveUp/Down → Store.Save

[- Delete] → 확인 → Poller.Stop → DeviceList.Remove → Store.Save

[Reboot] / 우클릭→재부팅 → 확인 → RemoteDeckClient.RebootAsync → 결과 토스트

행 선택 → Detail 패널에 raw status JSON
```

### 5.3 Component List

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `MainForm` | `UI/MainForm.cs` | 화면 구성, 이벤트 라우팅 |
| `MainForm.Designer.cs` | `UI/MainForm.Designer.cs` | Designer 코드 (Toolbar/DGV/Detail) |
| `DeviceEditDialog` | `UI/DeviceEditDialog.cs` | 단말 등록/편집 modal |
| `StatusFormatter` | `UI/StatusFormatter.cs` | 셀 렌더링(색상/이모지/포맷) |

### 5.4 Page UI Checklist

#### MainForm

- [ ] Toolbar Button: `Add` (단말 등록 → DeviceEditDialog)
- [ ] Toolbar Button: `Edit` (선택 단말 편집)
- [ ] Toolbar Button: `Delete` (확인 후 삭제)
- [ ] Toolbar Button: `↑` MoveUp (선택 단말 위로)
- [ ] Toolbar Button: `↓` MoveDown
- [ ] Toolbar Button: `Reboot` (선택 단말 재부팅, 확인 다이얼로그)
- [ ] Toolbar Label/Field: `Poll interval [sec]` (전역 기본값, 단말별 override 가능)
- [ ] Toolbar Label: 현재 시각 (1s tick)
- [ ] DataGridView columns: `Status(●)` / `Label` / `IP` / `PC` / `GPIO[3]` / `FW` / `Uptime` / `Last Seen`
- [ ] DataGridView DoubleBuffered = true
- [ ] DataGridView 우클릭 ContextMenu: Reboot / Edit / Delete
- [ ] Detail panel: 선택 단말의 mqtt/net_mode/heap/ntp/time 표시
- [ ] StatusStrip: 마지막 에러 메시지 (인증 실패 등)
- [ ] FormClosing 시 모든 Poller stop + Store.Save

#### DeviceEditDialog

- [ ] TextBox: Label (required)
- [ ] TextBox: IP (regex `^\d+\.\d+\.\d+\.\d+$` 검증)
- [ ] NumericUpDown: Port (default 80, 1~65535)
- [ ] TextBox: Auth User (default admin)
- [ ] TextBox: Auth Password (PasswordChar='*')
- [ ] NumericUpDown: Poll Interval (1~30s, default 3)
- [ ] NumericUpDown: Timeout (500~10000ms, default 2000)
- [ ] Button: OK / Cancel
- [ ] OK 클릭 시 입력 검증 실패하면 ErrorProvider 표시

---

## 6. Error Handling

### 6.1 Error Code Definition

| Code | Source | Cause | Handling |
|------|--------|-------|----------|
| `HTTP_401` | RemoteDeckClient | Basic Auth 실패 | Offline 표시 + 행 색상 노랑 + StatusStrip "인증 실패: {label}" |
| `HTTP_5xx` | RemoteDeckClient | 펌웨어 내부 오류 | LastError 기록, fail counter++ |
| `TIMEOUT` | RemoteDeckClient | 응답 없음 | Online=false, fail counter++ |
| `OFFLINE` | DevicePoller | 연속 3회 실패 | 행 상태 🔴, fail counter는 성공 시 리셋 |
| `DECRYPT_FAIL` | DeviceStore | DPAPI 복호화 실패 (다른 PC/계정) | 해당 단말 password 빈 값 → UI에 "인증 정보 재입력 필요" 배지 |
| `STORE_WRITE_FAIL` | DeviceStore | 디스크 쓰기 실패 | MessageBox + 로그 |

### 6.2 Error Response Format (internal)

`RemoteDeckClient` 메서드는 결과 record 반환:
```csharp
public sealed record RestResult<T>(bool Ok, T? Value, int StatusCode, string? Error);
```

UI는 `Ok==false` 시 `Error` 메시지를 StatusStrip / 토스트에 표시.

---

## 7. Security Considerations

- [x] 입력 검증: IP regex, Port 1~65535, Label 비어있지 않음.
- [x] 인증 처리: HTTP Basic Auth `Authorization: Basic base64(user:pw)`.
- [x] 민감 데이터 암호화: **Password는 메모리 외 영구 저장 전 DPAPI ProtectedData.Protect (CurrentUser scope, app entropy)**.
- [ ] HTTPS 강제: 펌웨어가 HTTP만 제공 → out of scope (사내망 한정 사용).
- [x] Rate Limiting: 단말별 폴링 주기 최소 1s 하한.
- [x] devices.json 평문 PW 금지 — 로드 시 Protected가 비어있으면 PW 빈 문자열.
- [x] Detail 패널의 raw JSON 표시는 PW 미포함 (펌웨어가 PW 응답 안함).

---

## 8. Test Plan

### 8.1 Test Scope

| Type | Target | Tool | Phase |
|------|--------|------|-------|
| L1: REST API 테스트 | `/api/status`, `/api/reboot` Basic Auth | curl + RemoteDeck_PC 실기기 | Do |
| L2: UI Action 테스트 | DataGridView, Toolbar, EditDialog | 수동 시연 (Windows GUI 자동화 미도입) | Do |
| L3: 시나리오 테스트 | 등록→폴링→재부팅→복구 흐름 | 수동 시연 | Do |
| L4: 단위 테스트 (선택) | DeviceStore DPAPI roundtrip, RemoteDeckClient parse | xUnit | Do (시간 허용 시) |

### 8.2 L1: REST 테스트 시나리오

| # | Endpoint | Method | Test Description | Expected |
|---|----------|--------|-----------------|----------|
| 1 | `/api/status` | GET | 정상 Basic Auth | 200, JSON에 `pc_on`/`gpio`/`uptime`/`fw_ver` 포함 |
| 2 | `/api/status` | GET | 잘못된 PW | 401 |
| 3 | `/api/status` | GET | 비-도달 IP | timeout → SocketException catch |
| 4 | `/api/reboot` | POST | 정상 | 200 `{"ok":true}` + 단말 재부팅 → 다음 3s 폴링서 Offline → Online |

### 8.3 L2: UI Action 테스트

| # | Page | Action | Expected | Verification |
|---|------|--------|----------|--------------|
| 1 | MainForm | 앱 시작 | 저장 단말 로드 + 폴링 자동 시작 | DGV 각 행이 3s 이내 Online 표시 |
| 2 | MainForm | Add | EditDialog 표시 | 유효 입력 OK → 행 추가 |
| 3 | MainForm | ↑/↓ | 선택 행 이동 | DGV 순서 변경 + devices.json 저장 |
| 4 | MainForm | Reboot | 확인 다이얼로그 → 전송 | 토스트 + 행 일시적 Offline → Online 복구 |
| 5 | DeviceEditDialog | 빈 IP로 OK | ErrorProvider 표시 | OK 차단 |

### 8.4 L3: 시나리오

| # | Scenario | Steps | Success Criteria |
|---|----------|-------|-----------------|
| 1 | 신규 단말 운영 흐름 | Add → 3s 폴링 Online → Reboot → 회복 | 전 과정 30s 이내 |
| 2 | 다중 단말 동시성 | N=3 단말 등록 → 동시 폴링 | UI freeze 없음 |
| 3 | 영속성 | 단말 추가 → 앱 재시작 → 동일 단말 로드 + 폴링 자동 | PW 복호화 성공 |
| 4 | 오프라인 회복 | 정상 → 케이블 분리 → 3회 실패→🔴 → 복원→🟢 | 자동 복구 |

### 8.5 Seed Data Requirements

| Entity | Minimum Count | Key Fields Required |
|--------|:------------:|---------------------|
| RemoteDeck_PC 실기기 | 1 | IP, admin/12345 (또는 변경된 자격증명) |

> Do phase에서 RemoteDeck_PC 1대를 LAN에 연결한 상태로 검증.

---

## 9. Clean Architecture (Pragmatic Mapping)

> 풀 Clean Architecture는 아님(Option B 미선택). 단, 책임을 3-folder로 분리.

### 9.1 Layer Structure

| Layer | Responsibility | Folder |
|-------|---------------|--------|
| **Presentation** | WinForms, DataGridView 렌더링, 사용자 입력 | `UI/` |
| **Application/Service** | 폴링 오케스트레이션, REST 호출, 영속성 | `Services/` |
| **Domain/Model** | 단말 엔티티, 상태 DTO | `Models/` |

### 9.2 Dependency Rules

```
UI/ ──────▶ Services/ ──────▶ Models/
   ▲                                ▲
   └────────────────────────────────┘
    (UI도 Models 직접 참조 허용 — BindingList 바인딩)
```

### 9.3 File Import Rules

| From | Can Import | Cannot Import |
|------|-----------|---------------|
| UI | Services, Models | (없음) |
| Services | Models, System.* | UI |
| Models | System.* only | UI, Services |

### 9.4 This Feature's Layer Assignment

| Component | Layer | Location |
|-----------|-------|----------|
| MainForm, DeviceEditDialog, StatusFormatter | Presentation | `UI/` |
| DeviceStore, RemoteDeckClient, DevicePoller | Service | `Services/` |
| DeviceEntry, DeviceStatus, DeviceList | Model | `Models/` |

---

## 10. Coding Convention Reference

### 10.1 Naming Conventions

| Target | Rule | Example |
|--------|------|---------|
| Classes | PascalCase | `DeviceEntry`, `RemoteDeckClient` |
| Methods | PascalCase | `GetStatusAsync`, `Save` |
| Private fields | _camelCase | `_httpClient`, `_devices` |
| Constants | PascalCase | `DefaultPollSec` |
| File | PascalCase.cs | `DeviceEntry.cs` |
| Folder | PascalCase | `Services/`, `UI/` |

### 10.2 Async / HttpClient

- 모든 IO는 `async Task<T>` (`async void` 금지, 단 이벤트 핸들러 제외).
- `HttpClient`는 static Singleton 1개. 요청별 `HttpRequestMessage`에 `Authorization` 헤더 부착.
- UI 마샬링: `if (InvokeRequired) BeginInvoke(...)`.

### 10.3 Environment Variables

| Variable | Purpose | Scope |
|----------|---------|-------|
| (none) | GUI 입력 + DPAPI 저장으로 충분 | — |

### 10.4 This Feature's Conventions

| Item | Convention Applied |
|------|-------------------|
| Project naming | `IntegrateController` (csproj=AssemblyName=Namespace) |
| File organization | Models/Services/UI 3-folder |
| State management | `BindingList<DeviceEntry>` (DataGridView DataSource) |
| Error handling | `RestResult<T>` record, UI에서 StatusStrip 메시지 표시 |
| Logging | `System.Diagnostics.Trace.WriteLine` (Debug 빌드만) |

---

## 11. Implementation Guide

### 11.1 File Structure

```
apitestutility_v2/integrate_controller/
└── IntegrateController/
    ├── IntegrateController.sln
    └── IntegrateController/
        ├── IntegrateController.csproj
        ├── Program.cs
        ├── app.manifest
        │
        ├── Models/
        │   ├── DeviceEntry.cs
        │   ├── DeviceStatus.cs
        │   └── DeviceList.cs
        │
        ├── Services/
        │   ├── DeviceStore.cs
        │   ├── RemoteDeckClient.cs
        │   └── DevicePoller.cs
        │
        └── UI/
            ├── MainForm.cs
            ├── MainForm.Designer.cs
            ├── DeviceEditDialog.cs
            ├── DeviceEditDialog.Designer.cs
            └── StatusFormatter.cs
```

### 11.2 Implementation Order

1. [ ] **M1**: csproj + sln + Program.cs + Models/* (DeviceEntry, DeviceStatus, DeviceList) + Services/DeviceStore.cs (DPAPI 저장 + 로드)
2. [ ] **M2**: Services/RemoteDeckClient.cs (`GetStatusAsync`, `RebootAsync`, Basic Auth) + Services/DevicePoller.cs (PeriodicTimer + CTS + 단말당 1 task)
3. [ ] **M3**: UI/MainForm.cs(.Designer) (Toolbar/DGV/Detail/StatusStrip) + UI/DeviceEditDialog (Add/Edit) + StatusFormatter + 이벤트 와이어링 + FormClosing
4. [ ] **M4**: Reboot 명령 + 우클릭 ContextMenu + 에러 토스트 + DPAPI 실패 시 UI 배지 + `dotnet publish -c Release -r win-x64 --self-contained` 검증

### 11.3 Session Guide

#### Module Map

| Module | Scope Key | Description | Estimated Turns |
|--------|-----------|-------------|:---------------:|
| Models + Store + project skeleton | `module-1` | csproj/sln, Models 3개, DeviceStore (DPAPI), Program.cs | 15~20 |
| REST Client + Poller | `module-2` | RemoteDeckClient, DevicePoller (PeriodicTimer/CTS), 단위 검증 | 15~20 |
| WinForms UI | `module-3` | MainForm + DeviceEditDialog + StatusFormatter + 와이어링 | 20~30 |
| Reboot + 에러 처리 + publish | `module-4` | reboot 명령, ContextMenu, 토스트, single-file publish 검증 | 10~15 |

#### Recommended Session Plan

| Session | Phase | Scope | Turns |
|---------|-------|-------|:-----:|
| Session 1 (현재) | Plan + Design | 전체 | ~25 |
| Session 2 | Do | `--scope module-1,module-2` | 30~40 |
| Session 3 | Do | `--scope module-3,module-4` | 30~45 |
| Session 4 | Check + Report | 전체 | 25~35 |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-30 | Initial draft, Option C 선정 | KDI |
