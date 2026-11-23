---
template: design
version: 1.0
feature: rcm-multi-encoder
date: 2026-11-23
author: KDI
component: rcm-api v1.3.0
architecture: 공통 라이브러리 확장 (LegacyState 채널 분리 + RecorderOps/StateBuilder + 에이전트 caps 필터)
status: draft
---

# 멀티인코더 채널별 녹화 Design Document

> **Planning Doc**: [rcm-multi-encoder.plan.md](plan.md)

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 멀티인코더 장비의 채널 녹화가 허브에서 전부 거절됨 + 채널 보고가 장비 전체 상태를 오염 |
| **WHO** | 허브 관리 화면 사용자, 녹화장비 담당, 서버 담당, 기존 HD-CTR 현장 |
| **RISK** | 구버전 허브 Ch4 거부 / 실제 앱 보고 형식 / 단일 인코더 회귀 / `Channel` 키 오인 |
| **SUCCESS** | check·test 통과 · conformance channel(시뮬·C# 1·2단계) PASS · 기존 시험 무수정 · caps 없으면 Ch4 미보고 |
| **SCOPE** | 규격 / 허브 caps / C# Core / 에이전트 필터 / 가짜 앱 2종 / Node 시뮬 / conformance / 문서 |

## 1. 구조 결정

C# 녹화장비 코드는 1단계(에이전트가 HTTP로 앱과 통신)와 2단계(앱 안의 RcmLink) 모두 `RcmRecorder.Core`의 `LegacyState` → `RecorderOps` → `RecorderStateBuilder`를 쓴다. 채널 처리를 **이 공통 라이브러리에 넣어** 두 단계를 한 번에 해결한다. Ch4 필터는 허브와 대화하는 쪽(에이전트 2종의 `Current()`)에만 둔다(2단계 앱은 허브 caps를 모름).

## 2. 규격 (catalog)

- `defs.Encoder.enum`: `["main","Ch1","Ch2","Ch3","Ch4"]`
- `defs.RecorderState.properties.encoders.propertyNames.enum`: `["Ch1","Ch2","Ch3","Ch4"]`
- `frames.welcome.properties.caps.description`: 녹화장비 `"ch4"` = `encoders.Ch4` 보고 가능
- `RecorderState.rec` 설명: main 우선, 없으면 채널 합산
- 허브: `recorderCaps` 기본 `["session","ch4"]`

## 3. C# 공통 라이브러리 (`RcmRecorder.Core`)

### 3.1 `LegacyState.Apply` — 채널 분리

보고 pairs에 `Channel`(정수 1~4, `Ch2` 형식도 허용)과 `RecState`가 **함께** 있으면, 그 보고의 녹화 키를 채널 키로 바꿔 저장한다.

| 원래 키 | 저장 키 |
|---------|---------|
| `RecState` | `Ch{N}.RecState` |
| `RecStart_Time` | `Ch{N}.RecStart_Time` |
| `Channel` | (저장 안 함) |
| 그 밖의 키 | 그대로(전역) |

`RecState` 없이 `Channel`만 오는 보고(예: 미리보기)는 지금과 같이 전역 저장.

### 3.2 `RecorderOps`

- `ChannelOf(encoder)`: `"ChN"` → N(1~4), `main` → 0, 그 외 BAD_REQUEST
- `ChRecState(n)` = `Get("Ch{n}.RecState") ?? "RecStop"`, `ChRecording(n)` = RecStart/RecContinue/RecPause
- `rec.start {encoder:ChN}`: 녹화 중이면 그대로 성공. 아니면 `_facts.ChannelTitles[n] = title`, 명령 `Ch{n}_RecStart` + (`title` 있으면 `:Title:<LegacyText.Arg(title)>`), `Ch{n}.RecState` 보고 = `RecStart` 대기(15초) → `{rec: encoders.ChN}`
- `rec.stop {encoder:ChN}`: 멈춰 있으면 성공, 아니면 `Ch{n}_RecStop`, `RecStop` 보고 대기(30초)
- main 경로는 그대로

### 3.3 `RecorderStateBuilder`

- `AgentFacts.ChannelTitles`(Dictionary<int,string>)
- `encoders`: N=1..4 중 `Ch{N}.RecState`가 있는 채널만 `{state, startedAt, title}`
- `rec`: main `RecState`가 녹화·일시정지면 지금과 같음. 아니면 encoders 합산(recording > paused > idle, startedAt = 진행 중 채널 최소, title 없음)

### 3.4 에이전트 Ch4 필터

`AgentUtil.ForHub(JObject state, JObject welcome)`: welcome `caps`에 `"ch4"`가 없으면 `state.encoders.Ch4` 제거(encoders가 비면 키 제거). `RecorderAgent.Current()`, `PipeRecorderAgent.Current()` 끝에서 적용. `welcome` 직후 full 상태도 이 함수를 거침(`LastWelcome`이 FullState 호출 전에 설정됨).

## 4. 가짜 녹화앱·시뮬레이터

- `sim/legacy-hdctr.js`(1단계 가짜 UCCTopXeno): `Ch1_RecStart`~`Ch4_RecStart`(`:Title:x` 허용), `ChN_RecStop` → `RecState=RecStart&Channel=N&RecStart_Time=…` / `RecState=RecStop&Channel=N`
- `RcmLink.SampleApp`(2단계): 같은 명령·보고
- `sim/recorder.js`(Node 시뮬): main 우선 합산을 `rec`에 반영, caps에 `"ch4"` 없으면 Ch4 미보고

## 5. conformance

녹화장비 선택 그룹 `channel`, 옵션 `--channels Ch1,Ch2`(기본 `Ch1,Ch2`):
1. 모든 지정 채널 `rec.stop` → idle
2. 첫 채널 `rec.start {title}` → 응답 recording, `encoders.Ch1.state=recording` 보고, 다른 채널 idle, `rec.state=recording`(main이 쉬고 있으면)
3. 같은 채널 `rec.start` 반복 → 성공
4. 첫 채널 `rec.stop` → idle, `rec` 원상
5. (선택) 둘째 채널도 같은 순서

## 6. 시험

| # | 내용 |
|---|------|
| M-01 | Node: caps 있는 허브에 Ch4 보고, caps 없는 허브엔 Ch4 미보고 |
| M-02 | conformance channel: Node 시뮬 PASS |
| M-03 | C# 1단계(가짜 HD-CTR) conformance channel PASS |
| M-04 | C# 2단계(SampleApp) conformance channel PASS |
| M-05 | 기존 시험 무수정 통과(단일 인코더 회귀 0) |
| M-06 | 채널 보고가 main `rec`를 오염시키지 않음(채널 녹화 중 main rec.stop 응답 = 합산 상태 그대로, main 명령 안 보냄) |

## 6A. 전원 판단 (FR-13~16, 추가 요청)

재현(시뮬): ① SmartDeck 미접속에서 손으로 켜면 녹화는 되지만 `actual=unknown`, `power.on`=OFFLINE. ② LED 감지 고장(꺼짐 표시)이면 `actual=off`, `power.off`=noop(원격으로 못 끔), `power.on`=켜져 있는 PC의 버튼을 눌러 종료시킴.

- 허브 `actualOf(roomId)`: **실제 연결**(`devices`) 기준 녹화장비 접속 → `on`; 아니면 SmartDeck 접속 시 LED, 미접속이면 `unknown`. `updateRoom`·`replaceDeviceState`마다 재계산, 접속/끊김 즉시 `refreshActual`(유예 없음). `deckPowerPatch` 제거.
- `power.on`: `_rec()` 있으면 `{noop:true, pc:"on"}`(SmartDeck 등록·접속 검사보다 먼저).
- `power.off`: SmartDeck 접속·LED off·`_rec()` 있음 = `ledWrong` → `_doOffUnverified`, `force`는 DEVICE_ERROR.
- `_checkLed`: `_rec()` + deck online + LED off가 `ledMismatchMs`(10초) 지속 → diag `led_mismatch`; LED on 보고 시 해제.
- 규격: `PowerDiag.code` += `led_mismatch`, `PowerState.actual` 설명.
- 시험 L-01~04 (`test/deck-independent.test.js`).

## 7. 문서

API.md(5.2 encoders·rec 합산, 6.3 encoder Ch4, 3.3 caps 표), RECORDER.md(제한 문구 → 채널 명령·보고 형식·실기 확인 항목), SERVER.md(recorderCaps), CHANGELOG v1.3.0(+ no_agent 문구).

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-23 | Initial | KDI |
| 0.2 | 2026-11-23 | 6A 전원 판단 추가 | KDI |
