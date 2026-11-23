---
template: analysis
version: 1.0
feature: rcm-multi-encoder
date: 2026-11-23
author: KDI
component: rcm-api v1.3.0 (branch multi-encoder-v1.3)
matchRate: 95
status: check
---

# 멀티인코더 채널별 녹화 Gap Analysis

> **Plan**: [rcm-multi-encoder.plan.md](plan.md) · **Design**: [rcm-multi-encoder.design.md](design.md)

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 멀티인코더 장비의 채널 녹화가 허브에서 전부 거절됨 + 채널 보고가 장비 전체 상태를 오염 |
| **WHO** | 허브 관리 화면 사용자, 녹화장비 담당, 서버 담당, 기존 HD-CTR 현장 |
| **RISK** | 구버전 허브 Ch4 거부 / 실제 앱 보고 형식 / 단일 인코더 회귀 / `Channel` 키 오인 |
| **SUCCESS** | check·test 통과 · conformance channel(시뮬·C# 1·2단계) PASS · 기존 시험 통과 · caps 없으면 Ch4 미보고 |
| **SCOPE** | 규격 / 허브 caps / C# Core / 에이전트 필터 / 가짜 앱 2종 / Node 시뮬 / conformance / 문서 |

## 1. 결과 요약

| 항목 | 결과 |
|------|------|
| `npm run check` | catalog OK (36 ops, 19 defs, 6 frames), 생성 파일 최신 |
| `npm test` | **103 / 103 통과** (v1.2.0 89건 + 신규 14건), PR #21·#22 병합 후 main에서도 103/103 |
| C# `dotnet build RcmClients.sln -c Release` | 경고 0 / 오류 0 |
| **Match Rate** | **95%** |

## 2. Success Criteria

| 기준 | 상태 | 근거 |
|------|------|------|
| FR-01 채널 보고 분리 | ✅ | `Legacy.cs` `LegacyState.ScopeChannel` (`Apply` 진입 시), `Channel`=`2`/`Ch2`, 1~4 |
| FR-02 rec.start ChN | ✅ | `RecorderOps.ChRecStart`: 이미 녹화 중이면 바로 성공, `ChN_RecStart[:Title:x]`, `ChN.RecState=RecStart` 15초 대기 |
| FR-03 rec.stop ChN | ✅ | `RecorderOps.ChRecStop`: 30초 대기 |
| FR-04 encoders.ChN | ✅ | `RecorderStateBuilder.Build` encoders (보고된 채널만, 채널별 제목). 채널 일시정지·재개는 `RecState`+`Channel`이면 같은 경로로 반영(별도 시험 없음) |
| FR-05 rec 합산 | ✅ | `RecorderStateBuilder.Combine`, `sim/recorder.js combined()`. M-01·M-02 |
| FR-06 규격 Ch4 | ✅ | `catalog.json` `Encoder`, `encoders.propertyNames` |
| FR-07 caps ch4 | ✅ | 허브 기본 `["session","ch4"]`, `AgentUtil.ForHub` (C# 2종 `Current()`), 시뮬 `forHub`. M-03·M-04, C# 구버전 허브 시험 |
| FR-08 가짜 녹화앱 2종 | ✅ | `sim/legacy-hdctr.js`, `RcmLink.SampleApp/Program.cs` |
| FR-09 Node 시뮬 | ✅ | `sim/recorder.js` main/채널 분리, 종료 시 모든 채널 정지 |
| FR-10 conformance channel | ✅ | `tools/conformance.js` `channel` 그룹, `--channels`. 시뮬(M-05)·C# 1단계·C# 2단계 PASS |
| FR-11 문서 | ✅ | API.md 3.3/5.2/6.3, RECORDER.md 4·5·7(13)·9장, SERVER.md, handoff/README.md, CHANGELOG v1.3.0, package.json 1.3.0 |
| FR-12 no_agent 문구 | ✅ | 보류 브랜치 커밋(b9bfff8)을 이 브랜치로 가져옴(af86983) |
| FR-13 actual = 에이전트 접속 우선 | ✅ | `hub.js` `actualOf`/`syncActual`/`refreshActual`. L-01(SmartDeck 미접속·손으로 켬 → on, 녹화 정상), 정상 종료 시 LED off 즉시 반영(power.test 기존 통과) |
| FR-14 power.on noop | ✅ | `power.js on()`. L-01·L-02 버튼 누름 0 |
| FR-15 power.off LED 고장 | ✅ | `power.js off()` `ledWrong`. L-03(에이전트로 종료, 누름 0, force DEVICE_ERROR) |
| FR-16 led_mismatch | ✅ | `_checkLed`. L-02 발생, L-04 해제 |
| 기존 시험 통과 | ⚠️ | 동작 회귀는 없음. 다만 기대값을 바꾼 기존 시험이 있음: `os-session`의 기본 caps 3곳(`ch4` 추가), `pairing`·`power`의 2건(에이전트 접속 중 `power.on` = noop, FR-14로 의도한 변경. 에이전트가 없을 때의 UNSUPPORTED/OFFLINE 검사는 그대로 남김). "무수정" 기준에서는 부분 충족 |
| 구버전 허브 Ch4 미보고·거부 0 | ✅ | M-04(시뮬), `csharp-agent.test.js` "hub without the ch4 cap"(C# 1단계: Ch4 녹화는 되지만 미보고, Ch1 보고, rejected 0) |
| gap ≥ 90% | ✅ | 95% |

## 2A. 리뷰 반영 (PR #21, 녹화장비 담당)

| # | 지적 | 처리 |
|---|------|------|
| R-1 | #22(`os.session`)와 `Current()` 충돌, #22 먼저 병합 제안 | #22 먼저 병합 → main 병합, `AddOsSession`·`ForHub` 둘 다 적용, `ForHub`가 `RcmDeviceClient.HasCap("ch4")` 사용(`AgentUtil.HubHas` 제거) |
| R-2 | pipe 모드 앱 종료 시 `encoders` 미정리 | `OnAppGone`에서 보고된 채널 모두 idle. legacy 모드도 같은 결함(채널 키 잔존 → 합산 `rec`까지 recording)이라 `RecorderOps.StoppedPairs`로 수정 |
| R-3 (추가 발견) | `app.restart`·`rec.config`가 main만 확인(채널 녹화를 끊음), `sys.shutdown`이 채널을 안 멈춤 | `AnyRecording`·`StopAllAsync`(에이전트 legacy, `RcmLinkHost` pipe) |
| R-4 | 합산 `rec.title` 없음 | 가장 먼저 시작한 채널의 제목(C#·시뮬) |
| R-5 | 실제 앱 `Channel=Ch2` 형식 | RECORDER.md 7장 13 기록, SampleApp은 `Ch2`·가짜 HD-CTR은 `2`로 두 형식 시험 |

새 C# e2e 4건(1·2단계 × 앱 강제 종료/종료 중 채널 정지)은 에이전트 수정을 되돌리면 3건 실패(나머지 1건은 되돌리지 않은 `RcmLinkHost` 수정으로 통과)해 실제 결함을 잡는 것을 확인.

## 3. 설계와 다른 점

| # | 내용 | 영향 |
|---|------|------|
| D-1 | conformance에 시험용 `recorderCaps` 옵션 추가(구버전 허브 흉내). 설계 5장에 없던 것 | 시험 전용. CLI 노출 없음 |
| D-2 | `test/csharp-agent.test.js`의 1단계 시작부를 `phase1(groups)`로 묶고 `allowFail` 추가(기대된 실패를 검사) | 시험 코드만 |
| D-3 | 기본 1·2단계 전체 conformance에 `channel`(Ch1·Ch4) 포함 | 시험 시간 약간 증가 |

## 4. 남은 일 (Carry Items)

| # | 항목 | 담당 |
|---|------|------|
| C-1 | 실제 Multi Encoder 5.0에서 채널 명령·보고 형식 확인(RECORDER.md 7장 13: `Channel` 값 형식, 채널별 일시정지 보고) | 녹화장비 담당(100Mir) |
| C-2 | (기존 동작) 허브의 깊은 병합은 키를 지우지 않아 main 정지 뒤에도 `rec.title`이 남음. v1.3.0과 무관, 필요하면 별도 이슈 | 규격 담당 |
| C-3 | 관리 화면(PHP) 채널별 표시·버튼 | 서버 담당 |
| C-5 | (전원) LED 감지가 고장 난 채 `power.on`을 하면 첫 누름에 PC가 부팅되어도 LED가 안 켜져 15초 뒤 재시도로 한 번 더 누름(부팅 중 종료 위험). 에이전트가 아직 접속 전이라 막을 근거가 없음. `led_mismatch` 진단이 뜬 방은 LED를 먼저 고칠 것 | 현장·규격 담당 |
| C-6 | SmartDeck 펌웨어(RemoteDeck_PC)가 허브 없이도 로컬 기능을 유지하는지, PC 전원 버튼과 SmartDeck 릴레이가 병렬 배선이라 SmartDeck 고장 시에도 손으로 켤 수 있는지 현장 확인 | 현장 |
| C-4 | 허브 배포 시 `recorderCaps`를 설정 파일로 덮어쓰고 있다면 `"ch4"` 추가 | 서버 담당 |

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-23 | Initial (95/95, matchRate 95%) | KDI |
| 0.2 | 2026-11-23 | 전원 판단 FR-13~16, PR #21 리뷰 반영(2A), 103/103 | KDI |
