---
template: design
version: 1.0
feature: rcm-led-ignore
date: 2026-11-28
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.4.0
status: archived
---

# PC LED 없이 전원 운영 (LED 무시 모드) Design Document

| Key | Value |
|-----|-------|
| **WHY** | LED 인식이 안 되는 방은 끈 뒤 다시 켤 수 없음(현장 운영 중단) |
| **WHO** | 현장 관리자·사용자, 서버 담당(허브 설정), SmartDeck 설치 담당 |
| **RISK** | 켜져 있는 PC를 눌러 끔 → ping 확인 / 재시도 누름 없음 / LED 정상 방 회귀 없음 |
| **SUCCESS** | 녹화장비 종료 → off → power.on 한 번 눌러 켬 → 에이전트로 확인. 기존 시험 무수정 |
| **SCOPE** | hub/power.js, hub/hub.js, spec, test, docs |

---

## 1. Overview

선택한 방식: **허브 설정(방 단위 또는 전체)**. 덱 펌웨어는 그대로 두고, 이미 있는 `deck.relay.pulse`(LED 확인 없이 릴레이 펄스)로 버튼을 누른다. 덱의 `pc.powerOn`/`pc.forceOff`는 LED를 먼저 확인해 "이미 켜짐"이면 누르지 않으므로 LED 무시 방에는 쓸 수 없다.

검토한 다른 안:
- 허브 자동 판단: 종료 중 멈춤(LED 켜짐이 맞음)과 LED 고장을 구별 못 함 → 제외(사용자 결정)
- 덱 `pc.powerOn {ignoreLed}` 추가: 현장 덱 OTA 필요 → 제외

## 2. 설정

```json
"power": {
  "ignoreLed": ["XHR131258"],   // false(기본) | true(모든 방) | 방 ID 목록
  "powerRelay": 1               // 전원 버튼 릴레이 (RemoteDeck_PC = 1)
}
```

`PowerController.ledIgnored(roomId)` = `cfg.ignoreLed === true || (Array.isArray(cfg.ignoreLed) && cfg.ignoreLed.includes(roomId))`.
룸 초기 상태와 `power` 패치에 `ledIgnored`를 넣어 웹이 알 수 있게 한다(규격 `PowerState.ledIgnored`).

## 3. 상태 판단 (`hub.js` actualOf)

| 조건 | 기존 | LED 무시 방 |
|------|------|------------|
| 에이전트 접속(live) | on | on |
| 에이전트 없음, 덱 접속 | LED 값 | **off** |
| 에이전트 없음, 덱 없음 | unknown | **off** |

## 4. 전원 작업 (`power.js`)

### 4.1 `on(roomId)` (LED 무시 방)

```
_busy
에이전트 접속            → noop {pc:"on"}
deck 미등록              → UNSUPPORTED
deck 미접속              → OFFLINE
_pcAnswers(room)         → diag no_agent, DEVICE_ERROR "PC가 네트워크에 응답함(켜져 있음): 녹화장비 에이전트 점검. 버튼을 누르지 않음"
job power.on → _doOnNoLed
```

`_pcAnswers(s)` = `s.deck.state.peer && s.deck.state.peer.net === 'up' && s.power.peerCheck !== 'unreliable'`.

`_doOnNoLed(job)`:
1. `sendReq(deck, 'deck.relay.pulse', {relay: powerRelay, ms: pressMs})`
2. 녹화장비 미등록 → `{pc:'on', verified:false}`
3. `_phase booting` → `waitRoom(recorder.online, bootWaitMs)`
4. 성공 → `{pc:'on', verified:true}`. 실패 → `_classify` 진단 + `_watchBoot` + `TIMEOUT` (재시도 누름 없음)

### 4.2 `off(roomId, {force})` (LED 무시 방)

```
_busy
에이전트 없음: force → DEVICE_ERROR "LED 미사용 방: 녹화장비가 없으면 켜져 있는지 알 수 없어 강제 종료 불가"
              아니면 noop {pc:"off"}
force + 덱 없음 → OFFLINE
job power.off → _doOffNoLed(job, force)
```

`_doOffNoLed(job, force)`:
- `force`가 아니면: `sys.shutdown` → `waitRoom(!_rec || phase=updating, shutdownWaitMs)`. 끊김 → `{pc:'off', forced:false, verified:false}`. `updating`이면 `updateWaitMs`까지 끊김 대기. 그래도 접속 중이면 진단(`shutdown_timeout`/`update_timeout`).
  - 판단은 live 접속(`hub.deviceConn`)으로: `recorder.online`은 `offlineGraceMs` 늦게 바뀜. `waitRoom` 술어에서 `this._rec(roomId)`를 본다(끊김 때 `refreshActual`이 room 갱신을 일으켜 waiter가 깨어남).
- 강제: 덱 없음 → OFFLINE. `deck.relay.pulse {ms: forcePressMs}` → `waitRoom(!_rec, forceWaitMs)` → `{pc:'off', forced:true, verified:false}`. 끊기지 않으면 `DEVICE_ERROR`.

### 4.3 진단

`_classify(roomId)` LED 무시 방:

| ping (`peer.net`) | 진단 |
|------|------|
| 없음/`unknown`, `peerCheck=unreliable` | `unknown` |
| `down` | **`no_boot`** "PC가 켜지지 않았거나 OS가 올라오지 않음(LED 미사용 방이라 구별 불가): 전원·전원 버튼 배선, BIOS/CMOS·부팅 장치 점검" |
| `up` | `no_agent` |

`_watchBoot`의 "`deck.state.pc !== 'on'`이면 중단"은 LED 무시 방에서 건너뛴다(대신 desired가 off가 되거나 새 작업이 시작되면 중단: 기존 token 방식).
`_checkLed`: LED 무시 방은 타이머를 만들지 않고, 남은 `led_mismatch`는 지운다.

## 5. 규격

- `PowerState.ledIgnored: boolean` "(v1.4.0) 허브 설정 power.ignoreLed로 이 방은 PC LED를 쓰지 않음. actual은 녹화장비 접속 여부"
- `PowerDiag.code` + `no_boot`
- `PowerState.actual` 설명에 LED 무시 방 추가

## 6. 시험 (`test/led-ignore.test.js`)

| ID | 시나리오 |
|----|----------|
| I-01 | LED 켜짐 고정: 녹화장비 종료 → `actual=off` → `power.on` 한 번 펄스 → 에이전트 접속 → `job.done ok`, presses 1 |
| I-02 | LED 꺼짐 고정: 같은 흐름, `led_mismatch` 없음, presses 1 |
| I-03 | 에이전트 없음 + ping 응답(에이전트만 멈춤) → `power.on` `DEVICE_ERROR`, presses 0, diag `no_agent` |
| I-04 | `power.off`: 에이전트 정상 종료 → presses 0, `actual=off`, `verified:false` |
| I-05 | 종료 멈춤(hangShutdown): `shutdownWaitMs` 뒤 길게 펄스 → off |
| I-06 | 부팅 실패(deadButton): `no_boot`, 재시도 누름 없음(presses 1) |
| I-07 | 방 목록 설정: 목록에 없는 방은 기존 동작(LED 켜짐이면 noop), `ledIgnored` false/true |

## 7. 문서

API.md 5.1·6.2·진단 표, SERVER.md 설정 안내(대상 방 고르기: 홈 PC 상태가 실제와 다른 방, `led_mismatch`가 뜨는 방 / 허브 재시작 필요 / ping 차단 PC는 보호 안 됨), CHANGELOG v1.4.0, package.json 1.4.0, handoff/README.md.

## 11. Implementation Guide

1. `hub/power.js`: `ledIgnored`, `_pcAnswers`, `on`/`off` 분기, `_doOnNoLed`, `_doOffNoLed`, `_classify`, `_watchBoot`, `_checkLed`, `no_boot` 문구, 기본값 `ignoreLed:false, powerRelay:1`
2. `hub/hub.js`: `actualOf` 분기, 룸 초기 `power.ledIgnored`
3. `spec/asyncapi.json` + API.md
4. `test/led-ignore.test.js`
5. 문서·버전

### 11.3 Session Guide

한 세션(모듈 1개: hub 전원 + 규격 + 시험 + 문서).
