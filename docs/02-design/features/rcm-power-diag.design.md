---
template: design
version: 1.0
feature: rcm-power-diag
date: 2026-11-11
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.1.0 (규격·허브·에이전트 참조·시뮬레이터) + RemoteDeck_PC v3.1.0 (deck)
architecture: Option C (ICMP ping + 허브 교차 확인)
status: draft
---

# RCM 전원 진단 (업데이트 종료 대기 + 부팅 실패 보고) Design Document

> **Summary**: 에이전트의 `os_updating` 단계로 허브가 업데이트 종료를 끝까지(상한 3시간) 기다리고, deck이 짝 PC에 보내는 ping 결과(`deck.state.peer`)와 에이전트 접속 여부로 허브가 부팅 실패 원인을 추정해 `power.diag`에 남긴다. 실패 뒤 30분 동안 관찰해 늦은 부팅이면 오류를 해제한다.
>
> **Planning Doc**: [rcm-power-diag.plan.md](../../01-plan/features/rcm-power-diag.plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 종료 중 업데이트 강제 중단 위험 제거 + 부팅 실패 원인(BIOS/CMOS·OS·에이전트) 구분 보고 |
| **WHO** | RCM 웹 운영자(점검 메시지 소비), 허브 운영(서버 담당), 녹화장비 에이전트(녹화장비 담당), SmartDeck(RemoteDeck_PC) |
| **RISK** | (1) 규격 키 추가가 `additionalProperties:false`라 구버전 허브가 새 deck.state를 위반으로 거부 → **허브 먼저 배포**. (2) 업데이트 보류 판정 오탐/누락 → 상한 3시간 + 기존 240초 경로 유지. (3) ping이 방화벽에 막힘 → 교차 확인으로 오판 대신 unknown. (4) deck이 OS 화면을 볼 수 없으므로 원인은 "추정" |
| **SUCCESS** | rcm-api `npm run check && npm test` 통과 + 새 시나리오 · conformance(deck) PASS · 실기: BIOS 화면 정지 → `no_os`, 이후 부팅 → `late_boot` · 업데이트 종료 대기(시뮬레이터 필수) · RemoteDeck_PC v3.1.0 빌드·conformance PASS |
| **SCOPE** | 규격 / 허브 power.js / 에이전트 참조(C#) / 시뮬레이터·시험·conformance / deck lib + 펌웨어 / 문서 |

---

## 1. Overview

### 1.1 Design Goals

- 업데이트 설치 중 강제 종료 0
- 부팅 실패를 4가지(전원·OS·에이전트·확인 불가)로 구분해 방 상태에 남김
- 늦게라도 부팅되면 오류가 남지 않음
- 구버전 deck(v3.0.0)·에이전트(v1.0.6)와 함께 써도 지금과 같은 동작

### 1.2 Design Principles

- 강제 종료 판단은 허브만 한다(기존 원칙). 에이전트는 사실(업데이트 보류)만 알린다
- 규격은 추가만(minor). 기존 키·의미 불변
- deck lib(`clients/esp32/lib/RcmHub`)는 플랫폼 중립: 네트워크 확인 방법은 펌웨어가 `RcmDeckIo.peerNet`으로 주입
- 원인은 단정하지 않고 "의심"으로 표현

---

## 2. Architecture

### 2.0 Option 비교 (Checkpoint 3)

| | A: ping만 | B: ARP + ping | **C: ping + 허브 교차 확인** |
|---|---|---|---|
| deck 판별 | ICMP | ARP(캐시 비움) + ICMP | ICMP |
| 방화벽 ICMP 차단 시 | 정상 PC를 no_os로 오판 | 영향 없음 | 그 방을 `peerCheck=unreliable` → 진단 `unknown` |
| 부작용 | – | ARP 캐시 전체 삭제가 허브 연결 등에 영향, lwIP 내부 의존 | – |
| 선택 | | | **Selected** (사용자 결정 2026-11-11) |

ARP를 쓰지 않는 이유: lwIP ARP 캐시(약 5분)가 남아 있으면 PC를 끈 직후 다시 켜 BIOS에 멈춘 상태도 "응답 있음"으로 보인다.

### 2.1 Component Diagram

```
 HD-CTR PC ──(OS 기동 시 ICMP 응답)──┐
   └ RcmRecorderAgent ── recorder.shutdown {phase: os_updating|os_shutdown}
                                     │                         │
 SmartDeck(RemoteDeck_PC v3.1.0)     │                         ▼
   ├ PC LED ─────────────── deck.state {pc}            ┌──────────────── 허브 (rcm-api v1.1.0) ───────────┐
   └ PeerProbe(esp_ping, 10s) ─ deck.state {peer:{net,ip}} ─▶ room.deck.state.peer                       │
                                                       │ power.js                                         │
                                                       │  _doOff: 240s → (os_updating) → updateWaitMs 3h  │
                                                       │  _doOn : bootWaitMs 4m → classify → diag         │
                                                       │  _watchBoot: bootWatchMs 30m → late_boot 등      │
                                                       │  peerCheck: 에이전트 접속 중 peer down 60s → unreliable
                                                       └──▶ room.state.power {phase, diag, peerCheck, lastError} ─▶ RCM 웹
```

### 2.2 Data Flow

**종료(power.off)**: `sys.shutdown {deadlineSec:240}` → 대기(LED off 또는 phase=updating) → `os_updating` 수신 시 `power.phase=updating`, 방 기억 `updatePending=true` → LED off를 `updateWaitMs`까지 대기 → 꺼지면 성공 / 초과 시 `pc.forceOff` + `diag=update_timeout`. `os_updating`이 없으면 기존과 같이 240초 후 강제 종료 + `diag=shutdown_timeout`.

**켜기(power.on)**: 누름 → LED on → 에이전트 접속 대기 4분 → 실패 시 `classify()` → `diag` + `lastError`(TIMEOUT, 같은 문구) → 작업 종료 → `_watchBoot` 시작(작업 아님, BUSY 아님).

---

## 3. 규격 변경 (rcm-api `spec/catalog.json`, package 1.1.0, protocol `version` 1 유지)

### 3.1 `recorder.shutdown.data.phase`

```json
"enum": ["rec_stopping", "app_exiting", "app_killed", "os_updating", "os_shutdown", "failed"]
```

`os_updating`: 보류된 Windows 업데이트를 설치하며 Windows 종료를 시작함. 허브는 강제 종료를 미루고 LED가 꺼질 때까지(상한) 기다린다. `os_shutdown`과 마찬가지로 이 이벤트 뒤 에이전트 연결은 끊긴다.

### 3.2 `DeckState.peer` (신규, 선택)

```json
"peer": {
  "type": "object",
  "description": "짝 녹화장비 PC(등록 IP)에 대한 SmartDeck의 ICMP 확인 결과. OS가 네트워크까지 올라왔는지 판단에 씀",
  "properties": {
    "net": { "enum": ["up", "down", "unknown"] },
    "ip":  { "type": "string", "maxLength": 64 }
  },
  "required": ["net"],
  "additionalProperties": false
}
```

### 3.3 `PowerState` 추가

```json
"phase": { "enum": ["idle", "powering_on", "booting", "shutting_down", "updating", "forcing_off"] },
"diag": { "oneOf": [ { "$ref": "defs#/$defs/PowerDiag" }, { "type": "null" } ] },
"peerCheck": { "enum": ["ok", "unreliable", null], "description": "SmartDeck의 짝 PC ping 결과를 믿을 수 있는지. 에이전트 접속 중 ping 무응답이 계속되면 unreliable(방화벽 ICMP 차단 의심)" }
```

신규 def `PowerDiag`:

```json
{
  "type": "object",
  "required": ["code", "msg", "at"],
  "properties": {
    "code": { "enum": ["no_power", "no_os", "no_agent", "unknown", "late_boot", "shutdown_timeout", "update_timeout"] },
    "msg":  { "type": "string", "maxLength": 500 },
    "at":   { "$ref": "defs#/$defs/Time" },
    "watching": { "type": "boolean", "description": "부팅 실패 뒤 관찰 중(bootWatchMs)" },
    "update": { "type": "boolean", "description": "직전 종료에 Windows 업데이트가 있었음" }
  },
  "additionalProperties": false
}
```

`job.progress.phase` 설명에 `updating` 추가(문자열이라 스키마 변경 없음).

### 3.4 진단 코드와 문구

| code | 조건 | msg (한글) |
|------|------|------|
| `no_power` | 누름 후 LED 안 켜짐(재시도 포함) | PC 전원이 켜지지 않음: 전원·어댑터·전원 버튼 배선 점검 |
| `no_os` | LED on, peer.net=down, peerCheck≠unreliable, 에이전트 없음 | PC 전원은 켜졌지만 OS가 올라오지 않음: BIOS/CMOS 오류(F1 대기)·부팅 장치 오류 의심 |
| `no_os` + update | 위 + 직전 종료에 업데이트 | …OS가 올라오지 않음: Windows 업데이트 진행 중일 수 있음(직전 종료에 업데이트 설치). 계속되면 BIOS/부팅 장치 점검 |
| `no_agent` | LED on, peer.net=up, 에이전트 없음 | Windows는 켜졌지만 녹화장비 에이전트가 접속하지 않음: 로그인 대기·에이전트 자동 실행·네트워크 설정 점검 |
| `unknown` | LED on, peer 없음(deck v3.0)·peer.net=unknown·peerCheck=unreliable | PC는 켜졌지만 녹화장비가 접속하지 않음(원인 확인 불가: SmartDeck 펌웨어 v3.1 미만이거나 PC가 ping에 응답하지 않음) |
| `late_boot` | 관찰 중 에이전트 접속 | 부팅 완료 (전원 켠 뒤 n분 n초) |
| `shutdown_timeout` | os_updating 없이 240초 초과 강제 종료 | 정상 종료가 240초 안에 끝나지 않아 강제 종료함 |
| `update_timeout` | os_updating 후 상한 초과 강제 종료 | Windows 업데이트 종료가 3시간 안에 끝나지 않아 강제 종료함 |

(시간 값은 설정에서 계산해 문구에 넣음)

---

## 4. 허브 (`hub/power.js`, `hub/hub.js`)

### 4.1 설정 (`DEFAULT_POWER`, `config.example.json`)

| 키 | 기본 | 설명 |
|----|------|------|
| `updateWaitMs` | 10800000 (3시간) | os_updating 후 LED off 대기 상한 |
| `bootWatchMs` | 1800000 (30분) | 부팅 실패 후 관찰 시간 |
| `peerUnreliableMs` | 60000 | 에이전트 접속 중 peer down이 이만큼 계속되면 unreliable |

### 4.2 `_doOff` 변경

```js
// sys.shutdown 후: LED off 또는 updating 전환까지 240초 대기
const r = await hub.waitRoom(id, s => pcOff(s) || s.power.phase === 'updating', waited);
if (pcOff(room)) return done;
if (room.power.phase === 'updating') {            // onRecorderShutdown이 os_updating에서 바꿔 둠
  if (await hub.waitRoom(id, pcOff, cfg.updateWaitMs)) return done;
  diag = update_timeout;
} else diag = shutdown_timeout;                     // 에이전트 없음(noAgentGrace)도 여기
→ forcing_off → pc.forceOff (기존)
```

`onRecorderShutdown(roomId, {phase})`: 진행 중 power.off 작업이 있고 `phase==='os_updating'`이면 `updateRoom(power.phase='updating')`, `this.updateSeen.set(roomId, true)`. 작업이 없을 때(사용자가 PC에서 직접 종료 등)도 `updateSeen`은 기록.

### 4.3 `_doOn` 변경 + `classify()`

- LED 안 켜짐 → 기존 DEVICE_ERROR, `diag=no_power`
- 에이전트 4분 미접속 → `diag = classify(room)`, `throw RcmError('TIMEOUT', diag.msg)` → `_finish` 후 `_watchBoot(job)` 시작
- 에이전트 접속 성공 → `diag=null`, `updateSeen.delete(roomId)`

```js
classify(s) {
  if (s.deck.state.pc !== 'on') return NO_POWER;
  const net = s.deck.state.peer?.net;
  if (!net || net === 'unknown' || s.power.peerCheck === 'unreliable') return UNKNOWN;
  if (net === 'down') return this.updateSeen.get(id) ? NO_OS_UPDATE : NO_OS;
  return NO_AGENT;
}
```

### 4.4 `_watchBoot(roomId, startedAt)` (FR-13/14)

- 토큰으로 관리: `this.watch.set(roomId, token)`. `_start()`(새 작업)에서 취소.
- 루프: 남은 시간 동안 `waitRoom(id, s => changed(s), remaining)` — 관심 값 `recorder.online`, `deck.state.pc`, `deck.state.peer.net`, `power.peerCheck`.
  - 에이전트 접속 → `updateRoom(power: {lastError:null, diag:{code:'late_boot', msg:'부팅 완료 (전원 켠 뒤 n분 n초)', at}})`, `updateSeen.delete`, 종료
  - LED off → `diag.watching=false`, 종료(누군가 끔)
  - 그 밖의 변화 → `classify()` 다시 해서 code가 바뀌면 `diag` 갱신(`watching:true` 유지)
- 시간 초과 → `diag.watching=false` (마지막 진단 유지)

### 4.5 `peerCheck` (FR-16)

허브 내부 훅: `hub.js`의 `updateRoom`/`replaceDeviceState` 끝에서 `power.onRoomChanged(roomId, state)` 호출(외부 `hooks.roomChanged`와 별개).

- `recorder.online && peer.net==='up'` → `peerCheck='ok'`, 타이머 취소
- `recorder.online && peer.net==='down'` → 타이머 `peerUnreliableMs` 시작, 만료 시 여전히 같으면 `peerCheck='unreliable'` + 로그
- 에이전트가 끊기면 타이머 취소(값 유지)

### 4.6 하위호환

- peer 없는 deck: `classify` → `unknown`, 기존 TIMEOUT과 같은 흐름(문구만 구체화)
- `os_updating` 안 보내는 에이전트: 기존 240초 흐름 그대로
- 방 상태 키 추가만: 구독 클라이언트는 v1.1.0 catalog로 검증해야 위반 경고가 없음(CHANGELOG에 명시)

---

## 5. 녹화장비 에이전트 참조 코드 (C#, `clients/csharp/RcmRecorderAgent`)

### 5.1 보류 업데이트 판정 — `AgentUtil.HasPendingUpdate(cfg)`

| 근거 | 위치 (HKLM, 64-bit view) |
|------|------|
| Windows Update 재부팅 필요 | `SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired` (키 존재) |
| 구성 요소 서비스 재부팅 보류 | `SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending` (키 존재) |

`PendingFileRenameOperations`는 일반 프로그램 설치에도 생겨 오탐이 많으므로 쓰지 않는다. 설정 `pendingUpdate`: `"auto"`(기본, 레지스트리) / `"yes"` / `"no"`(시험·현장 강제용). 레지스트리 읽기 실패는 `false`.

### 5.2 종료 절차

`RecorderAgent.ShutdownAsync`, `PipeRecorderAgent.ShutdownAsync` 모두 `phase("os_shutdown")` 자리에서:

```csharp
bool upd = AgentUtil.HasPendingUpdate(_cfg);
await phase(upd ? "os_updating" : "os_shutdown", upd ? "Windows 업데이트 설치 후 종료" : null);
```

`shutdownDelaySec`·`shutdown.exe` 동작은 그대로.

### 5.3 설치 안내 (RECORDER.md)

HD-CTR PC에 ICMP 에코 허용(관리자 1회):

```
netsh advfirewall firewall add rule name="RCM SmartDeck ping" dir=in action=allow protocol=icmpv4:8,any
```

---

## 6. SmartDeck

### 6.1 lib (`clients/esp32/lib/RcmHub`, rcm-api에서 수정 후 RemoteDeck_PC로 동기화)

- `RcmHub`: welcome의 `pair.peer.regIp`를 저장, `const String& peerIp() const` 제공(재접속마다 갱신, 없으면 빈 문자열)
- `RcmDeckIo`: `std::function<const char*()> peerNet;` (선택, "up"/"down"/"unknown")
- `RcmDeck::fullState`: `peerNet`이 있으면 `peer:{net, ip:hub.peerIp()}`
- `RcmDeck::loop`: `peerNet()` 값이 바뀌면 patch에 `peer` 포함(pc·relays와 같은 방식)

### 6.2 RemoteDeck_PC v3.1.0 — `HubBridge` PeerProbe

- ESP-IDF `ping/ping_sock.h` 세션(`esp_ping_new_session`): 대상 `_deck.hub().peerIp()`, count 1, timeout 1000 ms, 10초마다 `esp_ping_start` (콜백은 ping 태스크 → `std::atomic` 상태)
- 판정: 응답 1회 → `up`, 연속 3회 무응답 → `down`, 짝 IP 없음/아직 결과 없음 → `unknown`
- 짝 IP가 바뀌면(재접속 후 welcome) 세션 재생성
- loop를 막지 않음(비동기). 허브 미사용·미시작이면 동작 안 함
- 버전 3.1.0 (`deviceconfig.json`, ConfigManager 기본값)
- 웹 UI·IC 변경 없음(사용자 결정). `/api/hub` 상세 패널에는 기존 송수신 내역으로 peer 패치가 보임

---

## 7. Error Handling / Security

- 새 오류 코드 없음(`TIMEOUT`, `DEVICE_ERROR` 재사용, msg 구체화)
- 새 비밀값 없음. ping은 같은 LAN 등록 IP만 대상
- 레지스트리 읽기 실패 → 업데이트 없음으로 간주(기존 동작)

---

## 8. Test Plan

### 8.1 rcm-api 자동 시험 (`npm run check && npm test`)

| # | 시나리오 | 기대 |
|---|------|------|
| P-01 | 시뮬 PC: 종료 시 업데이트 보류(종료 6초) + 에이전트 os_updating, `shutdownWaitMs` 2초 | 강제 종료 없음, `power.phase` updating 거쳐 off, forced=false |
| P-02 | P-01 + `updateWaitMs` 초과 | forceOff, `diag.code=update_timeout` |
| P-03 | 에이전트 os_shutdown, OS 종료 멈춤 | 기존대로 강제 종료, `diag=shutdown_timeout` |
| P-04 | 시뮬 PC BIOS 정지(LED on, net down), deck v3.1 | TIMEOUT, `diag=no_os`, watching=true |
| P-05 | P-04 후 BIOS 해제 → OS·에이전트 기동 | `diag=late_boot`, lastError=null |
| P-06 | OS 기동·에이전트 없음 | `diag=no_agent` |
| P-07 | deck peer 미보고(v3.0 시뮬) | `diag=unknown`, 기존 흐름 |
| P-08 | 에이전트 접속 + peer down 지속(방화벽 시뮬) | `peerCheck=unreliable`, 이후 부팅 실패 진단 `unknown` |
| P-09 | 직전 종료 업데이트 + BIOS 정지 | `no_os` + update 문구 |
| P-10 | 관찰 중 새 power.off | 관찰 취소, BUSY 아님 |
| P-11 | 기존 power/state/commands 시험 | 무수정 PASS |
| P-12 | C# 에이전트 e2e(`pendingUpdate:"yes"`) | os_updating 송신 |

### 8.2 conformance

- deck: 선택 그룹 `peer` — `--peer-ip <ip>`로 방에 recorder 등록 IP를 넣어 deck이 peer를 보고하는지, net 값 enum, 짝 PC 상태와 일치(사람이 확인할 값 출력)
- recorder: 기존 `shutdown` 그룹이 `os_updating`도 정상 단계로 인정

### 8.3 실기 (RemoteDeck_PC v3.1.0, 가상 허브 v1.1.0)

| # | 시험 | 기대 |
|---|------|------|
| F-01 | `.193` 짝 PC IP를 RDTEST02 recorder 등록 IP로, PC 정상 켜짐 | `peer.net=up` |
| F-02 | PC 끔 | 30초 안에 `down` |
| F-03 | power.on 후 BIOS 설정 화면(F2/Del) 유지 | 4분 뒤 `diag=no_os` |
| F-04 | F-03 상태에서 BIOS 나가 부팅 → 에이전트 접속(가능하면 C# 에이전트/시뮬) | `late_boot` |
| F-05 | HD-CTR PC ICMP 차단 상태로 에이전트 접속 | 60초 뒤 `peerCheck=unreliable` |
| F-06 | 가능하면 실제 보류 업데이트 PC 종료 | `os_updating` → 강제 종료 없음 |

---

## 9. Layer / 배포 순서

1. rcm-api v1.1.0 (규격·허브·시뮬·시험·에이전트 참조) — 에이전트보다 **허브 먼저 배포**
2. 에이전트 실제 배포(녹화장비 담당)
3. RemoteDeck_PC v3.1.0 (lib 동기화 + PeerProbe) — **허브 버전과 무관**(아래 9.1)

### 9.1 구현 중 추가: `welcome.caps` (Do 단계, 2026-11-11)

실기에서 v3.1 SmartDeck을 v1.0.x 허브(192.168.10.179)에 붙이자, 허브가 모르는 키(`peer`)가 든 `deck.state`를 **통째로 거부**해 그 방의 `pc`까지 사라짐(`.193` 즉시 v3.0.0 롤백). 사용자 결정으로 다음을 추가:

- 규격: `welcome.caps`(SmartDeck에게만, 문자열 배열). v1.1 허브는 `["peer"]`(설정 `deckCaps`로 변경 가능)
- 허브: deck welcome에만 `caps` 포함(웹 클라이언트에는 보내지 않아 구버전 웹의 규격 경고 없음)
- lib: `RcmHub::hubHas(cap)`, `RcmDeck::peerOn()` = `peerNet && hubHas("peer")`일 때만 full·패치에 `peer`
- 시험: P-07b(caps 없는 허브 → peer 미송신, 거부 0), 실기 v1.0.6 허브 호환 확인

---

## 10. Coding Convention

- rcm-api: LF, `npm run gen`으로 생성 문서 갱신, CHANGELOG "v1.1.0", 버전 규칙 minor
- 주석 `// rcm-power-diag: ...` / Design 참조 `// Design Ref: rcm-power-diag §4.3`
- RemoteDeck_PC: lib/RcmHub는 rcm-api 태그와 동일하게 동기화(SOURCE.md), 커밋 `feat(RemoteDeck_PC): v3.1.0 — …`

---

## 11. Implementation Guide

### 11.1 Files

```
rcm-api/
├── spec/catalog.json                     (+ os_updating, DeckState.peer, PowerState.updating/diag/peerCheck, PowerDiag)
├── docs/OPS.md, spec/asyncapi.json       (npm run gen)
├── hub/power.js, hub/hub.js, hub/config.example.json
├── sim/pc.js (biosHang, osUp, updateOnShutdownMs), sim/deck.js (peer), sim/recorder.js (pendingUpdate)
├── test/power.test.js (+ P-01~10), test/csharp-agent.test.js (P-12)
├── tools/conformance.js (deck peer 그룹)
├── clients/csharp/RcmRecorderAgent/{AgentUtil,AgentConfig,RecorderAgent,PipeRecorderAgent}.cs
├── clients/esp32/lib/RcmHub/{RcmHub,RcmDeck}.{h,cpp}
├── docs/API.md, handoff/{RECORDER,SMARTDECK,SERVER}.md, CHANGELOG.md, package.json(1.1.0)
RemoteDeckSystem/RemoteDeck_PC/
├── lib/RcmHub/* (동기화), lib/RcmHub/SOURCE.md
├── src/network/HubBridge.{h,cpp} (PeerProbe), src/config/ConfigManager.cpp, data/deviceconfig.json (3.1.0)
└── docs/RemoteDeck_PC_Manual.md §12
```

### 11.2 Implementation Order

1. [ ] 규격 + `npm run gen` + `npm run check`
2. [ ] 시뮬레이터(pc/deck/recorder) 확장
3. [ ] 허브 power.js(updating·classify·watch·peerCheck) + hub.js 훅 + 설정
4. [ ] 시험 P-01~P-11
5. [ ] C# 에이전트 + P-12
6. [ ] ESP32 lib(peerIp, peerNet) + conformance peer 그룹
7. [ ] 문서·CHANGELOG·버전 → rcm-api PR
8. [ ] RemoteDeck_PC v3.1.0: lib 동기화, PeerProbe, 버전, Manual → 빌드 → 실기 F-01~06 → PR

### 11.3 Session Guide

| Module | Scope Key | Items | 비고 |
|--------|-----------|-------|------|
| 규격·시뮬·허브·시험 | `module-1` | 11.2 #1~4 | rcm-api, 가장 큼 |
| 에이전트 | `module-2` | #5 | C# 빌드 환경 확인 필요 |
| deck lib·conformance·문서 | `module-3` | #6~7 | rcm-api PR |
| 펌웨어·실기 | `module-4` | #8 | RemoteDeck_PC PR, 실기 PC 필요 |

권장: module-1 → 2 → 3 (rcm-api 한 PR), 이어서 module-4.

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.2 | 2026-11-12 | 9.1 `welcome.caps` 추가(실기에서 발견한 구버전 허브 호환 문제), 배포 순서 갱신 | KDI |
| 0.1 | 2026-11-11 | Initial draft (Option C) | KDI |
