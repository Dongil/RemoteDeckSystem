---
template: analysis
version: 1.0
feature: rcm-led-ignore
date: 2026-11-28
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.4.0 (branch led-ignore-v1.4, v1.3.1 위)
status: archived
matchRate: 96
---

# PC LED 없이 전원 운영 (LED 무시 모드) Gap Analysis

| Key | Value |
|-----|-------|
| **WHY** | LED 인식이 안 되는 방은 끈 뒤 다시 켤 수 없음(현장 운영 중단) |
| **WHO** | 현장 관리자·사용자, 서버 담당(허브 설정), SmartDeck 설치 담당 |
| **RISK** | 켜져 있는 PC를 눌러 끔 → ping 확인 / 재시도 누름 없음 / LED 정상 방 회귀 없음 |
| **SUCCESS** | 녹화장비 종료 → off → power.on 한 번 눌러 켬 → 에이전트로 확인. 기존 시험 무수정 |
| **SCOPE** | hub/power.js, hub/hub.js, spec, test, docs |

## 1. Strategic Alignment

- 현장 요청("PC LED 무관하게 녹화 장비에서 종료되면 오프, 서버웹 오프, 다시 켤 수 있게")을 그대로 충족: I-01(LED 켜짐 고정)·I-02(LED 꺼짐 고정).
- 사용자 결정(허브 설정, v1.3.1 뒤 별도 v1.4.0, PDCA 문서) 준수. SmartDeck 펌웨어 변경 없음 → 현장 OTA 불필요.

## 2. Requirements

| ID | 결과 | 근거 |
|----|------|------|
| FR-01 `power.ignoreLed` true/목록 | ✅ | `hub/power.js` `ledIgnoredIn`, I-07·I-08 |
| FR-02 actual = 에이전트 접속 여부 | ✅ | `hub/hub.js` actualOf, 초기값 `off`, I-01·I-07 |
| FR-03 on 가드(noop/UNSUPPORTED/OFFLINE/ping 응답 시 누르지 않음) | ✅ | `_onNoLed`, I-03·I-08 |
| FR-04 한 번 펄스 → 에이전트 접속 확인, 재시도 없음 | ✅ | `_doOnNoLed`, I-01·I-02·I-06(presses 1) |
| FR-05 진단 no_boot/no_agent/unknown, 부팅 관찰 | ✅ | `_classify`, `_watchBoot`, I-06 |
| FR-06 off: 정상 종료 → 끊김 완료, 업데이트 대기, 시간 초과 강제 | ✅ | `_doOffNoLed`, I-04·I-05 (업데이트 경로는 기존 코드와 같은 구조, 전용 시험 없음) |
| FR-07 off: 미접속 noop, force 규칙 | ✅ | `_offNoLed`, I-03 |
| FR-08 led_mismatch 없음 | ✅ | `_checkLed`, I-01·I-02 |
| FR-09 규격 ledIgnored, no_boot | ✅ | `spec/catalog.json` → `npm run gen`, `assertClean` |
| FR-10 `power.powerRelay` | ✅ | 기본 1, `config.example.json` |
| FR-11 문서 | ✅ | API.md, SERVER.md, CHANGELOG, README, package.json 1.4.0 |

## 3. Success Criteria

| 기준 | 결과 |
|------|------|
| LED 켜짐/꺼짐 고정 모두 끈 뒤 다시 켜기 | ✅ I-01, I-02 |
| ping 응답 중 누르지 않음 | ✅ I-03 |
| 설정 안 한 방 기존 시험 무수정 통과 | ✅ 기존 104건 통과(시험 파일 수정 없음), I-07 |
| `npm run check && npm test` | ✅ 112/112 |
| gap ≥ 90% | ✅ 96% |

## 4. Gaps / 설계와 다른 점

| 항목 | 내용 | 영향 |
|------|------|------|
| 시뮬레이터 수정 | `sim/pc.js` 길게 누름이 LED 대신 실제 전원을 봄(LED 고장 흉내 시험용). 설계에 없던 변경, 기존 시험 영향 없음 | 낮음 |
| 업데이트 종료 경로 시험 | LED 무시 방의 `os_updating` 대기는 전용 시험 없음 | 낮음 (다음 반복 후보) |
| 녹화장비 미등록 방 | `_doOnNoLed`가 `{pc:"unknown", verified:false}` 반환(설계는 `pc:"on"`). LED도 에이전트도 없어 확인할 수 없으므로 의도적으로 바꿈 | 없음 |

## 5. Runtime / 남은 확인

- 실기기: 현장 LED 고장 방에 허브 설정 적용 후 끄기·켜기 확인(현장).
- 알려진 한계: PC 방화벽이 ping을 막는 방, SmartDeck v3.1 미만(ping 없음)은 "켜져 있는데 에이전트만 없음" 보호가 없음(문서화).
- 녹화장비가 끊긴 직후 Windows가 아직 종료 중일 때 켜면 누름이 무시되고 `no_boot`가 날 수 있음(다시 켜면 됨).

**Match Rate: 96%**
