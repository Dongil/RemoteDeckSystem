# RemoteDeck_Touch_v2.5 Design Document

> **Summary**: 4 commit 단계적 코드 제거 (디렉토리 → main.cpp → platformio → 빌드/검증). v2.1 LCD/MQTT only 패턴 복귀.
>
> **Project**: RemoteDeck_Touch
> **Version**: v2.5.0 (target)
> **Author**: KDI
> **Date**: 2026-06-26
> **Status**: Draft
> **Planning Doc**: [RemoteDeck_Touch_v2.5.plan.md](../../01-plan/features/RemoteDeck_Touch_v2.5.plan.md)
> **Branch**: `v2.5-sunset`

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.3/v2.4 본질 한계 정식 수용 + dead code 정리 |
| **WHO** | 단말 사용자 (LCD 터치) + 외부 admin (MQTT publisher) |
| **RISK** | main.cpp 정리 시 의존성 깨짐 — 검증: 빌드 + L1 |
| **SUCCESS** | 빌드 통과 + Flash ~50KB 감소 + LCD/MQTT v2.1 동작 보존 |
| **SCOPE** | 5 Phase (제거 → main 정리 → 빌드 → 검증 → Report) |

---

## 1. Overview

### 1.1 Design Goals

- **단계적 제거**: 4 commit 으로 추적 가능한 sunset
- **의존성 안전**: 디렉토리 제거 → main.cpp 정리 순서로 compile error 가시화
- **v2.1 패턴 복귀**: WebServer 관련 코드만 제거, v2.1~v2.3 의 모든 LCD/MQTT/LAN 개선사항 그대로
- **검증 가능**: 빌드 통과 + Flash 사이즈 감소 + 단말 LCD/MQTT L3 시나리오 통과

### 1.2 Design Principles

1. **Subtractive Design** — 코드 추가 0, 코드 제거 ~700 LOC 만
2. **Single Source of Truth** — 각 commit 단계 후 grep 검증으로 잔존 참조 0 확인
3. **v2.1 동작 보존** — Long-click / Sleep / MQTT 양방향 / 이미지 표시 / DHCP 모두 v2.1 동일
4. **Rollback Ready** — 4 단계 commit 으로 어느 단계에서 fail 해도 git revert 단순

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | A: Minimal | B: Clean | **C: Pragmatic** ⭐ |
|----------|:-:|:-:|:-:|
| Commits | 1 | 7+ | **4** |
| Rollback | 전체 ↔ 무 | 매우 세밀 | **단계별** |
| Build runs | 1 | 7+ | **2** |
| Risk | 큰 rollback | over-engineering | **Low** |

**Selected**: **Option C — Pragmatic** (4 commit 단계적 제거 + 2회 빌드 검증)

### 2.1 Component Diagram (제거 전 → 제거 후)

```
== 제거 전 (v2.3-final = 단말 운영 펌웨어) ==

main.cpp ─┬─ webServer (esp_http_server core 0)
          ├─ imageApi   (콜백 시그니처 + buildStatusJson + upload)
          ├─ configApi  (deviceconfig GET/POST + reboot)
          ├─ webLogger  (50건 ring buffer)
          ├─ otaApi     (Update.h)
          ├─ controlApi (Short polling + MQTT bridge)
          ├─ touchAuth  (Basic admin:12345)
          │
          ├─ DeviceManager / images / mqtt / config (보존)
          ├─ LCD/LVGL/TFT_eSPI (보존)
          └─ HTTPClient http (보존, v2.1 downloadFile)


== 제거 후 (v2.5 = v2.1 + v2.1~v2.3 개선사항) ==

main.cpp ─┬─ DeviceManager / images / mqtt / config
          ├─ LCD/LVGL/TFT_eSPI
          ├─ HTTPClient http (downloadFile/sendHttpMessage)
          └─ MQTT 양방향 (room/{device_id} subscribe + room/client publish)
```

### 2.2 Data Flow (제거 후)

```
[외부 admin]                [현장 사용자]
   ↓ MQTT publish            ↓ LCD touch
   room/{device_id}          ibtnRoom click
   {"status":"IN/OUT",...}      ↓
   ↓                          message_process or 직접 publish
mqtt_ReceivedCallback              ↓
   ↓                          mqttEthernet_publish("IN"/"OUT")
message_process                    ↓
   ├─ LCD ibtnRoom 갱신       MQTT broker
   └─ NTP tick 동기화                ↓
                              외부 admin / 다른 client
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| MQTT 양방향 | mqttEthernet (v2.1 그대로) | room/{device_id} sub + room/client pub |
| LCD/Touch | LVGL + TFT_eSPI + FT6236G (v2.1 그대로) | 메인 화면 + Long-click + Sleep |
| 이미지 디코드 | images.cpp (v2.1 그대로) | BMP 안전장치 |
| HTTP 다운로드 | HTTPClient http (v2.1 그대로) | downloadFile/sendHttpMessage |
| DeviceManager | LVGL (v2.1 그대로) | 설정 화면 |

---

## 3. Data Model

### 3.1 변경 없음
v2.1 의 DeviceConfig, ServerConfig, ImagesConfig 그대로. 새 data model 없음.

### 3.2 제거되는 data model
- `WebActivityMonitor` (v2.4 추가)
- `ControlState`, `OtaState`, `UploadState` (v2.3 추가)
- `LogEntry` ring buffer (v2.3 추가)

---

## 4. API Specification

### 4.1 제거되는 API (14개 endpoint 전부)

| # | Endpoint | Method | Removed |
|---|----------|--------|:---:|
| 1-14 | / + /api/* (status, control, log, config, images, imagesconfig, ota, reboot) | GET/POST/DELETE | ✅ |

### 4.2 남은 인터페이스
- **MQTT subscribe**: `room/{device_id}` — 외부 admin → 단말 (IN/OUT 명령)
- **MQTT publish**: `room/client` — 단말 → 외부 admin (LCD 클릭 시)
- **HTTP client (단말 → 서버)**: `downloadFile()`, `sendHttpMessage()` (v2.1 부터 존재)

---

## 5. UI/UX Design

### 5.1 LCD 화면 (변경 없음)

v2.1/v2.3 와 100% 동일:
- 메인 화면: 로고 + 재실(IN)/부재(OUT) 이미지 버튼
- 설정 화면: DeviceManager (Long-click 진입)
- 화면보호기 (Sleep)
- 부팅 로고

### 5.2 User Flow (변경 없음)

```
부팅 → 로고 → ETH 연결 → MQTT 연결 → 메인 화면
   ↓
사용자 touch → IN/OUT 토글 → MQTT publish
   ↓
외부 admin publish → MQTT receive → LCD 자동 갱신
   ↓
Long-click → 설정 화면 (DeviceManager)
```

### 5.3 Component List (제거 후)

| Component | Location | Responsibility |
|-----------|----------|----------------|
| main.cpp | src/main.cpp | 모든 로직 hub (web 코드 제거 후 ~v2.1 수준 단순) |
| DeviceManager | src/device/ | 설정 화면 |
| images.cpp | src/images/ | BMP 디코드 + 안전장치 |
| ethernet_mqtt | src/mqtt/ | MQTT 양방향 + DHCP |
| ConfigManager | src/config/ | SPIFFS config load/save |
| lvgl_touch | src/ | LVGL + FT6236G 초기화 |
| ui.* (SquareLine) | lib/ui/ | UI 위젯 |

**제거되는 component**:
- ❌ WebServer + ImageApi + ConfigApi + Logger + OtaApi + ControlApi + WebActivityMonitor + embedded_assets

### 5.4 Page UI Checklist (LCD)

- [x] 메인 화면: 로고 + ibtnRoom (IN/OUT 이미지)
- [x] 설정 화면 (DeviceManager): WiFi/Ethernet/IP 설정 + Sleep 시간
- [x] 로고 화면 (boot)
- [x] 화면보호기 (Sleep timeout)

---

## 6. Error Handling

v2.1 동일. 신규 에러 처리 없음.

| 상황 | 동작 |
|------|------|
| ETH/DHCP fail | DeviceManager 자동 진입 (v2.1) |
| MQTT 연결 끊김 | mqttEthernet_reconnect 자동 시도 (v2.1) |
| BMP 디코드 fail | OLD dsc 유지 (v2.1 fail-soft) |
| SPIFFS 부족 | downloadFile 200KB 상한 (v2.1) |

---

## 7. Security Considerations

- ❌ Basic Auth 제거 (HTTP API 없음)
- ✅ MQTT 인증 (broker 측 설정)
- ⚠ MQTT broker 공개 IP 시 ACL 필요 (out of scope)
- ✅ Long-click 진입 가드 (단순 터치로 설정 진입 불가)

---

## 8. Test Plan

### 8.1 Test Scope

| Type | Target | Tool | Phase |
|------|--------|------|-------|
| L1: 빌드 | platformio run | pio | Do |
| L2: Flash 사이즈 | v2.3-final vs v2.5 비교 | platformio output | Do |
| L3: 단말 시나리오 | LCD/Touch/MQTT/Long-click/Sleep | 사용자 직접 + curl 없음 (HTTP API 제거) | Check |

### 8.2 L1: 빌드 검증

| # | Check | Expected |
|---|-------|---------|
| 1 | platformio run | SUCCESS |
| 2 | linker undefined reference | 0 |
| 3 | RAM 사용 | < v2.3-final (36.9%) |
| 4 | Flash 사용 | < v2.3-final (1.83MB, 57.6%) — 약 50KB 감소 기대 |

### 8.3 L2: Flash/RAM 감소 측정

| Metric | v2.3-final | v2.5 (예상) | 감소 |
|--------|:----------:|:-----------:|:----:|
| RAM | 121,184 (37.0%) | ~115KB (35%) | ~6KB |
| Flash | 1,838,300 (58.4%) | ~1.78MB (56%) | ~50-60KB |

### 8.4 L3: 단말 시나리오 (사용자 수동)

| # | 시나리오 | 통과 조건 |
|---|---------|---------|
| M1 | 부팅 → LCD 메인 화면 | 정상 표시 (v2.1 동일) |
| M2 | Long-click → DeviceManager | 진입 정상 |
| M3 | Sleep 시간 변경 → 저장 → 재부팅 → 값 유지 | regression 無 |
| M4 | MQTT 외부 admin publish → LCD ibtnRoom 갱신 | 양방향 동작 |
| M5 | LCD ibtnRoom 클릭 → MQTT room/client publish | 정상 |
| M6 | DHCP 끊김 → 재연결 | v2.1 동작 보존 |
| M7 | 24h 운영 → heap leak 무 (가능 시) | uptime 1+ 일 |

---

## 9. Clean Architecture

### 9.1 Layer Structure (제거 후)

| Layer | Responsibility | Location |
|-------|---------------|----------|
| Presentation | LCD UI (LVGL + ui.c) | `lib/ui/`, lvgl_touch |
| Application | DeviceManager, images_update, message_process | `src/device/`, `src/images/`, `src/main.cpp` |
| Domain | DeviceConfig, ServerConfig, ImagesConfig | `src/config/` |
| Infrastructure | ETH, MQTT, SPIFFS, HTTPClient, TFT_eSPI | ESP-IDF / Arduino-ESP32 |

### 9.2 Dependency Rules

```
LCD UI ──→ DeviceManager / images / main.cpp
                 │
                 ▼
            DeviceConfig / ServerConfig
                 │
                 ▼
            ETH / MQTT / SPIFFS / HTTPClient
```

**규칙**:
- main.cpp 가 모든 모듈 hub (v2.1 패턴)
- web/ 디렉토리 없음 (v2.5 신규 규칙)

---

## 10. Coding Convention Reference

### 10.1 Naming
v2.1 와 동일. 새로운 컨벤션 없음.

### 10.2 Code Comments
제거된 라인은 commit message 에서 추적. 인라인 주석 추가 없음.

### 10.3 v2.5 컨벤션
- 모든 제거는 grep 으로 잔존 참조 0 확인 후 commit
- 빌드는 commit 2 (main.cpp 정리) 후 1회, commit 3 (platformio) 후 1회 총 2회

---

## 11. Implementation Guide

### 11.1 File Structure (최종)

```
RemoteDeck_Touch/
├── platformio.ini                  [정리]
├── lib/lv_conf.h                   (그대로)
├── lib/ui/                         (그대로 — SquareLine UI)
├── lib/TFT_eSPI*/                  (그대로)
├── lib/lvgl/                       (그대로)
├── data/
│   ├── deviceconfig.json           (보존)
│   ├── serverconfig.json           (보존)
│   ├── imagesconfig.json           (보존)
│   └── (www/ 제거)
├── src/
│   ├── main.cpp                    [정리 — web 코드 제거]
│   ├── lvgl_touch.{h,cpp}          [수정 — h 의 getTouch extern 제거]
│   ├── images/                     (그대로)
│   ├── device/                     (그대로)
│   ├── mqtt/                       (그대로)
│   ├── config/                     (그대로)
│   ├── utils/                      (그대로)
│   └── (web/ 제거 — 8 파일)
├── tools/
│   └── (embed_www.py 제거)
└── test/poc/                       [정리 — web 스크립트 제거]
```

### 11.2 Implementation Order

#### Commit 1 — 디렉토리/파일 일괄 제거
```
git rm -r RemoteDeck_Touch/src/web/
git rm -r RemoteDeck_Touch/data/www/
git rm RemoteDeck_Touch/tools/embed_www.py
git rm RemoteDeck_Touch/test/poc/{v24_poc.py,control_verify.py,capture_serial.py,run_poc.sh,mqtt_pub.py,README.md}
git rm RemoteDeck_Touch/test/poc/{title.png,dummy.bmp,serial.log,serial_err.log} (잔존 시)
```
검증: `git status` 로 누락 없음 확인. **이 단계 빌드 안 함** (main.cpp 에서 web/* include 로 빌드 fail 예상).

#### Commit 2 — main.cpp + lvgl_touch.h 정리
```
src/main.cpp:
  - #include "web/*" 모두 제거
  - WebServer/ImageApi/ConfigApi/Logger/OtaApi/ControlApi/TouchAuth 인스턴스 제거
  - setup() 의 imageApi.attach + configApi.attach + webLogger.attach + otaApi.attach +
    controlApi.begin + setMqttPublisher + attach + webServer.begin + webLogger.log 제거
  - loop() 의 imageApi.loop / configApi.loop / otaApi.loop 호출 제거
  - message_process() 의 controlApi.notifyState 호출 제거
  - 코멘트 정리

src/lvgl_touch.h:
  - extern int getTouch(uint16_t* pPoints) 제거 (v2.4 추가분)
```
검증:
- `grep -r "web/" RemoteDeck_Touch/src/` → 결과 0 (또는 주석 only)
- `grep -rE "WebServer|ImageApi|ConfigApi|Logger|OtaApi|ControlApi|WebActivityMonitor" RemoteDeck_Touch/src/` → 0
- **빌드** → SUCCESS 기대

#### Commit 3 — platformio.ini 정리
```
platformio.ini:
  - build_flags 에서 -DCONFIG_HTTPD_* 제거 (잔존 시)
  - lib_deps 에서 web 관련 (mathieucarbou 등) 제거 (이미 v2.3 에서 제거됨, 확인만)
```
검증: 변경 없으면 commit skip.

#### Commit 4 — 빌드 검증 + 단말 flash + L3 시나리오
- 빌드 → Flash/RAM 감소 측정
- 단말 USB flash
- LCD 부팅 확인 + Long-click + Sleep + MQTT 양방향 검증
- 결과 commit message 에 기록

### 11.3 Session Guide

#### Module Map

| Module | Scope Key | Description | Turns |
|--------|-----------|-------------|:---:|
| 디렉토리/파일 제거 | `module-cleanup-dirs` | src/web/ + data/www/ + tools/ + test/poc/ | 5-10 |
| main.cpp 정리 + 빌드 | `module-main-cleanup` | main.cpp + lvgl_touch.h + 빌드 통과 | 15-20 |
| 단말 검증 + 검증 docs | `module-verify` | flash + LCD + MQTT + L3 + Report | 15-20 |

#### Recommended Session Plan

| Session | Phase | Scope | Turns | Gate |
|---------|-------|-------|:---:|------|
| Session 1 | Plan + Design | 전체 | ~20 (완료) | ✅ |
| Session 2 | Do | `--scope module-cleanup-dirs,module-main-cleanup` | 20-30 | **빌드 통과 + grep 잔존 0** |
| Session 3 | Check + Report | `--scope module-verify` | 20-25 | Match Rate ≥ 90% |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — Option C Pragmatic, 4 commit 단계, 3 module 2 세션 | KDI |
