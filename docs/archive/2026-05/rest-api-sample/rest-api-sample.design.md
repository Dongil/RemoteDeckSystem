# REST API 연동 샘플 작성 Design Document

> **Project**: REST-api (RemoteDeck HTTP 연동 샘플)
> **Target Framework**: .NET Framework 4.8 / WinForms
> **Author**: KDI
> **Date**: 2026-05-11
> **Status**: Design

> 참조: [Plan](../../01-plan/features/rest-api-sample.plan.md)

---

## 1. Architecture

### 1.1 컴포넌트 구성 (단일 Form1.cs 내부)

```
Form1
├── 필드
│   ├── HttpClient _http (정적, baseAddress 동적 변경)
│   ├── Timer _pollTimer (Interval = 1000ms)
│   ├── bool _isConnected
│   ├── int _failureCount (연속 실패 카운터)
│   └── const int MAX_LOG_LINES = 1000
│
├── 이벤트 핸들러
│   ├── Form1_Load               → 기본값 채움
│   ├── btnConn_Click            → Start/Stop 토글 (async)
│   ├── btnRelay1On_Click        → SendRelay(1, "on")
│   ├── btnRelay1Off_Click       → SendRelay(1, "off")
│   ├── btnRelay1Pluse_Click     → SendPulse(1)
│   ├── btnRelay2On_Click        → SendRelay(2, "on")
│   ├── btnRelay2Off_Click       → SendRelay(2, "off")
│   ├── btnRelay2Pluse_Click     → SendPulse(2)
│   └── _pollTimer_Tick          → FetchStatusAsync()
│
└── 헬퍼
    ├── ConnectAsync(ip, port, id, pw)
    ├── DisconnectAndReset()
    ├── FetchStatusAsync()        → GET /api/status
    ├── SendRelay(relay, state)   → POST /api/relay {state}
    ├── SendPulse(relay)          → POST /api/relay {cmd:pulse}
    ├── UpdateRelayUI(r1, r2)     → 색상 적용
    ├── SetControlsEnabled(bool)  → Relay 버튼 활성/비활성
    └── Log(string msg)           → 시각 + 본문 + 자동 스크롤 + 1000줄 제한
```

### 1.2 통신 흐름

```
[user] Start 클릭
   │
   ▼
btnConn_Click ─► ConnectAsync(ip, port, id, pw)
   │                  │
   │                  ├── _http.BaseAddress = "http://{ip}:{port}/"
   │                  ├── Authorization 헤더 설정 (Basic base64)
   │                  └── GET /api/status (Timeout 3s)
   │                       │
   │            성공: 200  ──► _isConnected = true
   │                            SetControlsEnabled(true)
   │                            UpdateRelayUI(r1, r2)
   │                            btnConn.Text = "Stop"
   │                            _pollTimer.Start()
   │                       │
   │            실패: ex   ──► Log("ERROR: ...")
   │                            _isConnected = false
   │                            (버튼 비활성 유지)
   │
[1초마다] _pollTimer_Tick
   │
   ▼
FetchStatusAsync() ─► GET /api/status
   │                       │
   │            성공: 200  ──► UpdateRelayUI(r1, r2)
   │                            _failureCount = 0
   │            실패        ──► _failureCount++
   │                            if (_failureCount >= 3) DisconnectAndReset()
   │
[user] Relay1 ON 클릭
   │
   ▼
SendRelay(1, "on") ─► POST /api/relay {"relay":1,"state":"on"}
                       (응답 무시, 다음 폴링으로 상태 자동 반영)
```

---

## 2. UI Mapping (Designer는 이미 배치됨)

### 2.1 기존 컨트롤 → 역할

| 컨트롤 (Designer.cs) | 역할 | 추가 작업 |
|---------------------|------|----------|
| `textIP` | 장치 IP 입력 | 기본값 "192.168.1.200" |
| `textPort` | 포트 입력 | 기본값 "5050" |
| `textID` | Basic Auth user | 기본값 "admin" |
| `textPW` | Basic Auth password | 기본값 "12345" + PasswordChar='*' |
| `btnConn` | Start/Stop 토글 | Click 이벤트 연결, 초기 Text="Start" |
| `groupBox1` "RELAY1" | 릴레이1 그룹 | 변경 없음 |
| `btnRelay1On` | 릴레이1 ON | Click → SendRelay(1,"on"), Enabled=false 시작, BackColor 추적 |
| `btnRelay1Off` | 릴레이1 OFF | Click → SendRelay(1,"off"), Enabled=false 시작, BackColor 추적 |
| `btnRelay1Pluse` | 릴레이1 Pulse | Click → SendPulse(1), Enabled=false 시작 |
| `groupBox2` "RELAY2" | 릴레이2 그룹 | 변경 없음 |
| `btnRelay2On/Off/Pluse` | 릴레이2 동일 | 위와 동일 |
| `textLog` | 송수신 로그 | ScrollBars=Vertical, Multiline=true(이미 true), ReadOnly=true(이미 true) |
| `label5` "STATUS :" | 라벨 | 변경 없음 |

### 2.2 색상 매핑 규칙

| 릴레이 상태 | btnRelay*On.BackColor | btnRelay*Off.BackColor | btnRelay*Pluse.BackColor |
|------------|----------------------|------------------------|--------------------------|
| ON (1) | `Color.Red` | `SystemColors.Control` | `SystemColors.Control` (변경 없음) |
| OFF (0) | `SystemColors.Control` | `SystemColors.Control` | `SystemColors.Control` (변경 없음) |

> Off 버튼은 색상 추적하지 않음(요구사항 "ON=red, OFF=control"은 ON 버튼의 토글 표시 의미로 해석). Pulse는 이벤트형이라 항상 기본 색상.

### 2.3 폼 제목

| 변경 전 | 변경 후 |
|---------|---------|
| `this.Text = "Form1"` | `this.Text = "RemoteDeck HTTP Sample"` |

---

## 3. API Contract (v2.2)

### 3.1 GET /api/status

**Request**
```
GET http://{ip}:{port}/api/status
Authorization: Basic base64(id:pw)
```

**Response (200 OK, application/json) — 실제 펌웨어 응답**
```json
{
  "pc_on": false,
  "relay1": 0,
  "relay2": 0,
  "gpio": [0, 0, 0],
  "uptime": 12345,
  "ip": "192.168.1.200",
  "mac": "AA:BB:CC:DD:EE:FF",
  "net_mode": "ethernet",
  "fw_ver": "2.2.0",
  "device_name": "새기기",
  "ntp_synced": true,
  "time": "12:34:56",
  "mqtt_connected": false
}
```

**Sample에서 사용하는 필드**: `relay1`, `relay2` (int 0/1)
**무시하는 필드**: 나머지 전부

**에러**
- 401 Unauthorized → 인증 실패
- 타임아웃/Connection refused → 네트워크 오류

### 3.2 POST /api/relay

**Relay ON/OFF**
```
POST http://{ip}:{port}/api/relay
Authorization: Basic base64(id:pw)
Content-Type: application/json

{"relay":1,"state":"on"}    또는    {"relay":1,"state":"off"}
{"relay":2,"state":"on"}    또는    {"relay":2,"state":"off"}
```

**Pulse**
```
POST http://{ip}:{port}/api/relay
Authorization: Basic base64(id:pw)
Content-Type: application/json

{"cmd":"pulse","relay":1}
{"cmd":"pulse","relay":2}
```

**Response (200 OK)**
```json
{"ok":true}
```

---

## 4. Data Structures

### 4.1 응답 파싱 DTO

```csharp
// .NET Framework 4.8 — JavaScriptSerializer 사용
// using System.Web.Script.Serialization;

private class StatusResponse
{
    public int relay1 { get; set; }
    public int relay2 { get; set; }
    // 나머지 필드는 deserialize에서 무시됨
}
```

> JavaScriptSerializer는 미선언 필드 자동 무시 → 미니멀 DTO 가능

### 4.2 요청 페이로드

```csharp
// SendRelay
new Dictionary<string, object> { { "relay", relay }, { "state", state } }
// → {"relay":1,"state":"on"}

// SendPulse
new Dictionary<string, object> { { "cmd", "pulse" }, { "relay", relay } }
// → {"cmd":"pulse","relay":1}
```

---

## 5. Component Detailed Design

### 5.1 csproj 변경

```xml
<!-- REST-api.csproj 의 ItemGroup 에 추가 -->
<Reference Include="System.Web.Extensions" />
```
(`JavaScriptSerializer` 사용을 위한 .NET Framework 표준 어셈블리)

### 5.2 Form1.cs — 전체 구조

```csharp
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace REST_api
{
    public partial class Form1 : Form
    {
        // ────── 필드 ──────
        private static readonly HttpClient _http = new HttpClient
        {
            Timeout = TimeSpan.FromSeconds(3)
        };
        private readonly Timer _pollTimer = new Timer { Interval = 1000 };
        private readonly JavaScriptSerializer _json = new JavaScriptSerializer();
        private bool _isConnected = false;
        private int _failureCount = 0;
        private const int MAX_LOG_LINES = 1000;
        private const int MAX_FAILURES = 3;

        public Form1()
        {
            InitializeComponent();
            WireUp();
        }

        // ────── 초기화 ──────
        private void WireUp()
        {
            this.Text = "RemoteDeck HTTP Sample";
            this.Load += Form1_Load;
            btnConn.Click += btnConn_Click;
            btnRelay1On.Click  += (s,e) => _ = SendRelayAsync(1, "on");
            btnRelay1Off.Click += (s,e) => _ = SendRelayAsync(1, "off");
            btnRelay1Pluse.Click += (s,e) => _ = SendPulseAsync(1);
            btnRelay2On.Click  += (s,e) => _ = SendRelayAsync(2, "on");
            btnRelay2Off.Click += (s,e) => _ = SendRelayAsync(2, "off");
            btnRelay2Pluse.Click += (s,e) => _ = SendPulseAsync(2);
            _pollTimer.Tick += async (s,e) => await FetchStatusAsync();
            textPW.PasswordChar = '*';
            SetControlsEnabled(false);
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(textIP.Text))   textIP.Text = "192.168.1.200";
            if (string.IsNullOrEmpty(textPort.Text)) textPort.Text = "5050";
            if (string.IsNullOrEmpty(textID.Text))   textID.Text = "admin";
            if (string.IsNullOrEmpty(textPW.Text))   textPW.Text = "12345";
        }

        // ────── Start/Stop 토글 ──────
        private async void btnConn_Click(object sender, EventArgs e)
        {
            if (_isConnected) { DisconnectAndReset(); return; }

            btnConn.Enabled = false;
            try
            {
                var ip = textIP.Text.Trim();
                var port = textPort.Text.Trim();
                var id = textID.Text;
                var pw = textPW.Text;

                _http.BaseAddress = new Uri($"http://{ip}:{port}/");
                var basic = Convert.ToBase64String(Encoding.ASCII.GetBytes($"{id}:{pw}"));
                _http.DefaultRequestHeaders.Authorization =
                    new AuthenticationHeaderValue("Basic", basic);

                Log($"CONNECT → http://{ip}:{port}/  (user={id})");
                bool ok = await FetchStatusAsync(true);
                if (ok)
                {
                    _isConnected = true;
                    _failureCount = 0;
                    _pollTimer.Start();
                    btnConn.Text = "Stop";
                    SetControlsEnabled(true);
                    Log("CONNECTED, polling 1s");
                }
            }
            catch (Exception ex) { Log("CONNECT ERROR: " + ex.Message); }
            finally { btnConn.Enabled = true; }
        }

        private void DisconnectAndReset()
        {
            _pollTimer.Stop();
            _isConnected = false;
            _failureCount = 0;
            SetControlsEnabled(false);
            btnConn.Text = "Start";
            UpdateRelayUI(0, 0);
            Log("DISCONNECTED");
        }

        // ────── HTTP ──────
        private async Task<bool> FetchStatusAsync(bool logRequest = false)
        {
            try
            {
                if (logRequest) Log("GET /api/status");
                using (var resp = await _http.GetAsync("api/status"))
                {
                    var body = await resp.Content.ReadAsStringAsync();
                    if (!resp.IsSuccessStatusCode)
                    {
                        Log($"  ← {(int)resp.StatusCode} {body}");
                        OnFailure();
                        return false;
                    }
                    if (logRequest) Log("  ← 200 " + body);
                    var s = _json.Deserialize<StatusResponse>(body);
                    UpdateRelayUI(s.relay1, s.relay2);
                    _failureCount = 0;
                    return true;
                }
            }
            catch (Exception ex)
            {
                Log("GET /api/status ERROR: " + ex.Message);
                OnFailure();
                return false;
            }
        }

        private async Task SendRelayAsync(int relay, string state)
        {
            var payload = new Dictionary<string, object> {
                { "relay", relay }, { "state", state }
            };
            await PostRelayAsync(payload, $"relay{relay}={state}");
        }

        private async Task SendPulseAsync(int relay)
        {
            var payload = new Dictionary<string, object> {
                { "cmd", "pulse" }, { "relay", relay }
            };
            await PostRelayAsync(payload, $"relay{relay}=pulse");
        }

        private async Task PostRelayAsync(Dictionary<string, object> payload, string tag)
        {
            try
            {
                var body = _json.Serialize(payload);
                Log($"POST /api/relay  {body}");
                using (var content = new StringContent(body, Encoding.UTF8, "application/json"))
                using (var resp = await _http.PostAsync("api/relay", content))
                {
                    var rsp = await resp.Content.ReadAsStringAsync();
                    Log($"  ← {(int)resp.StatusCode} {rsp}");
                }
            }
            catch (Exception ex) { Log($"POST {tag} ERROR: " + ex.Message); }
        }

        private void OnFailure()
        {
            _failureCount++;
            if (_failureCount >= MAX_FAILURES && _isConnected)
            {
                Log($"Polling failed {MAX_FAILURES} times — auto disconnect");
                DisconnectAndReset();
            }
        }

        // ────── UI 헬퍼 ──────
        private void UpdateRelayUI(int r1, int r2)
        {
            btnRelay1On.BackColor = r1 == 1 ? Color.Red : SystemColors.Control;
            btnRelay2On.BackColor = r2 == 1 ? Color.Red : SystemColors.Control;
        }

        private void SetControlsEnabled(bool enabled)
        {
            btnRelay1On.Enabled = btnRelay1Off.Enabled = btnRelay1Pluse.Enabled = enabled;
            btnRelay2On.Enabled = btnRelay2Off.Enabled = btnRelay2Pluse.Enabled = enabled;
        }

        private void Log(string msg)
        {
            string line = DateTime.Now.ToString("HH:mm:ss.fff") + "  " + msg + Environment.NewLine;
            if (textLog.Lines.Length > MAX_LOG_LINES)
            {
                var keep = new string[MAX_LOG_LINES / 2];
                Array.Copy(textLog.Lines, textLog.Lines.Length - keep.Length, keep, 0, keep.Length);
                textLog.Lines = keep;
            }
            textLog.AppendText(line);
            textLog.SelectionStart = textLog.Text.Length;
            textLog.ScrollToCaret();
        }

        // ────── DTO ──────
        private class StatusResponse
        {
            public int relay1 { get; set; }
            public int relay2 { get; set; }
        }
    }
}
```

### 5.3 Form1.Designer.cs — 변경 사항

이벤트 핸들러 와이어업은 모두 `WireUp()`에서 처리하므로 Designer.cs는 **변경 없음**.
폼 제목도 코드에서 변경하므로 Designer 수정 불필요.

> 단, Visual Studio Designer에서 이벤트 핸들러를 자동 생성하면 `Form1_Load`, `btnConn_Click` 등이 Designer.cs에 자동 추가될 수 있음 — 이 경우 중복 와이어업 방지 위해 `WireUp()`에서 제거 권장. 본 샘플은 학습성 우선으로 단일 파일 와이어업 채택.

---

## 6. Error Handling

| 시나리오 | 응답 | UI 표시 |
|----------|------|---------|
| 잘못된 IP/Port (Connection refused) | HttpRequestException | Log "ERROR: ...", 버튼 비활성 유지 |
| 타임아웃 (3초 초과) | TaskCanceledException | Log "ERROR: ...", 폴링 중 누적 |
| 401 Unauthorized (잘못된 ID/PW) | HTTP 401 | Log "← 401", 버튼 비활성 |
| 폴링 중 3회 연속 실패 | — | Log "auto disconnect" + DisconnectAndReset() |
| JSON 파싱 실패 | InvalidOperationException | Log "ERROR: ..." (catch) |
| Pulse / Relay 명령 실패 | HTTP non-2xx 또는 예외 | Log만 표시 (폴링 계속) |

---

## 7. Implementation Order

| Step | 작업 | 검증 |
|------|------|------|
| 1 | csproj에 `System.Web.Extensions` 참조 추가 | 빌드 성공 |
| 2 | Form1.cs 전체 구조(필드, WireUp, Form1_Load) 구현 | 폼 실행 시 기본값 표시 |
| 3 | `Log()` 헬퍼 구현 | textLog에 시각 + 메시지 표시 |
| 4 | `btnConn_Click` + `ConnectAsync` + Basic Auth 구현 | Start 클릭 시 textLog에 GET 송신 표시 |
| 5 | `FetchStatusAsync` + DTO + `UpdateRelayUI` | 실제 장치 응답으로 ON 버튼 색상 변경 |
| 6 | `_pollTimer` 1초 폴링 | 외부에서 릴레이 변경 시 색상 자동 추적 |
| 7 | `SendRelayAsync` / `SendPulseAsync` + Relay 버튼 핸들러 | 버튼 클릭 시 장치 동작 + 폴링으로 색상 변경 |
| 8 | `OnFailure` 카운터 + 자동 disconnect | 장치 끄면 3초 후 자동 disconnect |
| 9 | 폼 제목 변경, 최종 빌드 | bin/Debug/REST-api.exe 실행 검증 |

---

## 8. Test Plan

### 8.1 단위 동작 확인

| 케이스 | 입력 | 기대 결과 |
|--------|------|----------|
| 정상 연결 | 192.168.1.200:5050 / admin:12345 | Stop 표시, Relay 버튼 활성, Log에 GET 200 |
| 잘못된 IP | 1.2.3.4:5050 | Log에 ERROR, 버튼 비활성 유지, 3초 후 timeout |
| 잘못된 PW | admin:wrongpw | Log "← 401", 버튼 비활성 |
| Relay1 ON | (연결 후) 버튼 클릭 | 장치 릴레이 ON + 1초 내 ON 버튼 Red |
| Relay1 OFF | (연결 후) 버튼 클릭 | 장치 릴레이 OFF + 1초 내 ON 버튼 Control |
| Pulse | (연결 후) 버튼 클릭 | 500ms 동안 릴레이 ON → 자동 OFF, 색상 잠시 Red |
| 장치 재부팅 | (연결 후) 장치 전원 OFF | 3회 실패 후 자동 Stop |
| Stop | 연결 중 Start(=Stop) 클릭 | 폴링 중단, 버튼 비활성, 색상 초기화 |

### 8.2 외부 변경 추적 검증

1. RemoteDeck 장치에 다른 도구(curl, APITestUtility_v2)로 Relay1 ON
2. 본 샘플의 btnRelay1On 배경이 1초 내 Red로 변경되는지 확인
3. 외부에서 OFF → 본 샘플 배경이 Control로 복귀

---

## 9. Open Questions

| Q | 결정 |
|---|------|
| OFF 버튼도 색상 추적? | **NO** — ON 버튼만 토글 표시 (요구사항 "ON=red, OFF=control" 해석) |
| GPIO/PC-LED도 표시? | **NO** — Scope Out |
| HttpClient cookie/keep-alive | 기본값 사용 (.NET 4.8 HttpClient 기본 동작) |
| 설정 영구 저장 | **NO** — 매 실행 시 기본값 사용 |
| 동시 클릭 디바운스 | **NO** — 학습 샘플 단순성 우선, HttpClient.Timeout으로 자연 직렬화 |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2026-05-11 | Initial design | KDI |
