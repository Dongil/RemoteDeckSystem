---
template: plan
version: 1.0
feature: rcm-led-ignore
date: 2026-11-28
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.4.0 (허브 전원 작업 + 규격 PowerState/PowerDiag + 문서)
origin: 현장 적용 중 발견 (2026-11-28) — 일부 장비 PC LED 인식 불가로 켜기 명령이 나가지 않음
status: archived
---

# PC LED 없이 전원 운영 (LED 무시 모드) Planning Document

> **Summary**: PC LED를 믿을 수 없는 방은 허브 설정으로 "LED 무시"를 켠다. 그 방은 녹화장비 에이전트 접속 여부만으로 켜짐/꺼짐을 판단하고, 켜기는 LED 확인 없이 전원 버튼을 한 번 눌러 에이전트 접속으로 확인한다.
>
> **Version**: rcm-api 1.3.1 → **1.4.0** (호환되는 추가)
> **Date**: 2026-11-28

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | 현장 일부 SmartDeck이 PC LED를 인식하지 못한다. LED가 켜짐으로 붙어 있으면 녹화장비를 꺼도 서버 웹은 계속 "켜짐"이고, `power.on`은 "이미 켜져 있음"(noop)으로 버튼을 누르지 않으며 덱도 스스로 누르기를 거부한다. LED가 꺼짐으로 붙어 있으면 버튼은 누르지만 15초 안에 LED가 안 켜져 실패하고, 재시도 누름이 부팅 중인 PC를 끌 수 있다. |
| **Solution** | 허브 설정 `power.ignoreLed`(`true` = 전체, 방 ID 목록 = 그 방만). 그 방은 `actual` = 에이전트 접속이면 `on`, 아니면 `off`. `power.on`은 LED 대신 덱 ping으로 "켜져 있는데 에이전트만 없음"을 막고, 전원 릴레이를 짧게 한 번 펄스(`deck.relay.pulse`) 뒤 에이전트 접속을 기다린다(재시도 누름 없음). `power.off`는 에이전트 정상 종료 → 접속 끊김으로 완료, 시간 초과면 길게 눌러 강제 종료. |
| **Function/UX Effect** | LED가 고장 난 방도 녹화장비에서 끄면 서버 웹이 꺼짐으로 바뀌고, 다시 켜기가 된다. 덱 펌웨어는 바꾸지 않아도 된다(기존 v3.1.x의 `deck.relay.pulse` 사용). LED가 정상인 방은 지금과 동일. |
| **Core Value** | 현장 LED 배선 상태와 무관하게 방을 운영. 펌웨어 OTA 없이 허브 설정만으로 적용. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | LED 인식이 안 되는 방은 끈 뒤 다시 켤 수 없음(현장 운영 중단) |
| **WHO** | 현장 관리자·사용자(서버 웹으로 켜고 끔), 서버 담당(허브 설정), SmartDeck 설치 담당 |
| **RISK** | (1) LED 없이 누르면 켜져 있는 PC(에이전트만 멈춤)를 꺼 버림 → 덱 ping이 응답하면 누르지 않음. (2) 재시도 누름이 부팅 중 PC를 끔 → 재시도 없음. (3) LED 정상 방 회귀 → 설정한 방만 새 경로 |
| **SUCCESS** | 설정한 방: 녹화장비 종료 → `actual=off` → `power.on`이 한 번 눌러 켬 → 에이전트 접속으로 완료. LED가 켜짐/꺼짐 어느 쪽으로 붙어도 동작. 설정 안 한 방 기존 시험 무수정 통과 |
| **SCOPE** | 허브 `hub/power.js`·`hub/hub.js`(actualOf) / 설정 `power.ignoreLed`, `power.powerRelay` / 규격 `PowerState.ledIgnored`, `PowerDiag.code` `no_boot` / 시험 / 문서(API.md, SERVER.md, CHANGELOG) |

---

## 1. Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | 허브 설정 `power.ignoreLed`: `false`(기본) \| `true`(모든 방) \| `["방ID", ...]` | High |
| FR-02 | LED 무시 방의 `power.actual`: 에이전트 접속 중 `on`, 아니면 `off` (SmartDeck 접속·LED 값 무관) | High |
| FR-03 | `power.on`: 에이전트 접속 → noop. SmartDeck 미등록 → `UNSUPPORTED`, 미접속 → `OFFLINE`. 덱 ping이 PC 응답(`peer.net=up`, `peerCheck`≠`unreliable`) → 버튼을 누르지 않고 `DEVICE_ERROR` + 진단 `no_agent` | High |
| FR-04 | `power.on` 작업: `deck.relay.pulse {relay: powerRelay, ms: pressMs}` 한 번 → `booting` → `bootWaitMs` 안에 에이전트 접속이면 성공(`{pc:"on", verified:true}`). 재시도 누름 없음 | High |
| FR-05 | 부팅 실패 진단(LED 없음): ping 응답 → `no_agent`, ping 무응답 → 새 코드 `no_boot`(전원이 안 켜졌거나 OS가 안 올라옴), ping 판단 불가 → `unknown`. 부팅 관찰(`bootWatchMs`)과 늦은 부팅 `late_boot`는 그대로 | High |
| FR-06 | `power.off`: 에이전트 접속 → `sys.shutdown` → 접속 끊김으로 완료(`verified:false`). `os_updating`이면 `updateWaitMs`까지 대기. 시간 초과 + 덱 접속 → 길게 펄스(`forcePressMs`)로 강제 종료 | High |
| FR-07 | `power.off`: 에이전트 미접속 → noop `{pc:"off"}`. `force:true` → 에이전트 접속 중이면 바로 길게 펄스, 아니면 `DEVICE_ERROR`(켜져 있는지 알 수 없음) | Medium |
| FR-08 | LED 무시 방은 `led_mismatch` 진단을 만들지 않음 | Medium |
| FR-09 | 규격: `PowerState.ledIgnored` (boolean, 이 방이 LED를 무시하는지), `PowerDiag.code`에 `no_boot` | Medium |
| FR-10 | 설정 `power.powerRelay`(기본 1, RemoteDeck_PC 전원 릴레이) | Low |
| FR-11 | 문서: API.md(5.1 `actual`, 6.2 `power.on`/`off` 표, 진단 표), SERVER.md(설정 안내, 대상 방 고르는 법: `led_mismatch`·홈 PC 상태), CHANGELOG v1.4.0 | Medium |

## 2. Out of Scope

- 허브가 LED 고장을 스스로 판단해 자동 전환(종료 중 멈춤과 구별이 어려워 제외, 사용자 결정)
- DB(`tbl_lecture`) 컬럼으로 설정(서버 담당과 협의 뒤)
- SmartDeck 펌웨어 변경(덱 자체 HTTP/MQTT 전원 켜기의 LED 확인 포함)
- 서버 웹 화면 표시(서버 담당: `ledIgnored`로 안내 가능)

## 3. Success Criteria

- [ ] FR-01 ~ FR-11
- [ ] LED가 켜짐으로 붙은 방·꺼짐으로 붙은 방 모두: 녹화장비 종료 → `actual=off` → `power.on` 한 번 누름 → 에이전트 접속 → 성공
- [ ] ping 응답 중(에이전트만 없음) `power.on`은 누르지 않음
- [ ] 설정 안 한 방: 기존 시험 무수정 통과
- [ ] `npm run check && npm test` 통과
- [ ] gap-analysis ≥ 90%

## 4. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| 켜져 있지만 에이전트가 멈춘 PC를 눌러 끔 | High | 덱 ping 응답이면 누르지 않음. ping 차단 PC(`unreliable`)는 보호 못 함 → 문서에 명시 |
| 부팅 중 재시도 누름 | High | 재시도 없음, 실패는 진단으로 |
| 강제 종료 길게 누름이 꺼진 PC를 켬 | Medium | 에이전트 접속 중일 때만 길게 누름 |
| 전원 릴레이 번호가 다른 장비 | Low | `power.powerRelay` 설정 |
| 설정 변경은 허브 재시작 필요 | Low | SERVER.md에 명시 |

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-28 | Initial (사용자 결정: 허브 설정, v1.3.1 뒤 별도 v1.4.0, PDCA 문서) | KDI |
