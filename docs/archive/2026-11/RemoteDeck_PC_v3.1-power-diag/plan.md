---
template: plan
version: 1.0
feature: rcm-power-diag
date: 2026-11-11
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api (규격·허브·에이전트 참조 코드) + RemoteDeck_PC (deck)
target: rcm-api v1.1.0 / RemoteDeck_PC v3.1.0
status: draft
---

# RCM 전원 진단 (업데이트 종료 대기 + 부팅 실패 보고) Planning Document

> **Summary**: 종료 중 Windows 업데이트가 있으면 끝날 때까지(상한 3시간) 강제 종료하지 않고 기다리며, 켤 때 PC가 OS까지 올라오지 못한 경우(BIOS/CMOS 대기 등)를 deck의 짝 PC 네트워크 확인으로 구분해 허브에 상세 보고한다.
>
> **Project**: RemoteDeckSystem + rcm-api
> **Version**: rcm-api 1.0.6 → **1.1.0** (호환되는 추가), RemoteDeck_PC 3.0.0 → **3.1.0**
> **Author**: KDI
> **Date**: 2026-11-11
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | 허브는 `sys.shutdown` 후 240초가 지나면 무조건 강제 종료해, 종료 중 Windows 업데이트를 끊어 버린다. 켤 때는 LED만 보고 판단해 "PC는 켜졌지만 녹화장비가 240초 안에 접속하지 않음" 하나로만 보고하므로 BIOS/CMOS 대기·부팅 장치 오류·에이전트 미실행을 구분할 수 없다. |
| **Solution** | 에이전트가 종료 직전 보류 업데이트를 확인해 `os_updating` 단계를 보고하면 허브는 강제 종료 타이머를 멈추고 LED가 꺼질 때까지(상한 3시간, 설정) 기다린다. deck은 짝 녹화장비 PC(등록 IP)의 네트워크 응답을 확인해 `deck.state`로 보고하고, 허브는 부팅 실패 시 LED·네트워크·에이전트 조합으로 원인을 추정해 방 상태(`power.diag`)와 오류 문구에 남긴다. |
| **Function/UX Effect** | 업데이트가 강제 종료로 끊기지 않는다. 부팅 실패 시 RCM 웹이 "OS 미기동(BIOS/CMOS 대기 의심)" / "Windows 켜짐, 에이전트 미실행" / "전원 안 켜짐"을 구분한 점검 메시지를 띄울 수 있다. |
| **Core Value** | 원격 전원 제어가 PC를 망가뜨리지 않고(업데이트 보호), 현장 출동 전에 원인을 좁힌다. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 종료 중 업데이트 강제 중단 위험 제거 + 부팅 실패 원인(BIOS/CMOS·OS·에이전트) 구분 보고 |
| **WHO** | RCM 웹 운영자(점검 메시지 소비), 허브 운영(서버 담당), 녹화장비 에이전트(녹화장비 담당), SmartDeck(RemoteDeck_PC) |
| **RISK** | (1) 규격 키 추가가 `additionalProperties:false`라 구버전 허브가 새 deck.state를 위반으로 거부 → **허브 먼저 배포**. (2) 업데이트 보류 판정 오탐/누락 → 상한 3시간 + 기존 240초 경로 유지. (3) 짝 PC 네트워크 확인이 방화벽/네트워크 구성에 막힘 → ARP 우선. (4) deck이 OS 화면을 볼 수 없으므로 원인은 "추정" |
| **SUCCESS** | rcm-api `npm run check && npm test` 통과 + 새 시나리오 시험 · conformance(deck·recorder) PASS · 실기: 업데이트 보류 상태 종료에서 강제 종료 없음, PC를 BIOS 화면에 세워 "OS 미기동" 보고 · RemoteDeck_PC 빌드 성공·conformance PASS |
| **SCOPE** | 규격(catalog) / 허브 power.js·설정 / 에이전트 참조 코드(C#)·시뮬레이터 / deck 펌웨어(lib/RcmHub 업스트림 + HubBridge) / 문서·conformance |

---

## 1. Overview

### 1.1 Purpose

1. 종료 중 Windows 업데이트를 강제 종료로 끊지 않는다.
2. 켤 때 PC가 정상 부팅하지 못한 경우 원인을 추정해 허브에 상세 보고한다.

### 1.2 Background

- 현재 `hub/power.js` `_doOff`: `sys.shutdown {deadlineSec:240}` → LED off를 240초 대기 → `pc.forceOff`. 에이전트의 `recorder.shutdown` 단계는 구독자에게 전달만 하고 타이머에 영향 없음.
- 현재 `_doOn`: 짧게 누름 → LED on 확인 → 에이전트 접속을 `bootWaitMs`(240초) 대기 → 실패 시 `TIMEOUT "PC는 켜졌지만 녹화장비가 240초 안에 접속하지 않음"`.
- deck은 welcome의 `pair.peer.regIp`로 짝 녹화장비 PC의 등록 IP를 이미 받는다(같은 LAN).
- 에이전트 v1.0.6에 종료 전 지연(`shutdownDelaySec`) 등이 생기면서 종료 시간 관리가 중요해짐(사용자 문의, 2026-11-11).

### 1.3 Related Documents

- rcm-api: `hub/power.js`, `spec/catalog.json`, `handoff/RECORDER.md` 6장, `handoff/SMARTDECK.md`, `docs/API.md`
- RemoteDeck_PC: `docs/archive/2026-11/RemoteDeck_PC_v3.0/` (rcm-hub-v3.0)

---

## 2. Scope

### 2.1 In Scope

- [ ] 규격 v1.1.0: `recorder.shutdown.phase`에 `os_updating`, `DeckState.peer`, `PowerState.phase`에 `updating`, `PowerState.diag`
- [ ] 허브: 업데이트 종료 대기(상한 `updateWaitMs` 3시간), 부팅 실패 진단(`diag`), 직전 종료 업데이트 기억
- [ ] 에이전트 참조 코드(C# RcmRecorderAgent): 보류 업데이트 감지 → `os_updating` 보고
- [ ] 시뮬레이터(sim/recorder.js, sim/deck.js)·시나리오 시험·conformance 보강
- [ ] RemoteDeck_PC v3.1.0: 짝 PC 네트워크 확인 → `deck.state.peer` 보고 (lib/RcmHub는 rcm-api에서 고친 뒤 동기화)
- [ ] 문서: API.md, OPS.md(생성), RECORDER.md, SMARTDECK.md, CHANGELOG, RemoteDeck_PC Manual

### 2.2 Out of Scope

- deck 웹 UI·IntegrateController 표시 (사용자 결정: 허브에만 상세 보고, RCM 웹이 필요 시 점검 메시지로 사용)
- 부팅 대기 시간 연장 (사용자 결정: 평소처럼 4분, 판단 문구에만 업데이트 가능성 표시)
- 실제 UCCTopXeno(녹화앱) 2단계 통합, RCM 웹(PHP) 화면 작업
- 화면 캡처 등으로 BIOS 메시지 직접 판독

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | 규격: `recorder.shutdown.phase` enum에 `os_updating`(보류 업데이트를 설치하며 Windows 종료 시작) 추가 | High |
| FR-02 | 에이전트: Windows 종료 직전 보류 업데이트 판정(RebootRequired·RebootPending 등) → 있으면 `os_updating`, 없으면 기존 `os_shutdown` | High |
| FR-03 | 허브: `os_updating`을 받으면 240초 강제 종료 타이머를 멈추고 LED off를 `updateWaitMs`(기본 3시간, 설정)까지 대기. `power.phase="updating"` | High |
| FR-04 | 허브: `updateWaitMs` 초과 시 기존과 같이 `pc.forceOff`, 사유 문구 "업데이트 대기 시간 초과" | Medium |
| FR-05 | 허브: 방별로 "직전 종료에 업데이트 있었음"을 기억(메모리, 허브 재시작 시 초기화 허용) | Medium |
| FR-06 | 규격: `DeckState.peer` = `{ "net": "up"\|"down"\|"unknown" }` (짝 녹화장비 PC 네트워크 응답) | High |
| FR-07 | deck: 짝 PC 등록 IP에 비동기 ICMP ping(10초 주기, 응답 1회=up, 3회 연속 무응답=down), 바뀌면 즉시 `deck.state` 패치. 짝 IP가 없으면 `unknown` (설계 C안: ARP는 lwIP 캐시 5분 잔존으로 오판 위험) | High |
| FR-08 | 허브: 부팅 대기(4분) 실패 시 LED·peer.net·에이전트 조합으로 진단 `diag.code` 결정: `no_power`(LED 안 켜짐) / `no_os`(LED on, net down) / `no_agent`(LED on, net up, 에이전트 없음) / `unknown`(deck 구버전 등 peer 없음) | High |
| FR-09 | 허브: `no_os`이고 직전 종료에 업데이트가 있었으면 문구에 "Windows 업데이트 진행 중일 수 있음" 추가(대기 시간은 그대로 4분) | Medium |
| FR-10 | 규격: `PowerState.diag` = `{code, msg, at}` 또는 null. 전원 작업 실패 시 설정, 다음 성공 시 null. `lastError.msg`에도 같은 한글 문구 | High |
| FR-11 | 하위호환: peer 없는 deck(v3.0.0)·`os_updating` 없는 에이전트(v1.0.6)와도 기존 동작 그대로 | High |
| FR-12 | 시뮬레이터·시나리오 시험·conformance: 업데이트 대기, 부팅 진단 3종, 하위호환 | High |
| FR-13 | 허브: 부팅 대기 실패 후라도 에이전트가 접속하면 `lastError`를 지우고 `diag`를 `late_boot`("부팅 완료, n분 n초 걸림")로 갱신 | High |
| FR-14 | 허브: 부팅 실패 뒤 `bootWatchMs`(기본 30분, 설정) 동안 LED·peer·에이전트 변화를 관찰해 진단을 갱신(예: BIOS 대기 → F1 → 부팅). 새 전원 작업이 시작되면 관찰 취소 | High |
| FR-15 | 허브: 종료 쪽도 진단 기록 — 240초 초과 강제 종료 `shutdown_timeout`, 업데이트 상한 초과 `update_timeout` | Medium |
| FR-16 | 허브: ping 신뢰도 교차 확인 — 에이전트 접속 중 peer.net=down이 60초 이상 지속되면 그 방을 `peerCheck=unreliable`로 표시하고 부팅 진단을 `unknown`으로 낮춤. up이 보이면 `ok` | High |
| FR-17 | 문서: HD-CTR PC 방화벽 ICMP 에코 허용 규칙을 에이전트 설치 안내(RECORDER.md)에 추가 | Medium |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement |
|----------|----------|-------------|
| 호환성 | 규격 변경은 추가만(minor) | `npm run check`, 기존 test 무수정 통과 |
| 배포 순서 | 허브 v1.1.0 → 에이전트·deck | 문서(CHANGELOG·handoff)에 명시 |
| deck 부하 | 네트워크 확인이 loop를 막지 않음(비동기), 주기 ≥ 5초 | 코드 리뷰, heartbeat 끊김 0 |
| 보안 | 새 비밀값 없음 | 리뷰 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-12 구현
- [ ] rcm-api: `npm run check && npm test` 통과 + 새 시나리오(업데이트 대기·초과, no_power/no_os/no_agent, 하위호환)
- [ ] conformance: deck(`.193`, 새 peer 검사 포함) PASS, recorder는 시뮬레이터로 `os_updating` 검사
- [ ] RemoteDeck_PC v3.1.0 `pio run` 성공, 가상 허브 연결 유지
- [ ] 실기: `.193` 짝 PC를 BIOS 설정 화면에 세운 상태로 power.on → `diag.code=no_os` 보고, 이후 부팅시키면 관찰 중 `late_boot`로 갱신
- [ ] 실기: 짝 PC Windows 켜짐·에이전트 없음 → `no_agent`
- [ ] 업데이트 종료 대기: 시뮬레이터(필수), 실제 보류 업데이트 PC(가능하면)
- [ ] 문서·CHANGELOG·Manual 반영, 배포 순서 명시

### 4.2 Quality Criteria

- [ ] gap-analysis matchRate ≥ 90%
- [ ] 빌드 0 error, 기존 시험 회귀 0

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| 구버전 허브가 새 `deck.state.peer`를 규격 위반으로 처리 | High | High | 허브 v1.1.0 먼저 배포. deck은 허브 welcome 버전/설정으로 peer 송신 여부를 정하거나, 허브 업데이트 후 OTA |
| 업데이트 보류 판정 오탐(업데이트 없는데 os_updating) | Medium | Medium | 상한 3시간 + LED가 꺼지면 즉시 완료. 판정 키는 설계에서 확정·실기 확인 |
| 업데이트 보류 판정 누락 | Medium | Medium | 기존 240초 경로 유지(현재와 같음) |
| 짝 PC가 ICMP 차단 | Medium | High | 설치 안내에 ICMP 허용 규칙 + 허브 교차 확인(FR-16)으로 오판 대신 `unknown` |
| 짝 PC가 다른 서브넷 | Medium | Low | ARP 불가 → ICMP, 그래도 불가면 `unknown`(진단 문구도 unknown) |
| 진단은 추정 | Low | - | 문구에 "의심" 사용, BIOS 설정·CMOS 배터리 점검 안내 |
| 에이전트 실제 코드는 녹화장비 담당 | Medium | - | 참조 코드·시뮬레이터·RECORDER.md 갱신으로 인계 |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change |
|----------|------|--------|
| `spec/catalog.json` (+ 생성물 OPS.md, asyncapi.json) | Spec | enum 2곳 추가, DeckState.peer, PowerState.diag |
| `hub/power.js`, `hub/config.example.json` | Hub | 업데이트 대기, 부팅 진단, 설정 키 `updateWaitMs` |
| `clients/csharp/...RcmRecorderAgent` | Agent ref | 보류 업데이트 판정, os_updating |
| `clients/esp32/lib/RcmHub/RcmDeck.*` | Deck lib | peer 상태 보고 훅 |
| `sim/*.js`, `test/*.test.js`, `tools/conformance.js` | Test | 시나리오·검사 추가 |
| `RemoteDeck_PC/src/network/HubBridge.*`, `lib/RcmHub` | Firmware | 짝 PC 네트워크 확인·보고 |

### 6.2 Current Consumers

| Resource | Consumer | Impact |
|----------|----------|--------|
| `room.state.power` | RCM 웹(구독), web-cli | `diag` 키·`updating` 단계 추가 — 클라이언트 검증 시 v1.1.0 catalog 필요 |
| `deck.state` | 허브 | peer 키 추가 — 허브 먼저 배포 |
| `recorder.shutdown` | 허브 | 새 단계 — 허브 먼저 배포 |

### 6.3 Verification

- [ ] 구버전 deck(v3.0.0)·에이전트(v1.0.6) + 신규 허브 조합 회귀
- [ ] 신규 deck + 신규 허브 조합

---

## 7. Architecture Considerations

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| 업데이트 대기 판단 주체 | 허브 / deck / 에이전트 | 에이전트가 알리고 허브가 대기 | 업데이트 여부는 OS 안에서만 알 수 있고, 강제 종료 결정은 허브가 함 |
| 업데이트 대기 상한 | 없음 / 1h / 3h | 3시간, 설정 가능 | 사용자 결정 |
| 부팅 원인 신호 | deck 짝 PC 네트워크 / 허브 ping / 화면 | deck 짝 PC 네트워크 | deck이 같은 LAN, 허브는 원격일 수 있음 |
| 네트워크 확인 방식 | ARP / ICMP / 둘 다 | ICMP + 허브 교차 확인 (설계 C안) | ARP는 lwIP 캐시가 5분 남아 BIOS 대기를 'up'으로 오판할 수 있음. 방화벽 문제는 교차 확인으로 오판 대신 unknown |
| 부팅 대기 | 업데이트 시 연장 / 그대로 | 그대로 4분 | 사용자 결정 |
| 보고 위치 | 허브 방 상태 / deck UI / IC | 허브 방 상태(`power.diag`, lastError) | 사용자 결정 |

---

## 8. Convention Prerequisites

- rcm-api: 규격 변경은 `spec/catalog.json` → `npm run gen`(생성 문서), CHANGELOG, 버전 규칙(handoff/README.md 2.3: minor), LF 고정
- RemoteDeck_PC: lib/RcmHub는 rcm-api에서 먼저 고치고 동기화(SOURCE.md), 커밋 `feat(RemoteDeck_PC): v3.1.0 — …`
- 사용자 표시 문구 한글, "RCM 허브" 표기

---

## 9. Next Steps

1. [ ] `/pdca design rcm-power-diag`
2. [ ] rcm-api 구현 → 시험 → PR
3. [ ] RemoteDeck_PC v3.1.0 구현 → 실기 → PR

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.2 | 2026-11-11 | FR-07 ICMP로 변경, FR-13~17 추가(늦은 부팅 해제, 30분 관찰, 종료 진단, ping 교차 확인, 방화벽 안내) | KDI |
| 0.1 | 2026-11-11 | Initial draft (사용자 결정: 상한 3시간 설정 가능, 허브에만 보고, 부팅 대기 4분 유지) | KDI |
