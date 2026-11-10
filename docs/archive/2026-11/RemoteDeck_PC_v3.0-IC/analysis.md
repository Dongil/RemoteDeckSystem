---
template: analysis
version: 0.1
feature: ic-hub-v3.0
date: 2026-11-10
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
consumes: RemoteDeck_PC v3.0.0 /api/status hub_*
branch: v3.0-ic-hub
plan: docs/archive/2026-11/RemoteDeck_PC_v3.0-IC/plan.md
design: docs/archive/2026-11/RemoteDeck_PC_v3.0-IC/design.md
phase: check
matchRate: 98
---

# IntegrateController RCM 허브 컬럼 (v3.0.0) — Gap Analysis (Check)

> **Summary**: FR-01~09 전부 구현. `dotnet build` 0/0. IC 어셈블리를 직접 호출하는 시험 하네스로 실제 장비 2대 + 목 장비 4종 + 저장소 왕복 11/11 PASS, 가상 허브 중지 시 "연결 중" 전환 실기 확인. GUI(툴바·제목 전환·선택 행 색·재시작 유지)는 사용자 실행 확인. **matchRate 98%**, 갭 없음.

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 허브 현장에서 장비별 허브 연결·정지 상태를 IC 그리드에서 한눈에 봐야 함 (현재는 장비 웹 화면을 하나씩 열어야 함) |
| **WHO** | 허브 현장 관제자(주 사용자), 기존 재부재 현장 관제자(회귀 없어야 함), 혼재 펌웨어(v2.x/v3.0) 장비 |
| **RISK** | Designer 컬럼 소실, v2.x 장비 오표시, devices.json 호환 |
| **SUCCESS** | build 0/0 · RCM 허브 모드 정확 표시 · v2.x "미지원" · 재부재 모드 무변화 · 옵션 유지 |
| **SCOPE** | 모델·파싱 / 표시·전환 / 옵션 저장 / 상세 패널 |

---

## 1. 검증 방식

| 층 | 방법 | 결과 |
|----|------|------|
| L0 빌드 | `dotnet build IntegrateController.sln -c Debug` | 경고 0 / 오류 0 |
| 정적 | 설계 §3~§5 체크리스트 ↔ 코드 대조 | 전 항목 일치 |
| L1 로직 | 시험 하네스(빌드된 IntegrateController.dll의 `ParseStatus`·`StatusFormatter`·`DeviceStore`를 리플렉션 호출, 사용자 devices.json 미사용) | 11/11 PASS |
| L1 실기 | 가상 허브 중지/재기동 중 `.109`·`.193` status 해석 | 연결됨 → 연결 중 → 연결됨 |
| L2 GUI | 사용자 실행(publish exe) | 툴바, 제목·값 즉시 전환, 선택 행 색, 재시작 유지 확인 |
| 배포 | `publish.bat`와 같은 인자 | 단일 exe 생성 (≈162 MB) |

시험용으로 실제 IC 설정 파일을 덮어쓰는 방식은 권한 검사에서 차단되어, 설정을 건드리지 않는 하네스 + 사용자 GUI 확인으로 대체.

---

## 2. Functional Requirements

| ID | 요구사항 | 구현 | 검증 | 근거 |
|----|----------|:--:|:--:|------|
| FR-01 | DeviceStatus hub 필드 7개 | ✅ | ✅ | `Models/DeviceStatus.cs:32-40` |
| FR-02 | hub_* 하위호환 파싱, 타입 불일치 내성 | ✅ | ✅ | `RemoteDeckClient.cs:260-266, 307-315` · 하네스 FR-02(문자열 bool/숫자 host → 기본값, pc_on 유지) |
| FR-03 | FormatHub 7상태 | ✅ | ✅ | `StatusFormatter.cs:67-77` · T-03/05/06/07/07b/08 |
| FR-04 | 상태별 글자색, 선택 행 유지 | ✅ | ✅ | `HubColor`, `MainForm.SetFeatureCell`(SelectionForeColor) · 사용자 확인 |
| FR-05 | 툴바 "표시 컬럼" 선택, 코드 생성 | ✅ | ✅ | `MainForm.AddFeatureColumnSelector` (Designer 미수정) · 사용자 확인 |
| FR-06 | 전환 즉시 제목·값 갱신 | ✅ | ✅ | `SetFeatureMode` → HeaderText + `RefreshAllUnboundCells` · 사용자 확인 |
| FR-07 | devices.json 저장·복원, 기존 파일 호환 | ✅ | ✅ | `DeviceStore.cs:27,53-55,74,126` · 하네스 T-02/T-09/모르는 값 · 사용자 재시작 확인 |
| FR-08 | 상세 패널 "RCM 허브:" 요약 + 정지 사유 한글 | ✅ | ✅ | `FormatHubDetail`, `MainForm.cs:472-473` · T-07 "장비 키 또는 제품번호 오류 (4001)" |
| FR-09 | 오프라인 `-` | ✅ | ✅ | `FormatHub` 첫 규칙 · T-08 |

---

## 3. Success Criteria

| 기준 | 상태 | 근거 |
|------|:--:|------|
| FR-01~09 구현 | ✅ | §2 |
| `dotnet build` 0/0 | ✅ | |
| RCM 허브 모드: `.109`·`.193` 연결됨, 허브 중지 시 연결 중 | ✅ | 하네스 T-03a/b, T-04 실기 |
| 미사용 / 미지원 표시 | ✅ | 목 5094 / 5091 (v2.6.2 형식) |
| 재부재 모드 회귀 0 | ✅ | 재부재 값은 기존 `FormatAttendance` 그대로(하네스에서 재실/미설정 확인), 기본 모드 화면 사용자 확인 |
| 옵션 전환 즉시·재시작 유지·기존 파일 호환 | ✅ | 하네스 + 사용자 확인 |
| `publish.bat` 단일 exe | ✅ | |

---

## 4. Design Decision 준수

| 결정 | 준수 | 비고 |
|------|:--:|------|
| Option C (enum + StoreDto 필드 + MainForm 분기, 새 파일 없음) | ✅ | 5개 파일 수정 |
| 재부재 자리 전환, 기본 재부재 | ✅ | |
| 셀은 상태+색, 사유는 상세 패널 | ✅ | |
| `/api/hub` 미사용(요청 수 불변) | ✅ | |
| UI 표기 "RCM 허브" | ✅ | 단독 "허브" 표기 없음 |
| Designer 미수정, `ColAttendance.Index` 접근 | ✅ | |

설계와 다른 점: 상세 문구 2곳 다듬음(설정 불완전일 때 사유 중복 제거 → "허브 주소·제품번호·장비 키 입력 필요", host가 비어 ":port"만 오면 주소 `-`). 설계 의도 범위 안.

---

## 5. Gap 목록

- Critical / Important: 없음
- Minor:
  - 4001 정지·설정 불완전·미지원은 목 장비로 검증(실기 v2.x·4001 장비 없음)
  - 선택 행 색 등 GUI 항목은 자동 시험 없음(사용자 확인)

---

## 6. Match Rate

| 축 | 점수 | 가중치 |
|----|:--:|:--:|
| Structural | 100 | 0.15 |
| Functional | 100 | 0.25 |
| Contract (§4.1 hub_* ↔ ParseStatus ↔ 표시) | 100 | 0.25 |
| Runtime (실기·목·GUI 확인, 일부 목 대체) | 95 | 0.35 |
| **Overall** | **98** | |

---

## 7. Carry Items

- 정식 허브 운영 시 실제 4001/4009 장비로 표시 재확인 (펌웨어 rcm-hub-v3.0 carry와 함께)
