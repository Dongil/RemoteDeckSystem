---
template: report
version: 1.1
feature: rcm-multi-encoder
date: 2026-11-23
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.3.0
branch: rcm-api multi-encoder-v1.3 (PR #21, 병합 6680854) + recorder/os-session (PR #22, 병합 a3836be)
matchRate: 95
status: completed
---

# 멀티인코더 채널별 녹화 + SmartDeck 없이도 맞는 전원 판단 완료 보고

> **Status**: Complete (실제 Multi Encoder 5.0 실기는 녹화장비 담당 이월)
>
> **Project**: RemoteDeckSystem + rcm-api
> **Version**: rcm-api 1.3.0 (규격: 호환되는 추가, 태그 `v1.3.0` = `6680854`)
> **Author**: KDI
> **Completion Date**: 2026-11-23
> **PDCA Cycle**: #1 (Do 중 사용자 추가 요청 1건, Check 뒤 PR 리뷰 반영 1회)

---

## Executive Summary

### 1.1 Project Overview

| Item | Content |
|------|---------|
| Feature | rcm-multi-encoder — rcm-api 이슈 #20(C# 에이전트 채널 녹화) + 추가 요청(SmartDeck 문제와 무관하게 방 사용) |
| Start / End | 2026-11-23 / 2026-11-23 |
| 출발점 | 녹화장비 담당(100Mir) 이슈 #20: 관리 화면의 채널 지정 녹화가 C# 참조 구현에서 전부 `UNSUPPORTED`(실기 9건), 채널 보고가 장비 전체 상태를 덮어씀, 4채널 장비 표현 불가. 사용자 추가 질문: "deck 연결·네트워크 문제로 LED 보고 없이 켜지면 녹화장비 상태가 안 바뀌나? deck이 문제여도 직접 켜면 정상 사용 가능해야" |
| 결과물 | rcm-api PR #21(`6b155f9`→`b7d42ab`→`0805dc1`, 병합 `6680854`), PR #22(녹화장비 담당 `os.session`, 병합 `a3836be`), 태그 `v1.3.0`, 이슈 #20 닫힘 |

### 1.2 Results Summary

```
┌─────────────────────────────────────────────┐
│  Completion Rate: 94% (16/17 성공 기준 항목)  │
├─────────────────────────────────────────────┤
│  ✅ Complete:   FR 16/16                     │
│  ⏳ Partial:    1 (기존 시험 기대값 변경)       │
│  ❌ Cancelled:  0                            │
└─────────────────────────────────────────────┘
```

### 1.3 Value Delivered

| Perspective | Content |
|-------------|---------|
| **Problem** | 멀티인코더 장비의 채널 녹화 요청이 허브에서 모두 거절됐고, 채널 하나만 녹화해도 장비 전체 녹화 상태가 틀어졌다. 허브는 전원을 SmartDeck LED 하나로만 판단해, SmartDeck이 끊기거나 LED 선이 빠지면 켜져 있는 PC를 꺼짐으로 보이고, `power.on`으로 켜진 PC의 버튼을 눌러 꺼 버릴 수 있었다. |
| **Solution** | C# 공통 라이브러리에서 채널 보고(`RecState`+`Channel`)를 채널 상태로 분리하고 `ChN_RecStart:Title:` / `ChN_RecStop`으로 채널을 제어, `encoders.ChN`과 합산 `rec`(main 우선)를 보고. Ch4는 `welcome.caps "ch4"`로 구버전 허브 보호. 허브는 **녹화장비 에이전트 접속(실제 연결)**을 "PC 켜짐"의 가장 강한 근거로 쓰고, LED와 어긋나면 `led_mismatch`로 알림. |
| **Function/UX Effect** | 관리 화면에서 채널별 녹화 시작·정지·상태 표시 가능, 채널을 모르는 화면도 `rec`로 "녹화 중"을 봄. SmartDeck이 없어도 PC를 직접 켜면 전원 `on`, 녹화 정상, `power.on`은 버튼을 누르지 않고 성공. LED 고장도 원격 종료 가능하고 점검 문구가 남음. |
| **Core Value** | 멀티인코더(2~4채널) 제품군의 허브 통합. SmartDeck은 "켜고 부가 정보를 보고하는" 역할로 한정되어, SmartDeck 고장이 방 전체의 장애가 되지 않음. 구버전 허브·단일 인코더 회귀 0. |

---

## 1.4 Success Criteria Final Status

| 기준 | 상태 | 근거 |
|------|:----:|------|
| FR-01~12 (채널 녹화, encoders, 합산, Ch4·caps, 가짜 앱, 시뮬, conformance channel, 문서, no_agent 문구) | ✅ | analysis §2, `test/multi-encoder.test.js` M-01~05, C# conformance channel(1·2단계) |
| FR-13~16 (전원 판단, power.on noop, LED 고장 시 power.off, led_mismatch) | ✅ | `test/deck-independent.test.js` L-01~04 |
| check·test 통과 | ✅ | `npm run check` OK, `npm test` 103/103 (main 병합 후 재확인) |
| C# 빌드 | ✅ | 경고 0 / 오류 0 |
| caps 없는 허브에 Ch4 미보고·거부 0 | ✅ | M-04, C# 1단계 구버전 허브 시험 |
| 기존 시험 무수정 | ⚠️ | 동작 회귀 0. 기본 caps(`ch4`) 3곳, 에이전트 접속 중 `power.on`=noop 2건 기대값 변경(의도된 변경) |
| matchRate ≥ 90% | ✅ | 95% |

## 1.5 Decision Record Summary

| 결정 | 근거 | 결과 |
|------|------|------|
| Ch4 추가 + caps `"ch4"` (사용자 선택) | v1.1.0 `peer` 때 구버전 허브가 상태 전체를 거부한 교훈 | 구버전 허브에서 Ch1~3 정상, 거부 0 |
| `rec` = main 우선, 아니면 채널 합산 (사용자 선택) | 단일 인코더 동작 유지 + 채널을 모르는 화면 지원 | HD-CTR 회귀 0, M-02 |
| 채널 처리를 `RcmRecorder.Core`에 | 1·2단계가 같은 라이브러리 사용 | 한 번 수정으로 두 단계 해결, Ch4 필터만 에이전트 |
| 전원 판단을 같은 v1.3.0에, 새 진단 코드 (사용자 선택) | 사용자 추가 요청, 규격 enum 추가는 호환 | `led_mismatch` |
| 에이전트 접속은 **실제 연결** 기준 | `recorder.online`은 끊긴 뒤 offlineGrace만큼 늦음 → 정상 종료 직후 `actual`이 on으로 남는 회귀를 시험이 잡음 | `actualOf`/`refreshActual`, `power.on`·`_checkLed`도 `_rec()` |
| #22 먼저 병합 (리뷰어 제안) | #22가 작고 독립 | 충돌은 #21 쪽에서 해결, `HasCap`으로 통일 |

## 2. Related Documents

| 문서 | 경로 |
|------|------|
| Plan | `plan.md` |
| Design | `design.md` |
| Analysis | `analysis.md` |
| rcm-api 변경 기록 | rcm-api `CHANGELOG.md` v1.3.0 |
| 이슈 / PR | rcm-api #20, #21, #22 |

## 3. Completed Items

### 3.1 Deliverables

| 영역 | 내용 |
|------|------|
| 규격 | `Encoder`·`encoders`에 `Ch4`, 녹화장비 caps `"ch4"`, `rec` 합산 규칙, `PowerState.actual` 설명, `PowerDiag.code` `led_mismatch` |
| 허브 | `recorderCaps` 기본 `["session","ch4"]`, `actualOf`/`syncActual`/`refreshActual`(에이전트 실제 연결 우선), `power.on` noop, `power.off` LED 고장 경로, `_checkLed`, `no_agent` 문구 |
| C# Core | `LegacyState.ScopeChannel`, `RecorderOps` 채널 시작·정지·`AnyRecording`·`StopAllAsync`·`StoppedPairs`, `RecorderStateBuilder` encoders·`Combine`(제목 포함) |
| C# 에이전트 | `AgentUtil.ForHub`(`HasCap("ch4")`), 앱 종료 시 채널 정리(legacy·pipe), 채널 녹화 중 재시작 BUSY, 종료 시 채널 정지(`RcmLinkHost` 포함) |
| 가짜 앱·시뮬 | `sim/legacy-hdctr.js`(`Channel=2`), `RcmLink.SampleApp`(`Channel=Ch2`, 실제 앱 형식), `sim/recorder.js` |
| 시험 | multi-encoder 5, deck-independent 4, C# 구버전 허브 1, C# 채널 e2e 4, conformance `channel` 그룹 |
| 문서 | API.md, RECORDER.md(4·5·7(13)·9장), SERVER.md, handoff/README.md, CHANGELOG |
| 함께 병합 | PR #22(녹화장비 담당): 서비스형 에이전트 `os.session` 보고(v1.2.0 규격 구현) |

### 3.2 Non-Functional

| 항목 | 결과 |
|------|------|
| 호환성 | 배포 순서 무관(caps 협상). 구버전 허브 거부 0 |
| 안전 | 켜진 PC의 버튼을 누르지 않음(에이전트 접속 시), 채널 녹화 중 재시작 거부, 종료 시 채널 녹화 먼저 정지 |
| 시험 실효성 | 새 C# e2e는 수정을 되돌리면 3/4 실패 확인 |

## 4. Incomplete Items

### 4.1 Carried Over

| # | 항목 | 담당 |
|---|------|------|
| C-1 | 실제 Multi Encoder 5.0 채널 명령·보고 실기(이슈 #20에 결과) | 녹화장비 담당 |
| C-2 | (기존 동작) 허브 깊은 병합이 키를 지우지 않아 main 정지 뒤 `rec.title` 잔존 | 규격 담당, 필요 시 이슈 |
| C-3 | 관리 화면 채널 표시·버튼, `led_mismatch`·`os.session` 표시 | 서버 담당 |
| C-4 | `recorderCaps`를 설정으로 덮는 허브는 `"ch4"` 추가 | 서버 담당 |
| C-5 | LED 고장 상태의 원격 `power.on`은 15초 뒤 재시도 누름으로 부팅 중 종료 위험(에이전트 접속 전이라 근거 없음) | 현장(`led_mismatch` 방은 LED 먼저 수리) |
| C-6 | PC 전원 버튼과 SmartDeck 릴레이 병렬 배선 여부(SmartDeck 고장 시 손으로 켜기) 현장 확인 | 현장 |
| C-7 | 서비스 모드 `os.session` 경로 자동 시험(PR #22 후속) | 녹화장비 담당 |
| C-8 | 원격 브랜치 `recorder/os-session`(녹화장비 담당 브랜치) 정리 | 녹화장비 담당 |
| (이전) | 실PC 업데이트 종료 대기·peerCheck 실기, `.179` 허브 녹화장비 등록 IP(.105) | 이전 PDCA 이월 |

## 5. Quality Metrics

| 지표 | 값 |
|------|-----|
| matchRate | 95% |
| 시험 | 103/103 (v1.2.0 89 + 신규 14) |
| C# 빌드 | 경고 0 / 오류 0 |
| 변경 | PR #21 3커밋 + 병합 1, PR #22 1커밋 |

### 5.1 Resolved During Do/Check

| # | 문제 | 해결 |
|---|------|------|
| 1 | 정상 종료 직후 `actual`이 on으로 남음(offlineGrace) | 실제 연결 기준 `actualOf` + `refreshActual` |
| 2 | SmartDeck 없는 방에서 에이전트가 끊겨도 `power.on`이 noop | `power.on`도 `_rec()` 기준 |
| 3 | 앱 종료 시 채널 상태 잔존(리뷰), 재시작·종료가 채널 녹화 무시(추가 발견) | `StoppedPairs`·`OnAppGone`·`AnyRecording`·`StopAllAsync` |
| 4 | #21·#22 충돌 | 둘 다 적용, `HasCap` 통일 |

## 6. Lessons Learned

### 6.1 Keep
- 질문("이런 케이스가 있나?")은 시뮬레이터로 먼저 재현하고 답함: 녹화는 정상이고 문제는 전원 판단이라는 점, 켜진 PC를 끄는 위험을 사실로 보여 줌.
- 새 시험은 수정을 되돌려 실패하는지 확인.
- 선택 키는 처음부터 caps 협상(`ch4`), 앞선 `peer` 교훈이 바로 적용됨.

### 6.2 Problem
- 첫 구현이 앱 종료·재시작·종료 경로의 채널 처리를 빠뜨림. 리뷰가 하나를 잡았고 나머지는 그 확인 중에 발견.
- 상태의 "끊김 유예"(`recorder.online`)와 "실제 연결"을 섞어 써 판단이 엇갈림.

### 6.3 Try
- 새 상태 축(채널 등)을 넣을 때 "이 상태를 바꾸는 모든 경로"(명령, 보고, 앱 종료, 재시작, 종료, 재접속)를 표로 만들어 설계에 넣기.
- 판단 근거는 실제 연결 기준으로 통일하고, 화면 표시용 유예 플래그와 구분.

## 7. Next Steps
- 녹화장비 담당: v1.3.0 태그 기준 복사본 갱신, Multi Encoder 5.0 채널 보고 연동, 실기 결과를 #20에.
- 서버 담당: 채널 UI, `power.diag`(`led_mismatch` 포함)·`os.session` 표시, 허브 배포 시 caps 확인.

## 8. Changelog

### rcm-api v1.3.0 (2026-11-23, 태그 `v1.3.0`)
멀티인코더 채널별 녹화(#20), 녹화장비 로그온 세션 보고(#22), SmartDeck 없이도 맞는 전원 판단(`led_mismatch`), `no_agent` 문구.

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2026-11-23 | 완료 보고 | KDI |
