---
template: design
version: 1.0
feature: ic-hub-v3.0
date: 2026-11-10
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
consumes: RemoteDeck_PC v3.0.0 /api/status hub_*
architecture: Option C (Pragmatic Balance)
status: draft
---

# IntegrateController RCM 허브 컬럼 (v3.0.0) Design Document

> **Summary**: 재부재 컬럼(ColAttendance)을 그대로 두고, 툴바 선택(재부재 / RCM 허브)에 따라 제목과 셀 값을 바꾼다. 설정은 devices.json에 저장한다.
>
> **Planning Doc**: [ic-hub-v3.0.plan.md](../../01-plan/features/ic-hub-v3.0.plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 허브 현장에서 장비별 허브 연결·정지 상태를 IC 그리드에서 한눈에 봐야 함 (현재는 장비 웹 화면을 하나씩 열어야 함) |
| **WHO** | 허브 현장 관제자(주 사용자), 기존 재부재 현장 관제자(회귀 없어야 함), 혼재 펌웨어(v2.x/v3.0) 장비 |
| **RISK** | (1) WinForms Designer가 컬럼을 날리는 문제 → 신규 컨트롤은 코드에서 생성, 셀은 `column.Index`로 접근. (2) v2.x 장비의 hub 필드 부재 시 오표시 → "미지원". (3) devices.json 스키마 변경으로 기존 파일 로드 실패 → 필드 추가만, 없으면 기본값 |
| **SUCCESS** | `dotnet build` 0 경고/0 오류 · RCM 허브 모드에서 v3.0 장비 상태 정확 표시(연결/정지/미사용) · v2.x 장비 "미지원" · 재부재 모드 기존과 동일 · 옵션 재시작 후 유지 |
| **SCOPE** | 모델·파싱(DeviceStatus, RemoteDeckClient) / 표시(StatusFormatter, 컬럼 전환) / 옵션 저장(DeviceStore) / 상세 패널 요약 |

---

## 1. Overview

### 1.1 Design Goals

- 컬럼 수를 늘리지 않고 같은 자리에서 재부재 ↔ RCM 허브 전환
- 기존 재부재 현장의 화면·동작 무변화(기본값 재부재, 재부재 표시 로직 그대로)
- v2.x 장비 혼재에도 예외·오표시 없음

### 1.2 Design Principles

- 기존 패턴 재사용: v2.6.2 재부재(미니 필드 하위호환 파싱 → StatusFormatter → 셀 세팅 2곳)
- WinForms 3중 방어선: 새 컨트롤은 MainForm.cs에서 코드 생성, 셀은 `ColAttendance.Index`로만 접근, Designer 미수정
- UI 표기는 "RCM 허브" (사용자 지시)

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | A: Minimal | B: Clean | C: Pragmatic |
|----------|:-:|:-:|:-:|
| 접근 | MainForm 필드 + 별도 settings.json | ViewSettings/SettingsStore + IFeatureColumn 전략 | enum + StoreDto 필드 + MainForm 분기 함수 |
| New Files | 1 | 4 | 0 |
| Modified Files | 4 | 5 | 5 |
| Complexity | Low | High | Low |
| Maintainability | Medium (설정 파일 2개) | High | High |
| Risk | Low | Medium | Low |
| Recommendation | | | **Selected** |

**Selected**: Option C — **Rationale**: 설정이 한 파일(devices.json)에 모이고, 기존 재부재 패턴과 같은 모양이라 리뷰·유지보수가 쉽다. 컬럼 종류가 2개뿐이라 전략 클래스는 과함.

### 2.1 Component Diagram

```
RemoteDeck_PC /api/status ──(DevicePoller 5s)──▶ RemoteDeckClient.ParseStatus
                                                     │  hub_* → DeviceStatus.Hub*
                                                     ▼
devices.json ◀─▶ DeviceStore (FeatureColumn)   MainForm
                                                 ├─ cboFeatureColumn (툴바, 코드 생성)
                                                 ├─ ApplyFeatureColumnHeader()
                                                 ├─ SetFeatureCell(row, status)  ─▶ StatusFormatter.FormatHub/HubColor
                                                 │                                └▶ StatusFormatter.FormatAttendance (기존)
                                                 └─ RefreshDetailPanel() + "RCM 허브:" 요약 줄
```

### 2.2 Data Flow

폴링 결과 → `ParseStatus`(hub_* 채움) → `_statusById` → `ApplyStatusToRow` → `SetFeatureCell`(모드에 따라 재부재 또는 RCM 허브 값·색) / 툴바 변경 → `_featureMode` 갱신 → 제목 변경 + `RefreshAllUnboundCells()` + `RefreshDetailPanel()` + 저장

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| MainForm | DeviceStore.FeatureColumn | 시작 시 모드 복원, 변경 시 저장 |
| SetFeatureCell | StatusFormatter | 값·색 계산 |
| ParseStatus | 펌웨어 `/api/status` v3.0 계약 | hub_* 6필드 |

---

## 3. Data Model

### 3.1 Entity Definition

```csharp
// Models/FeatureColumnMode.cs 가 아니라 DeviceStatus.cs 하단에 함께 둔다 (새 파일 없음)
public enum FeatureColumnMode { Attendance, Hub }

// DeviceStatus 추가 필드 (v3.0.0)
public bool   HubSupported  { get; set; }        // hub_enabled 키 존재 = v3.0 이상
public bool   HubEnabled    { get; set; }
public bool   HubConnected  { get; set; }
public bool   HubStopped    { get; set; }
public string HubStopReason { get; set; } = "";  // "4001 auth failed" | "config incomplete" | ""
public string HubHost       { get; set; } = "";  // "192.168.10.84:8891"
public string HubProductId  { get; set; } = "";
```

### 3.2 devices.json (StoreDto)

```json
{
  "Version": 1,
  "FeatureColumn": "Attendance",
  "Devices": [ ... 기존 그대로 ... ]
}
```

- `FeatureColumn`: `"Attendance"`(기본) | `"Hub"`. 없거나 모르는 값이면 Attendance.
- `Version`은 올리지 않는다(구버전 IC는 모르는 필드를 무시하므로 같은 파일을 계속 읽음).

### 3.3 DeviceStore API

```csharp
public FeatureColumnMode FeatureColumn { get; set; } = FeatureColumnMode.Attendance;
public DeviceList Load();                 // 기존 시그니처 유지, FeatureColumn 프로퍼티도 채움
public void Save(DeviceList devices);     // 기존 시그니처 유지, FeatureColumn 프로퍼티도 기록
```

시그니처를 바꾸지 않아 호출부 3곳(`MainForm.cs:82,117,495`)은 수정 불필요. 모드 변경 시 `_store.FeatureColumn = mode; _store.Save(_devices);`.

---

## 4. API Specification (소비하는 펌웨어 계약)

### 4.1 `GET /api/status` hub_* (RemoteDeck_PC v3.0.0)

| 필드 | 타입 | 비고 |
|------|------|------|
| `hub_enabled` | bool | 존재 여부로 v3.0 판별 (`HubSupported`) |
| `hub_connected` | bool | |
| `hub_stopped` | bool | 4001/4009/4010으로 재접속 중단 |
| `hub_stop_reason` | string | `"<code> <reason>"`, `"config incomplete"`, `""` |
| `hub_host` | string | `"host:port"`, 미사용 시 `""` |
| `hub_product_id` | string | 미사용 시 `""` |

파싱 규칙: 키별 `TryGetProperty` + `ValueKind` 확인(True/False/String). 타입이 다르면 그 키만 기본값. 전체 파싱 실패 시 기존과 같이 `LastError="parse: ..."`.

### 4.2 표시 규칙 (FormatHub / HubColor)

| 조건 (위에서부터 먼저 맞는 것) | 셀 텍스트 | 글자색 |
|------|------|------|
| status == null 또는 `!Online` | `-` | 기본 |
| `!HubSupported` | `미지원` | Gray |
| `!HubEnabled` | `미사용` | Gray |
| `HubConnected` | `● 연결됨` | ForestGreen |
| `HubStopped` | `● 정지` | Firebrick |
| `HubStopReason == "config incomplete"` | `● 설정 불완전` | Firebrick |
| 그 외 | `● 연결 중` | DarkOrange |

재부재 모드는 기존 `FormatAttendance` 그대로(오프라인 시 표시도 기존과 동일, 회귀 방지). 재부재 모드로 돌아갈 때 셀 글자색을 기본으로 되돌린다.

### 4.3 정지 사유 한글 (상세 패널)

| code | 문구 |
|------|------|
| 4001 | 장비 키 또는 제품번호 오류 |
| 4009 | 같은 제품번호 장비가 이미 접속 중 |
| 4010 | 버전 불일치 |
| config incomplete | 설정 불완전(허브 주소·제품번호·장비 키) |

장비 웹 UI(`app.js` HUB_STOP_TEXT)와 같은 문구.

---

## 5. UI/UX Design

### 5.1 Screen Layout

```
[+ Add] [Edit] [Delete] | [▲] [▼] | [Reboot]                  표시 컬럼: [재부재    ▾]
┌──┬──┬────────┬────────┬─────────────┬──────────┬────┬─────┬──────┬───────┬─────────┐
│No│상태│이름    │기기ID  │IP           │RCM 허브   │PC  │GPIO │FW    │Uptime │LastSeen │
├──┼──┼────────┼────────┼─────────────┼──────────┼────┼─────┼──────┼───────┼─────────┤
│1 │● │새기기  │node_2  │192.168.10.109│● 연결됨  │OFF │010  │3.0.0 │2m10s  │11:40:02 │
│2 │● │새기기  │node_1  │192.168.10.193│● 정지    │ON  │010  │3.0.0 │...    │         │
│3 │● │...     │...     │192.168.10.50 │미지원    │ON  │...  │2.6.2 │       │         │
└──┴──┴────────┴────────┴─────────────┴──────────┴────┴─────┴──────┴───────┴─────────┘
상세: ...
      RCM 허브: ● 정지 — 장비 키 또는 제품번호 오류 (4001)  주소 192.168.10.84:8891  제품번호 RDTEST02
```

### 5.2 User Flow

툴바 선택 변경 → 컬럼 제목이 "재부재"↔"RCM 허브"로 바뀌고 모든 행이 즉시 다시 그려짐 → 설정 저장 → 다음 실행에도 유지.

### 5.3 Component List

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `ToolStripLabel` "표시 컬럼:" + `ToolStripComboBox cboFeatureColumn` | MainForm.cs `AddFeatureColumnSelector()` (코드 생성, `Alignment=Right`, DropDownList) | 모드 선택 |
| `ApplyFeatureColumnHeader()` | MainForm.cs | 제목 "재부재" / "RCM 허브", ToolTip |
| `SetFeatureCell(row, status)` | MainForm.cs | 모드별 값·색 (셀 세팅 2곳에서 호출) |
| `FormatHub`, `HubColor`, `HubStopText` | StatusFormatter.cs | 표시 규칙 §4.2/§4.3 |

### 5.4 Page UI Checklist

#### MainForm
- [ ] 툴바 오른쪽 "표시 컬럼:" 라벨 + 선택(재부재 / RCM 허브), 편집 불가(DropDownList)
- [ ] 시작 시 저장된 모드 선택 상태 + 컬럼 제목 일치
- [ ] 모드 전환 즉시 제목·모든 행 값 갱신
- [ ] RCM 허브 모드 셀 7상태(§4.2) + 색, 선택 행에서도 색 유지(SelectionForeColor)
- [ ] 재부재 모드로 복귀 시 글자색 기본, 값은 기존 FormatAttendance
- [ ] 상세 패널: v3.0 장비면 "RCM 허브:" 요약 1줄(상태, 사유 한글, 주소, 제품번호). v2.x면 "RCM 허브: 미지원(펌웨어 v3.0 미만)"
- [ ] 어떤 사용자 표시 문자열에도 단독 "허브" 대신 "RCM 허브" 사용(셀 상태값 "● 연결됨" 등은 컬럼 제목이 RCM 허브이므로 그대로)

---

## 6. Error Handling

| 상황 | 처리 |
|------|------|
| hub_* 키 없음(v2.x) | `HubSupported=false` → "미지원" |
| hub_* 타입 불일치 | 해당 키만 기본값, 예외 없음 |
| devices.json `FeatureColumn` 없음/잘못된 값 | Attendance |
| 저장 실패 | 기존과 동일하게 무시(종료 시 저장에서 메시지) — 모드 변경 시 저장 실패는 상태바 메시지 |

---

## 7. Security Considerations

- 새 네트워크 요청 없음, device_key 등 비밀값 미취급(펌웨어가 status에 넣지 않음)
- devices.json DPAPI 비밀번호 저장 방식 변경 없음

---

## 8. Test Plan

| # | 시험 | 기대 |
|---|------|------|
| T-01 | `dotnet build -c Debug` | 0 경고 / 0 오류 |
| T-02 | 기존 devices.json(FeatureColumn 없음)으로 실행 | 재부재 모드, 장비·비밀번호 유지 |
| T-03 | RCM 허브 모드: `.109`, `.193`(가상 허브 연결) | `● 연결됨` 초록 |
| T-04 | 가상 허브 중지 | 다음 폴링에 `● 연결 중` 주황 |
| T-05 | hub off 장비(설정 저장 후 재부팅) | `미사용` 회색 |
| T-06 | hub_* 없는 응답(v2.x 장비 또는 목 서버) | `미지원` 회색, 예외 없음 |
| T-07 | 잘못된 키로 접속(4001) | `● 정지` 빨강, 상세 패널 "장비 키 또는 제품번호 오류 (4001)" |
| T-08 | 오프라인 장비 | `-` |
| T-09 | 모드 전환 → 재시작 | 선택 유지, devices.json에 `"FeatureColumn": "Hub"` |
| T-10 | 재부재 모드 | v2.6.2와 동일 표시(회귀 0) |
| T-11 | 선택 행 | 허브 셀 색 유지 |
| T-12 | `publish.bat` | 단일 exe 생성 |

---

## 9. Layer Assignment

| Component | Layer | Location |
|-----------|-------|----------|
| FeatureColumnMode, DeviceStatus.Hub* | Model | `Models/DeviceStatus.cs` |
| ParseStatus hub_* | Service | `Services/RemoteDeckClient.cs` |
| FeatureColumn 저장 | Service | `Services/DeviceStore.cs` |
| FormatHub/HubColor/HubStopText | UI helper | `UI/StatusFormatter.cs` |
| 툴바 선택·전환·셀 분기·상세 패널 | UI | `UI/MainForm.cs` |

---

## 10. Coding Convention Reference

- 주석 태그: `// v3.0.0: RemoteDeck_PC v3.0.0 /api/status hub_* (ic-hub-v3.0)`
- Design 참조 주석: `// Design Ref: ic-hub-v3.0 §4.2 — ...`
- `MainForm.Designer.cs`는 수정하지 않는다
- 사용자 표시 문자열 "RCM 허브"

---

## 11. Implementation Guide

### 11.1 File Structure (수정만, 새 파일 없음)

```
IntegrateController/
├── Models/DeviceStatus.cs        (+ FeatureColumnMode enum, Hub* 7 필드)
├── Services/RemoteDeckClient.cs  (+ hub_* 파싱)
├── Services/DeviceStore.cs       (+ FeatureColumn 프로퍼티, StoreDto 필드)
├── UI/StatusFormatter.cs         (+ FormatHub, HubColor, HubStopText)
└── UI/MainForm.cs                (+ 툴바 선택, 헤더/셀 분기, 상세 패널)
```

### 11.2 Implementation Order

1. [ ] DeviceStatus 필드 + enum
2. [ ] ParseStatus hub_* (타입 확인 헬퍼)
3. [ ] StatusFormatter FormatHub/HubColor/HubStopText
4. [ ] DeviceStore FeatureColumn (Load/Save)
5. [ ] MainForm: AddFeatureColumnSelector, ApplyFeatureColumnHeader, SetFeatureCell, 셀 세팅 2곳 교체, 상세 패널
6. [ ] `dotnet build` 0/0 → 실기(가상 허브 + 2대) T-01~12 → `publish.bat`

### 11.3 Session Guide

| Module | Scope Key | Items | 예상 |
|--------|-----------|-------|------|
| 모델·파싱·표시 | `module-1` | 11.2 #1~3 | ~60줄 |
| 저장·UI | `module-2` | 11.2 #4~5 | ~90줄 |
| 검증 | `module-3` | 11.2 #6 | 실기 |

규모가 작아 한 세션에서 module-1~3 연속 진행 권장.

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-10 | Initial draft (Option C) | KDI |
