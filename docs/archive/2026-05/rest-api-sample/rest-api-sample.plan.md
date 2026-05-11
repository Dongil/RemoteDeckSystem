# REST API 연동 샘플 작성 Planning Document

> **Summary**: `/REST-api` 폴더의 .NET Framework 4.8 WinForms 프로젝트를 RemoteDeck_PC HTTP API 연동 테스트 샘플로 완성
>
> **Project**: REST-api (RemoteDeck HTTP 연동 샘플)
> **Target Framework**: .NET Framework 4.8 / WinForms
> **Author**: KDI
> **Date**: 2026-05-11
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | RemoteDeck_PC 장치는 HTTP/MQTT/WS/RS485 4개 인터페이스를 지원하지만, 외부 개발자가 HTTP REST API로 연동할 때 참조할 수 있는 .NET Framework 기반 최소 코드 샘플이 없음. APITestUtility_v2는 .NET 8 + 4종 프로토콜 통합으로 학습 곡선이 높음 |
| **Solution** | `/REST-api`에 이미 배치된 WinForms UI(IP/Port/ID/PW 입력, Start, Relay1/2 ON/OFF/Pulse, Status 로그)에 HttpClient + Basic Auth + JSON 파싱 로직을 연결. Start 시 `/api/status` polling 시작, 버튼 클릭 시 `/api/relay` POST 송신, 상태에 따라 버튼 배경 ON=Red / OFF=Control 자동 변경 |
| **Function/UX Effect** | 외부 개발자가 단일 Form1 파일만 보고 v2.2 HTTP API 연동 패턴을 즉시 학습 가능. 실시간 상태 표시 + 버튼 색상 피드백으로 동작 검증 직관적 |
| **Core Value** | 배포 시 "최소 통합 샘플" 제공으로 SI 협력사 연동 진입 장벽 제거, .NET Framework 4.8 레거시 환경(공장/현장 PC) 호환성 확보 |

---

## 1. Overview

### 1.1 Purpose

1. **연동 학습 자료**: RemoteDeck_PC HTTP API(v2.2)를 최소 코드로 시연하는 .NET Framework 4.8 WinForms 샘플 제공
2. **레거시 호환**: 공장/현장의 .NET Framework 4.8 환경에서 별도 패키지 의존 없이 동작
3. **실시간 피드백**: 폴링 기반으로 릴레이 상태 변화를 UI에 즉시 반영(색상)

### 1.2 Background

- `/REST-api/Form1.Designer.cs`에 UI 컨트롤은 이미 배치됨 (textIP/Port/ID/PW, btnConn, groupBox1/2 + btnRelay1/2 On/Off/Pulse, textLog)
- `Form1.cs`에는 이벤트 핸들러 미구현 (기본 생성자만 존재)
- v2.2 HTTP API 엔드포인트:
  - `GET /api/status` — 전체 상태 조회 (Basic Auth, JSON: relay1/relay2/pc_on/gpio1~3)
  - `POST /api/relay` — `{"relay":1,"state":"on"|"off"}` 또는 `{"cmd":"pulse","relay":1}`
- APITestUtility_v2는 .NET 8 기반으로 .NET Framework 4.8 환경에 직접 사용 불가

### 1.3 Related Documents

- `docs/RemoteDeck_PC_Manual.md` — HTTP API 명세
- `docs/04-report/features/v2.2-api-simplification.report.md` — v2.2 API 구조
- `APITestUtility_v2/RemoteDeckTest/RemoteDeckTest/MainForm.cs` — 참조용 (HTTP 호출 패턴)

---

## 2. Scope

### 2.1 In Scope

- [ ] Form1.cs에 HttpClient 기반 HTTP 통신 로직 구현
- [ ] btnConn(Start) 핸들러: 연결 시도 + 상태 polling 시작/중지 토글
- [ ] `GET /api/status` 호출 + 응답 JSON 파싱(JavaScriptSerializer 사용, .NET 4.8 내장)
- [ ] Polling Timer(1초 주기): 상태 갱신 + 버튼 색상 업데이트
- [ ] btnRelay1On/Off/Pulse, btnRelay2On/Off/Pulse 핸들러: `POST /api/relay` 송신
- [ ] HTTP Basic Auth (textID + textPW) 헤더 자동 부착
- [ ] textLog에 송수신 로그 표시 (시각 + 요청/응답)
- [ ] 릴레이 상태 → 버튼 배경 색상 자동 변경: ON = Color.Red, OFF = SystemColors.Control
- [ ] 연결 실패 / HTTP 에러 시 textLog에 에러 메시지 + 버튼 비활성화
- [ ] 기본값 자동 채움 (IP=192.168.1.200, Port=5050, ID=admin, PW=12345)

### 2.2 Out of Scope

- WebSocket / MQTT / RS485 인터페이스 (HTTP 전용 샘플)
- 스케줄, WOL, Web Request 설정 UI (제어만)
- GPIO 입력 표시 (Relay1/2만)
- PC-LED 상태 표시
- 설정 저장 (앱 종료 시 입력값 휘발 OK)
- Wake-on-LAN, Reboot 명령
- 외부 NuGet 패키지 (Newtonsoft.Json 등) 사용 — 내장 라이브러리만으로 구현

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | Start 버튼 클릭 시 IP/Port/ID/PW로 장치 연결 시도 | High | Pending |
| FR-02 | 연결 성공 시 1초 주기로 `/api/status` 폴링 시작 | High | Pending |
| FR-03 | 연결 실패 / 인증 실패 시 textLog에 에러 표시 + 버튼 비활성 유지 | High | Pending |
| FR-04 | 정상 응답 수신 시 Relay1/2 ON/OFF/Pulse 버튼 활성화 | High | Pending |
| FR-05 | Relay 버튼 클릭 시 `POST /api/relay` 호출 (state/pulse) | High | Pending |
| FR-06 | 응답 상태(relay1, relay2)에 따라 ON 버튼 배경색 변경 (ON=Red, OFF=Control) | High | Pending |
| FR-07 | 송수신 모든 HTTP 통신을 textLog에 시각 + 요청/응답 형태로 기록 | Medium | Pending |
| FR-08 | Start 버튼 토글 동작(Start → Stop, Stop 시 폴링 중지 + 버튼 비활성) | Medium | Pending |
| FR-09 | 기본값 자동 채움 (IP=192.168.1.200, Port=5050, ID=admin, PW=12345) | Low | Pending |
| FR-10 | Pulse 버튼은 toggle 상태 무관(이벤트형) — 색상 변경 대상 아님 | Low | Pending |

### 3.2 Non-Functional Requirements

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-01 | .NET Framework 4.8 단독 빌드 가능 (외부 NuGet 0개) | 필수 |
| NFR-02 | UI 응답성 — HTTP 호출은 비동기(async/await) 또는 백그라운드 처리로 UI 블로킹 없음 | 필수 |
| NFR-03 | 폴링 주기 1초, HTTP 타임아웃 3초 | 권장 |
| NFR-04 | Log 영역 자동 스크롤 + 최대 1000줄 유지 | 권장 |
| NFR-05 | 단일 파일(Form1.cs) 내 구현 — 학습용 샘플의 가독성 우선 | 필수 |

---

## 4. Solution Approach

### 4.1 Architecture

```
[Form1 UI]
   │
   ├── btnConn_Click  ──► HttpStateClient.StartAsync(ip, port, id, pw)
   │                            │
   │                            └── Timer(1s) ──► GET /api/status ──► UpdateUI(relay1, relay2)
   │
   ├── btnRelay1On_Click ──► POST /api/relay {"relay":1,"state":"on"}
   ├── btnRelay1Off_Click ──► POST /api/relay {"relay":1,"state":"off"}
   ├── btnRelay1Pulse_Click ──► POST /api/relay {"cmd":"pulse","relay":1}
   ├── (Relay2 동일)
   │
   └── Log(string msg)  ──► textLog.AppendText(timestamp + msg)
```

### 4.2 Key Technical Decisions

| 항목 | 결정 | 이유 |
|------|------|------|
| JSON 라이브러리 | `JavaScriptSerializer` (System.Web.Extensions) | .NET Framework 내장, NuGet 의존성 0 |
| HTTP 클라이언트 | `HttpClient` (정적 인스턴스) | TIME_WAIT 누적 방지, async/await 지원 |
| 폴링 | `System.Windows.Forms.Timer` + async handler | UI 스레드에서 안전, Invoke 불필요 |
| Basic Auth | `Authorization: Basic base64(id:pw)` 헤더 | 표준 방식, 매 요청마다 부착 |
| 상태 → 색상 매핑 | `bool on ? Color.Red : SystemColors.Control` | 시각 직관성, OS 테마 호환 |

### 4.3 API 매핑

```
[Start 클릭]
  → GET http://{ip}:{port}/api/status
     Headers: Authorization: Basic base64(admin:12345)
     Response: {"id":"node_1","relay1":0,"relay2":0,"pc_on":false,...}

[Relay1 ON]
  → POST http://{ip}:{port}/api/relay
     Body: {"relay":1,"state":"on"}
     Response: {"ok":true,"relay1":1,"relay2":0}

[Relay1 Pulse]
  → POST http://{ip}:{port}/api/relay
     Body: {"cmd":"pulse","relay":1}
     Response: {"ok":true}
```

---

## 5. Implementation Plan

| Phase | 작업 | 산출물 |
|-------|------|--------|
| 1 | csproj에 `System.Web.Extensions` 참조 추가 | REST-api.csproj |
| 2 | Form1.cs: 필드(httpClient, pollTimer, isConnected) + 기본값 초기화 | Form1.cs |
| 3 | Designer에 이벤트 핸들러 연결 (btnConn_Click, btnRelay*_Click) | Form1.Designer.cs |
| 4 | Start/Stop 토글 + HttpClient 준비 + Basic Auth 헤더 | Form1.cs |
| 5 | Polling Timer + GET /api/status + UpdateUI | Form1.cs |
| 6 | Relay 버튼 핸들러 (POST /api/relay) | Form1.cs |
| 7 | Log 헬퍼 + 자동 스크롤 + 최대 라인 유지 | Form1.cs |
| 8 | Form1.Text 변경("RemoteDeck HTTP Sample"), 빌드 확인 | bin/Debug/REST-api.exe |

---

## 6. Risks & Mitigations

| 위험 | 영향 | 완화 방안 |
|------|------|-----------|
| HTTP 타임아웃으로 UI 멈춤 | UX 저하 | `HttpClient.Timeout = 3s` + `async/await` 전면 적용 |
| 폴링 중 응답 지연 누적 | 명령 응답 지연 | 폴링과 명령은 동일 HttpClient 공유, 응답 대기는 비동기 |
| JavaScriptSerializer deprecated 경고 | 컴파일 경고 | `#pragma warning disable 618` 또는 무시 (학습 샘플 우선) |
| 인증 실패 시 405/401 | 사용자 혼란 | StatusCode별 메시지 + textLog 기록 |
| 폴링 중 장치 재부팅 | 연속 에러 로그 | 3회 연속 실패 시 자동 Stop + 안내 메시지 |

---

## 7. Acceptance Criteria

- [ ] `dotnet build` 또는 Visual Studio에서 .NET Framework 4.8 단독 빌드 성공 (외부 NuGet 0개)
- [ ] 기본값(192.168.1.200:5050 / admin:12345) 자동 채움
- [ ] Start 클릭 시 실제 RemoteDeck_PC 장치와 통신, textLog에 GET/POST 송수신 표시
- [ ] Relay1 ON 클릭 → 장치 릴레이 동작 + ON 버튼 배경이 Red로 변경
- [ ] Relay1 OFF 클릭 → 장치 릴레이 OFF + ON 버튼 배경이 Control(기본)로 복귀
- [ ] Relay2도 동일 동작
- [ ] Pulse 버튼 클릭 시 500ms 펄스 → 릴레이 잠시 ON 후 OFF (색상은 폴링으로 자동 추적)
- [ ] 잘못된 IP/PW 입력 시 textLog에 에러 표시 + 버튼 비활성 유지
- [ ] Start → Stop 토글로 폴링 중단

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2026-05-11 | Initial plan | KDI |
