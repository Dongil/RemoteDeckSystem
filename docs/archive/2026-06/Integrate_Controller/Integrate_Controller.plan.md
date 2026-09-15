---
template: plan
version: 1.3
feature: Integrate_Controller
date: 2026-06-30
author: KDI
project: RemoteDeckSystem
status: Draft
---

# Integrate_Controller Planning Document

> **Summary**: RemoteDeck_PC 펌웨어 다수 단말을 한 화면에서 통합 모니터링/제어하는 .NET 8 WinForms 데스크톱 앱. REST API 기반.
>
> **Project**: RemoteDeckSystem
> **Version**: v2.3.x (RemoteDeck_PC firmware)
> **Author**: KDI
> **Date**: 2026-06-30
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | 현장에 배포된 다수의 RemoteDeck_PC 단말 상태(전원 LED·GPIO·연결)를 일일이 웹 UI에 접속해 확인·재부팅해야 해서 운영 부담이 큼. |
| **Solution** | `apitestutility_v2/integrate_controller/` 에 .NET 8 WinForms 통합 컨트롤러를 추가, 등록된 단말 목록을 한 화면에서 REST 폴링으로 모니터링하고 재부팅 명령 전송. |
| **Function/UX Effect** | 단말 N대 등록(IP/ID/PW) → 1초~5초 주기 폴링 → Online/PC LED/GPIO/Uptime 컬럼 표시 → 우클릭/버튼으로 재부팅. 순서 변경·추가·삭제·암호화 저장. |
| **Core Value** | 다수 단말 운영을 1인이 단일 PC에서 수행 가능. 현장 출동·웹 UI 개별 접속 불필요. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 단말 다수화에 따른 개별 웹UI 접속 운영 부담 제거 |
| **WHO** | 사내 RemoteDeck 운영자 (xenoglobal kdi 외 현장 인력) |
| **RISK** | (1) 평문 PW 저장 시 유출 (2) 단말 수 증가 시 폴링 부하 (3) DPAPI 등 암호화 키 PC 종속성 |
| **SUCCESS** | N=10대 동시 폴링 정상, reboot 명령 RTT < 3s, 단말 추가/삭제/순서변경 UX 클릭 ≤ 3회 |
| **SCOPE** | M1 데이터모델/저장소 → M2 REST 클라이언트/폴러 → M3 WinForms UI → M4 reboot/에러 처리 |

---

## 1. Overview

### 1.1 Purpose

RemoteDeck_PC 펌웨어가 노출하는 REST API (`/api/status`, `/api/reboot`, Basic Auth)를 활용해, **여러 단말을 한 화면에서 모니터링·재부팅**하는 Windows 데스크톱 운영 도구를 만든다.

### 1.2 Background

- 펌웨어 분기 정리 완료: RemoteDeck_PC는 **REST + MQTT 하이브리드** 유지(v2.3 계열 운영). RemoteDeck_Touch v2.5 sunset 사이클은 종료.
- 기존 단일 단말 테스트 도구 (`apitestutility_v2/RemoteDeckTest/`)와 `IPSetupTool/` 은 **.NET 8 WinForms / win-x64 self-contained 단일 exe** 패턴.
- 운영 자산이 사내 다수로 늘어남에 따라 **N대 동시 관리 UI** 필요.

### 1.3 Related Documents

- 펌웨어 REST 사양: `RemoteDeck_PC/src/web/WebServer.cpp` (Basic Auth, `/api/status` `/api/reboot`)
- 상태 JSON 스펙: `RemoteDeck_PC/src/main.cpp` `buildStatusJson()`
- 기술스택 참조: `apitestutility_v2/RemoteDeckTest/RemoteDeckTest/RemoteDeckTest.csproj`, `IPSetupTool/IPSetupTool/IPSetupTool.csproj`

---

## 2. Scope

### 2.1 In Scope

- [ ] `apitestutility_v2/integrate_controller/` 폴더에 신규 .NET 8 WinForms 솔루션 생성
- [ ] 단말 등록 UI: IP / Device ID(라벨) / Auth User / Auth Password
- [ ] 단말 목록: 추가 / 삭제 / 위/아래 순서 변경
- [ ] 단말 자격증명 로컬 저장 (DPAPI 암호화, JSON)
- [ ] 주기 폴링 (기본 3s, UI 조정 가능) — `GET /api/status` Basic Auth
- [ ] 상태 컬럼 표시:
  - 연결 상태 (Online/Offline, 마지막 응답 시각)
  - PC 전원 LED (`pc_on`)
  - GPIO 핀 3개 (`gpio[0..2]`)
  - IP, 펌웨어 버전 (`fw_ver`), Uptime, MQTT 연결 여부
- [ ] 재부팅 명령: `POST /api/reboot` (확인 다이얼로그 + 결과 토스트)
- [ ] 오프라인 단말 자동 재시도, 응답 타임아웃 별도 설정

### 2.2 Out of Scope

- 펌웨어 측 신규 API 추가/수정 (기존 v2.3 REST 사양만 사용)
- 릴레이/스케줄/WOL 등 RemoteDeck 전체 제어 기능 (별도 차기 사이클)
- 다중 사용자/권한, 클라우드 동기화
- macOS/Linux 지원 (Windows 10/11 win-x64 only)
- MQTT 직접 구독 (REST 폴링만)
- RemoteDeck_Touch (v2.5 sunset) 단말 지원

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | 단말 등록 폼: IP/Label/User/Password 입력 후 추가 | High | Pending |
| FR-02 | 등록 단말 목록 UI: 표/리스트로 표시 (DataGridView) | High | Pending |
| FR-03 | 단말 삭제 (확인 다이얼로그) | High | Pending |
| FR-04 | 단말 순서 변경 (위/아래 버튼 또는 드래그) | Medium | Pending |
| FR-05 | 자격증명 영구 저장: `%LOCALAPPDATA%\IntegrateController\devices.json` (PW는 DPAPI ProtectedData) | High | Pending |
| FR-06 | 주기 폴링 `GET /api/status` (Basic Auth, 기본 3s, 1~30s 조정) | High | Pending |
| FR-07 | 컬럼 표시: Online/마지막응답/pc_on/gpio[]/ip/fw_ver/uptime/mqtt_connected | High | Pending |
| FR-08 | 오프라인 판정 = 연속 3회 timeout / 비-200 응답 | High | Pending |
| FR-09 | 재부팅 명령 `POST /api/reboot` + 확인 다이얼로그 + 결과 표시 | High | Pending |
| FR-10 | 단말 행 우클릭 컨텍스트 메뉴 (재부팅/편집/삭제) | Medium | Pending |
| FR-11 | 행 선택 시 상세 패널 (전체 status JSON raw view) | Low | Pending |
| FR-12 | 종료 시 자동 저장, 시작 시 자동 로드 | High | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| Performance | N=10 단말 동시 폴링 3s 주기, UI freeze 없음 | Stopwatch + async/await |
| Performance | reboot RTT (요청→200 응답) < 3s on LAN | 수동 측정 |
| Security | PW 평문 저장 금지 — Windows DPAPI CurrentUser scope | `ProtectedData.Protect` |
| Reliability | 단말 오프라인이 다른 단말 폴링을 막지 않음 | 단말별 독립 Task |
| Compatibility | Windows 10/11 x64, .NET 8 self-contained single-file exe | `dotnet publish -c Release` |
| Usability | 신규 단말 등록 ≤ 3 클릭 (Add → 폼 → OK) | UX 점검 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-12 모두 구현 + 수동 시연 완료
- [ ] 실제 RemoteDeck_PC 단말 1대 이상 등록·폴링·재부팅 성공
- [ ] N=10 시뮬레이션 단말 (또는 로컬 mock) 동시 폴링 안정
- [ ] 단일 exe `publish` 성공 (RuntimeIdentifier=win-x64, SelfContained=true, PublishSingleFile=true)
- [ ] `devices.json` 재실행 후 정상 복원

### 4.2 Quality Criteria

- [ ] 빌드 경고 0 (warning as error 지향)
- [ ] DataGridView UI 깜빡임 없음 (DoubleBuffered)
- [ ] `await`/`HttpClient` Singleton 패턴 — 소켓 누수 없음
- [ ] PW가 devices.json 평문에 나타나지 않음 (base64 ProtectedData)

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| DPAPI는 사용자/PC 종속 → 다른 PC로 옮기면 복호화 실패 | Medium | High | 사양으로 명시(설계 의도). 내보내기 기능은 차기 사이클. |
| 단말 수 ↑ 시 폴링 부하 (펌웨어 CPU) | Medium | Medium | 단말별 폴링 주기 조정 가능. 기본 3s. |
| Basic Auth 평문 (HTTP) → 사내망 한정 | Medium | High | 사양 명시. HTTPS는 펌웨어 한계로 차기 과제. |
| WinForms 폴링 중 UI 블록 | High | Low | `HttpClient` async, `IProgress<T>` 또는 `BeginInvoke` 마샬링. |
| `HttpClient` per-device 인스턴스화 시 소켓 고갈 | High | Medium | 정적 Singleton `HttpClient` + `Authorization` 헤더는 요청 단위 부착. |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `apitestutility_v2/integrate_controller/` | 신규 폴더 | .NET 8 WinForms 솔루션 신규 추가 |
| RemoteDeck_PC 펌웨어 REST API | 외부 의존 (변경 없음) | `GET /api/status`, `POST /api/reboot` 소비 |
| `%LOCALAPPDATA%\IntegrateController\` | 신규 파일 | `devices.json` 생성 (사용자 단위) |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| `/api/status` | READ | (기존) www/ 정적 웹 UI, 차기 controller | None — read-only |
| `/api/reboot` | WRITE | (기존) www/ 정적 웹 UI | None — 동일 의미 |
| `devices.json` | CREATE/READ | (신규) controller 단독 | None |

### 6.3 Verification

- [x] 펌웨어 REST 사양 변경 없음 — 기존 웹 UI와 공존
- [x] 인증/권한 변화 없음 — admin/12345 default + 사용자 변경 가능
- [x] 추가/제거 필드 없음

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

| Level | Characteristics | Recommended For | Selected |
|-------|-----------------|-----------------|:--------:|
| **Starter** | 단일 폴더 구조 | 단일 화면 도구 | ☐ |
| **Dynamic** | Feature 모듈화 | 본 프로젝트의 유사 도구 패턴 | ☑ |
| **Enterprise** | 엄격한 레이어 분리 | 대규모 시스템 | ☐ |

**선택 사유**: `IPSetupTool` / `RemoteDeckTest` 와 동일한 단일 WinForms 프로젝트 패턴. 단, 내부적으로 Models/Services/UI 폴더 분리.

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| Framework | .NET 8 WinForms / WPF / MAUI | **.NET 8 WinForms** | 기존 `RemoteDeckTest`/`IPSetupTool` 와 동일 스택 |
| Target | win-x64 self-contained | **win-x64 SCD single-file** | 운영 PC에 .NET 런타임 설치 불요 |
| Storage | JSON file (DPAPI) / SQLite / Registry | **JSON + DPAPI** | 단순, 백업/이식 용이 |
| HTTP Client | HttpClient (Singleton) / RestSharp | **HttpClient Singleton** | 외부 의존 최소화 |
| JSON | System.Text.Json / Newtonsoft | **System.Text.Json** | .NET 8 기본, 의존성 0 |
| Polling | Per-device `Task` + `PeriodicTimer` | **PeriodicTimer + CancellationToken** | .NET 8 표준, 안정적 |
| UI | DataGridView | **DataGridView (DoubleBuffered)** | WinForms 표준, 가벼움 |

### 7.3 Folder Structure Preview

```
apitestutility_v2/integrate_controller/
└── IntegrateController/
    ├── IntegrateController.sln
    └── IntegrateController/
        ├── IntegrateController.csproj   # net8.0-windows, WinExe, SingleFile, SCD
        ├── Program.cs
        ├── app.manifest
        │
        ├── Models/
        │   ├── DeviceEntry.cs           # IP/Label/User/Password(encrypted)
        │   ├── DeviceStatus.cs          # parsed /api/status payload
        │   └── DeviceList.cs            # ordered list, JSON serializable
        │
        ├── Services/
        │   ├── DeviceStore.cs           # devices.json load/save (DPAPI)
        │   ├── RemoteDeckClient.cs      # HttpClient wrapper (status/reboot)
        │   └── DevicePoller.cs          # PeriodicTimer per device
        │
        └── UI/
            ├── MainForm.cs(.Designer.cs) # DataGridView + toolbar
            ├── DeviceEditDialog.cs       # add/edit form
            └── StatusFormatter.cs        # column rendering helpers
```

---

## 8. Convention Prerequisites

### 8.1 Existing Project Conventions

- [x] `apitestutility_v2/RemoteDeckTest/` — .NET 8 WinForms 레퍼런스 존재
- [x] `IPSetupTool/` — 단일 exe publish 설정 레퍼런스 존재
- [ ] `.editorconfig` — 차기 작업 시 검토
- [x] CLAUDE.md 한국어 + 영어 혼용 허용

### 8.2 Conventions to Define/Verify

| Category | Current State | To Define | Priority |
|----------|---------------|-----------|:--------:|
| Project naming | (신규) | `IntegrateController` (PascalCase) | High |
| Folder layout | (신규) | Models / Services / UI 3폴더 | High |
| Async style | exists (RemoteDeckTest) | `async Task` + `ConfigureAwait(false)` in services | High |
| Logging | none | `System.Diagnostics.Trace` + Debug 윈도우, MVP 단계 file log 미사용 | Low |

### 8.3 Environment Variables Needed

| Variable | Purpose | Scope | To Be Created |
|----------|---------|-------|:-------------:|
| (none) | 모두 GUI 입력 → DPAPI 저장 | - | ☐ |

### 8.4 Pipeline Integration

본 사이클은 사내 단일 desktop tool — Development Pipeline 9-Phase 통합 대상 아님. 단일 PDCA 사이클 진행.

---

## 9. Next Steps

1. [ ] `/pdca design Integrate_Controller` — 3가지 아키텍처 옵션 비교 + 선정
2. [ ] Design Anchor 불필요 (Pencil MCP 미사용, WinForms 표준)
3. [ ] `/pdca do Integrate_Controller --scope module-1` (Models/Storage부터) — 세션 분할 구현

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-30 | Initial draft (Checkpoint 1·2 답변 반영) | KDI |
