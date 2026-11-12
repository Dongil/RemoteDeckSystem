---
template: analysis
version: 0.1
feature: rcm-power-diag
date: 2026-11-12
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.1.0 (규격·허브·에이전트 참조·시뮬레이터) + RemoteDeck_PC v3.1.0 (deck)
branch: rcm-api power-diag-v1.1 (721035c, a14b148) / RemoteDeckSystem v3.1-power-diag
plan: docs/archive/2026-11/RemoteDeck_PC_v3.1-power-diag/plan.md
design: docs/archive/2026-11/RemoteDeck_PC_v3.1-power-diag/design.md
phase: check
matchRate: 96
---

# RCM 전원 진단 (업데이트 종료 대기 + 부팅 실패 보고) — Gap Analysis (Check)

> **Summary**: FR-01~17 전부 구현. rcm-api `npm run check` + 시험 84/84(신규 11), C# 에이전트 e2e 5/5, ESP32 lib·펌웨어 빌드. 실기: 짝 PC ping up/down, `no_power`, **실제 PC를 BIOS에 세워 `no_os` → 부팅 후 `no_agent` → 에이전트 접속 `late_boot`**, 구버전(v1.0.6) 허브 호환. 실기 중 발견한 구버전 허브 호환 문제는 `welcome.caps`로 해결(설계 9.1에 반영). **matchRate 96%**.

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 종료 중 업데이트 강제 중단 위험 제거 + 부팅 실패 원인(BIOS/CMOS·OS·에이전트) 구분 보고 |
| **WHO** | RCM 웹 운영자(점검 메시지 소비), 허브 운영(서버 담당), 녹화장비 에이전트(녹화장비 담당), SmartDeck(RemoteDeck_PC) |
| **RISK** | 구버전 허브와 새 키, 업데이트 판정 오탐/누락, ping 방화벽 차단, 원인은 추정 |
| **SUCCESS** | rcm-api check·test 통과 · conformance PASS · 실기 BIOS → `no_os` → `late_boot` · 업데이트 종료 대기(시뮬레이터) · v3.1.0 빌드·연결 |
| **SCOPE** | 규격 / 허브 / 에이전트 참조 / 시뮬·시험·conformance / deck lib + 펌웨어 / 문서 |

---

## 1. 검증 방식

| 층 | 방법 | 결과 |
|----|------|------|
| L0 빌드 | `npm run check`, `dotnet build RcmClients.sln -c Release`, `pio run`(rcm-api esp32 예제, RemoteDeck_PC) | 모두 성공 (C# 0/0, 펌웨어 Flash 77.1%·RAM 19.0%) |
| L1 자동 시험 | `npm test` | **84/84** (신규: power-diag 11, conformance peer 1, C# os_updating 1) |
| L1 하위호환 | 기존 시험 무수정 통과, P-07(peer 없는 deck), P-07b(caps 없는 허브) | 통과 |
| L2 실기 `.109` | 가상 허브 v1.1, 짝 IP = 이 PC / 없는 IP | `up` / 29초 뒤 `down`, `power.on` → `no_power` |
| L2 실기 구버전 허브 | rcm-api v1.0.6 허브(worktree, 8893)에 v3.1.0 `.109` 접속 | `peer` 미송신, 거부 0, 방 상태 `pc` 정상 |
| L3 실기 BIOS | `.193` + 짝 PC `192.168.10.105`, 가상 허브(원래 장비 키 사용, bootWaitMs 60초) | 아래 §4 |

---

## 2. Functional Requirements

| ID | 요구사항 | 구현 | 검증 | 근거 |
|----|----------|:--:|:--:|------|
| FR-01 | `os_updating` 단계 | ✅ | ✅ | catalog `recorder.shutdown.phase`, API.md |
| FR-02 | 에이전트 보류 업데이트 판정 | ✅ | ⚠️ | `AgentUtil.HasPendingUpdate` (RebootRequired/RebootPending), `pendingUpdate` yes/no e2e 통과. **`auto`의 실제 레지스트리 판정은 실기 미확인** |
| FR-03 | os_updating → 240초 대기 해제, `updateWaitMs` 3h | ✅ | ✅ | `power.js _doOff`, P-01 |
| FR-04 | 상한 초과 강제 종료 | ✅ | ✅ | P-02 `update_timeout` |
| FR-05 | 직전 종료 업데이트 기억 | ✅ | ✅ | `updateSeen`, P-09 |
| FR-06 | `DeckState.peer` | ✅ | ✅ | catalog, 실기 `.109`/`.193` |
| FR-07 | deck 비동기 ICMP(10s, 1회 up, 3회 down) | ✅ | ✅ | `HubBridge::updatePeerProbe` (esp_ping 세션), 실기 up/down |
| FR-08 | 부팅 실패 진단 4종 | ✅ | ✅ | `_classify`, 실기 `no_power`·`no_os`·`no_agent`, 시뮬 `unknown` |
| FR-09 | 직전 업데이트 시 `no_os` 문구 | ✅ | ✅ | P-09 |
| FR-10 | `PowerState.diag` + `lastError` 문구 | ✅ | ✅ | 실기 TIMEOUT msg = diag msg |
| FR-11 | 구버전 deck·에이전트 하위호환 | ✅ | ✅ | P-07, 기존 시험, (추가) caps로 구버전 **허브** 호환 |
| FR-12 | 시뮬·시험·conformance | ✅ | ✅ | `sim/pc.js`(biosHang 등), deck `peer`/`pingBlocked`, conformance `peer` 그룹 |
| FR-13 | 늦은 부팅 → `lastError` 해제 + `late_boot` | ✅ | ✅ | 실기 "부팅 완료 (전원 켠 뒤 2분 25초)", P-05 |
| FR-14 | `bootWatchMs` 30분 관찰·갱신·취소 | ✅ | ✅ | 실기 `no_os`→`no_agent` 갱신, P-06(종료), P-10(새 작업 시 취소) |
| FR-15 | 종료 진단 `shutdown_timeout`/`update_timeout` | ✅ | ✅ | P-02, P-03 |
| FR-16 | `peerCheck` 교차 확인 | ✅ | ⚠️ | P-08(시뮬). 실기(F-05)는 미실시 |
| FR-17 | ping 허용 방화벽 안내 | ✅ | ✅ | RECORDER.md 8장, 실기 PC에 적용해 동작 확인(규칙 없을 땐 ping 0) |

---

## 3. Success Criteria

| 기준 | 상태 | 근거 |
|------|:--:|------|
| FR-01 ~ FR-17 구현 | ✅ | §2 |
| rcm-api check·test 통과 + 새 시나리오 | ✅ | 84/84 |
| conformance(deck peer) PASS | ✅ | 시뮬레이터 대상 PASS + 구버전 deck 검출. 실기 deck 대상 conformance 실행은 생략(실기 시험으로 대체) |
| RemoteDeck_PC v3.1.0 빌드·허브 연결 | ✅ | `.109`, `.193` |
| 실기 BIOS 정지 → `no_os`, 부팅 → `late_boot` | ✅ | §4 (`late_boot`는 시뮬레이터 에이전트로 접속) |
| 실기 `no_agent` | ✅ | §4 |
| 업데이트 종료 대기(시뮬 필수, 실PC 가능하면) | ⚠️ | 시뮬레이터 ✅, 실제 보류 업데이트 PC 미실시 |
| 문서·CHANGELOG·Manual | ✅ | API/RECORDER/SMARTDECK/SERVER/CHANGELOG, Manual §12 |

---

## 4. 실기 BIOS 시험 기록 (2026-11-12, `.193` + PC `192.168.10.105`)

| 시각 | 상황 | 허브 |
|------|------|------|
| 10:04:27 | Windows 정상 종료, LED off | 스크립트가 `power.on` |
| 10:04:34 | LED on, BIOS 화면 정지 | `booting` |
| 10:04:47 | ping 3회 무응답 | `peer.net=down` |
| 10:05:34 | 60초 대기 끝 | `no_os` (watching) + `TIMEOUT` "BIOS/CMOS 오류(F1 대기)·부팅 장치 오류 의심" |
| 10:06:46 | BIOS 나가 Windows 부팅, ping 응답 | 관찰 중 `no_agent`로 갱신 |
| 10:06:55 | 녹화장비 접속(시뮬레이터, 실제 녹화장비 키) | `late_boot` "부팅 완료 (전원 켠 뒤 2분 25초)", `lastError=null` |

시험 전 PC는 Windows 방화벽이 ICMP를 막아 ping 0(ARP만 응답) → 설치 안내의 `netsh` 규칙 적용 후 정상. 시험 후 `.193`은 v3.0.0 + 원래 허브(192.168.10.179:8890, XHR131258)로 복구, 장비 키는 바꾸지 않음(가상 허브 설정에서도 삭제).

---

## 5. Design 준수 / 편차

| 결정 | 준수 | 비고 |
|------|:--:|------|
| Option C (ICMP + 허브 교차 확인) | ✅ | |
| 업데이트 상한 3h·설정 가능, 부팅 대기 4분 유지, 허브에만 보고 | ✅ | deck 웹 UI·IC 무변경 |
| **추가: `welcome.caps`** | 편차(개선) | 실기에서 발견한 구버전 허브 거부 문제 해결. 설계 9.1, API.md, SMARTDECK.md, CHANGELOG 반영 |
| 추가: `/api/hub`에 `peer_ip/peer_net/peer_replies` | 편차(점검용) | 웹 표시는 없음, Manual §12에 기재 |

---

## 6. Gap 목록

### 6.1 Important
없음 (Check 중 발견한 문서 갭 2건은 즉시 수정: Manual의 "허브 v1.1 이상에 연결" 문구, 설계에 caps 미반영).

### 6.2 Minor / 미실시
- FR-02 `pendingUpdate:auto` 실제 레지스트리 판정과 **실제 업데이트가 걸린 PC의 종료 대기**(F-06) 미실시 — 업데이트 보류 PC가 생기면 확인
- FR-16 `peerCheck: unreliable` 실기(F-05) 미실시 — 시뮬레이터로만 확인
- `updateSeen`은 허브 메모리에만 있음(허브 재시작 시 초기화) — 설계에서 허용
- rcm-api README·handoff/README의 "최신 태그 v1.0.6, 규격은 v1.0.0과 같음" 문구는 v1.1.0 태그 때 규격 담당이 갱신
- 실기 F-04의 `late_boot`는 실제 에이전트가 아니라 시뮬레이터 접속(그 PC의 에이전트가 다른 허브를 향함)

---

## 7. Match Rate

| 축 | 점수 | 가중치 |
|----|:--:|:--:|
| Structural | 100 | 0.15 |
| Functional | 98 | 0.25 |
| Contract (catalog ↔ 허브 ↔ 시뮬 ↔ ESP32 lib ↔ C#) | 100 | 0.25 |
| Runtime (자동 84/84, 실기 BIOS·호환·ping. 실PC 업데이트·peerCheck 실기 미실시) | 90 | 0.35 |
| **Overall** | **96** | |

---

## 8. Carry Items

- 실제 보류 업데이트 PC로 종료 대기(F-06), `peerCheck` 실기(F-05)
- 녹화장비 실제 에이전트(UCCTopXeno 환경)에 v1.1.0 반영·현장 배포 시 ping 허용 규칙 포함 — 녹화장비 담당
- RCM 웹(PHP)에서 `power.diag`로 점검 메시지 표시 — 서버 담당
- rcm-api v1.1.0 태그·README 버전 문구 — 규격 담당
