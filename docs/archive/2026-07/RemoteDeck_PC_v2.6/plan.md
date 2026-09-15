---
template: plan
version: 1.3
feature: RemoteDeck_PC_v2.6
date: 2026-07-05
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.5.1 → v2.6.0 (target)
---

# RemoteDeck_PC v2.6.0 Planning Document

> **Summary**: GPIO2 상태 변화 이벤트 자동 fire 활성화. `pinMode(INPUT)` → `INPUT_PULLUP`, 폴링+3x debounce로 상태 전이를 감지하여 기존 `gpio2_high`/`gpio2_low` WebRequest 이벤트를 발화한다. **신규 이벤트/URL/UI 필드/스키마 모두 도입하지 않고 기존 자산 100% 재사용**. 첫 사용 사례는 조명 스위치→광커플러→GPIO2 접점(GND) 배선을 통한 재부재 판정 신호이지만, GPIO2에 무엇을 물려도 동일하게 동작하는 범용 확장이다. PCLED/PIR 경로와의 조합·우선순위 조율은 스코프 외 (서버 측에서 처리).
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-07-05
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.5.1까지 GPIO2 는 `/api/status` 스냅숏과 부팅 sync 1회 호출에만 쓰였고, 런타임 상태 변화가 자동으로 `gpio2_high/low` URL을 호출하지 않았다. 재부재 판정용 조명 스위치를 광커플러 경유 GPIO2로 배선해도 이벤트가 서버로 전파되지 않음. |
| **Solution** | GPIO2 를 `INPUT_PULLUP`으로 초기화하고, 1s poll + 3x debounce의 상태 감지 루프를 추가하여 상태 전이 시점에 **기존** `gpio2_high` / `gpio2_low` 이벤트를 `webRequestHandler.fire()`로 발화. 신규 이벤트/URL/UI/스키마 없음. |
| **Function/UX Effect** | 광커플러 접점이 GND로 당겨지면 GPIO2 LOW → `gpio2_low` URL 호출, 개방되면 HIGH → `gpio2_high` URL 호출. 조명 스위치 → 광커플러 → GPIO2 배선만 갖추면 서버는 두 URL로 재실/부재 신호 수신. URL을 비우면 자동 skip이라 배선 없는 기기는 무동작. |
| **Core Value** | 기존 자산 재사용 극대. 신규 코드는 GPIO2 감지 로직 소량만 추가. 서버 API 변경 없음, 웹 UI 변경 없음, config 스키마 변경 없음, 14대 필드 기존 pcled 경로 무영향. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | GPIO2 상태 변화 이벤트가 자동 발화되지 않는 공백을 채워, 재부재 등 접점 기반 입력 시나리오에 범용 대응 |
| **WHO** | 재부재 시스템 서버 측(gpio2_high/low URL 수신), 필드 설치 인력(조명 스위치+광커플러 배선) |
| **RISK** | GPIO2 를 INPUT_PULLUP 으로 바꾸는 것이 필드에 미치는 영향 (기존 사용 이력 확인 필요) / 광커플러 접지 부실로 노이즈 오감지 / v2.5 마감 방어선(NetManager diff=0) 유지 |
| **SUCCESS** | (1) GPIO2 상태 전이 후 ≤4초에 해당 URL 호출됨 / (2) URL 미설정 시 HTTP 호출 없음 / (3) 기존 pcled_on/off 흐름 무변화 / (4) NetManager diff=0 / (5) 필드 스위치 배선 1대에서 하루 관찰 오탐/미탐 ≤5% |
| **SCOPE** | Phase 1: GPIO2 감지 루프 + fire 연결 / Phase 2: 빌드·필드 dogfood (배선 준비 별도) |

---

## 1. Overview

### 1.1 Purpose

RemoteDeck_PC의 GPIO2 핀은 이미 다음이 갖춰져 있으나 상태 변화 발화가 빠져 있다:
- `PIN_GPIO2 = 14`, 현재 `pinMode(INPUT)` (floating)
- `WebRequestConfig::gpio2_high`, `gpio2_low` URL 슬롯 (`DeviceConfig.h:75-76`)
- `WebRequestHandler::getURL()` 의 `gpio2_high` / `gpio2_low` 케이스 반환
- 웹 UI 설정 > Web Request 탭 `cfg-wr-gpio2-high` / `cfg-wr-gpio2-low` 입력 필드
- ConfigManager 저장/불러오기 라운드트립
- 부팅 시 `syncCurrentStates()`에서 GPIO2 현재값에 대한 초기 URL 1회 호출 (v2.5.1)
- `/api/status` JSON `gpio2` 스냅숏 노출

빠져 있는 유일한 조각은 **런타임 GPIO2 상태 전이 시 자동 fire**. 이번 사이클에서 이 조각만 채운다. 사용자의 첫 사용 사례는 조명 스위치 → 광커플러 → GPIO2 접점(GND) 배선을 통한 재실/부재 판정이지만, 펌웨어 로직은 특정 사용 사례에 결합되지 않는 범용 상태 감지+발화이다.

### 1.2 Background

- **v2.5.1 baseline** (2026-07-05 마감, matchRate 99.2%): 관리 탭 + 부팅 sync + IC 로그 뷰 + SPIFFS OTA(config 보존). 14대 필드 배포 완료.
- **이전 검토와 다른 점**: 이전에는 기기-스위치 거리가 길어 220V 배선 부담 문제였으나, 이번에는 **기기를 스위치 바로 옆에 설치 가능**해져 배선 길이 이슈 해소.
- **하드웨어 회로**:
  - 조명 스위치 220V ↔ 광커플러 입력측
  - 광커플러 출력측 한쪽: GND 공통
  - 광커플러 출력측 반대쪽: GPIO2
  - 스위치 ON → 광커플러 도통 → GPIO2 를 GND로 당김 → GPIO2 LOW
  - 스위치 OFF → 광커플러 개방 → INPUT_PULLUP으로 GPIO2 HIGH
- **PIR/PCLED와의 관계**: 이 사이클에서 다루지 않음. GPIO2와 PCLED는 독립 채널이며, 둘 다 이벤트 발화가 가능하다면 서버 측에서 어느 채널을 신뢰할지 결정한다.

### 1.3 Related Documents

- 참조 baseline: v2.5.1 firmware (`RemoteDeck_PC/firmware/RemoteDeck_PC_V2.5.1_OTA_20260703.bin`) + v2.5.2 SPIFFS
- v2.5 마감: `docs/archive/2026-07/RemoteDeck_PC_v2.5/{plan,design,analysis,report}.md`
- Memory: `[[project-rdpc-reboot-flush]]`, `[[project-spiffs-ota-preserve]]`
- 다음 단계 Design 문서: `docs/02-design/features/RemoteDeck_PC_v2.6.design.md` (작성 예정)

---

## 2. Scope

### 2.1 In Scope

**FR1 — GPIO2 pinMode 변경**
- [ ] `pinMode(PIN_GPIO2, INPUT_PULLUP)` (기존 `INPUT`에서 변경)

**FR2 — GPIO2 상태 감지 + fire**
- [ ] 상태 감지 루프: 1s poll + 3x debounce (PCMonitor와 동일 파라미터)
- [ ] 상태 전이 시 `webRequestHandler.fire("gpio2_high"|"gpio2_low", value)` 호출
- [ ] 상태 유지 중에는 재발화 없음 (edge-triggered)
- [ ] 구현 방식은 Design 단계에서 결정 (신규 클래스 vs main.cpp 인라인)

**FR3 — 빌드 · 배포**
- [ ] 버전 스탬프 2.5.1 → 2.6.0 (ConfigManager)
- [ ] firmware bin (app) 재빌드. SPIFFS bin은 변경 없음(웹 UI 미변경) → 기존 v2.5.2 spiffs 그대로 사용 가능

### 2.2 Out of Scope

- **신규 이벤트 이름 (`switch_on/off` 등)** — 사용자 확정: 기존 `gpio2_high/low` 그대로 사용
- **웹 UI 신규 URL 필드** — 사용자 확정: 기존 `cfg-wr-gpio2-high/low` 그대로 사용
- **Config 스키마 확장** — 이미 `WebRequestConfig::gpio2_high/low` 있음. 변경 없음
- **PCLED / PIR 경로와의 조합·우선순위 조율** — 사용자 확정: 이번 스코프 아님. 서버 측 처리
- **GPIO1 / GPIO3 상태 변화 발화 확장** — 이번 스코프 GPIO2에 한정. 후속 사이클에서 검토
- **`/api/status` 응답 shape 변경** — GPIO2 스냅숏 필드는 이미 있음, 무변경
- **NetManager 방어선** — diff=0 유지

### 2.3 Constraints

- Flash 증가 ≤ 3KB (매우 소량 변경)
- Heap free ≥ 100KB
- `NetManager.h/.cpp` diff = 0
- 기존 pcled_on/off, gpio1/3 흐름 무변화
- `/deviceconfig.json` 스키마 하위호환 (변경 없음)
- 서버 API 변경 없음 (기존 gpio2_high/low URL이 있으면 그대로 수신)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|:--------:|
| FR1-1 | `pinMode(PIN_GPIO2, INPUT_PULLUP)` 로 변경 | P0 |
| FR2-1 | GPIO2 1s poll + 3x debounce 상태 감지 루프 | P0 |
| FR2-2 | 상태 전이 시 `webRequestHandler.fire("gpio2_high"\|"gpio2_low", value)` | P0 |
| FR2-3 | 상태 유지 중 재발화 없음 (edge-triggered) | P0 |
| FR2-4 | 상태 변화 Serial 로그 출력 (진단용) | P1 |
| FR3-1 | ConfigManager 버전 2.5.1 → 2.6.0 | P0 |
| FR3-2 | firmware bin 재빌드 (spiffs bin 변경 없음) | P0 |

### 3.2 Non-Functional Requirements

| ID | Requirement | Metric |
|----|-------------|--------|
| NFR-1 | Flash 사용량 증가 | ≤ 3KB vs v2.5.1 |
| NFR-2 | Heap free | ≥ 100KB |
| NFR-3 | 상태 감지 latency (physical → fire 호출) | ≤ 4초 (poll 1s + debounce 3회) |
| NFR-4 | fire → HTTP 도달 | ≤ 3초 (기존 WebRequest workerTask) |
| NFR-5 | 기존 API/UI/응답 shape 무변화 | 100% 하위호환 |
| NFR-6 | v2.4.7 방어선 무결성 | NetManager diff=0 |

### 3.3 Success Criteria

- [ ] SC-1: GPIO2 HIGH → LOW 물리 전이 시점에서 4초 이내 `fire("gpio2_low")` 호출됨 (Serial 로그로 확인)
- [ ] SC-2: GPIO2 LOW → HIGH 물리 전이 시점에서 4초 이내 `fire("gpio2_high")` 호출됨
- [ ] SC-3: `gpio2_low` URL 이 설정된 상태에서 스위치 ON → 서버 액세스 로그에 GET 도달
- [ ] SC-4: `gpio2_high` URL 이 설정된 상태에서 스위치 OFF → 서버 액세스 로그에 GET 도달
- [ ] SC-5: 두 URL 모두 비어 있으면 외부 HTTP 없음 (fire의 empty URL skip 동작 유지)
- [ ] SC-6: 기존 PCMonitor 흐름 무변화, pcled_on/off 필드 배포 14대에서 회귀 없음
- [ ] SC-7: 상태가 계속 LOW 유지되는 동안 반복 fire 없음 (edge-triggered)
- [ ] SC-8: `NetManager.h/.cpp` git diff = empty
- [ ] SC-9: `/api/status` gpio2 필드 shape 무변화 (IntegrateController 파서 회귀 없음)
- [ ] SC-10: 필드 dogfood: 조명 스위치 배선한 기기 1대, 하루 이상 사용에서 오탐/미탐 각각 ≤ 5%

---

## 4. Approach

### 4.1 High-Level Strategy

- **최소 변경**: PIN 초기화 1줄 + 감지 루프 소량 추가. 그 외 자산은 모두 기존 재사용.
- **PCMonitor 패턴 미러링**: 1s poll + 3x debounce는 이미 필드에서 검증된 파라미터. 별도 튜닝 없이 그대로 채택.
- **Edge-triggered**: 상태 전이만 fire. 서버가 이벤트 스팸 받지 않음.
- **URL empty skip**: 기존 `fire()` 로직에서 URL 빈 채널은 자체 skip → 배선 없는 기기는 무동작.

### 4.2 Phased Rollout

| Phase | Goal | Deliverable |
|-------|------|-------------|
| Phase 1 | 펌웨어 감지 루프 + fire 연결 | pio build 성공, Serial에서 상태 전이 로그 확인 |
| Phase 2 | 필드 dogfood | 조명 스위치 배선 1대, 하루 관찰 |

---

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|------|:------:|------------|
| GPIO2 INPUT_PULLUP 도입이 기존 필드에 미묘한 영향 (이전에 GPIO2에 뭔가 연결되어 있는데 pull-up으로 인해 상태 해석이 바뀌는 경우) | M | v2.5.1까지 GPIO2 상태 변화 fire가 없었기 때문에 실제로 자동화 로직에 연결된 사이트는 없다고 판단. 릴리스 노트에 pinMode 변경 명시. |
| 광커플러 접지 부실로 GPIO2 노이즈 튀어서 오감지 | M | 3x debounce가 완화. 필요 시 debounce 상향은 Design에서 설정 노출 여부 결정 (일단 상수). |
| 조명 스위치 켜져 있는 채로 부팅 → 첫 부팅 sync에서 gpio2_low URL 호출 (v2.5.1 syncCurrentStates 동작) | L | 정상 동작. 서버는 부팅 후 초기 상태로 판단하면 됨. Plan/Release notes에 명시. |
| 220V 배선 안전 이슈 | H (H/W) | 광커플러로 저압 절연. 220V 결선은 전기공사 필수. 릴리스 노트에 시공 주의 문구. |
| v2.5.1/v2.5.2 방어선 훼손 | H | SC-6 (PIR 무변화) + SC-8 (NetManager diff=0) 로 검증 |

---

## 6. Timeline

| Milestone | Est. Duration | Notes |
|-----------|---------------|-------|
| Phase 1 | 0.5 세션 | pinMode 변경 + 감지 루프 + fire 연결 + 버전 스탬프 + 빌드 |
| Phase 2 | 실측 (배선 준비 별도) | 조명 스위치 배선 후 하루 관찰 |

---

## 7. Open Questions

- [x] 이벤트 이름: 기존 `gpio2_high` / `gpio2_low` 재사용 (사용자 확정)
- [x] URL 필드: 기존 `cfg-wr-gpio2-high/low` 재사용 (사용자 확정)
- [x] PCLED와의 조합/우선순위: 이번 스코프 아님, 서버 처리 (사용자 확정)
- [x] GPIO2 pull: INPUT_PULLUP (사용자 확정)
- [ ] 감지 루프 구현 방식: 신규 `SwitchMonitor` 클래스 vs `main.cpp` 인라인 (static locals) → Design 단계 결정
- [ ] 부팅 sync 시점 이미 v2.5.1에서 gpio2 채널 호출됨 — 스위치 상태에 따라 초기 URL 호출됨을 릴리스 노트에 명시
- [ ] GPIO1 / GPIO3 도 동일 패턴 확장할지 → v2.6.1+ 후보 (Plan/Design에서 확장 지점만 표시)

---

**Next Step**: `/pdca design RemoteDeck_PC_v2.6`
