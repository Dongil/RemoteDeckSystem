---
template: plan
version: 1.0
feature: rcm-multi-encoder
date: 2026-11-23
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.3.0 (규격 + C# 녹화장비 참조 구현 + 가짜 녹화앱 + conformance)
origin: rcm-api 이슈 #20 (녹화장비 담당 100Mir, 2026-11-23)
status: draft
---

# 멀티인코더 채널별 녹화 (rcm-api #20) Planning Document

> **Summary**: 규격에는 있지만 C# 참조 구현에 빠진 채널별 녹화(`rec.start/stop {encoder:"ChN"}`, `recorder.state.encoders`)를 구현하고, 4채널 장비를 위해 `Ch4`를 규격에 추가한다(구버전 허브 보호용 `welcome.caps` "ch4").
>
> **Version**: rcm-api 1.2.0 → **1.3.0** (호환되는 추가)
> **Date**: 2026-11-23

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | 멀티인코더(Multi Encoder 5.0, 2~4채널) 장비를 허브에 붙이면 관리 화면이 채널을 지정한 녹화 요청을 보낼 때마다 C# 참조 구현이 `UNSUPPORTED`로 거절한다(실기 9건). 녹화앱의 채널별 보고(`RecState=…&Channel=N`)는 채널 구분 없이 장비 전체 `RecState`를 덮어써, 채널 하나만 녹화해도 전체 상태가 틀어진다. 규격은 `Ch3`까지라 4채널(Quad) 장비를 표현할 수 없다. |
| **Solution** | 녹화앱 보고의 `Channel`을 읽어 채널별 상태로 분리하고, `ChN_RecStart:Title:<제목>` / `ChN_RecStop`을 보낸 뒤 채널별 보고로 완료를 확인한다. `recorder.state.encoders`를 만들고, 장비 전체 `rec`는 main(채널 없는 보고)이 우선이며 main이 쉬면 채널 합산. 규격 `Encoder`에 `Ch4`를 추가하고 허브가 `welcome.caps`에 `"ch4"`를 줄 때만 `encoders.Ch4`를 보고. |
| **Function/UX Effect** | 관리 화면에서 채널별 녹화 시작·정지가 동작하고 채널 상태가 보인다. 채널을 모르는 기존 화면도 `rec`로 "녹화 중"을 본다. HD-CTR(단일 인코더)은 지금과 동일. |
| **Core Value** | 멀티인코더 제품군(2·3·4채널)을 RCM 허브로 통합 운영. 구버전 허브·단일 인코더 장비에 회귀 없음. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 멀티인코더 장비의 채널 녹화가 허브에서 전부 거절됨 + 채널 보고가 장비 전체 상태를 오염 |
| **WHO** | 허브 관리 화면 사용자, 녹화장비 담당(Multi Encoder 5.0 적용), 서버 담당(화면), 기존 HD-CTR 현장(회귀 없어야 함) |
| **RISK** | (1) 구버전 허브가 `encoders.Ch4`가 든 상태를 통째로 거부 → caps. (2) 채널 보고 형식 추정(실기 앱은 녹화장비 담당 확인) → 가짜 앱으로 이슈 형식 재현. (3) 단일 인코더 회귀 → 채널 없는 보고는 기존 경로 그대로 |
| **SUCCESS** | rcm-api check·test 통과 + 채널 시험 · conformance 채널 그룹(시뮬·C# 1단계·2단계) PASS · 단일 인코더 기존 시험 무수정 통과 · 구버전 허브(caps 없음)에 Ch4 미보고 |
| **SCOPE** | 규격(Encoder·encoders·welcome.caps) / 허브 recorderCaps 기본값 / C# Core(LegacyState 채널 분리·RecorderOps·StateBuilder) / 에이전트 caps 필터 / 가짜 녹화앱 2종 / Node 시뮬 / conformance / 문서 |

---

## 1. Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | 녹화앱 보고에 `Channel=N`(1~4)과 `RecState`가 함께 있으면 `RecState`·`RecStart_Time`을 채널 N 값으로 저장하고 장비 전체 `RecState`는 건드리지 않음 (1·2단계 공통) | High |
| FR-02 | `rec.start {encoder:"ChN"}`: 채널이 이미 녹화·일시정지 중이면 바로 성공, 아니면 `ChN_RecStart`(제목이 있으면 `ChN_RecStart:Title:<제목>`) 송신 후 채널 N `RecState=RecStart` 보고를 기다려 `{rec: 채널 상태}` 응답 | High |
| FR-03 | `rec.stop {encoder:"ChN"}`: 멈춰 있으면 바로 성공, 아니면 `ChN_RecStop` 후 `RecStop` 보고 대기 | High |
| FR-04 | `recorder.state.encoders.ChN = {state, startedAt, title}` (보고가 있었던 채널만). 채널별 일시정지·재개 보고 반영 | High |
| FR-05 | 장비 전체 `rec`: main(채널 없는 보고)이 녹화·일시정지면 main, 아니면 채널 합산(하나라도 recording → recording, 아니면 하나라도 paused → paused, 아니면 idle; startedAt = 진행 중 채널 중 가장 이른 값) | High |
| FR-06 | 규격 `Encoder`·`encoders` 키에 `Ch4` 추가 | High |
| FR-07 | `welcome.caps`(녹화장비)에 `"ch4"`: 허브 기본 `recorderCaps = ["session","ch4"]`. 에이전트는 caps에 `"ch4"`가 없으면 `encoders.Ch4`를 빼고 보고 | High |
| FR-08 | 가짜 녹화앱(`sim/legacy-hdctr.js`, `RcmLink.SampleApp`)이 `ChN_RecStart/Stop`을 받고 이슈 형식(`RecState=…&Channel=N&RecStart_Time=…`)으로 보고 | High |
| FR-09 | Node 시뮬레이터 `sim/recorder.js`가 FR-05 합산 규칙·Ch4 caps를 따름 | Medium |
| FR-10 | conformance 녹화장비 선택 그룹 `channel`(`--channels Ch1,Ch2`): 채널 시작 → 그 채널만 recording, 다른 채널 영향 없음, `rec` 합산, 정지 → idle, 반복 요청 성공 | High |
| FR-11 | 문서: API.md(Encoder·encoders·`rec` 합산 규칙·caps), RECORDER.md(제한 문구 삭제, 채널 명령·보고 형식, Multi Encoder 실기 항목), CHANGELOG v1.3.0 | Medium |
| FR-12 | 보류 중인 `diag-no-agent-msg`(허브 `no_agent` 문구) 함께 반영 | Low |
| FR-13 | (추가 요청) `power.actual`: 녹화장비 에이전트가 접속해 있으면 `on`(LED 무관), 아니면 SmartDeck LED, 둘 다 없으면 `unknown`. SmartDeck은 전원을 켜고 부가 정보를 보고하는 역할, SmartDeck 고장·미접속에도 PC를 직접 켜면 방을 정상 사용 | High |
| FR-14 | 에이전트 접속 중 `power.on` = noop(켜져 있는 PC의 버튼을 누르지 않음) | High |
| FR-15 | LED 꺼짐 + 에이전트 접속 중 `power.off` = 에이전트로 정상 종료(noop 아님), `force`는 거부 | High |
| FR-16 | 진단 `led_mismatch`(에이전트 접속 중 LED 꺼짐 지속, LED 복구 시 해제) | Medium |

## 2. Out of Scope

- 채널별 `rec.pause/resume` op (규격에 `encoder` 없음 — 상태 보고만 반영)
- 채널별 오버레이(제목 외 subject/teacher 등) — Multi Encoder 형식 미확인
- 실제 Multi Encoder 5.0 앱 실기 시험(녹화장비 담당, 이 태그 기준 PR/확인)
- 허브 관리 화면(PHP) 채널 UI(서버 담당)

## 3. Success Criteria

- [ ] FR-01 ~ FR-12
- [ ] `npm run check && npm test` 통과, 기존 시험 무수정 통과(단일 인코더 회귀 0)
- [ ] conformance `channel` 그룹: Node 시뮬 · C# 1단계(가짜 HD-CTR) · C# 2단계(SampleApp) 모두 PASS
- [ ] caps 없는 허브(구버전 흉내)에 Ch4 상태 미송신, 거부 0
- [ ] C# 빌드 0/0
- [ ] gap-analysis ≥ 90%

## 4. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| 구버전 허브의 Ch4 상태 거부 | High | caps `"ch4"` 필터 |
| 실제 앱 보고 형식 차이(키 이름·Channel 번호 범위) | Medium | 이슈 형식 그대로 구현, RECORDER.md 실기 확인 항목 + 녹화장비 담당 확인 |
| 단일 인코더 회귀 | High | 채널 없는 보고는 기존 경로, 기존 시험 무수정 통과로 확인 |
| `Channel` 키가 다른 보고(미리보기 등)에 쓰일 가능성 | Medium | `RecState`와 같이 올 때만 채널로 분리 |

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-23 | Initial (사용자 결정: Ch4 + caps, main 우선 합산, PDCA 문서) | KDI |
| 0.2 | 2026-11-23 | FR-13~16 추가: SmartDeck 없이도 쓸 수 있는 방(전원 판단), 같은 v1.3.0, 새 진단 코드 | KDI |
