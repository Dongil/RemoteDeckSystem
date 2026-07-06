---
template: plan
version: 1.3
feature: RemoteDeck_PC_v2.5
date: 2026-07-03
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.4.7 (baseline) → v2.5.0 (target)
---

# RemoteDeck_PC v2.5 기능 보강 Planning Document

> **Summary**: 현장 운용 피드백 3건 반영 — (1) 상단 "펌웨어" 탭을 "관리" 로 개편해 즉시 재부팅 + 재부팅 스케줄 + OTA 통합, (2) IntegrateController(C# WinForms)에서 RemoteDeck_PC 로그 폴링 수신, (3) 부팅 완료 후 GPIO 3채널·PCLED에 대해 WebRequest URL이 설정되어 있으면 현재 실측값에 맞는 URL을 1회 호출(초기 상태 sync).
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-07-03
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | (1) OTA 페이지가 상단 최상위 탭인데 정작 자주 쓰는 "재부팅" 기능은 설정 카드에 묻혀 있음. 재부팅 스케줄 부재로 매일 물리적 리셋 필요. (2) IntegrateController가 상태(polling)만 볼 뿐 이벤트 로그를 볼 수 없어 원인 분석은 매번 웹 UI 접속 필요. (3) 부팅 직후 외부 시스템(홈어시스턴트 등)은 GPIO/PCLED 상태를 알 수 없어 최초 이벤트 발생 전까지 stale 상태로 오인. |
| **Solution** | (1) 상단 탭 `펌웨어` → `관리` 개명, 관리 탭 내부에 카드 스택 방식으로 `기기 관리`(즉시 재부팅 + 재부팅 스케줄) + `펌웨어 업데이트` 배치. 재부팅 스케줄은 기존 `ScheduleManager`에 `action="reboot"` 추가 확장으로 재사용. (2) 기존 `/api/log` 엔드포인트를 IntegrateController `DevicePoller`에서 5초 주기 폴링 후 로그 뷰에 표시. RD_PC 신규 서버 코드 없음. (3) 부팅 완료 + WebRequest 활성 시 `wr-*` URL이 설정된 GPIO1/2/3·PCLED 채널에 대해 현재 실측값(HIGH/LOW, ON/OFF)에 맞는 URL을 1회 호출. |
| **Function/UX Effect** | 운영자가 관리 탭 한 곳에서 재부팅/스케줄/OTA를 완결. 매일 새벽 자동 재부팅으로 필드 신뢰성 향상. IntegrateController 사용자는 각 기기에 접속하지 않고도 통합 UI에서 최근 이벤트를 시계열로 확인. 외부 자동화 시스템은 부팅 직후 즉시 실제 상태를 반영. |
| **Core Value** | 필드 운용 부담 감소(재부팅/모니터링 원격 완결) + 자동화 통합 신뢰도 향상(부팅 sync). 소프트웨어 추가 최소, 기존 자산 재사용(ScheduleManager, /api/log, WebRequestHandler) 극대화. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.4.7까지 콜드 부팅 이슈 마감 후 필드 운용에서 확인된 3가지 편의성·통합성 gap 해소 |
| **WHO** | RemoteDeck_PC 필드 운영자(재부팅/스케줄), IntegrateController 통합 감시자(로그 뷰), 외부 자동화 시스템(WebRequest 수신측) |
| **RISK** | (1) ScheduleManager schema 변경으로 기존 스케줄 파일(`/schedule.json`) 마이그레이션 필요 — 파싱 실패 시 기존 릴레이 스케줄 손실. (2) `/api/log` 폴링 주기가 짧으면 ESP32 flash·CPU 부담. (3) 부팅 sync URL 호출이 실패해도 부팅은 정상 완료해야 함(비동기 처리). (4) WebRequest 인증 필요한 홈어시스턴트 API 호출 시 초기 상태와 실제 원격 상태 mismatch로 오작동 가능. |
| **SUCCESS** | (1) 관리 탭에서 재부팅·스케줄·OTA 3기능 완결, (2) 재부팅 스케줄 등록 → 정해진 요일·시간에 자동 재부팅 확인, (3) IntegrateController에 로그가 5초 이내 반영, (4) 부팅 후 15초 이내 GPIO/PCLED 초기 sync URL 호출 완료, (5) 기존 릴레이 스케줄 하위호환 유지(마이그레이션 or fallback), (6) v2.4.7 최소 방어선 유지(v2.4.0~v2.4.6 미검증 개입 재도입 금지) |
| **SCOPE** | Phase 1: FR1 웹 UI 재편성 + ScheduleManager 확장 / Phase 2: FR3 부팅 sync 로직 / Phase 3: FR2 IntegrateController 로그 폴링 뷰 / Phase 4: 통합 검증 + v2.5.0 릴리스 |

---

## 1. Overview

### 1.1 Purpose

v2.4.7에서 **콜드 부팅 문제 마감**(아답터 원인 확정, [[project-rdpc-coldboot-adapter]])과 미검증 개입 제거가 완료됐다. 이후 실제 필드 운용에서 확인된 편의성·통합성 관련 3가지 gap을 v2.5.0으로 반영한다:

1. **관리 접근성**: OTA(펌웨어 업데이트)는 상단 최상위 탭인 반면, 매일 필요한 재부팅과 아예 부재한 재부팅 스케줄이 각각 설정 카드에 파편화. 관리 성격의 기능을 한 곳에 통합.
2. **통합 모니터링**: IntegrateController는 이미 기기 상태(릴레이/PC상태)를 폴링하지만 이벤트 로그(부팅, 릴레이 명령, MQTT 재연결 등)는 각 기기 웹 UI에서만 확인 가능. 원인 분석 시 매번 개별 접속 필요.
3. **자동화 통합 신뢰도**: WebRequest는 상태 변화 시 URL을 호출하지만 부팅 직후에는 첫 상태 변화 이벤트가 발생하기 전까지 외부 시스템(홈어시스턴트, Node-RED 등)이 실제 값을 알 수 없어 stale 상태로 오인.

### 1.2 Background

- **v2.4.7 baseline**: NetManager 최소 방어선 확정, brownout/MAC stagger/NVS 추적 등 미검증 개입 모두 제거. Flash·RAM 여유 있음.
- **기존 자산**:
  - `RemoteDeck_PC/src/control/ScheduleManager.h` — Schedule struct + JSON 저장 + 요일 비트마스크 + `MAX_SCHEDULES=8`.
  - `RemoteDeck_PC/src/web/WebServer.cpp:97` — `GET /api/log` 이미 존재(이벤트 로그 반환).
  - `RemoteDeck_PC/src/network/WebRequestHandler.cpp` — 이벤트 트리거 시 URL 호출 로직 완성. 플레이스홀더 지원.
  - `APITestUtility_v2/integrate_controller/.../Services/DevicePoller.cs` — 이미 주기 폴링 인프라 존재.
- **필드 피드백 시점**: 2026-07-03(v2.4.7 배포 후 2일차 운영).

### 1.3 Related Documents

- 참조 baseline: v2.4.7 firmware (`RemoteDeck_PC/firmware/RemoteDeck_PC_V2.4.7_OTA_20260701.bin`)
- 참조 LAN_Recovery archive: `docs/archive/2026-07/RemoteDeck_PC_LAN_Recovery/`
- 참조 Integrate_Controller archive: `docs/archive/2026-XX/Integrate_Controller/`(원본 사이클)
- 다음 단계 Design 문서: `docs/02-design/features/RemoteDeck_PC_v2.5.design.md`

---

## 2. Scope

### 2.1 In Scope

**FR1 — 관리 탭 재편성 + 재부팅 스케줄** (RemoteDeck_PC 펌웨어 + Web UI)
- [ ] 상단 탭 `펌웨어` → `관리`로 개명(id `ota` → `admin` or `manage`)
- [ ] 관리 탭 내부에 카드 스택 방식으로 2개 카드 배치:
  - `기기 관리` 카드 (상단): 즉시 재부팅 버튼, 재부팅 스케줄 목록/추가/삭제 UI
  - `펌웨어 업데이트` 카드 (하단): 기존 OTA UI 그대로 이동
- [ ] `ScheduleManager` 확장: `Schedule.action` 에 `"reboot"` 허용, `relay=0`은 reboot 시 미사용
- [ ] `ScheduleManager` 콜백에서 `action="reboot"` 시 `ESP.restart()` 호출
- [ ] 스케줄 탭은 기존대로 유지하되 `reboot`은 별도 표시(구분 아이콘/라벨) — 관리 탭·스케줄 탭 양쪽에서 조회 가능

**FR2 — IntegrateController 로그 폴링 뷰** (C# WinForms only)
- [ ] `DevicePoller` 또는 신규 `LogPoller`에서 대상 기기의 `GET /api/log`를 5초 주기 폴링
- [ ] 응답 로그를 기기별 timestamped 컬렉션에 병합 저장(중복 제거)
- [ ] MainForm에 로그 뷰 패널 추가(선택 기기의 최근 N건 표시, 시간 역순)
- [ ] 로그 필터(기기, 카테고리) — 최소 기기 필터만 v2.5 스코프

**FR3 — 부팅 후 초기 상태 sync** (RemoteDeck_PC 펌웨어)
- [ ] 부팅 완료 시점(네트워크 GOT_IP + WebRequest 초기화 완료) 이후 1회 실행
- [ ] `WebRequestHandler`에 `syncCurrentStates()` 신규 메서드 추가
- [ ] 대상 채널: GPIO1/2/3, PCLED
- [ ] 각 채널의 URL이 설정되어 있고 현재 실측값을 얻을 수 있는 경우에만 호출
- [ ] 실패해도 부팅·다른 기능은 정상 진행(비동기, best-effort)

### 2.2 Out of Scope

- **콜드 부팅 H/W 보완 (EN 핀 1uF 콘덴서)** — 향후 보드 revision 사이클 별도 이관, v2.5 펌웨어 스코프 아님.
- **릴레이 부팅 sync** — 릴레이는 "명령" 성격이라 부팅 시 sync 대상 아님(사용자 확인 완료).
- **IntegrateController SSE/MQTT 전환** — 5초 폴링으로 충분, 실시간성 요구 없음.
- **로그 검색·엑스포트, 알림 시스템** — v2.6+ 이후 별도 사이클.
- **재부팅 스케줄의 릴레이 스케줄 통합 뷰** — 각 탭에서 필터링 표시로 처리, 새 통합 뷰 제작 안 함.
- **v2.4.0~v2.4.6에서 제거된 개입 재도입 금지** — brownout disable, MAC stagger, NVS 추적 등.

### 2.3 Constraints

- **Flash·RAM**: v2.4.7 대비 flash 증가 ≤ 10KB, heap free ≥ 100KB 유지
- **하위호환**: 기존 `/schedule.json` 파일(action=on/off/toggle) 무손실 로딩
- **API 하위호환**: 기존 웹 UI JS(`app.js`)의 `/api/*` 엔드포인트 응답 shape 변경 금지 — reboot용은 신규 엔드포인트 추가로 처리
- **v2.4.7 방어선 유지**: pre-init delay 500ms, W5500 SW reset, ETH.begin retry 3회, GOT_IP watchdog 20s 원형 유지

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|:--------:|
| FR1-1 | 상단 탭 `펌웨어` → `관리` 표시명 및 tab id 변경 | P0 |
| FR1-2 | `관리` 탭 내부에 `기기 관리` / `펌웨어 업데이트` 카드 스택 배치 | P0 |
| FR1-3 | `기기 관리` 카드에 즉시 재부팅 버튼(확인 dialog 포함) 배치 | P0 |
| FR1-4 | `기기 관리` 카드에 재부팅 스케줄 목록/추가/삭제 UI 배치 (시간 + 요일 다중 선택) | P0 |
| FR1-5 | ScheduleManager 확장: `action="reboot"` 지원, 실행 시 `ESP.restart()` 호출 | P0 |
| FR1-6 | 스케줄 탭은 기존 릴레이 스케줄 + reboot 스케줄 모두 표시(구분 표시) | P1 |
| FR1-7 | 기존 `/schedule.json` 하위호환 로딩(unknown action 무시 대신 relay 스케줄만 유지) | P0 |
| FR2-1 | IntegrateController DevicePoller/LogPoller가 `GET /api/log` 5초 주기 폴링 | P0 |
| FR2-2 | 기기별 로그 컬렉션 병합·중복 제거·시간 역순 정렬 | P0 |
| FR2-3 | MainForm에 로그 뷰 패널 추가(선택 기기 최근 100건, 스크롤) | P0 |
| FR2-4 | 기기 필터 UI (라디오/드롭다운) | P1 |
| FR3-1 | 부팅 완료 후 15초 이내 `WebRequestHandler::syncCurrentStates()` 1회 실행 | P0 |
| FR3-2 | GPIO1/2/3 채널: 각 채널의 URL이 설정된 경우 현재 값(HIGH/LOW)에 맞는 URL 호출 | P0 |
| FR3-3 | PCLED 채널: URL이 설정된 경우 현재 값(ON/OFF)에 맞는 URL 호출 | P0 |
| FR3-4 | sync 호출 실패 시 부팅·이후 로직에 영향 없음(로그만 남김) | P0 |
| FR3-5 | sync 호출 결과를 이벤트 로그(`_onLog("BOOT_SYNC", ...)`)에 기록 | P1 |

### 3.2 Non-Functional Requirements

| ID | Requirement | Metric |
|----|-------------|--------|
| NFR-1 | Flash 사용량 증가 | ≤ 10KB vs v2.4.7 |
| NFR-2 | Heap free (부팅 후 정상 상태) | ≥ 100KB |
| NFR-3 | 재부팅 스케줄 실행 정확도 | 등록 시각 ±1분 이내 |
| NFR-4 | IntegrateController 로그 폴링 latency | 이벤트 발생 후 ≤ 5초 |
| NFR-5 | 부팅 sync 완료 시점 | GOT_IP 후 15초 이내 |
| NFR-6 | 하위호환 | 기존 `/schedule.json` 무손실 로딩 |
| NFR-7 | v2.4.7 방어선 무결성 | pre-init delay/SW reset/retry/watchdog 원형 유지 |

### 3.3 Success Criteria

- [ ] SC-1: 관리 탭에서 즉시 재부팅 클릭 → 확인 dialog → 재부팅 실행됨
- [ ] SC-2: 관리 탭에서 "화 09:00 재부팅" 등록 → 다음 화 09:00±1분에 자동 재부팅 발생
- [ ] SC-3: 기존 릴레이 스케줄 있는 상태에서 v2.5 업그레이드 → 릴레이 스케줄 손실 없음
- [ ] SC-4: IntegrateController 실행 → 대상 기기 선택 → 로그 뷰에 5초 이내 최근 이벤트 표시
- [ ] SC-5: RD_PC에서 이벤트 발생 → IntegrateController 로그 뷰에 5초 이내 반영
- [ ] SC-6: GPIO1 HIGH 상태에서 URL 설정된 채 부팅 → 부팅 완료 후 15초 이내 GPIO1 HIGH URL 호출 확인
- [ ] SC-7: PCLED URL 미설정 상태에서 부팅 → sync 시도 없음(에러 없이 skip)
- [ ] SC-8: v2.4.7 최소 방어선 코드 diff 0줄(NetManager 원형 유지)
- [ ] SC-9: Flash 증가 ≤ 10KB, heap free ≥ 100KB
- [ ] SC-10: 필드 검증(1주 이상 운영) — 매일 자동 재부팅 성공률 ≥ 99%, WebRequest 부팅 sync 성공률 ≥ 95%

---

## 4. Approach

### 4.1 High-Level Strategy

- 기존 자산 최대 재사용 원칙:
  - **FR1**: 기존 `sub-tabs` DOM 패턴(설정 탭에서 이미 사용)을 관리 탭에 적용하되, 카드 스택 방식으로 단순화. Schedule 구조체에 필드 추가 없이 문자열 action 만 확장(하위호환 유지).
  - **FR2**: 신규 서버 코드 0. `/api/log`는 이미 존재. IntegrateController 쪽에만 폴링 + 병합 로직 추가.
  - **FR3**: 신규 URL 채널 정의 0. 기존 `wr-*` 설정과 채널 상태 게터 재사용. 부팅 완료 직후 loop() 1회에서만 호출.
- **v2.4.7 방어선 무결성**: NetManager.h/cpp 및 관련 상수 diff 0줄 목표. FR3의 sync 호출은 NetManager 완료 이후 시점에서만 실행.

### 4.2 Phased Rollout

| Phase | Goal | Deliverable |
|-------|------|-------------|
| Phase 1 | FR1 웹 UI 재편성 + ScheduleManager 확장 | 관리 탭 완성, reboot 스케줄 등록·실행 검증 |
| Phase 2 | FR3 부팅 sync | `WebRequestHandler::syncCurrentStates()` + 이벤트 로그 |
| Phase 3 | FR2 IntegrateController 로그 뷰 | LogPoller + MainForm 로그 패널 |
| Phase 4 | 통합 검증 + 릴리스 | 필드 dogfood 1주, v2.5.0 OTA bin |

---

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|------|:------:|------------|
| ScheduleManager schema 변경으로 기존 스케줄 파일 파싱 실패 | H | `fromJson()`에서 unknown action은 skip, relay 스케줄은 유지. 마이그레이션 스크립트 불필요(하위호환 로딩). |
| `/api/log` 폴링 부하로 ESP32 CPU 압박 | M | 응답 크기 상한(예: 최근 100건), 5초 주기 유지. AsyncWebServer 특성상 non-blocking. |
| WebRequest 부팅 sync 실패로 부팅 hang | H | HTTP 호출 timeout 5초, best-effort. sync는 loop() 진입 후 별도 flag로 처리해 setup() 미영향. |
| 초기 상태 sync URL과 실제 원격 값 mismatch(홈어시스턴트 등에 잘못된 상태 push) | M | 이벤트 로그에 명시 기록, 문서에 "부팅 sync는 RD_PC 실측값 기준" 명시. |
| v2.4.7 방어선 훼손 | H | NetManager 코드 diff 0줄 검증을 SC-8에 편성. |
| FR2 로그 폴링이 다수 기기(예: 14대)에서 동시에 발생 시 IntegrateController 부하 | L | 폴링을 기기별 stagger(0.3초 오프셋), 대상 기기만 활성 폴링. |

---

## 6. Timeline

| Milestone | Est. Duration | Notes |
|-----------|---------------|-------|
| Phase 1 (FR1) | 1 세션 | ScheduleManager 확장 + HTML/JS 재편성 |
| Phase 2 (FR3) | 0.5 세션 | 기존 WebRequestHandler에 추가 |
| Phase 3 (FR2) | 1 세션 | C# LogPoller + UI 통합 |
| Phase 4 (검증·릴리스) | 필드 1주 dogfood | v2.5.0 OTA bin 배포 |

---

## 7. Open Questions

- [x] 관리 탭 UI 패턴: **카드 스택** 확정 (사용자 확정)
- [x] 재부팅 스케줄 저장: **기존 ScheduleManager 확장** 확정 (사용자 확정)
- [x] IntegrateController 로그 수신: **HTTP GET /api/log 폴링 5초** 확정 (사용자 확정)
- [x] 부팅 sync 대상: **GPIO 3채널 + PCLED** 확정 (사용자 확정, 릴레이 제외)
- [ ] 재부팅 스케줄 executeTime 정밀도: 분 단위 유지(초 단위 불필요)? — Design 단계에서 확정
- [ ] IntegrateController 로그 뷰 최대 표시 건수: 100건? 500건? — Design 단계에서 확정
- [ ] 부팅 sync 실행 시점 정확화: GOT_IP 즉시 vs 부팅 완료 후 N초 대기 — Design 단계에서 확정

---

**Next Step**: `/pdca design RemoteDeck_PC_v2.5`
