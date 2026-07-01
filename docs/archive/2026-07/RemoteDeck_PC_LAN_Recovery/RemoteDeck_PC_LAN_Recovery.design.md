---
template: design
version: 1.3
feature: RemoteDeck_PC_LAN_Recovery
date: 2026-06-30
author: KDI
project: RemoteDeck_PC
status: Draft
---

# RemoteDeck_PC_LAN_Recovery Design Document

> **Summary**: 콜드 부팅 시 W5500 LAN 초기화 실패에 대한 4-layer 펌웨어 측 복구. NetManager 내부에 helper + 3 state 필드 추가 (Option C Pragmatic).
>
> **Project**: RemoteDeck_PC v2.3.x firmware
> **Author**: KDI
> **Date**: 2026-06-30
> **Status**: Draft
> **Planning Doc**: [RemoteDeck_PC_LAN_Recovery.plan.md](../../01-plan/features/RemoteDeck_PC_LAN_Recovery.plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 5V 콜드 부팅 시 W5500 POR 불완전 → 운영자 매번 C-type 재연결 수동 |
| **WHO** | RemoteDeck_PC 운영자 (정전/UPS 절체/유지보수 후 자동 복귀) |
| **RISK** | 정상 부팅 +1~2s 지연 / chip-check 거짓-실패 / restart 후도 동일 (H/W 한계 가능) |
| **SUCCESS** | 콜드 10회 중 ≥9회 자동 연결 (≤60s), 정상 부팅 +지연 ≤2s |
| **SCOPE** | M1 NetManager 수정 → M2 OTA 빌드 + 1대 검증 → M3 운영 단계 배포 |

---

## 1. Overview

### 1.1 Design Goals

- W5500 POR 타이밍 불완전을 펌웨어 측 다중 방어선으로 흡수
- 기존 NetManager 인터페이스 (`begin`/`loop`/`localIP`/`macAddress`) 0 변경 — 모든 consumer 영향 없음
- 4-layer 복구 (delay / retry / watchdog / chip-check) 를 NetManager 내부에 응집
- Serial 로그 강화로 어느 layer에서 회복했는지 분석 가능

### 1.2 Design Principles

- **인터페이스 안정성**: 외부 API 변경 0
- **응집도**: 복구 로직은 모두 NetManager 내부 — 다른 모듈 변경 없음
- **상태 최소화**: 신규 멤버 변수 3개만 (`_ethBeginAt`, `_ethWaitingGotIp`, `_ethBeginRetryCount`)
- **임베디드 단발성 fix**: YAGNI — 신규 클래스 분리 안 함 (Option C 채택 사유)
- **Fail-safe**: 모든 retry 실패 시 명확한 종료 조건 (ESP.restart())

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | Option A: Minimal | Option B: Clean | Option C: Pragmatic |
|----------|:-:|:-:|:-:|
| **Approach** | initEthernet 인라인 | EthernetRecovery 클래스 분리 | NetManager 내부 helper + 3 state |
| **New Files** | 0 | 2 | 0 |
| **NetManager 변경** | ~20 LOC | ~30 LOC | ~50 LOC |
| **Complexity** | Low | High | Medium |
| **임베디드 적합** | ✅ 가장 단순 | ❌ 과설계 | ✅ 균형 |
| **추천** | hotfix | 대형 시스템 | **선택됨** |

**Selected**: **Option C — Pragmatic** — **Rationale**: 단발성 fix에 신규 클래스는 과설계. NetManager 응집도 유지하면서 4-layer 복구를 자족적으로 구현. 사용자 채택.

### 2.1 Component Diagram

```
┌────────────────────────────────────────────────────────────────┐
│  main.cpp                                                       │
│    setup() → networkManager.begin(config.network)               │
│    loop()  → networkManager.loop()  (변경 없음)                 │
└────────────────────────┬───────────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────────┐
│  NetManager  (수정)                                             │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ begin(config) → initEthernet(config)                     │  │
│  │   1) delay(kEthPreInitDelayMs)                           │  │
│  │   2) SPI.begin(SCK,MISO,MOSI,CS)                         │  │
│  │   3) [opt] readW5500Version() — 0x0039=0x04?             │  │
│  │   4) tryEthBegin() loop ≤ kEthBeginRetryMax              │  │
│  │   5) on success: _ethBeginAt=millis(), _ethWaitingGotIp= │  │
│  │      true                                                │  │
│  │   6) all fail → log critical + ESP.restart()             │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │ loop()                                                    │  │
│  │   - existing notify logic                                 │  │
│  │   - checkGotIpWatchdog():                                 │  │
│  │     if _ethWaitingGotIp && millis()-_ethBeginAt           │  │
│  │        > kEthGotIpTimeoutMs                               │  │
│  │     → ESP.restart()                                       │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │ onNetworkEvent(ETH_GOT_IP)                                │  │
│  │   - existing logic                                        │  │
│  │   - _ethWaitingGotIp = false  (watchdog 해제)             │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                         │ SPI
                         ▼
┌────────────────────────────────────────────────────────────────┐
│  W5500 (HW)                                                     │
│   - POR 불완전 시: SPI 응답 garbage 또는 무응답                  │
│   - 정상 시: VERSIONR (0x0039) = 0x04                           │
└────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

```
[Cold boot]
ESP32 boot → setup() → networkManager.begin(config)
                       └─ initEthernet(cfg)
                          ├─ delay(1000)          ← Layer 1
                          ├─ SPI.begin(...)
                          ├─ readW5500Version()   ← Layer 4 (optional)
                          │  └─ if fail: log warn, continue (begin's own retry will save us)
                          ├─ for attempt=1..5:    ← Layer 2
                          │  ├─ ETH.begin(...)
                          │  ├─ if true → break, log "ETH.begin OK on #1..5"
                          │  └─ if false → delay(1000), log "Retry attempt #N"
                          ├─ if all fail → log critical, ESP.restart() [after delay(5000)]
                          └─ if success → _ethBeginAt=millis(), _ethWaitingGotIp=true

[main.cpp loop tick]
networkManager.loop()
  ├─ existing connect/disconnect notify
  └─ checkGotIpWatchdog():       ← Layer 3
     ├─ if _ethWaitingGotIp == false: return (이미 IP 받음)
     ├─ if millis() - _ethBeginAt > 30000:
     │  ├─ log critical "Got_IP timeout, restarting"
     │  ├─ delay(500) — flush serial
     │  └─ ESP.restart()
     └─ else: continue (대기 중)

[onNetworkEvent: ETH_GOT_IP]
  ├─ _connected = true (기존)
  ├─ _ethWaitingGotIp = false (watchdog 해제)
  └─ existing static IP / MAC logging
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| `NetManager::initEthernet` | `delay`, `SPI`, `ETH`, `ESP.restart` | 4-layer 복구 |
| `NetManager::readW5500Version` | `SPI.transfer` (low-level) | chip 응답 검증 |
| `NetManager::loop::checkGotIpWatchdog` | `millis()`, `ESP.restart` | 30s timeout |
| `NetManager::onNetworkEvent ETH_GOT_IP` | (수정) `_ethWaitingGotIp` clear | watchdog 종료 |

외부 라이브러리: 변경 없음 (`ETH.h`, `SPI.h` 기존). NTP/MQTT/UDPDiscovery 등 consumer 영향 0.

---

## 3. Data Model

### 3.1 New State Fields (NetManager.h)

```cpp
class NetManager {
private:
    // ... 기존 필드 ...
    NetworkConfig*  _config        = nullptr;
    bool            _connected     = false;
    bool            _notifiedConnected = false;
    bool            _ethMgmtActive = false;
    NetMode         _mode          = NetMode::NONE;

    // === 신규 (Recovery) ===
    unsigned long   _ethBeginAt    = 0;     // ETH.begin() 성공 시각 (millis)
    bool            _ethWaitingGotIp = false; // GOT_IP 대기 중인지
    uint8_t         _ethBeginRetryCount = 0; // 마지막 시도 횟수 (로그용, 누적 아님)
};
```

### 3.2 Constants (NetManager.cpp 파일 상단)

```cpp
namespace {
    // Layer 1
    constexpr uint32_t kEthPreInitDelayMs    = 1000;

    // Layer 2
    constexpr uint8_t  kEthBeginRetryMax     = 5;
    constexpr uint32_t kEthBeginRetryDelayMs = 1000;

    // Layer 3
    constexpr uint32_t kEthGotIpTimeoutMs    = 30000;

    // Layer 4 (W5500 VERSIONR)
    constexpr uint16_t kW5500VersionRegAddr  = 0x0039;
    constexpr uint8_t  kW5500ExpectedVersion = 0x04;

    // Watchdog post-fail
    constexpr uint32_t kRestartDelayMs       = 5000;
}
```

---

## 4. API Specification (Internal)

### 4.1 New Private Methods

| Method | Signature | Purpose |
|--------|-----------|---------|
| `tryEthBegin` | `bool tryEthBegin()` | `ETH.begin()` 호출 (1회), 반환값 그대로 |
| `readW5500Version` | `uint8_t readW5500Version()` | VERSIONR (0x0039) SPI read 후 1 byte 반환. SPI 통신 실패 시 0xFF |
| `checkGotIpWatchdog` | `void checkGotIpWatchdog()` | `loop()` 매번 호출, timeout 시 `ESP.restart()` |

### 4.2 Modified Existing Methods

| Method | Change |
|--------|--------|
| `initEthernet(config)` | 4-layer 로직으로 전면 재작성 (~50 LOC) |
| `loop()` | 마지막에 `checkGotIpWatchdog()` 1줄 추가 |
| `onNetworkEvent(ETH_GOT_IP)` | 기존 로직 유지 + `_ethWaitingGotIp = false;` 1줄 추가 |

### 4.3 Public Interface — 변경 없음

| Method | Status |
|--------|--------|
| `begin(NetworkConfig&)` | 변경 없음 |
| `loop()` | signature 동일 |
| `localIP()` | 변경 없음 |
| `macAddress()` | 변경 없음 |
| `ethManagementIP()` | 변경 없음 |

→ main.cpp / MQTT / NTP / UDP / WebRequest 영향 0.

---

## 5. UI/UX (Serial Logging Spec)

### 5.1 Log Marker Convention

기존 `Network: ...` prefix 유지. 신규 마커는 식별 가능한 keyword 사용:

| Stage | Log Format | Level |
|-------|-----------|:----:|
| Pre-init delay | `Network: ETH pre-init delay {ms}ms (W5500 POR settle)` | Info |
| SPI.begin | `Network: SPI bus initialized (CS={cs}, SCK={sck}, MISO={miso}, MOSI={mosi})` | Info |
| Chip version read | `Network: W5500 VERSIONR = 0x{XX} (expect 0x04)` | Info |
| Chip version warn | `Network: W5500 chip version unexpected, continuing anyway` | Warn |
| ETH.begin attempt | `Network: ETH.begin() attempt {n}/{max}` | Info |
| ETH.begin OK | `Network: ETH.begin() OK on attempt {n}` | Info |
| ETH.begin fail (retry) | `Network: ETH.begin() failed, retry in {ms}ms ({attempts_remaining} left)` | Warn |
| ETH.begin all fail | `Network: ETH.begin() exhausted {max} attempts, restarting in {ms}ms` | Critical |
| Got_IP wait start | `Network: Awaiting GOT_IP event (timeout {ms}ms)` | Info |
| Got_IP received | `Network: ETH Got IP: {ip}, MAC: {mac}` (기존 유지) | Info |
| Got_IP timeout | `Network: GOT_IP watchdog timeout ({ms}ms), restarting` | Critical |

### 5.2 Boot Sequence Visualization (정상 케이스)

```
[boot] Serial init
[boot] Network: ETH pre-init delay 1000ms (W5500 POR settle)
[boot] Network: SPI bus initialized (CS=5, SCK=18, MISO=19, MOSI=23)
[boot] Network: W5500 VERSIONR = 0x04 (expect 0x04)
[boot] Network: ETH.begin() attempt 1/5
[boot] Network: ETH.begin() OK on attempt 1
[boot] Network: Awaiting GOT_IP event (timeout 30000ms)
[+2s ] Network: ETH Started
[+2s ] Network: ETH Link Up
[+2s ] Network: Applying static IP: 192.168.10.141
[+3s ] Network: ETH Got IP: 192.168.10.141, MAC: FA:B3:B7:D8:D2:4C
[main loop] Network: Ethernet connected, IP: 192.168.10.141
```

### 5.3 Boot Sequence Visualization (콜드 부팅 실패 → retry → 회복)

```
[boot] Network: ETH pre-init delay 1000ms
[boot] Network: SPI bus initialized (CS=5, ...)
[boot] Network: W5500 VERSIONR = 0xFF (expect 0x04)
[boot] Network: W5500 chip version unexpected, continuing anyway
[boot] Network: ETH.begin() attempt 1/5
[boot] Network: ETH.begin() failed, retry in 1000ms (4 left)
[boot] Network: ETH.begin() attempt 2/5
[boot] Network: ETH.begin() OK on attempt 2
[boot] Network: Awaiting GOT_IP event (timeout 30000ms)
[+5s ] Network: ETH Link Up
[+5s ] Network: ETH Got IP: 192.168.10.141
```

### 5.4 Boot Sequence (모든 retry 실패 → watchdog restart)

```
[boot] Network: ETH.begin() attempt 5/5
[boot] Network: ETH.begin() failed, retry in 1000ms (0 left)
[boot] Network: ETH.begin() exhausted 5 attempts, restarting in 5000ms
[+5s ] ESP.restart()
[reboot] (전체 부팅 재시작 → 다시 Layer 1부터)
```

---

## 6. Error Handling

### 6.1 Error Codes / Decision Table

| 조건 | 조치 | 다음 단계 |
|------|------|----------|
| `readW5500Version()` returns ≠ 0x04 | Warn 로그, **계속 진행** (begin 자체 retry로 복구 시도) | tryEthBegin loop 진입 |
| `tryEthBegin()` returns false (1차) | Warn 로그, `delay(kEthBeginRetryDelayMs)` | retry attempt #2 |
| `tryEthBegin()` returns false (5차) | Critical 로그, `delay(kRestartDelayMs)`, `ESP.restart()` | 완전 재부팅 |
| `tryEthBegin()` returns true | `_ethBeginAt = millis()`, `_ethWaitingGotIp = true` | event loop 대기 |
| `ETH_GOT_IP` 30s 이내 미수신 | Critical 로그, `ESP.restart()` | 완전 재부팅 |
| `ETH_GOT_IP` 정상 수신 | `_ethWaitingGotIp = false` (watchdog 해제) | 정상 운영 |
| `ETH_DISCONNECTED` (link drop, 운영 중) | 기존 동작 (notify only) | watchdog 재시작 안 함 — 별도 keepalive 문제 |

### 6.2 Restart Loop 방지

- `_ethWaitingGotIp = false` 가 GOT_IP 수신 즉시 해제 → restart 호출 절대 안 됨
- 운영 중 link drop (ETH_DISCONNECTED) 은 watchdog 트리거 안 함 (관련 없음)
- 만약 H/W 고장으로 N회 재부팅 후에도 실패 → 운영자 인지 (LED 표시는 차기 사이클)

---

## 7. Security Considerations

- 신규 외부 노출 인터페이스 없음
- W5500 register read (`readW5500Version`)는 read-only, MAC/auth 정보 미접근
- 로그에 IP/MAC만 노출 (기존과 동일)
- `ESP.restart()`는 정상 부팅 시퀀스로 진입 → 보안 영향 없음

---

## 8. Test Plan

### 8.1 Test Scope

| Type | Target | Tool | Phase |
|------|--------|------|-------|
| L1: 정상 부팅 | 빌드/펌웨어 부팅 + Serial 로그 | Serial monitor | Do |
| L2: 콜드 부팅 (5V OFF→ON) | 10회 시도, 성공률 측정 | 수동 시연 + Serial 캡처 | Do |
| L3: 단일 실패 후 retry 회복 | retry attempt 2~5에서 회복 케이스 | Serial 로그 검토 | Do |
| L4: 모든 retry 실패 → restart | 단말의 W5500 SPI 임의로 끊고 시도 (선택, 위험) | 수동 | Do (선택) |
| L5: GOT_IP timeout → restart | begin OK인데 30s 미수신 시 restart | 케이블 빼서 link 안 올림 | Do |

### 8.2 L1 Boot Tests

| # | Scenario | Expected | Pass 기준 |
|---|----------|----------|----------|
| 1 | 정상 부팅 (이미 운영 중인 단말) | Layer 1 delay + Layer 2 1st attempt OK | "ETH.begin() OK on attempt 1" 로그 |
| 2 | 부팅 후 운영 중 link down (케이블 분리) | ETH_DISCONNECTED, watchdog 트리거 안 됨 | restart 안 일어남 |
| 3 | 부팅 시간 측정 | 정상 부팅 시 ~+1~2s 추가 (delay) | Serial 첫 로그 ~ "Got IP" 시각 |

### 8.3 L2 Cold-boot Tests (10회 반복)

| # | Step | Expected |
|---|------|----------|
| 1 | 5V 아답터 OFF (모든 단말 → 2초 대기 → 5V ON) | 10회 시도 |
| 2 | 각 시도마다 Serial 로그 캡처 | 시도 횟수, layer별 결과 기록 |
| 3 | 성공률 측정 | ≥ 9/10 (90%) |
| 4 | 평균 연결 시간 측정 | ≤ 30초 |

### 8.4 L3 Retry Recovery

| # | Inject | Expected |
|---|--------|----------|
| 1 | (자연 발생) 콜드 부팅 시 1차 begin fail | retry #2에서 OK → 연결 |
| 2 | (자연 발생) 1~3차 begin fail | retry #4 또는 #5에서 OK |
| 3 | 5차 모두 fail | "exhausted, restarting" 로그 → 재부팅 후 정상 |

### 8.5 Seed Data Requirements

- 단말 1대: 192.168.10.141:5050 admin/12345 (현재 운영 중)
- Serial USB 케이블 (로그 캡처)
- 5V 아답터 (현재 운영 동일 모델)
- 가능하다면 다른 5V 아답터 1개 (비교용)

---

## 9. Clean Architecture (단일 모듈 응집)

신규 클래스 없음. NetManager 응집도 유지.

### 9.1 Layer Structure (변경 없음)

| Layer | Files |
|-------|-------|
| Hardware abstraction | `ETH.h`, `SPI.h` (라이브러리) |
| Network manager | `RemoteDeck_PC/src/network/NetManager.{h,cpp}` (수정) |
| Main loop | `RemoteDeck_PC/src/main.cpp` (변경 없음) |

### 9.2 Dependency Rules

```
main.cpp → NetManager (public API) — 변경 없음
NetManager → ETH, SPI, ESP (내부) — 신규 ESP.restart() 호출 추가
```

---

## 10. Coding Convention Reference

### 10.1 Naming

| Target | Rule | Example |
|--------|------|---------|
| Constants | `k`PascalCase + suffix | `kEthPreInitDelayMs`, `kEthBeginRetryMax` |
| Private member | `_camelCase` (기존) | `_ethBeginAt`, `_ethWaitingGotIp` |
| Private method | camelCase (기존) | `tryEthBegin`, `readW5500Version`, `checkGotIpWatchdog` |
| Log prefix | `Network:` (기존) | `Network: ETH.begin() OK on attempt 2` |

### 10.2 Comment Convention

각 신규 메서드/상수에 Design Ref 코멘트:

```cpp
// Design Ref: §3.2 — W5500 POR settle delay before SPI init.
constexpr uint32_t kEthPreInitDelayMs = 1000;

// Design Ref: §4.1, §6.1 — ETH.begin() retry up to 5 times on cold-boot W5500 garbage SPI.
bool NetManager::tryEthBegin() { ... }
```

---

## 11. Implementation Guide

### 11.1 File Structure (변경 사항)

```
RemoteDeck_PC/src/network/
├── NetManager.h          ← +3 state 필드, +3 private 메서드 선언
└── NetManager.cpp        ← +constexpr block, +helper 메서드 구현, initEthernet 재작성, loop watchdog 호출, ETH_GOT_IP _ethWaitingGotIp clear
```

### 11.2 Implementation Order

1. [ ] **M1**: NetManager.h 수정 — 3 state 필드 + 3 private 메서드 선언
2. [ ] **M2**: NetManager.cpp 수정:
   - constexpr block 추가 (파일 상단 익명 namespace)
   - `tryEthBegin()` 구현
   - `readW5500Version()` 구현 (옵션 시도, 실패해도 진행)
   - `checkGotIpWatchdog()` 구현
   - `initEthernet()` 재작성 (4-layer)
   - `loop()` 마지막에 `checkGotIpWatchdog()` 호출 추가
   - `onNetworkEvent(ETH_GOT_IP)` 안에 `_ethWaitingGotIp = false;` 추가
3. [ ] **M3**: PlatformIO 빌드 — `pio run -e remotedeck_pc` 경고 0 / 오류 0 확인
4. [ ] **M4**: OTA 빌드 + 테스트 단말 1대 배포 — `pio run -t upload -e remotedeck_pc-ota` 또는 동등 명령
5. [ ] **M5**: 콜드 부팅 10회 시연 (5V 아답터 OFF→ON), Serial 로그 캡처, 성공률 측정

### 11.3 Session Guide

#### Module Map

| Module | Scope Key | Description | Estimated Turns |
|--------|-----------|-------------|:---------------:|
| 코드 수정 (헤더+구현) | `module-1` | NetManager.h state + 메서드 선언, NetManager.cpp 4-layer 구현 + 로그 | 10~15 |
| 빌드 검증 | `module-2` | `pio run` 빌드 클린 확인, Flash/RAM diff | 3~5 |
| OTA 배포 + 시연 | `module-3` | OTA upload, 콜드 부팅 10회, Serial 캡처, 성공률 측정 | 5~10 |

#### Recommended Session Plan

| Session | Phase | Scope |
|---------|-------|-------|
| Session 1 (현재) | Plan + Design | 전체 — 즉시 Do로 이동 가능 |
| Session 2 | Do M1+M2+M3 | NetManager 수정 + 빌드 |
| Session 3 | Do M4+M5 + Check + Report | 운영 단말 OTA + 시연 + Match Rate 측정 |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-30 | Initial draft, Option C 선정 | KDI |
