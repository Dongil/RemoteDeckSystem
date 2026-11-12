---
template: report
version: 1.1
feature: rcm-power-diag
date: 2026-11-12
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.1.0 + RemoteDeck_PC v3.1.0
branch: rcm-api power-diag-v1.1 / RemoteDeckSystem v3.1-power-diag
matchRate: 96
status: completed
---

# RCM 전원 진단 (업데이트 종료 대기 + 부팅 실패 보고) 완료 보고

> **Status**: Complete (실PC 업데이트 종료·peerCheck 실기는 이월)
>
> **Project**: RemoteDeckSystem + rcm-api
> **Version**: rcm-api 1.1.0 (규격: 호환되는 추가) / RemoteDeck_PC 3.1.0
> **Author**: KDI
> **Completion Date**: 2026-11-12
> **PDCA Cycle**: #1 (Check 중 문서 갭 2건 즉시 수정)

---

## Executive Summary

### 1.1 Project Overview

| Item | Content |
|------|---------|
| Feature | rcm-power-diag — 종료 중 Windows 업데이트 보호 + 켜기 실패 원인 진단 |
| Start / End | 2026-11-11 / 2026-11-12 |
| Duration | 2일 |
| 출발점 | 사용자 질문: "recorder 종료 지연 설정이 생겼는데 deck이 몇 분 뒤 강제 종료하나 / 업데이트로 종료가 늦으면? / 부팅이 BIOS·CMOS에 걸리면 보고했으면" |
| 커밋 | rcm-api `721035c`, `a14b148` (브랜치 `power-diag-v1.1`, push 전) / RemoteDeckSystem `v3.1-power-diag`(커밋 전) |

### 1.2 Results Summary

```
┌─────────────────────────────────────────────┐
│  Completion Rate: 88% (7/8 성공 기준)        │
├─────────────────────────────────────────────┤
│  ✅ Complete:   FR 17/17, 성공 기준 7/8       │
│  ⏳ Partial:    1/8 (실PC 업데이트 종료 대기) │
│  ❌ Cancelled:  0                            │
└─────────────────────────────────────────────┘
```

### 1.3 Value Delivered

| Perspective | Content |
|-------------|---------|
| **Problem** | 허브는 `sys.shutdown` 후 240초가 지나면 무조건 강제 종료해 종료 중 Windows 업데이트를 끊을 수 있었다. 켤 때 실패는 "녹화장비가 240초 안에 접속하지 않음" 하나로만 보고돼 BIOS/CMOS 대기·에이전트 미실행을 구분할 수 없었고, 늦게 부팅돼도 오류가 남았다. |
| **Solution** | 에이전트가 보류 업데이트를 `os_updating`으로 알리면 허브가 강제 종료 대신 LED가 꺼질 때까지(최대 3시간) 기다린다. SmartDeck이 짝 PC에 ping을 보내 `deck.state.peer`로 알리고, 허브는 LED·ping·에이전트로 원인을 추정해 `power.diag`에 남기며 30분 동안 관찰해 늦은 부팅이면 오류를 지운다. 구버전 허브 호환은 `welcome.caps`로 보장. |
| **Function/UX Effect** | 실제 PC를 BIOS에 세운 시험에서 "OS가 올라오지 않음: BIOS/CMOS 오류(F1 대기) 의심" → 부팅 뒤 "Windows는 켜졌지만 에이전트가 접속하지 않음" → 접속 뒤 "부팅 완료 (전원 켠 뒤 2분 25초)"로 자동 갱신. RCM 웹은 이 문구를 그대로 점검 메시지로 쓸 수 있다. |
| **Core Value** | 원격 전원 제어가 PC를 망가뜨리지 않고(업데이트 보호), 현장 출동 전에 원인을 좁힌다. 구버전 허브·deck·에이전트와 섞여도 기존 동작 유지. |

---

## 1.4 Success Criteria Final Status

| # | 기준 | Status | 근거 |
|---|------|:------:|------|
| SC-1 | FR-01 ~ FR-17 구현 | ✅ | analysis §2 |
| SC-2 | rcm-api check·test + 새 시나리오 | ✅ | 84/84 (신규 13) |
| SC-3 | conformance(deck peer) | ✅ | 시뮬레이터 PASS + 구버전 deck 검출 |
| SC-4 | RemoteDeck_PC v3.1.0 빌드·연결 | ✅ | Flash 77.1%, RAM 19.0%, `.109`·`.193` |
| SC-5 | 실기 BIOS → `no_os`, 부팅 → `late_boot` | ✅ | 2026-11-12 10:04~10:06 기록 |
| SC-6 | 실기 `no_agent` | ✅ | BIOS 해제 후 관찰 중 갱신 |
| SC-7 | 업데이트 종료 대기 | ⚠️ | 시뮬레이터 ✅, 실제 보류 업데이트 PC 미실시 |
| SC-8 | 문서·CHANGELOG·Manual | ✅ | API/RECORDER/SMARTDECK/SERVER/CHANGELOG, Manual §12 |

**Success Rate**: 7/8 met, 1 partial

## 1.5 Decision Record Summary

| Source | Decision | Followed? | Outcome |
|--------|----------|:---------:|---------|
| [Plan] | 업데이트 대기·부팅 진단을 한 PDCA로 | ✅ | 규격 변경을 v1.1.0 한 번으로 묶음 |
| [Plan] | 업데이트 상한 3시간, 설정 가능 | ✅ | `updateWaitMs` |
| [Plan] | 보고는 허브에만(웹 점검 메시지용) | ✅ | deck 웹 UI·IC 무변경 |
| [Plan] | 부팅 대기 4분 유지 | ✅ | 문구에만 업데이트 가능성 |
| [Plan] | 늦은 부팅 시 오류 해제 + 30분 관찰 | ✅ | 실기 `late_boot` |
| [Design] | Option C: ICMP ping + 허브 교차 확인 | ✅ | ARP 캐시 오판 회피, `peerCheck` |
| [Do] | **`welcome.caps` 추가** (실기 발견) | ✅ | v1.0 허브가 `deck.state` 통째 거부 → 허브가 지원할 때만 `peer` 송신. v1.0.6 허브 실기 거부 0 |

---

## 2. Related Documents

| Phase | Document | Status |
|-------|----------|--------|
| Plan | [plan.md](plan.md) | ✅ v0.2 |
| Design | [design.md](design.md) | ✅ v0.2 (9.1 caps) |
| Check | [analysis.md](analysis.md) | ✅ 96% |
| Report | 현재 문서 | ✅ |

---

## 3. Completed Items

### 3.1 Deliverables

| 영역 | 산출물 |
|------|--------|
| 규격 (rcm-api) | `os_updating`, `DeckState.peer`, `PowerState.updating/diag/peerCheck`, `PowerDiag`, `welcome.caps` + 생성 문서(OPS.md, asyncapi.json) |
| 허브 | `hub/power.js` 업데이트 대기·진단·관찰·peerCheck, `hub/hub.js` 방 변경 훅·caps·초기 상태, 설정 키 3개 |
| 에이전트 참조(C#) | `AgentUtil.HasPendingUpdate`, `pendingUpdate` 설정, 두 에이전트 모두 `os_updating` |
| ESP32 lib | `RcmHub::peerIp()`, `hubHas()`, `RcmDeckIo.peerNet`, `RcmDeck::peerOn()` |
| 시뮬·시험 | PC `biosHang`/`continueBoot`/`pendingUpdate`/`net`, deck `peer`/`pingBlocked`/caps, recorder `pendingUpdate`, `test/power-diag.test.js` 11건, conformance `peer` 그룹 |
| 펌웨어 | RemoteDeck_PC v3.1.0 `HubBridge` PeerProbe(esp_ping 10초), `/api/hub` peer 필드, lib 동기화(SOURCE.md) |
| 문서 | API.md(6.2 "전원 진단", welcome.caps), RECORDER.md(6·8장, ping 허용 규칙), SMARTDECK.md, SERVER.md, CHANGELOG, Manual §12 |

### 3.2 Non-Functional

| Item | Target | Achieved |
|------|--------|----------|
| 규격 호환 | 추가만(minor) | ✅ 기존 시험 무수정 통과 |
| 구버전 허브 | 기존 동작 유지 | ✅ v1.0.6 허브 실기 거부 0 |
| deck 부하 | loop 비블로킹 | ✅ ping은 별도 태스크, 허브 연결 유지 |
| 보안 | 새 비밀값 없음 | ✅ (시험용 장비 키는 git 무시 설정에만, 시험 후 삭제) |

---

## 4. Incomplete Items

### 4.1 Carried Over

| Item | Reason | 담당 |
|------|--------|------|
| 실제 보류 업데이트 PC 종료 대기(F-06), `pendingUpdate:auto` 실판정 | 보류 업데이트 PC 필요 | 현장 |
| `peerCheck: unreliable` 실기(F-05) | 시뮬레이터로만 확인 | 현장 |
| 실제 에이전트 v1.1.0 반영 + 현장 PC ping 허용 규칙 | 녹화장비 소스 | 녹화장비 담당 |
| RCM 웹 `power.diag` 점검 메시지 | 웹(PHP) | 서버 담당 |
| rcm-api v1.1.0 태그, README "최신 태그" 문구 | 릴리스 | 규격 담당 |
| 정식 허브 전환 후 SmartDeck v3.1.0 배포 | 현재 시험 장비만 적용 | SmartDeck |

---

## 5. Quality Metrics

| Metric | Target | Final |
|--------|--------|-------|
| Match Rate | 90% | 96% |
| Structural / Functional / Contract / Runtime | – | 100 / 98 / 100 / 90 |
| 자동 시험 | 전부 통과 | 84/84 |
| C# 빌드 | 0/0 | 0/0 |

### 5.1 Resolved During Do/Check

| Issue | Resolution |
|-------|------------|
| v3.1 deck이 v1.0 허브에서 `deck.state` 통째 거부(`.193` 실기) | 즉시 v3.0.0 롤백 → `welcome.caps` 도입, 실기 재확인 |
| 짝 PC가 ICMP를 막아 ping 0 | 설치 안내 `netsh` 규칙 적용 후 정상 (현장 필수 항목으로 문서화) |
| 짝 PC 등록 IP 불일치(.103 → 실제 .105) | 시험 중 확인·수정 (현장 등록값 점검 필요) |
| Manual·설계에 구식 배포 순서 | Check에서 수정 |

---

## 6. Lessons Learned

### 6.1 Keep
- 실기를 단계적으로: 가상 허브 + 시험 장비(.109) → 구버전 허브 호환 → 실제 PC BIOS. 각 단계에서 다른 문제가 드러남.
- 기존 장비 키를 바꾸지 않고 가상 허브에 원래 키로 방을 등록해 시험 → 복구가 주소 되돌리기만으로 끝남.
- 시뮬레이터에 고장(BIOS 정지, 업데이트 종료, ping 차단)을 넣어 실기 전 로직을 검증.

### 6.2 Problem
- **다른 사람이 쓰는 장비(.193)에 확인 없이 OTA**해 다른 허브의 전원 상태 보고를 잠시 끊었다. 장비가 어느 허브에 붙어 있는지 먼저 확인했어야 함.
- "허브 먼저 배포"라는 문서상 규칙은 여러 허브가 섞여 도는 현실에서 지켜지기 어려웠다 → 프로토콜 수준 보호(caps)가 필요했음.
- 긴 작업 중 진행 메시지가 영어로 바뀌어 사용자 지적 → 지침 강화(CLAUDE.md, 메모리).

### 6.3 Try
- 장비에 펌웨어를 올리기 전 `/api/hub`로 현재 허브·방을 확인하는 절차를 기본으로.
- 앞으로 규격에 선택 키를 추가할 때는 처음부터 `caps` 협상을 함께 설계.

---

## 7. Next Steps

- [ ] RemoteDeckSystem `v3.1-power-diag` 커밋 → PR → main
- [ ] rcm-api `power-diag-v1.1` push → PR(규격 담당 검토) → v1.1.0 태그 → SOURCE.md 갱신
- [ ] `/pdca archive rcm-power-diag`
- [ ] 배포 파일(v3.1.0 OTA/spiffs/FULL)은 rcm-api 병합 후

---

## 8. Changelog

### rcm-api v1.1.0 (미출시)
- 업데이트 종료 대기(`os_updating`, `updateWaitMs`), 부팅 실패 진단(`power.diag`), 관찰·늦은 부팅 해제, `peerCheck`, `welcome.caps`

### RemoteDeck_PC v3.1.0
- 짝 PC ping(`deck.state.peer`, 허브가 caps로 지원할 때만), `/api/hub` peer 필드

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2026-11-12 | Completion report created | KDI |
