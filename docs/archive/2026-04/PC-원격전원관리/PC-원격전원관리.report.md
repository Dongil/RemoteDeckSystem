# PC-원격전원관리 Completion Report

## Executive Summary

| Item | Detail |
|------|--------|
| **Feature** | PC-원격전원관리 (RemoteDeck_PC v2.0 + IPSetupTool v1.0) |
| **Period** | 2026-04-06 ~ 2026-04-07 (2일) |
| **Match Rate** | 97% |
| **Commit** | `bb8729d` (96 files, 11,756 lines) |

### 1.3 Value Delivered

| Perspective | Result |
|-------------|--------|
| **Problem** | 기존 펌웨어에 CEC/IR 불필요 코드 혼재, Web UI 미구현, IP 설정 도구 없음, 현장 배포 시 시리얼 접속 필요 |
| **Solution** | 15개 모듈 모듈화 펌웨어 + Ethernet2/WiFi AP 하이브리드 네트워크 + UDP 기반 IPSetupTool + GPIO 공장 초기화 |
| **Function UX Effect** | 한글 Web UI 6개 탭(홈/제어/스케줄/설정/펌웨어/로그), IPSetupTool로 크로스 서브넷 기기 검색/설정 변경, GPIO1 3초 홀드로 공장 초기화 |
| **Core Value** | 원격 PC 전원 관리 + 현장 방문 없는 기기 설정/업데이트 = WiFi 제한 환경(관공서/학교)에서도 동작하는 무인 운영 시스템 |

---

## 2. PDCA Cycle Summary

```
[Plan] ✅ → [Design] ✅ → [Do] ✅ → [Check] ✅ (97%) → [Report] ✅
```

| Phase | Date | Output |
|-------|------|--------|
| Plan | 04-06 | `docs/01-plan/features/PC-원격전원관리.plan.md` |
| Design | 04-06 | `docs/02-design/features/PC-원격전원관리.design.md` |
| Do | 04-06~07 | 96 files, 11,756 lines |
| Check | 04-06 | `docs/03-analysis/PC-원격전원관리.analysis.md` (97%) |
| Report | 04-07 | This document |

---

## 3. Implementation Results

### 3.1 RemoteDeck_PC Firmware v2.0

| Category | Detail |
|----------|--------|
| **Platform** | ESP32 + W5500 (pioarduino, Arduino 3.x) |
| **Network** | Ethernet2 (MQTT/UDP) + WiFi AP (Web UI) |
| **Modules** | 15개: config(5), control(8), network(8), web(6), serial(2), utils(4) |
| **main.cpp** | 865줄 → ~350줄 (60% 감소) |
| **RAM** | 15.8% (51,832 / 327,680 bytes) |
| **Flash** | 63.2% (1,242,744 / 1,966,080 bytes) |

**제거된 기능:** HDMI CEC, IR 리모컨, esp-cec-esp8266, IRremote

**신규 기능:**

| Feature | Status | Note |
|---------|--------|------|
| 릴레이 제어 (펄스/ON/OFF) | ✅ 동작 확인 | MQTT + RS485 + Web |
| PC 상태 자동 감시 (PCLED) | ✅ 구현 | 폴링 + 이벤트 알림 |
| 스케줄 전원관리 | ✅ 구현 | 최대 8개, NTP 기반 |
| WOL 매직패킷 | ✅ 구현 | UDP 브로드캐스트 |
| Web UI (한글) | ✅ 동작 확인 | WiFi AP 192.168.4.1:5050 |
| REST API (11개) | ✅ 구현 | /api/status, relay, schedule, config, wol, ota, reboot, log |
| WebSocket 실시간 | ✅ 동작 확인 | 상태/로그/OTA 진행률 |
| UDP Discovery | ✅ 동작 확인 | DISCOVER, GET_CONFIG, SET_CONFIG, REBOOT |
| OTA 업데이트 | ✅ 구현 | Dual OTA 파티션 |
| 공장 초기화 | ✅ 동작 확인 | GPIO1+GND 3초 홀드 |
| STATUS LED | ✅ 동작 확인 | STATUS1=네트워크, STATUS2=송수신 |

### 3.2 IPSetupTool v1.0

| Category | Detail |
|----------|--------|
| **Platform** | C# .NET 8, WinForms |
| **Build** | 단일 실행파일 (155MB, self-contained) |
| **권한** | 관리자 권한 자동 요청 (app.manifest) |
| **UI** | 한글 |

**기능:**

| Feature | Status | Note |
|---------|--------|------|
| UDP 기기 검색 (브로드캐스트) | ✅ | 5초 타임아웃, 모든 인터페이스 |
| 크로스 서브넷 임시 IP 추가 | ✅ | netsh + PowerShell 폴백 |
| ARP 워밍업 (ping) | ✅ | 임시 IP 추가 후 자동 |
| UDP GET_CONFIG (설정 로드) | ✅ | ping 워밍업 + 3회 재시도 |
| UDP SET_CONFIG (설정 저장) | ✅ | |
| UDP REBOOT (재부팅) | ✅ | 20초 대기 + 3회 재검색 |
| 수동 IP 직접 연결 | ✅ | 자동 서브넷 감지 + 임시 IP |
| 방화벽 규칙 자동 등록 | ✅ | |
| 임시 IP 자동 정리 | ✅ | 프로그램 종료 시 |

### 3.3 Architecture

```
┌─────────────────────────────────────────┐
│         RemoteDeck_PC (ESP32)            │
│                                         │
│  Ethernet2 (W5500)     WiFi AP          │
│  ├── MQTT              ├── Web UI :5050 │
│  ├── UDP Discovery     ├── WebSocket    │
│  ├── RS485             └── OTA          │
│  └── WOL                                │
│                                         │
│  [RELAY1] [RELAY2] [PCLED] [GPIO1-3]    │
│  [STATUS1] [STATUS2]                    │
└─────────────────────────────────────────┘
         │                    │
    ┌────┴────┐          ┌────┴────┐
    │IPSetupTool│         │ MQTT    │
    │(UDP 5051) │         │ Broker  │
    └──────────┘          └─────────┘
```

---

## 4. Issues & Resolutions

### 4.1 Major Issues

| # | Issue | Root Cause | Resolution |
|---|-------|-----------|------------|
| 1 | ESPAsyncWebServer가 Ethernet2에서 동작 안 함 | Ethernet2는 자체 TCP/IP 스택, AsyncWebServer는 lwIP 필요 | WiFi AP 하이브리드 구조 채택 |
| 2 | ESP32 ETH.h W5500 드라이버 매니지드 스위치 호환 안 됨 | ETH.h INT 핀 의존 + STP 지연 | Ethernet2로 복귀 (검증된 안정성) |
| 3 | 크로스 서브넷 UDP 검색 불가 | IP 서브넷 불일치 시 ARP 해석 불가 | 임시 IP 자동 추가 + ping ARP 워밍업 |
| 4 | Windows 보조 IP에서 UDP 응답 수신 불가 | Strong host model + ARP 캐시 지연 | 0.0.0.0 바인딩 단일 소켓 + ping 워밍업 |
| 5 | Ethernet2 Arduino 3.x 호환 안 됨 | Client 클래스 API 변경 | 로컬 패치 (connect timeout 오버로드 추가) |

### 4.2 Design Deviation

| Item | Design | Actual | Reason |
|------|--------|--------|--------|
| Network | ETH.h 단일 | Ethernet2 + WiFi AP 하이브리드 | ETH.h 매니지드 스위치 호환 문제 |
| Web UI 접속 | Ethernet :5050 | WiFi AP 192.168.4.1:5050 | AsyncWebServer는 lwIP만 지원 |
| Platform | espressif32 6.6.0 | pioarduino 53.03.10 | ESPAsyncWebServer 3.x 호환 |
| IPSetupTool 통신 | HTTP + UDP | UDP only | Ethernet2에서 HTTP 불가 |

---

## 5. Test Results

### 5.1 Firmware Tests

| Test | Result |
|------|--------|
| Ethernet 연결 (DHCP) | ✅ |
| Ethernet 연결 (Static IP) | ✅ |
| WiFi AP 시작 | ✅ |
| Web UI 접속 (WiFi AP) | ✅ |
| RS485 명령 수신/응답 | ✅ |
| MQTT 연결/발행/구독 | ✅ (브로커 설정 시) |
| UDP Discovery 응답 | ✅ |
| UDP GET_CONFIG 응답 | ✅ |
| UDP SET_CONFIG 저장 | ✅ |
| UDP REBOOT 실행 | ✅ |
| 릴레이 ON/OFF/펄스 | ✅ |
| STATUS1 LED (네트워크) | ✅ |
| STATUS2 LED (송수신) | ✅ |
| GPIO1 공장 초기화 | ✅ |

### 5.2 IPSetupTool Tests

| Test | Result | Note |
|------|--------|------|
| 같은 서브넷 기기 검색 | ✅ | 1회에 검색 |
| 다른 서브넷 기기 검색 (임시 IP) | ✅ | 자동 추가 후 검색 |
| 장치 연결 (설정 로드) | ✅ | ping 워밍업 후 1~2회 |
| IP 변경 + 저장 + 재부팅 | ✅ | |
| 수동 IP 직접 연결 | ✅ | 자동 서브넷 감지 |
| 공장 초기화 후 재설정 | ✅ | GPIO1+GND 3초 |

### 5.3 Known Limitations

| Item | Description | Workaround |
|------|-------------|------------|
| 크로스 서브넷 첫 검색 지연 | ARP 캐시 안정화에 시간 필요 | 2회 시도 시 검색됨 |
| Web UI Ethernet 접속 불가 | AsyncWebServer는 WiFi AP만 지원 | WiFi AP 접속 (192.168.4.1:5050) |
| WiFi 제한 환경에서 Web UI | WiFi AP 차단 가능 | IPSetupTool UDP로 설정 변경 |

---

## 6. File Statistics

| Component | Files | Lines |
|-----------|-------|-------|
| Firmware src/ | 29 | ~3,200 |
| Firmware data/ | 5 | ~350 |
| Firmware lib/Ethernet2 (patched) | 20 | ~5,500 |
| IPSetupTool | 8 | ~900 |
| PDCA docs | 4 | ~1,800 |
| Config/Build | 4 | ~50 |
| **Total** | **96** | **~11,800** |

---

## 7. Lessons Learned

1. **ESP32 ETH.h W5500 드라이버는 매니지드 스위치에서 불안정** → Ethernet2가 더 신뢰성 높음
2. **AsyncWebServer는 lwIP 전용** → Ethernet2와 혼용 시 WiFi AP 필수
3. **Windows 보조 IP의 UDP 수신은 불안정** → 0.0.0.0 바인딩 + ping ARP 워밍업 필수
4. **크로스 서브넷 IoT 기기 검색**은 IP 레벨에서 근본적 한계 → 공장 초기화(물리) + 직접 IP 입력이 가장 확실
5. **Arduino 3.x 마이그레이션** 시 `std::string` 헤더, `NetworkUDP`, `Client` 인터페이스 변경 주의

---

*Report completed: 2026-04-07*
*PDCA Phase: Completed*
*Commit: bb8729d*
