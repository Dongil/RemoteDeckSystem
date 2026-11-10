---
template: report
version: 1.1
feature: ic-hub-v3.0
date: 2026-11-10
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
consumes: RemoteDeck_PC v3.0.0 /api/status hub_*
branch: v3.0-ic-hub
matchRate: 98
status: completed
---

# IntegrateController RCM 허브 컬럼 (v3.0.0) 완료 보고

> **Status**: Complete
>
> **Project**: RemoteDeckSystem
> **Version**: 3.0.0 (소비하는 펌웨어 계약 버전)
> **Author**: KDI
> **Completion Date**: 2026-11-10
> **PDCA Cycle**: #1 (Act 없음)
> **Origin**: rcm-hub-v3.0 report §4.1 Carry "IntegrateController hub 컬럼"

---

## Executive Summary

### 1.1 Project Overview

| Item | Content |
|------|---------|
| Feature | ic-hub-v3.0 — IC 그리드에서 장비별 RCM 허브 연결 상태 표시 |
| Start / End | 2026-11-10 / 2026-11-10 |
| Duration | 1일 |
| 변경 | 5 files, +178 / -3 (새 파일 없음, Designer 미수정) |

### 1.2 Results Summary

```
┌─────────────────────────────────────────────┐
│  Completion Rate: 100%                       │
├─────────────────────────────────────────────┤
│  ✅ Complete:     7 / 7 success criteria     │
│                   (FR 9/9)                   │
│  ⏳ In Progress:   0                         │
│  ❌ Cancelled:     0                         │
└─────────────────────────────────────────────┘
```

### 1.3 Value Delivered

| Perspective | Content |
|-------------|---------|
| **Problem** | v3.0 펌웨어가 허브 연결 상태(`hub_*`)를 내보내지만, IC에서는 장비마다 웹 화면을 열어야 허브 연결 여부를 알 수 있었다. |
| **Solution** | 재부재 컬럼 자리를 툴바 "표시 컬럼: 재부재 / RCM 허브"로 전환(기본 재부재, devices.json 저장). RCM 허브 모드는 7가지 상태를 색과 함께 표시하고, 상세 패널에 주소·제품번호·정지 사유 한글을 요약. |
| **Function/UX Effect** | 그리드 한 화면에서 모든 장비의 허브 연결(초록)·연결 중(주황)·정지/설정 불완전(빨강)·미사용/미지원(회색)을 확인. 가상 허브를 멈추면 다음 폴링에 "연결 중"으로 바뀌는 것을 실기 확인. 재부재 현장은 기본값 그대로라 화면 변화 0. |
| **Core Value** | v3.0 허브 연동의 통합 관제 표면 완결(장비 홈 카드 → IC 그리드). 두 기능을 같이 쓰는 현장이 없다는 운영 사실을 반영해 컬럼을 늘리지 않음. |

---

## 1.4 Success Criteria Final Status

| # | 기준 | Status | 근거 |
|---|------|:------:|------|
| SC-1 | FR-01~09 구현 | ✅ | analysis §2 |
| SC-2 | `dotnet build` 0 경고 / 0 오류 | ✅ | Debug 빌드 |
| SC-3 | RCM 허브 모드: `.109`·`.193` 연결됨, 허브 중지 시 연결 중 | ✅ | 하네스 T-03, T-04 실기 |
| SC-4 | 미사용 / 미지원 표시 | ✅ | 목 장비(허브 off, v2.6.2 형식) |
| SC-5 | 재부재 모드 회귀 0 | ✅ | 기존 FormatAttendance 유지, 사용자 화면 확인 |
| SC-6 | 전환 즉시·재시작 유지·기존 devices.json 호환 | ✅ | 하네스 T-02/T-09 + 사용자 확인 |
| SC-7 | `publish.bat` 단일 exe | ✅ | ≈162 MB |

**Success Rate**: 7/7 (100%)

## 1.5 Decision Record Summary

| Source | Decision | Followed? | Outcome |
|--------|----------|:---------:|---------|
| [Plan] | 재부재 자리를 옵션으로 전환(동시 사용 현장 없음) | ✅ | 컬럼 수 불변 |
| [Plan] | 툴바 선택, 기본 재부재 | ✅ | 기존 현장 무변화 |
| [Plan] | 셀은 상태+색, 사유는 상세 패널 | ✅ | 그리드 폭 유지 |
| [Plan] | `/api/hub` 미사용, status 요약만 | ✅ | 폴링 요청 수 불변 |
| [Plan] | UI 표기 "RCM 허브" (사용자 지시) | ✅ | 컬럼 제목·툴바·상세 패널 |
| [Design] | Option C: enum + StoreDto 필드 + MainForm 분기 | ✅ | 새 파일 0, 저장 API 시그니처 불변 |
| [Design] | Designer 미수정, `ColAttendance.Index` 접근 | ✅ | WinForms 3중 방어선 유지 |

---

## 2. Related Documents

| Phase | Document | Status |
|-------|----------|--------|
| Plan | [plan.md](plan.md) | ✅ |
| Design | [design.md](design.md) | ✅ |
| Check | [analysis.md](analysis.md) | ✅ (98%) |
| Report | 현재 문서 | ✅ |

---

## 3. Completed Items

### 3.1 Functional Requirements

| ID | Requirement | Status |
|----|-------------|--------|
| FR-01 | DeviceStatus hub 필드 7개 | ✅ |
| FR-02 | hub_* 하위호환 파싱, 타입 불일치 내성 | ✅ |
| FR-03 | FormatHub 7상태 | ✅ |
| FR-04 | 상태별 글자색, 선택 행 유지 | ✅ |
| FR-05 | 툴바 "표시 컬럼" 선택(코드 생성) | ✅ |
| FR-06 | 전환 즉시 제목·값 갱신 | ✅ |
| FR-07 | devices.json 저장·복원, 기존 파일 호환 | ✅ |
| FR-08 | 상세 패널 "RCM 허브:" 요약 + 정지 사유 한글 | ✅ |
| FR-09 | 오프라인 `-` | ✅ |

### 3.2 Non-Functional Requirements

| Item | Target | Achieved | Status |
|------|--------|----------|--------|
| 빌드 | 0/0 | 0/0 | ✅ |
| 호환성(v2.x 혼재) | 예외 0, "미지원" | 확인(목 v2.6.2) | ✅ |
| 호환성(devices.json) | 기존 파일 로드·저장 | 확인(필드 없음 → 재부재, 모르는 값 → 재부재) | ✅ |
| 안정성 | `column.Index`만 사용 | 확인 | ✅ |
| 성능 | 폴링·요청 수 불변 | 새 요청 없음 | ✅ |

### 3.3 Deliverables

| Deliverable | Location | Status |
|-------------|----------|--------|
| 모델 | `Models/DeviceStatus.cs` (+ FeatureColumnMode) | ✅ |
| 파싱 | `Services/RemoteDeckClient.cs` | ✅ |
| 저장 | `Services/DeviceStore.cs` | ✅ |
| 표시 | `UI/StatusFormatter.cs`, `UI/MainForm.cs` | ✅ |
| 배포 | `IntegrateController/publish/IntegrateController.exe` (git 무시) | ✅ |
| 문서 | Plan / Design / Analysis / Report | ✅ |

---

## 4. Incomplete Items

### 4.1 Carried Over

| Item | Reason | Priority |
|------|--------|----------|
| 실제 4001/4009 장비로 표시 재확인 | 정식 허브 운영 전(목 장비로 검증) | Low |

### 4.2 Cancelled/On Hold

없음.

---

## 5. Quality Metrics

| Metric | Target | Final |
|--------|--------|-------|
| Design Match Rate | 90% | 98% |
| Structural / Functional / Contract / Runtime | - | 100 / 100 / 100 / 95 |
| Build warnings / errors | 0 / 0 | 0 / 0 |

### 5.1 Resolved During Do

| Issue | Resolution |
|-------|------------|
| 설정 불완전일 때 상세 문구 중복 | "허브 주소·제품번호·장비 키 입력 필요"로 변경 |
| 빈 host가 ":8891"로 표시 | host 부분이 없으면 `-` |

---

## 6. Lessons Learned

### 6.1 Keep
- v2.6.2 재부재 컬럼 패턴(필드 하위호환 파싱 → Formatter → 셀 세팅 2곳)을 그대로 따라 설계·구현이 짧았음.
- 운영 사실(두 기능 동시 사용 현장 없음)을 Plan 단계에서 확인해 컬럼을 늘리지 않음.

### 6.2 Problem
- 시험용으로 사용자 설정 파일(devices.json)을 바꾸는 방식이 권한 검사에서 막혀, GUI 확인은 사용자 손을 빌림.

### 6.3 Try
- IC에 `--store <path>` 같은 시험용 설정 경로 인자를 두면, 사용자 설정을 건드리지 않고 GUI 시험을 자동화할 수 있음(필요 시 별도 작업).
- 빌드된 어셈블리를 리플렉션으로 호출하는 하네스는 WinForms 앱 로직 검증에 유효 — 다음 IC 작업에도 재사용.

---

## 7. Next Steps

- [ ] `v3.0-ic-hub` 커밋 → PR → main 병합
- [ ] `/pdca archive ic-hub-v3.0`
- [ ] 다음 후보: UDP 인증 + IPSetupTool, 정식 허브 현장 시험

---

## 8. Changelog

### IntegrateController v3.0.0 (2026-11-10)

**Added:**
- 툴바 "표시 컬럼: 재부재 / RCM 허브" 선택(devices.json `FeatureColumn` 저장)
- RCM 허브 상태 표시(연결됨·연결 중·정지·설정 불완전·미사용·미지원·`-`)와 색상
- 상세 패널 "RCM 허브:" 요약 줄(정지 사유 한글)

**Changed:**
- `/api/status` 파싱에 `hub_*` 6필드 추가(하위호환)

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2026-11-10 | Completion report created | KDI |
