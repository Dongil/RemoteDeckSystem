---
template: plan
version: 1.3
feature: RemoteDeck_PC_LAN_Recovery
date: 2026-06-30
author: KDI
project: RemoteDeck_PC
status: Draft
---

# RemoteDeck_PC_LAN_Recovery Planning Document

> **Summary**: 5V 아답터 콜드 부팅 시 W5500 Ethernet 초기화 실패 → 4-layer 펌웨어 측 복구 메커니즘 (delay + retry + watchdog + chip-check)
>
> **Project**: RemoteDeck_PC v2.3.x firmware
> **Author**: KDI
> **Date**: 2026-06-30
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | 5V 아답터 OFF→ON (콜드 부팅) 시 W5500 Ethernet 초기화 실패, 네트워크 미연결 상태 잔존. 단, 아답터 C-type 단자 분리→재연결 (hot reconnect) 시에는 정상 연결. 추정 원인은 5V rail ramp-up이 느려 W5500 Power-On Reset (POR) 이 불완전 → SPI 통신 garbage → `ETH.begin()` 또는 link-up 단계 실패. |
| **Solution** | 펌웨어만 수정 (H/W 변경 없음): (1) **Pre-init delay 1s** — 5V/W5500 안정화 대기, (2) **`ETH.begin()` 최대 5회 retry** — 1초 간격, (3) **Got_IP wait 30s timeout + `ESP.restart()`** — 자가 복구, (4) **W5500 chip version SPI 검증** — 0x0039 레지스터 = 0x04 (W5500) 확인으로 SPI 통신 자체 진단. |
| **Function/UX Effect** | 콜드 부팅 시 운영자 개입 없이 자동 복구. 최악의 경우 ~30~60초 후 자가 재부팅으로 정상 동작. 기존 동작 시 추가 부팅 지연은 +1~2초. 운영 단말 OTA 배포 가능. |
| **Core Value** | 운영 단말 무인 자가 복구 — C-type 재연결 노가다 제거. 5V 정전→복전 사이클을 안전하게 통과. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 콜드 부팅 시 W5500 POR 불완전으로 네트워크 미연결 — 운영자가 매번 C-type 재연결 수동 수행 |
| **WHO** | RemoteDeck_PC 운영자 (정전/UPS 절체/유지보수 후 자동 복귀 필요) |
| **RISK** | (1) Retry/restart 루프가 정상 부팅을 +1~5초 지연시킬 수 있음 (2) chip version 검증이 거짓-실패 시 무한 retry (3) ESP.restart() 후에도 동일 실패 가능성 (H/W 한계, 별도 사이클 필요) |
| **SUCCESS** | 콜드 부팅 시 10회 시도 중 ≥ 9회 자동 연결 성공 (60초 이내), 정상 부팅 시 추가 지연 ≤ 2초 |
| **SCOPE** | M1 `NetManager.cpp` 수정 (initEthernet 4-layer) → M2 OTA 빌드 + 단일 단말 검증 → M3 운영 단말 OTA 배포 |

---

## 1. Overview

### 1.1 Purpose

운영 RemoteDeck_PC 단말이 5V 아답터 콜드 부팅 시 자주 LAN 연결에 실패하는 현상을 펌웨어 측 다중 방어선으로 복구. 운영자 수동 개입 (C-type 재연결) 제거.

### 1.2 Background

- 운영 펌웨어: RemoteDeck_PC v2.3.0 (`fw_ver` 응답값 기준)
- 네트워크: W5500 SPI Ethernet, 고정 IP, 내부망
- 전원: 5V 아답터 → ESP32 (내부 3.3V 레귤레이터)
- 현재 코드: `RemoteDeck_PC/src/network/NetManager.cpp:72 initEthernet()` — pre-init delay 0, retry 0, watchdog 0, chip check 0
- 현재 핀: `PIN_ETH_CS=5, SCK=18, MISO=19, MOSI=23, INT=17` (PinConfig.h)
- W5500 HW reset 핀: **미사용** (`ETH.begin(..., -1, SPI)` — RST 핀 = `-1`)
- 증상 빈도: 콜드 부팅 시 "자주 실패" (운영자 확인)
- Workaround: C-type 단자 분리→재연결 (5V rail이 더 sharp하게 ramp-up되면서 W5500 POR 정상 동작 추정)

### 1.3 Related Documents

- 펌웨어 코드: `RemoteDeck_PC/src/network/NetManager.cpp` (`initEthernet`, `onNetworkEvent`)
- 펌웨어 핀맵: `RemoteDeck_PC/src/config/PinConfig.h`
- Arduino-ESP32 ETH driver: `ETH.begin(phy, addr, cs, int, rst, spi)` — RST 핀 wiring 시 자동 reset 가능
- W5500 데이터시트: VERSIONR 레지스터 0x0039 = 0x04

---

## 2. Scope

### 2.1 In Scope

- [ ] `NetManager.cpp:initEthernet()` 에 pre-init delay 1000ms 추가
- [ ] `NetManager.cpp:initEthernet()` 에 `ETH.begin()` retry 로직 (최대 5회, 1초 간격)
- [ ] `NetManager.h` / `NetManager.cpp` 에 Got_IP wait timeout state + 30s 만료 후 `ESP.restart()`
- [ ] `NetManager.cpp` 에 W5500 chip version SPI read 사전 검증 (옵션, fallback)
- [ ] Serial log 강화 — 어느 단계에서 실패했는지 식별 가능하도록
- [ ] OTA 단일 단말 테스트 단말로 콜드 부팅 시연 5회 이상 (성공률 측정)

### 2.2 Out of Scope

- W5500 HW reset 핀 wiring (보드 재설계 필요 — 차기 사이클)
- DHCP 모드 변경 (고정 IP 유지)
- WiFi STA 경로 변경 (`initWiFiSTA` 미수정 — Ethernet only 경로만 대상)
- ETH management 모드 변경 (`initEthManagement` 미수정 — WiFi 모드에서만 사용)
- 5V 아답터 사양 변경 / 외부 전해 캐패시터 추가 (H/W 변경 out of scope)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | `initEthernet()` 시작 시 1000ms delay 후 SPI.begin() 호출 | High | Pending |
| FR-02 | `ETH.begin()` 반환값 false 시 최대 5회 retry (각 1초 간격) | High | Pending |
| FR-03 | retry 모든 시도 실패 시 Serial 로그에 명시 + watchdog 진입 | High | Pending |
| FR-04 | `ETH.begin()` OK 후 ARDUINO_EVENT_ETH_GOT_IP를 30초 이내 수신 못하면 `ESP.restart()` | High | Pending |
| FR-05 | W5500 VERSIONR (0x0039) SPI 읽기로 chip 응답 0x04 사전 검증 (옵션) | Medium | Pending |
| FR-06 | chip version 검증 실패 시 SPI clock 절반으로 fallback retry (옵션) | Low | Pending |
| FR-07 | 각 단계 (delay/begin/retry/got_ip/restart) Serial 로그 강화 | High | Pending |
| FR-08 | 정상 부팅 시 추가 지연 ≤ 2초 (1000ms delay + 1st begin 즉시 성공 가정) | High | Pending |
| FR-09 | OTA 빌드로 운영 단말 무중단 배포 가능 (펌웨어 크기 변화 ≤ 1KB) | Medium | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| Reliability | 콜드 부팅 10회 시도 중 ≥ 9회 자동 연결 (60초 이내) | 수동 시연 (전원 OFF/ON 10회 반복) |
| Performance | 정상 부팅 시 추가 지연 ≤ 2초 | Serial 로그 타임스탬프 비교 |
| Recoverability | 모든 retry 실패 시 60초 이내 ESP.restart()로 자가 복구 | Watchdog 동작 확인 |
| Backwards Compatibility | 기존 OTA 메커니즘과 호환, partition 변경 없음 | `pio run -t size` |
| Code Size | Flash 증가 ≤ 1 KB | `pio run -t size` before/after |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01~04, FR-07~09 구현 + 컴파일 성공
- [ ] FR-05~06 (옵션) 구현 또는 deferred 결정
- [ ] 테스트 단말 1대 OTA 배포 + 콜드 부팅 10회 시연
- [ ] 성공률 ≥ 9/10, 평균 연결 시간 ≤ 30초 측정
- [ ] Serial 로그가 4-layer 동작을 명확히 보여줌 (delay/begin/retry/got_ip/restart 마커)
- [ ] 운영 단말 N대 (사용자 결정) OTA 배포

### 4.2 Quality Criteria

- [ ] PlatformIO 빌드 경고 0 (또는 변경 없음)
- [ ] Flash 증가 ≤ 1 KB
- [ ] RAM 증가 ≤ 256 B
- [ ] Watchdog (ESP.restart()) 루프 방지 — 정상 IP 획득 후에는 절대 restart 호출 안 함

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| 1000ms delay가 너무 짧아 콜드 부팅 5V ramp-up 마감 전 | High | Medium | Plan에 1000ms 시작, Check 단계에서 실패 시 1500/2000ms로 조정 |
| ETH.begin() retry가 무한 루프 (W5500 H/W 자체 고장) | High | Low | 최대 5회 cap + 모두 실패 시 ESP.restart() 진입 (분명한 종료 조건) |
| ESP.restart() 후에도 동일 실패 (H/W 한계) | Medium | Medium | restart 횟수 EEPROM 기록 옵션 — N회 초과 시 LED 점멸 알림 (차기 사이클) |
| W5500 chip version 검증이 false negative (W5500R/W5500S 변형 응답값 차이) | Low | Low | Chip check를 옵션으로 두고 실패해도 progressive ETH.begin() 진행 |
| OTA 후 새 펌웨어가 같은 문제 재현 (배포 자체가 콜드 부팅 트리거) | Medium | Medium | 테스트 단말 1대 먼저 검증 + 운영 단말은 단계적 배포 |
| Pre-init delay가 watchdog (다른 driver) trigger | Low | Low | 1000ms는 ESP32 task watchdog 5s 기본값 내 안전 |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `RemoteDeck_PC/src/network/NetManager.h` | header | (선택) retry/watchdog state 필드 추가 |
| `RemoteDeck_PC/src/network/NetManager.cpp` | implementation | `initEthernet()` 재작성: delay + retry + chip-check, `loop()` 또는 event handler에 got_ip timeout watchdog 추가 |
| `RemoteDeck_PC/src/main.cpp` | 변경 없음 | `networkManager.begin(config.network)` 호출은 유지 |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| `NetManager::begin()` | 호출 | `main.cpp:472` setup에서 호출 | None — interface 유지 |
| `NetManager::loop()` | 호출 | `main.cpp` main loop에서 호출 | watchdog 체크 추가, 기존 동작 유지 |
| `NetManager::localIP()` | 읽기 | API status, MQTT, NTP, UDPDiscovery 등 다수 | None — IP 획득 후 동일 |
| WiFi 모드 (`initEthManagement` + `initWiFiSTA`) | 별경로 | `config.mode == "wifi"` 일 때만 | **변경 없음** — Ethernet 단일 모드 (`config.mode != "wifi"`) 만 대상 |

### 6.3 Verification

- [x] interface (begin/loop/localIP/macAddress) 변경 없음 → main.cpp 및 모든 consumer 영향 0
- [x] WiFi 모드 경로 미수정 → WiFi-mode 단말 (있다면) 영향 0
- [x] Static IP 적용 로직 (`onNetworkEvent` ETH_CONNECTED) 변경 없음

---

## 7. Architecture Considerations

### 7.1 Project Level

| Level | Selected |
|-------|:--------:|
| Embedded firmware (ESP32 Arduino-PlatformIO) | ☑ |

기존 RemoteDeck_PC v2.3 패턴 유지. 신규 모듈 추가 없음.

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| 복구 전략 | (a) delay only / (b) retry only / (c) watchdog only / (d) 다층 복합 | **(d) 다층 복합** | 단일 방어선은 실패 시 회복 불가. 4-layer (delay→retry→watchdog→chip-check) 가 콜드 부팅 신뢰성 ↑ |
| Retry interval | 즉시 / 500ms / 1000ms / exponential backoff | **1000ms 고정** | W5500 POR 완료 추가 시간 확보, exponential은 운영 단말 부팅 시간 예측성 ↓ |
| Watchdog 방식 | `ESP.restart()` / hardware WDT / 무한 retry | **`ESP.restart()`** | clean state 재시작 가장 안전. 30s timeout으로 stuck 회피. |
| Chip check 시점 | begin 이전 / begin 이후 / 안함 | **begin 이전 (옵션)** | begin이 chip 응답 없으면 silent fail 가능 — 사전 검증으로 빠른 진단. 실패 시 begin 진행 (자체 retry로 회복 시도). |
| Logging | minimal / verbose | **verbose** (단계별 마커) | 차기 분석을 위해 어느 단계에서 실패하는지 식별 필요 |

### 7.3 Logic Flow

```
NetManager::initEthernet(config)
│
├─ 1️⃣ Pre-init delay (FR-01)
│   └─ delay(1000) — 5V rail + W5500 POR 안정화
│
├─ 2️⃣ SPI.begin(SCK, MISO, MOSI, CS)
│
├─ 3️⃣ W5500 chip version check (FR-05, 옵션)
│   ├─ readVersionR() — 0x0039 register
│   ├─ 0x04 (W5500) → OK
│   └─ != 0x04 → log warning, fallback SPI clock, retry once
│
├─ 4️⃣ ETH.begin() retry loop (FR-02, FR-03)
│   ├─ for i in 1..5:
│   │   ├─ ETH.begin(W5500, addr=1, CS, INT, RST=-1, SPI)
│   │   ├─ if true → break, log "ETH.begin OK on attempt #i"
│   │   └─ if false → log warn, delay(1000)
│   └─ all fail → log critical, schedule ESP.restart() after 5s
│
└─ 5️⃣ Got_IP timeout watchdog (FR-04)
    ├─ schedule timestamp _ethBeginTime = millis()
    ├─ loop() 에서 매 호출마다:
    │   ├─ if !_connected && (millis() - _ethBeginTime > 30000):
    │   │   └─ log critical, ESP.restart()
    │   └─ if _connected → clear watchdog (정상 IP 획득)
    └─ ESP.restart() 후 setup() 처음부터 (delay 1000ms 다시 시작)
```

---

## 8. Convention Prerequisites

### 8.1 Existing Project Conventions

- [x] `RemoteDeck_PC` 펌웨어 — Arduino-ESP32 3.x / PlatformIO `pioarduino 53.x`
- [x] Logging: `Serial.printf("Network: ...")` 패턴 (NetManager.cpp 기존)
- [x] Class state: `NetManager` singleton (`_instance` 패턴 기존)
- [x] Pin constants: `RemoteDeck_PC/src/config/PinConfig.h` (변경 없음)

### 8.2 Conventions to Define/Verify

| Category | Current | To Define | Priority |
|----------|---------|-----------|----------|
| State naming | exists (`_connected`, `_ethMgmtActive`) | `_ethBeginAt`, `_ethRetryCount`, `_ethDeadlineAt` 추가 | High |
| Log prefix | `Network: ...` | 신규 로그도 동일 prefix 유지 | High |
| Magic number 분리 | 인라인 상수 | `kEthPreInitDelayMs=1000`, `kEthBeginRetryMax=5`, `kEthBeginRetryDelayMs=1000`, `kEthGotIpTimeoutMs=30000` constexpr | Medium |

### 8.3 Environment Variables / Config

| Variable | Purpose | Source |
|----------|---------|--------|
| (none) | 모두 컴파일 시 constexpr 상수 | 운영 중 조정 미필요 (실 데이터로 검증 후 다음 사이클서 config 노출 고려) |

---

## 9. Next Steps

1. [ ] `/pdca design RemoteDeck_PC_LAN_Recovery` — Option A/B/C 비교, 구현 방식 확정
2. [ ] `/pdca do RemoteDeck_PC_LAN_Recovery` — NetManager.{h,cpp} 수정 + 빌드
3. [ ] 테스트 단말 OTA 배포 + 콜드 부팅 10회 시연 + 성공률 측정
4. [ ] 운영 단말 OTA 단계적 배포 결정

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-30 | Initial draft (Checkpoint 1+2 답변 반영, 4-layer 솔루션 채택) | KDI |
