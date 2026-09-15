---
template: plan
version: 1.3
feature: RemoteDeck_PC_v2.6.2
date: 2026-07-06
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.6.1 → v2.6.2 (target)
---

# RemoteDeck_PC v2.6.2 Planning Document

> **Summary**: v2.6.1의 재부재 시스템 카드 설정을 기반으로, 홈 화면에 실시간 모니터링 카드를 추가하고 홈 전반 UI를 통일한다. 상태 모니터 카드 개편(이름·순서·PC LED dot), GPIO 실시간 갱신, 부팅 시 재부재 초기 상태 서버 전송, 외부 API에서 재부재 상태 조회 지원까지 포함.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-07-06
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.6.1에서 재부재 시스템 카드로 설정 UI는 통합됐지만, 실시간 모니터링 화면이 없어 설치·감시 현장에서 상태와 서버 전송 결과를 확인하려면 로그 탭이나 시리얼을 뒤져야 함. GPIO 상태는 새로고침해야만 반영되고, 부팅 시 재부재 초기 상태가 서버에 전송되지 않음. 외부 자동화 시스템이 API로 재부재 상태를 물어볼 수단도 없음. |
| **Solution** | 홈 최상단에 attendance.enabled 조건부로 `재부재 시스템` 카드 신규(센서 종류·현재 상태·전송 내역 8건). AttendanceHandler 내부 링버퍼[8] + `GET /api/attendance/history` 신규 엔드포인트. `상태 모니터` 카드 개편(이름 변경 + 순서 릴레이→PC LED→GPIO + PC LED dot). SwitchMonitor.onChange에서 WebSocket broadcastStatus 호출 추가하여 GPIO2 실시간. 부팅 시 attendance 초기 상태 fire. `/api/status`에 attendance 미니 블록 추가하여 외부 API 조회 지원. 홈 전반 시인성 개선(폰트·아이콘·상태 색상 배지). |
| **Function/UX Effect** | 감시자는 홈 접속 즉시 재부재 상태와 전송 이력을 한눈에 확인. 상태 모니터 순서가 릴레이→PC LED→GPIO로 정렬되고 PC LED에 시각적 dot이 추가되어 판별 용이. 외부 시스템은 `/api/status` 한 번으로 재부재 상태를 얻음. 부팅 직후에도 서버가 초기 상태 이벤트를 수신하여 정합성 확보. |
| **Core Value** | 재부재 시스템 UX 완결(설정 v2.6.1 + 모니터링 v2.6.2). 외부 통합 표면(API+웹소켓) 정비. NetManager 방어선 유지. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.6.1 재부재 설정 UI 완결 이후, 감시·운영 화면과 부팅 정합성·외부 API 조회 표면 정비 |
| **WHO** | 재부재 감시자(홈 접속), 외부 자동화 시스템(홈어시스턴트/Node-RED), 설치 운영자 |
| **RISK** | (1) 링버퍼 도입으로 RAM 증가 (~200B). (2) WebSocket broadcast 증가로 다중 클라이언트 부하. (3) 부팅 sync 로직에 attendance 추가 시 이중 fire 위험 (첫 부팅 채널 fire + attendance fire). (4) v2.4.7 방어선 훼손 금지. (5) 홈 UI 재구성 중 기존 기능 회귀 방지. |
| **SUCCESS** | 홈 최상단 재부재 카드 표시(조건부) · 상태 모니터 순서/이름/dot 반영 · GPIO2 실시간 갱신 · 부팅 후 15s 이내 attendance URL 도달 · `/api/status` attendance 필드 조회 가능 · NetManager diff=0 · 기존 홈/기능 회귀 없음 |
| **SCOPE** | Phase1 펌웨어(링버퍼+엔드포인트+broadcast+부팅sync+/api/status 확장) / Phase2 Web UI(홈 재부재 카드+상태 모니터 개편+아이콘/뱃지) / Phase3 필드 dogfood |

---

## 1. Overview

### 1.1 Purpose

v2.6.1은 설정 UX를 완결했지만 사용자 관찰 결과:
- 홈에는 PC 상태 스냅숏만 있어 재부재 이벤트 시계열을 볼 수 없음.
- GPIO 상태는 홈 접속 시점만 반영됨(WebSocket이 GPIO 변화를 broadcast 안 함).
- 부팅 직후에는 개별 채널(pcled_on/off, gpio2_low/high) URL만 호출되고 attendance URL은 호출 안 됨 → 재부재 서버는 부팅 시점의 상태 이벤트를 못 받음.
- 외부 시스템이 재부재 현재 상태를 API로 물어볼 수 없음.
- PC 상태 카드 표시 순서와 이름이 재부재 시스템에 맞게 조정 필요(첨부 이미지).

v2.6.2는 감시·통합 UX를 완결한다.

### 1.2 Background

- **v2.6.1**: AttendanceHandler + 설정 카드(체크박스+source+ON/OFF URL). 실기 검증 완료(matchRate 100%). WebRequestConfig에 attendance_on/off URL, DeviceConfig.attendance{enabled, source}.
- **기존 홈 화면 카드**: `PC 상태` (릴레이1/2, PC 상태, GPIO 값) + `시스템 정보` (장치 이름/ID/IP/uptime/MQTT/시각).
- **WebSocket broadcast 흐름**:
  - PCMonitor.setOnChange → `onPCStateChange` → `broadcastStatus(buildStatusJson())` → 클라이언트 status 반영.
  - SwitchMonitor.setOnChange → fire + attendance dispatch. **broadcastStatus 미호출** → GPIO2 상태 UI 갱신 안 됨.
  - GPIO1/3은 상태 감지 로직 자체 없음(사용자도 지금 시나리오에서는 GPIO1/3 실시간성 요구 없음, GPIO2만 문제).
- **부팅 sync (v2.5.1 `syncCurrentStates`)**: 개별 채널 URL만 호출. attendance_on/off는 대상 아님.
- **외부 API**: `/api/status`는 이미 GPIO/PC/relay 상태 노출. attendance 미노출.

### 1.3 Related Documents

- baseline: v2.6.1 firmware (`RemoteDeck_PC_V2.6.1_OTA_20260706.bin`) + v2.6.1 SPIFFS
- 이전 사이클: `docs/archive/2026-07/RemoteDeck_PC_v2.6.1/{plan,design,analysis,report}.md`
- 첨부 mockup: `C:\Users\Administrator\Desktop\debug1.png` (상태 모니터 순서 참조)
- 다음 Design 문서: `docs/02-design/features/RemoteDeck_PC_v2.6.2.design.md`

---

## 2. Scope

### 2.1 In Scope

**FR1 — 홈 최상단 재부재 시스템 카드 (Web UI 신규)**
- [ ] `data/www/index.html` 홈 섹션 최상단(기존 `.card-row` 위)에 `#attendance-card` 신규
- [ ] `attendance.enabled === true` 인 경우에만 표시 (loadStatus 응답 attendance 미니 블록 참조)
- [ ] 카드 내용:
  - 센서 종류 badge: `PC LED (PIR)` 또는 `SwitchMonitor (GPIO2)`
  - 현재 상태 대형 뱃지: `❤️ 재실` (present) / `⚪ 부재` (absent)
  - 전송 내역 8건: 시간, 상태, 전송 결과(`✅ 200` / `❌ -1` 등)
- [ ] `/api/attendance/history` 폴링 (3-5초 주기 or WebSocket 이벤트 시 갱신)

**FR2 — AttendanceHandler 링버퍼 + /api/attendance/history 엔드포인트**
- [ ] `AttendanceHandler`에 링버퍼 `Entry history[8]` + `head` index 추가
- [ ] `onSourceStateChange` fire 후 링버퍼 push (timestamp, active, 성공여부)
- [ ] `WebRequestHandler` fire 결과 콜백을 AttendanceHandler에 연결하여 httpCode 저장
- [ ] `String toJson() const` 메서드
- [ ] `WebServer`에 `GET /api/attendance/history` 라우트 추가 → AttendanceHandler.toJson() 반환

**FR3 — 상태 모니터 카드 개편 (Web UI)**
- [ ] `h2` 텍스트 `PC 상태` → `상태 모니터`
- [ ] 순서 재정렬: 릴레이1, 릴레이2, PC LED (강조 스타일 + dot), GPIO(1,2,3 값)
- [ ] PC LED: 큰 폰트 + `ON` 초록 dot / `OFF` 빨강 dot (첨부 이미지 스타일)
- [ ] GPIO 표시는 기존 유지(변화값만 텍스트로 노출)

**FR4 — 홈 UI 시인성 개선**
- [ ] 카드 헤더 폰트 크기·색상 통일(청록/다크 톤 유지)
- [ ] 상태 라인 폰트 사이즈 상향(가독성)
- [ ] 아이콘 도입(재실/부재 뱃지, 전송 결과 ✅/❌, 필요 시 상단 탭 이모지)
- [ ] 기존 시스템 정보 카드는 유지, 폰트 크기만 통일

**FR5 — WebSocket broadcast 강화 (GPIO2 실시간)**
- [ ] `main.cpp` `switchMonitor.setOnChange` 콜백에 `webServer.ws().broadcastStatus(buildStatusJson().c_str())` 호출 추가
- [ ] AttendanceHandler 상태 변화(fire) 후에도 broadcast (attendance UI 즉시 갱신용) — Design에서 방식 확정
- [ ] GPIO1/GPIO3는 이번 스코프 아님(추가 감지 로직 없음, 지금 status는 스냅숏만 갱신 시 함께 나감)

**FR6 — 부팅 시 재부재 초기 상태 서버 전송**
- [ ] AttendanceHandler에 `void syncOnBoot()` 메서드 추가
- [ ] enabled + source에 따라 pcMonitor.isPCOn() 또는 switchMonitor.isActive() 참조
- [ ] 초기 상태로 fire("attendance_on"|"attendance_off") 1회 실행
- [ ] main.cpp의 부팅 sync 로직(GOT_IP + 5s stabilization 이후)에 `attendanceHandler.syncOnBoot()` 호출 추가

**FR7 — /api/status 응답에 attendance 미니 블록 추가**
- [ ] `buildStatusJson`에 `attendance: { enabled, source, current }` 추가
- [ ] `current`는 "present" | "absent" 문자열 (현재 소스 상태 기준)
- [ ] 하위호환: 필드 추가만이므로 기존 파서(IntegrateController 등)는 무영향

**FR8 — 버전 스탬프 · 빌드 · 산출물**
- [ ] ConfigManager firmware 2.6.1 → 2.6.2
- [ ] firmware bin + spiffs bin 재빌드 (홈 UI 변경으로 spiffs 필수)

### 2.2 Out of Scope

- **GPIO1 / GPIO3 상태 감지·실시간 갱신** — v2.6.2 스코프 아님. 필요 시 v2.6.3에서 GpioMonitor 승격.
- **관리 탭·설정 탭 UI 재편** — 홈 화면에만 집중.
- **재부재 카드 필터·검색·엑스포트** — 8건 표시만.
- **NetManager 방어선** — diff=0 유지.
- **인증·권한** — 기존 Basic Auth 그대로.
- **웹UI 언어/다국어** — 한국어 그대로.

### 2.3 Constraints

- Flash 증가 ≤ 8KB vs v2.6.1
- Heap free ≥ 100KB
- `NetManager.h/.cpp` diff = 0
- `/deviceconfig.json` 스키마 무변경 (신규 저장 안 함, RAM 링버퍼만)
- 기존 `/api/status` 응답 shape 하위호환 (필드 추가만)
- 기존 WebRequest fire pipeline 재사용 (attendance_on/off 이벤트는 v2.6.1과 동일)
- 첨부 이미지의 상태 모니터 카드 레이아웃 준수

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|:--------:|
| FR1-1 | 홈 최상단 `#attendance-card` 신규 (attendance.enabled 조건부 표시) | P0 |
| FR1-2 | 센서 종류 badge (pcled / gpio2) | P0 |
| FR1-3 | 현재 상태 대형 뱃지 (present/absent) | P0 |
| FR1-4 | 전송 내역 8건 리스트 (시간·상태·전송결과) | P0 |
| FR1-5 | `/api/attendance/history` 폴링 or WebSocket 갱신 | P0 |
| FR2-1 | AttendanceHandler 링버퍼[8] 구조 | P0 |
| FR2-2 | onSourceStateChange fire 후 링버퍼 push | P0 |
| FR2-3 | httpCode 저장(WebRequestHandler fire 결과 연결) | P1 |
| FR2-4 | AttendanceHandler.toJson() 메서드 | P0 |
| FR2-5 | GET /api/attendance/history 엔드포인트 | P0 |
| FR3-1 | 카드 h2 `PC 상태` → `상태 모니터` | P0 |
| FR3-2 | 순서: 릴레이1 → 릴레이2 → PC LED → GPIO | P0 |
| FR3-3 | PC LED 강조 스타일 + 색상 dot | P0 |
| FR4-1 | 홈 카드 헤더 폰트 통일 | P1 |
| FR4-2 | 상태 라인 폰트 사이즈 상향 | P1 |
| FR4-3 | 상태 아이콘/뱃지 도입 | P1 |
| FR5-1 | switchMonitor onChange 콜백에 broadcastStatus 추가 | P0 |
| FR5-2 | AttendanceHandler fire 후 broadcastStatus (또는 별도 attendance 이벤트) | P1 |
| FR6-1 | AttendanceHandler.syncOnBoot() 메서드 | P0 |
| FR6-2 | 부팅 sync (GOT_IP+5s) 시점에 호출 | P0 |
| FR7-1 | /api/status buildStatusJson에 attendance 미니 블록 추가 | P0 |
| FR7-2 | current 값: "present" | "absent" | P0 |
| FR8-1 | ConfigManager 버전 2.6.1 → 2.6.2 | P0 |
| FR8-2 | firmware + spiffs bin 재빌드 | P0 |

### 3.2 Non-Functional Requirements

| ID | Requirement | Metric |
|----|-------------|--------|
| NFR-1 | Flash 사용량 증가 | ≤ 8KB vs v2.6.1 |
| NFR-2 | Heap free | ≥ 100KB |
| NFR-3 | GPIO2 상태 변화 → 웹UI 반영 | ≤ 2초 |
| NFR-4 | 재부재 전송 이력 갱신 | ≤ 5초 |
| NFR-5 | 부팅 후 attendance 초기 fire | GOT_IP + 15초 이내 |
| NFR-6 | v2.4.7 방어선 무결성 | NetManager diff=0 |
| NFR-7 | /api/status 하위호환 | 기존 IntegrateController 파서 회귀 없음 |
| NFR-8 | 홈 UI 회귀 | 기존 릴레이/PC/시스템정보 표시 그대로 |

### 3.3 Success Criteria

- [ ] SC-1: attendance.enabled=true 상태로 홈 접속 → 최상단 재부재 카드 표시
- [ ] SC-2: attendance.enabled=false → 재부재 카드 미표시(홈 전체 레이아웃 유지)
- [ ] SC-3: 재부재 상태 변화 → 대형 뱃지가 3초 이내 갱신
- [ ] SC-4: 재부재 이벤트 발생 → 카드 하단 전송 내역에 5초 이내 추가 (최근 8건)
- [ ] SC-5: 홈 카드 `상태 모니터` 이름 + 순서 릴레이→PC LED→GPIO + PC LED dot 표시(첨부 이미지 준수)
- [ ] SC-6: GPIO2 물리적 상태 변화 후 2초 이내 웹UI 반영 (새로고침 없이)
- [ ] SC-7: 부팅 후 15초 이내 attendance_on/off URL 서버 도달 (활성 상태일 때)
- [ ] SC-8: `GET /api/status` 응답에 `attendance` 필드 포함 (`enabled, source, current`)
- [ ] SC-9: `GET /api/attendance/history` 응답에 최근 이벤트 최대 8건 포함
- [ ] SC-10: NetManager.h/.cpp diff = 0
- [ ] SC-11: 기존 IntegrateController 파서 회귀 없음(/api/status attendance 필드 unknown 이면 무시)
- [ ] SC-12: 필드 dogfood 하루, 감시자가 홈만으로 재부재 상태·이력 확인 가능

---

## 4. Approach

### 4.1 High-Level Strategy

- **기존 자산 확장**: AttendanceHandler에 링버퍼+toJson+syncOnBoot 추가 (얕은 dispatcher → 얕은 dispatcher + 이력 버퍼). 신규 클래스 없음.
- **엔드포인트 신규 1개**: `/api/attendance/history` 만 추가. `/api/status`는 필드 확장.
- **홈 UI 재구성**: `.card-row`를 유지하되 조건부 재부재 카드를 위에 삽입. 상태 모니터 카드 내부만 변경.
- **broadcast 강화**: switchMonitor onChange에 1줄 추가.
- **부팅 sync 확장**: main.cpp에 `attendanceHandler.syncOnBoot()` 1줄 추가.
- **v2.4.7 방어선 무결성**: NetManager diff = 0.

### 4.2 Phased Rollout

| Phase | Goal | Deliverable |
|-------|------|-------------|
| Phase 1 | 펌웨어 (링버퍼+엔드포인트+broadcast+syncOnBoot+/api/status 확장) | pio run + Serial + curl 검증 |
| Phase 2 | 홈 Web UI 재구성 + 스타일 통일 | 브라우저 시각 확인 (첨부 이미지 준수) |
| Phase 3 | 필드 dogfood | 감시자가 홈 진입만으로 상태 확인 |

---

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|------|:------:|------------|
| 링버퍼 도입으로 heap 증가 | L | Entry 구조 최소화(~20B × 8 = 160B). 힙 여유 100KB+ 충분. |
| WebSocket broadcast 급증으로 다중 클라이언트 부하 | M | onChange 시점만 broadcast (edge-triggered), 폴링 아님. 부하 미미. |
| 부팅 syncOnBoot 이중 fire (개별 채널 fire + attendance fire) | L | 사용자 확정: 이중 발화 정책 (v2.6.1). 서버가 중복 처리. |
| /api/status 응답 크기 증가로 IntegrateController 파서 부담 | L | attendance 미니 블록 3필드만 추가(~50B). 기존 파서는 unknown 필드 무시(자체 검증 완료). |
| 홈 UI 재구성 중 기존 status 표시 회귀 | M | 기존 DOM id 유지(pc-status, r1, r2 등). CSS/순서만 변경. |
| v2.4.7 방어선 훼손 | H | SC-10 검증 |

---

## 6. Timeline

| Milestone | Est. Duration | Notes |
|-----------|---------------|-------|
| Phase 1 (펌웨어) | 0.5 세션 | AttendanceHandler 확장 + 엔드포인트 + broadcast + syncOnBoot + status |
| Phase 2 (홈 Web UI) | 0.5 세션 | 재부재 카드 + 상태 모니터 개편 + 스타일 통일 |
| Phase 3 (필드) | 실측 | 감시자 dogfood 하루 |

---

## 7. Open Questions

- [x] 전송 이력 저장 방식: AttendanceHandler 내부 링버퍼 + `/api/attendance/history` (사용자 확정 A)
- [x] GPIO 실시간 갱신 방식: SwitchMonitor onChange에 broadcastStatus 추가 (사용자 확정 A)
- [x] 외부 API 형태: /api/status에 attendance 미니 블록 (사용자 확정 A)
- [x] 시각 스타일: 기존 청록/다크 + 상태 색상 강조 + 아이콘 (사용자 확정 A)
- [ ] 재부재 카드 갱신 방식: 폴링 vs WebSocket → Design 단계 결정. broadcast 강화 시 WebSocket 통일 자연스러움.
- [ ] AttendanceHandler.Entry 타입 정의 세부 (timestamp 소스: millis vs NTP epoch) — Design 단계 결정. NTP 있으면 epoch 우선.
- [ ] `/api/attendance/history` 응답에 device_id/source 메타 포함 여부 — Design 단계 결정.

---

**Next Step**: `/pdca design RemoteDeck_PC_v2.6.2`
