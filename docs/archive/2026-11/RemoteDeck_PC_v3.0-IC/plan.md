---
template: plan
version: 1.0
feature: ic-hub-v3.0
date: 2026-11-10
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
consumes: RemoteDeck_PC v3.0.0 /api/status hub_* fields
status: draft
---

# IntegrateController 허브 컬럼 (v3.0.0) Planning Document

> **Summary**: IC 그리드의 재부재 컬럼 자리를 툴바 옵션으로 "재부재 / RCM 허브" 중 하나로 전환해, v3.0 장비의 RCM 허브 연결 상태를 관제 화면에서 바로 보이게 한다.
>
> **Project**: RemoteDeckSystem
> **Version**: 3.0.0 (클라이언트 버전은 소비하는 펌웨어 계약 버전을 따름)
> **Author**: KDI
> **Date**: 2026-11-10
> **Status**: Draft
> **Origin**: rcm-hub-v3.0 report §4.1 Carry "IntegrateController hub 컬럼"

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v3.0 펌웨어가 `/api/status`에 허브 연결 상태(`hub_*`)를 내보내지만, 여러 장비를 보는 IntegrateController에서는 장비마다 웹 화면을 열어야 허브 연결 여부를 알 수 있다. |
| **Solution** | 재부재 컬럼 자리를 툴바 옵션으로 "재부재 / RCM 허브" 전환(기본 재부재, devices.json 저장). 허브 모드에서는 연결됨·연결 중·정지·설정 불완전·미사용·미지원을 색상과 함께 표시하고, 상세 패널에 주소·제품번호·정지 사유를 요약한다. |
| **Function/UX Effect** | 허브 현장 관제자는 그리드 한 화면에서 모든 장비의 허브 연결과 정지(키 오류·중복 등)를 즉시 확인한다. 재부재 현장은 기본값 그대로라 화면 변화가 없다. |
| **Core Value** | v3.0 허브 연동의 통합 관제 표면 완결(장비 홈 카드 → IC 그리드). 재부재와 허브를 같이 쓰는 현장이 없다는 운영 사실을 반영해 컬럼 수를 늘리지 않는다. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 허브 현장에서 장비별 허브 연결·정지 상태를 IC 그리드에서 한눈에 봐야 함 (현재는 장비 웹 화면을 하나씩 열어야 함) |
| **WHO** | 허브 현장 관제자(주 사용자), 기존 재부재 현장 관제자(회귀 없어야 함), 혼재 펌웨어(v2.x/v3.0) 장비 |
| **RISK** | (1) WinForms Designer가 컬럼을 날리는 문제 → 신규 컨트롤은 코드에서 생성, 셀은 `column.Index`로 접근. (2) v2.x 장비의 hub 필드 부재 시 오표시 → "미지원". (3) devices.json 스키마 변경으로 기존 파일 로드 실패 → 필드 추가만, 없으면 기본값 |
| **SUCCESS** | `dotnet build` 0 경고/0 오류 · 허브 모드에서 v3.0 장비 상태 정확 표시(연결/정지/미사용) · v2.x 장비 "미지원" · 재부재 모드 기존과 동일 · 옵션 재시작 후 유지 |
| **SCOPE** | 모델·파싱(DeviceStatus, RemoteDeckClient) / 표시(StatusFormatter, 컬럼 전환) / 옵션 저장(DeviceStore) / 상세 패널 요약 |

---

## 1. Overview

### 1.1 Purpose

IC 그리드에서 장비별 RCM 허브 연결 상태를 보여 준다. 재부재와 허브는 같은 현장에서 함께 쓰지 않으므로, 컬럼을 늘리지 않고 같은 자리를 옵션으로 전환한다.

### 1.2 Background

- RemoteDeck_PC v3.0.0(`rcm-hub-v3.0`, 2026-11-10 완료)이 `/api/status`에 `hub_enabled, hub_connected, hub_stopped, hub_stop_reason, hub_host, hub_product_id`를 추가했다.
- IC는 v2.6.2에서 재부재 컬럼을 같은 방식(미니 블록 하위호환 파싱 → StatusFormatter 렌더)으로 추가했다. 이번도 그 패턴을 따른다.
- 현장 운영: 재부재 시스템과 RCM 허브를 동시에 쓰는 현장은 없다(사용자 확인).

### 1.3 Related Documents

- 펌웨어 계약: `docs/RemoteDeck_PC_Manual.md` §5 `/api/status`, §12 RCM 허브 연동
- 선행: `docs/archive/2026-11/RemoteDeck_PC_v3.0/` (rcm-hub-v3.0)
- 패턴: `docs/archive/2026-09/RemoteDeck_PC_v2.6.2-IC/` (재부재 컬럼)

---

## 2. Scope

### 2.1 In Scope

- [ ] `DeviceStatus`에 hub 필드 6개 + "필드 존재 여부"(구버전 판별)
- [ ] `RemoteDeckClient.ParseStatus`에서 hub_* 하위호환 파싱(없으면 미지원)
- [ ] `StatusFormatter.FormatHub` + 상태별 글자색
- [ ] 툴바 "표시 컬럼: 재부재 / 허브" 선택(코드 생성), 전환 시 컬럼 제목·셀 즉시 갱신
- [ ] 선택값을 devices.json에 저장·복원(기본 재부재, 기존 파일 호환)
- [ ] 상세 패널에 허브 요약(주소, 제품번호, 상태, 정지 사유 한글)
- [ ] 버전 주석 태그 v3.0.0

### 2.2 Out of Scope

- `/api/hub` 송수신 내역 조회(사용자 결정: status 요약만)
- 허브 명령 전송(전원 제어 등) — 허브/서버 쪽 기능
- 재부재와 허브 동시 표시(운영상 불필요)
- LogPoller/로그뷰 변경

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | `DeviceStatus`: `HubSupported`(hub_enabled 필드 존재), `HubEnabled`, `HubConnected`, `HubStopped`, `HubStopReason`, `HubHost`, `HubProductId` | High | Pending |
| FR-02 | `ParseStatus`: hub_* 필드를 있으면 읽고, 없으면 `HubSupported=false`. 타입이 달라도 예외 없이 기본값 | High | Pending |
| FR-03 | `FormatHub`: 미지원(v2.x) / 미사용(enabled=false) / ● 연결됨 / ● 연결 중 / ● 정지 / ● 설정 불완전 / `-`(폴링 결과 없음·오프라인) | High | Pending |
| FR-04 | 허브 셀 글자색: 연결됨 초록, 연결 중 주황, 정지·설정 불완전 빨강, 미사용·미지원 회색. 선택 행에서도 색 유지 | Medium | Pending |
| FR-05 | 툴바 오른쪽 "표시 컬럼: [재부재 ▾]" 선택(재부재/RCM 허브). 컨트롤은 코드에서 생성(Designer 미사용) | High | Pending |
| FR-06 | 전환 시 같은 컬럼의 제목("재부재"↔"RCM 허브")과 모든 행 값을 즉시 갱신. 다음 폴링을 기다리지 않음 | High | Pending |
| FR-07 | 선택값을 devices.json에 저장, 시작 시 복원. 필드가 없는 기존 파일은 "재부재" | High | Pending |
| FR-08 | 상세 패널: 허브 지원 장비면 "RCM 허브: 상태 / 주소 / 제품번호 / 정지 사유(4001·4009·4010 한글)" 요약 줄 추가 | Medium | Pending |
| FR-09 | 오프라인 장비: 마지막 값 대신 `-` 표시(재부재 컬럼의 오프라인 처리와 동일하게) | Medium | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| 빌드 | `dotnet build` 0 경고 / 0 오류 (릴리스 게이트) | dotnet build 출력 |
| 호환성 | v2.x 장비 혼재 시 파싱 예외 0, "미지원" 표시 | 실기 v2.x 1대 또는 hub 필드 제거한 응답 |
| 호환성 | 기존 devices.json 로드·저장 후 장비 목록·DPAPI 비밀번호 유지 | 기존 파일로 실행 |
| 안정성 | 컬럼 접근은 `column.Index`만 사용(고정 인덱스 금지) | 코드 리뷰 |
| 성능 | 폴링 주기·요청 수 변화 없음 | 코드 리뷰 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-09 구현
- [ ] `dotnet build IntegrateController.sln -c Debug` 0/0
- [ ] 허브 모드: `.109`(RDTEST01)·`.193`(RDTEST02) 연결됨 표시, 가상 허브 중지 시 "연결 중"으로 바뀜
- [ ] 허브 모드: 허브 미사용 장비 "미사용", v2.x(또는 hub 필드 없는) 장비 "미지원"
- [ ] 재부재 모드: v2.6.2 때와 같은 표시(회귀 0)
- [ ] 옵션 전환 즉시 반영, 재시작 후 유지, 기존 devices.json 호환
- [ ] `publish.bat` 단일 exe 생성

### 4.2 Quality Criteria

- [ ] gap-analysis matchRate ≥ 90%
- [ ] 빌드 0 경고 / 0 오류

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Designer 재생성 시 새 컨트롤·컬럼 소실 | High | Medium | 툴바 선택 컨트롤은 MainForm.cs에서 코드로 생성. 기존 ColAttendance를 재사용(제목만 바꿈) |
| 고정 컬럼 인덱스 사용으로 IP 컬럼 런타임 삽입과 충돌 | High | Low | `ColAttendance.Index`로만 접근 |
| devices.json에 필드 추가 → 구버전 IC가 새 파일을 못 읽음 | Medium | Low | System.Text.Json은 모르는 필드 무시. Version 값은 올리지 않음 |
| hub_stop_reason 자유 문자열로 셀 폭 초과 | Low | Medium | 셀에는 상태만, 사유는 상세 패널(사용자 결정) |
| v3.0 장비가 정지 상태에서 오프라인과 혼동 | Medium | Low | 오프라인은 `-`, 정지는 빨간 "● 정지"로 구분 |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `Models/DeviceStatus.cs` | Model | hub 필드 7개 추가 |
| `Services/RemoteDeckClient.cs` | Parser | hub_* 하위호환 파싱 |
| `UI/StatusFormatter.cs` | Formatter | FormatHub, HubColor |
| `UI/MainForm.cs` | UI | 툴바 선택 컨트롤(코드 생성), 컬럼 모드 전환, 셀 세팅 2곳, 상세 패널 |
| `Services/DeviceStore.cs` | Persistence | StoreDto에 표시 컬럼 설정 필드, Load/Save 시그니처 확장 |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| DeviceStore.Load/Save | READ/WRITE | MainForm 시작·장비 편집·순서 변경 | 시그니처 변경 시 호출부 모두 수정(Needs verification) |
| ColAttendance | READ | `RefreshAllUnboundCells`, `ApplyStatusToRow` | 모드별 값 분기 |
| DeviceStatus | READ | MainForm, DevicePoller, 상세 패널 | 필드 추가만(None) |

### 6.3 Verification

- [ ] DeviceStore 호출부 전수 확인
- [ ] 기존 devices.json으로 실행 → 장비·비밀번호 유지

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

기존 WinForms 단일 프로젝트(.NET 8, net8.0-windows). 새 프로젝트·패키지 없음.

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| 컬럼 구성 | 컬럼 추가 / 재부재 자리 전환 | 재부재 자리 전환 | 동시 사용 현장 없음(사용자) |
| 전환 UI | 툴바 / 메뉴 / 자동 | 툴바 선택, 기본 재부재 | 사용자 선택, 기존 현장 무변화 |
| 셀 내용 | 상태만 / +제품번호 / +사유 | 상태 + 색상 | 사용자 선택, 상세는 패널 |
| 상세 패널 | status 요약 / /api/hub 내역 | status 요약 | 사용자 선택, 요청 수 불변 |
| 설정 저장 | devices.json / 별도 파일 / 레지스트리 | devices.json(StoreDto) | 기존 저장소 재사용, 필드 추가만 |

---

## 8. Convention Prerequisites

- **UI 표기**: 사용자에게 보이는 곳(컬럼 제목, 툴바 선택지, 상세 패널)은 "허브"가 아니라 **"RCM 허브"**로 쓴다(사용자 지시, 2026-11-10)

- 버전 주석: `// v3.0.0: RemoteDeck_PC v3.0.0 /api/status hub_*` (클라이언트 버전 = 계약 버전)
- WinForms 3중 방어선: 코드 생성 컨트롤, `column.Index` 접근, Designer 재생성 후 컬럼 확인
- 커밋: `feat(IntegrateController): v3.0.0 — …`

---

## 9. Next Steps

1. [ ] `/pdca design ic-hub-v3.0`
2. [ ] 구현 → `dotnet build` 0/0
3. [ ] 실기(가상 허브 + 2대) 확인 → `/pdca analyze ic-hub-v3.0`

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.2 | 2026-11-10 | UI 표기 "RCM 허브"로 통일 | KDI |
| 0.1 | 2026-11-10 | Initial draft (사용자 결정: 재부재 자리 전환, 툴바 기본 재부재, 상태+색상, status 요약) | KDI |
