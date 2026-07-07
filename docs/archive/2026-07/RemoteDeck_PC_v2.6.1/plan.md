---
template: plan
version: 1.3
feature: RemoteDeck_PC_v2.6.1
date: 2026-07-06
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.6.0 → v2.6.1 (target)
---

# RemoteDeck_PC v2.6.1 Planning Document

> **Summary**: 재부재 시스템 연동을 설치·운영자가 직관적으로 설정할 수 있도록 설정 > 기타 탭 하단에 `재부재 시스템` 카드 신규. 활성 체크박스 + 소스 선택(PC LED / SwitchMonitor GPIO2) + ON/OFF URL 2개. 기존 Web Request 탭의 개별 채널 URL 슬롯은 무변화. 두 파이프라인이 독립 병렬 실행 — 사이트별로 필요한 것만 URL 채워 사용.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-07-06
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.6.0에서 SwitchMonitor + PCMonitor 두 감지 채널이 확보됐지만, 재부재 시스템 연동을 위해서는 설치 운영자가 (a) Web Request 탭에서 개별 채널 URL 슬롯(pcled_on/off 또는 gpio2_low/high)에 URL을 입력하고 (b) 서버 측에서 채널별 이벤트를 재실/부재 의미로 매핑해야 하는 두 단계가 필요. 사이트별 배선(PIR vs 조명 스위치) 차이도 사용자가 직접 채널명을 이해해야 함. |
| **Solution** | 설정 > 기타 탭 하단에 `재부재 시스템` 카드 신규. 활성 체크박스 + 소스 드롭다운(PC LED / SwitchMonitor+GPIO2) + ON URL / OFF URL 2개. 펌웨어에 `AttendanceHandler` 신규 모듈이 선택된 소스의 onChange 이벤트를 받아 재실=ON URL / 부재=OFF URL 을 호출. 기존 개별 채널 URL 슬롯은 그대로 유지되어 독립 발화(둘 다 원하는 사이트도 지원). |
| **Function/UX Effect** | 설치 운영자는 카드 하나에서 배선 방식(PIR/스위치)만 고르고 서버 URL 2개만 입력하면 재부재 연동 완결. Web Request 탭의 채널명(pcled_on/off, gpio2_low/high 등) 이해 불필요. 기존 Web Request 탭은 개별 자동화(릴레이, 특정 GPIO 트리거 등) 용도로 그대로 유지. |
| **Core Value** | 재부재 배포 학습 곡선 대폭 감소. 배선/센서 종류에 무관한 통일된 UX. 기존 개별 자동화 사용자에게 무영향. NetManager 방어선 유지. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.6.0으로 감지 채널 두 개(PIR, 스위치) 확보 후, 설치 운영자가 채널명 지식 없이도 재부재 연동을 설정할 수 있는 통합 UI 필요 |
| **WHO** | 재부재 시스템 설치·운영 인력, 서버 측 재부재 시스템 담당 |
| **RISK** | (1) 설정 스키마 확장으로 v2.5.1 SPIFFS OTA config preserve 로직에 영향 여부 확인 필요 (기존 파일이라 자동 preserve). (2) 두 파이프라인 동시 발화 시 서버 중복 처리 부담. (3) NetManager 방어선 훼손 금지. (4) 기존 Web Request 탭 UI/저장 흐름과 이름/식별자 충돌 방지. |
| **SUCCESS** | (1) 카드에서 체크 + 소스 + URL 2개 입력만으로 재부재 이벤트 발화. (2) 소스 전환(PC LED ↔ SwitchMonitor GPIO2) 시 재부팅 없이 즉시 반영 (또는 재부팅 후 반영). (3) 기존 Web Request 탭 개별 URL 동작 무변화. (4) NetManager diff=0. (5) `/deviceconfig.json` 하위호환 — attendance 블록 없으면 disabled 기본. |
| **SCOPE** | Phase 1: AttendanceHandler 펌웨어 + config 스키마 확장 + PCMonitor/SwitchMonitor 콜백 브릿지 / Phase 2: Web UI 카드 + JS 저장/불러오기 / Phase 3: 필드 dogfood (한 사이트에 배포) |

---

## 1. Overview

### 1.1 Purpose

v2.6.0 배포로 재부재 시스템 서버는 채널별 URL(pcled_on/off, gpio2_low/high)로 이벤트를 받고 있음. 문제는:
- 설치 운영자가 Web Request 탭에서 8개 이상의 채널명(pcled_on/off, gpio1_high/low, gpio2_high/low, gpio3_high/low)을 이해하고 그 중 어느 것을 재실/부재로 사용할지 판단해야 함.
- PIR 사이트(pcled_on/off) 와 스위치 사이트(gpio2_low/high) 는 서버 측 매핑도 달라야 함 (`_on/high` vs `_low/high` 의미 혼동).
- 배선 방식 변경(PIR→스위치) 시 사이트 재구성 필요.

v2.6.1은 이 복잡성을 `재부재 시스템` 카드 하나로 은닉:
- 활성 체크
- 소스 선택 (PC LED / SwitchMonitor)
- ON URL / OFF URL

내부적으로 소스의 활성 상태(재실=PCLED ON 또는 GPIO2 LOW, 부재=반대)를 매핑하여 ON/OFF URL 호출.

### 1.2 Background

- **v2.6.0** (2026-07-06 마감, matchRate 100%): SwitchMonitor 신규 (GPIO2 INPUT_PULLUP + 3x debounce + edge-triggered). `fire("gpio2_low"|"gpio2_high")` 발화.
- **v2.5.1** (2026-07-05 마감, matchRate 99.2%): SPIFFS OTA config preserve 확립. `/deviceconfig.json`은 OTAHandler backup/restore 대상.
- **기존 pcMonitor** 는 활성 상태를 `_currentState` 로 노출 (`isPCOn()`), onChange 콜백 시 활성 여부 인자 전달.
- **SwitchMonitor** 도 동일 패턴 (`isActive()`, onChange(bool active)).
- **PCLED 매핑 규약** (v2.4.7 이전부터): `_currentState = !digitalRead(_pin)` 즉 LOW=활성(재실). PCMonitor 자체는 pin의 반전 논리를 내장. UI 표기는 PC ON = 재실.
- **SwitchMonitor 매핑**: `_currentState = (digitalRead == LOW)` — GND 접점 = 활성(재실).
- 두 소스 모두 "활성(재실) = true" 로 통일된 의미. `AttendanceHandler`는 활성일 때 ON URL, 비활성일 때 OFF URL을 호출하면 됨.

### 1.3 Related Documents

- baseline: v2.6.0 firmware (`RemoteDeck_PC/firmware/RemoteDeck_PC_V2.6.0_OTA_20260705.bin`) + v2.5.2 SPIFFS
- 이전 사이클: `docs/archive/2026-07/RemoteDeck_PC_v2.6/{plan,design,analysis,report}.md`
- 관련 Memory: [[project-spiffs-ota-preserve]], [[project-rdpc-reboot-flush]]
- 다음 Design 문서: `docs/02-design/features/RemoteDeck_PC_v2.6.1.design.md`

---

## 2. Scope

### 2.1 In Scope

**FR1 — AttendanceHandler 펌웨어 모듈 (신규)**
- [ ] `src/control/AttendanceHandler.{h,cpp}` 신규
- [ ] `AttendanceConfig` 상태 유지 (enabled/source/onUrl/offUrl)
- [ ] `onSourceStateChange(bool active)` API — 소스 콜백 브릿지
- [ ] active 값에 따라 `webRequestHandler.fire("attendance_on"|"attendance_off", …)` 호출
- [ ] `WebRequestConfig`에 `attendance_on` / `attendance_off` URL 필드 추가 (사용자 확정: 기존 fire() 파이프라인 재사용)
- [ ] `WebRequestHandler::getURL()` 케이스 추가

**FR2 — 설정 스키마 확장**
- [ ] `DeviceConfig`에 `AttendanceConfig` 블록 추가
  - `enabled: bool`
  - `source: string` (`pcled` | `gpio2`)
  - `onUrl: string` — attendance_on URL (실제 저장 위치는 WebRequestConfig.attendance_on)
  - `offUrl: string` — attendance_off URL (WebRequestConfig.attendance_off)
- [ ] `ConfigManager` 로드/저장 하위호환 (attendance 블록 없으면 disabled 기본)
- [ ] URL 자체는 WebRequestConfig에 두어 fire() 파이프라인 재사용 (UI에서는 attendance 카드에서 편집)

**FR3 — main.cpp 콜백 브릿지**
- [ ] pcMonitor.setOnChange + switchMonitor.setOnChange 에서 attendanceHandler 라우팅
- [ ] AttendanceHandler.enabled + source 일치 시에만 fire
- [ ] source 일치 여부 판정은 config.attendance.source 참조

**FR4 — Web UI 카드 신규**
- [ ] 설정 > 기타 탭 하단에 `재부재 시스템` 카드
- [ ] ☑️ 재부재 시스템 연동하기 (checkbox)
- [ ] 사용 센서 (select): `PC LED` / `SwitchMonitor (GPIO2)`
- [ ] ON URL (text input) — 재실 시 호출
- [ ] OFF URL (text input) — 부재 시 호출
- [ ] `saveEtc()` 함수 확장하여 attendance 값 저장
- [ ] `loadConfig()` 확장하여 attendance 값 로드

**FR5 — Web UI 안내**
- [ ] 카드 상단에 짧은 설명: "선택한 센서의 상태 변화 시 ON/OFF URL이 서버로 호출됩니다. 기존 Web Request 탭의 개별 URL과 독립적으로 동작합니다."

### 2.2 Out of Scope

- **GPIO1 / GPIO3 를 SwitchMonitor 소스로** — 사용자 확정: GPIO2 only. 확장은 v2.6.2+.
- **재부재 활성 시 개별 URL suppress** — 사용자 확정 A안 (독립 발화). 서버가 중복 처리.
- **URL 별도 파일 저장** — deviceconfig.json 통합.
- **AttendanceHandler 자체 재시도 정책** — 기존 WebRequestHandler.fire() 파이프라인 재사용, 실패 정책 동일.
- **부팅 시 재부재 초기 sync** — 별도 로직 안 만듦. v2.5.1의 `syncCurrentStates`가 gpio2/pcled 채널을 이미 호출하므로 서버는 부팅 후 초기 상태를 개별 채널로 받음. attendance 초기 sync는 v2.6.2+ 후보.
- **UI 소스 전환 시 재부팅 없이 즉시 반영** — 저장 후 재부팅 사이클 유지 (기존 saveEtc 패턴).

### 2.3 Constraints

- Flash 증가 ≤ 5KB vs v2.6.0
- Heap free ≥ 100KB
- `NetManager.h/.cpp` diff = 0
- `/deviceconfig.json` **하위호환 필수**: attendance 블록 없으면 `{enabled:false, source:"pcled", ...}` 기본값
- 기존 Web Request 탭 UI/저장 로직 무변경
- SPIFFS OTA config preserve (v2.5.1 로직) 재적용 — deviceconfig.json이므로 자동 포함
- URL placeholder 지원: `[device_id] [device_name] [ip] [mac] [event] [value]` (기존 WebRequestHandler.replacePlaceholders 재사용)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|:--------:|
| FR1-1 | AttendanceHandler 클래스 신규 (enabled/source 판정 + fire 호출) | P0 |
| FR1-2 | WebRequestConfig에 attendance_on / attendance_off URL 필드 추가 | P0 |
| FR1-3 | WebRequestHandler::getURL() attendance_on/off 케이스 추가 | P0 |
| FR2-1 | DeviceConfig.attendance 블록 (enabled/source/…) 추가 | P0 |
| FR2-2 | ConfigManager 하위호환 로드 (attendance 없으면 기본값) | P0 |
| FR2-3 | ConfigManager 저장 시 attendance 블록 포함 | P0 |
| FR3-1 | pcMonitor.setOnChange 콜백에서 attendanceHandler.onSourceStateChange 호출 (source가 pcled인 경우) | P0 |
| FR3-2 | switchMonitor.setOnChange 콜백에서 attendanceHandler.onSourceStateChange 호출 (source가 gpio2인 경우) | P0 |
| FR3-3 | AttendanceHandler는 enabled=false 시 즉시 return | P0 |
| FR3-4 | AttendanceHandler는 config.source 와 인자 채널 일치 여부 확인 후 fire | P0 |
| FR4-1 | 설정 > 기타 탭 하단에 `재부재 시스템` 카드 추가 | P0 |
| FR4-2 | 체크박스 + 소스 select(pcled/gpio2) + ON URL + OFF URL | P0 |
| FR4-3 | saveEtc()에서 attendance 값을 config로 저장 | P0 |
| FR4-4 | loadConfig()에서 attendance 값을 UI에 반영 | P0 |
| FR5-1 | 카드 상단 안내 문구 (개별 URL과 독립 동작 명시) | P1 |
| FR5-2 | 소스가 SwitchMonitor일 때만 GPIO 번호 드롭다운 표시 (지금은 GPIO2 하나) | P1 |

### 3.2 Non-Functional Requirements

| ID | Requirement | Metric |
|----|-------------|--------|
| NFR-1 | Flash 사용량 증가 | ≤ 5KB vs v2.6.0 |
| NFR-2 | Heap free | ≥ 100KB |
| NFR-3 | 상태 변화 감지 latency (physical → attendance URL 호출) | ≤ 4초 (기존 debounce + fire 파이프라인) |
| NFR-4 | 하위호환 (deviceconfig.json에 attendance 없어도 부팅) | 필드 14대 무영향 |
| NFR-5 | 기존 Web Request 탭 UI/저장/불러오기 무변경 | 회귀 없음 |
| NFR-6 | v2.4.7 방어선 무결성 | NetManager diff=0 |

### 3.3 Success Criteria

- [ ] SC-1: 재부재 카드 체크 + 소스=pcled + ON URL 설정 후 PIR 감지 → 서버 로그에 ON URL GET 도달
- [ ] SC-2: 재부재 카드 체크 + 소스=gpio2 + OFF URL 설정 후 GPIO2 HIGH → 서버 로그에 OFF URL GET 도달
- [ ] SC-3: 재부재 체크 해제 후 상태 변화 → attendance URL 호출 없음
- [ ] SC-4: 재부재 활성 상태에서 기존 개별 URL(pcled_on/off, gpio2_low/high) 설정되어 있으면 둘 다 발화 (독립)
- [ ] SC-5: 저장 & 재부팅 후 attendance 설정 값 재로드 유지
- [ ] SC-6: 기존 14대 필드 기기(deviceconfig.json에 attendance 블록 없음) → v2.6.1 부팅 시 disabled 기본값으로 로드, 기존 동작 유지
- [ ] SC-7: Web Request 탭 UI 무변화, 기존 개별 URL 저장/불러오기 정상
- [ ] SC-8: NetManager.h/.cpp diff = 0
- [ ] SC-9: URL placeholder([device_id] 등) 치환 정상 작동
- [ ] SC-10: 필드 dogfood (한 사이트 재부재 카드로 설정) 후 하루 관찰, 오탐/미탐 ≤ 5%

---

## 4. Approach

### 4.1 High-Level Strategy

- **기존 자산 재사용**:
  - `WebRequestHandler.fire()` + `getURL()` 파이프라인 그대로 → attendance_on/off 이벤트 케이스만 추가
  - `PCMonitor.setOnChange` + `SwitchMonitor.setOnChange` 두 콜백에서 AttendanceHandler로 브릿지
  - 기존 URL placeholder 규칙 재사용
- **얇은 AttendanceHandler**: enabled/source 확인 + fire 호출만. 상태 자체는 소스가 관리.
- **UI 카드 독립 배치**: 기타 탭 하단에 신규 카드. Web Request 탭 무변경.
- **하위호환 우선**: attendance 블록 없으면 disabled 기본. 14대 필드 무영향.

### 4.2 Phased Rollout

| Phase | Goal | Deliverable |
|-------|------|-------------|
| Phase 1 | 펌웨어 (Handler + config + wiring + WebRequest 이벤트) | pio build 성공, Serial에서 attendance fire 로그 확인 |
| Phase 2 | Web UI 카드 + JS 저장/불러오기 | 브라우저에서 저장 후 GET /api/config 로 attendance 블록 확인 |
| Phase 3 | 필드 dogfood | 한 사이트에서 재부재 카드 사용 → 서버 도달 확인 |

---

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|------|:------:|------------|
| attendance 블록 없는 기존 deviceconfig.json 로드 시 파싱 오류 | H | ArduinoJson `\|` default 연산자로 필드별 기본값 지정. 필드 14대 회귀 테스트 필수. |
| SPIFFS OTA 후 attendance 값 소실 | M | v2.5.1 OTAHandler.backupSpiffsConfigs가 deviceconfig.json 통째로 backup — 자동 유지. |
| 두 파이프라인 동시 발화로 서버 부담 | L | 사용자 확정: 독립 발화. 서버가 중복 처리. 로그에 명시. |
| 소스 전환 후 즉시 반영 기대와 재부팅 필요 사이 UX 갭 | L | 저장 후 자동 재부팅 (기존 saveEtc 패턴). 안내 문구 명시. |
| 잘못된 소스 선택 (GPIO2 배선 없는데 gpio2 선택 등) | L | 사용자 책임. 초기값은 pcled. |
| v2.4.7 방어선 훼손 | H | SC-8 검증 |

---

## 6. Timeline

| Milestone | Est. Duration | Notes |
|-----------|---------------|-------|
| Phase 1 (Firmware) | 0.5 세션 | AttendanceHandler + config schema + main.cpp wiring |
| Phase 2 (Web UI + JS) | 0.5 세션 | 기타 탭 카드 + saveEtc/loadConfig 확장 |
| Phase 3 (필드 dogfood) | 실측 | 한 사이트에서 하루 관찰 |

---

## 7. Open Questions

- [x] GPIO 선택 범위: GPIO2 only (사용자 확정)
- [x] 활성 시 개별 URL 동작: 독립 발화 (A안, 사용자 확정)
- [x] 저장 위치: deviceconfig.json 통합 (A안, 사용자 확정)
- [x] URL placeholder: 기존 규칙 재사용 (A안, 사용자 확정)
- [ ] attendance_on/off 이벤트 이름 vs 직접 URL 저장: Design 단계에서 결정 (WebRequestConfig에 필드 추가하는 방식이 fire() 재사용이라 가장 간단)
- [ ] `_syncCurrentStates`에 attendance 채널 포함 여부: Design 단계 결정 (일단 out-of-scope로 두고 향후 v2.6.2 후보)
- [ ] "사용 센서" UI에서 소스 전환 시 저장 즉시 재부팅 vs 저장만: Design 단계에서 확정 (기타 탭 기존 저장 버튼 패턴 재사용 방향)

---

**Next Step**: `/pdca design RemoteDeck_PC_v2.6.1`
