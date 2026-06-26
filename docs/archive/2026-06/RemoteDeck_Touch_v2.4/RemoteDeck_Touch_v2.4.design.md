# RemoteDeck_Touch_v2.4 Design Document

> **Summary**: 시간 분할 (WebActivityMonitor 단일 클래스) 로 SPI 충돌 본질 회피. v2.3 코드 자산 100% 재활용 + 4 phase 진행.
>
> **Project**: RemoteDeck_Touch
> **Version**: v2.4.0 (target)
> **Author**: KDI
> **Date**: 2026-06-26
> **Status**: Draft
> **Planning Doc**: [RemoteDeck_Touch_v2.4.plan.md](../../01-plan/features/RemoteDeck_Touch_v2.4.plan.md)
> **Branch**: `v2.4-spi`

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.3 SPI 충돌 해결 + 보존된 5 모듈/WebUI/PNG/OTA 모두 재활성 |
| **WHO** | 외부 admin (브라우저) / 현장 사용자 (LCD 터치) |
| **RISK** | 시간 분할도 fail → PoC 통과 안 되면 v2.5 분기 |
| **SUCCESS** | PoC 엄격 통과 + Match Rate ≥ 90% + LCD regression 無 |
| **SCOPE** | Phase 1 SPI PoC → 2 WebUI restore → 3 PNG → 4 OTA partition → 5 검증 |

---

## 1. Overview

### 1.1 Design Goals

- **시간 분할 본질 회피**: WebUI 활성 동안 LVGL/TFT_eSPI 정지 → ETH 가 SPI 단독 점유 → 큰 응답 hang 없음
- **명확한 책임 분리**: `WebActivityMonitor` 클래스가 web active state 단독 보유 + 콜백으로 LCD 제어 hook
- **v2.3 자산 100% 재활용**: 5 모듈 코드 무수정, WebUI 4탭 그대로, INDEX_HTML_GZ 그대로
- **LCD UX 명확화**: 전체 화면 안내 텍스트 + Touch 우선 정책 (tap-to-acquire)

### 1.2 Design Principles

1. **단일 책임** — WebActivityMonitor 가 web active flag + timestamp 만 보유. LCD 제어는 콜백으로 분리
2. **Minimal invasive** — v2.3 의 5 모듈 / WebUI / PoC 스크립트 무수정. main.cpp + WebServer.cpp 만 최소 수정
3. **Fail Fast** — PoC 시나리오 1개라도 fail → v2.5 분기 결정 (시간 분할도 본질 해결 안 됨 명백)
4. **YAGNI** — LcdModeController 같은 state machine 비도입. 2 state (NORMAL/FROZEN) 는 boolean flag 로 충분

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | A: Minimal | B: Clean | **C: Pragmatic** ⭐ |
|----------|:-:|:-:|:-:|
| Approach | main.cpp 전역 변수 | WebActivityMonitor + LcdModeController + state machine | WebActivityMonitor 단일 + 콜백 |
| New Files | 0 | 4 | **2** |
| Modified Files | 2 | 3 | **3** |
| Complexity | Low | High | **Medium** |
| Maintainability | Low (main.cpp 비대) | High | **High** |
| Effort | Fast | Slow | **Medium** |
| Risk | race + 추적 어려움 | over-engineering | **Low** |

**Selected**: **Option C — Pragmatic**
**Rationale**: 명확한 인터페이스 + 최소 변경 + 추후 다른 동시성 이슈 추가 시 확장 용이.

### 2.1 Component Diagram

```
                  ESP32 (Arduino-ESP32 3.x + pioarduino 53.x)
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  ┌─ Core 0 (httpd task) ────────┐   ┌─ Core 1 (Arduino loop) ────┐ │
│  │                              │   │                            │ │
│  │  esp_http_server (port 80)   │   │  main loop                 │ │
│  │   ├─ /  → INDEX_HTML_GZ      │   │   ├─ if (!monitor.shouldFreezeLcd()) │
│  │   ├─ /api/* (14 endpoints)   │   │   │      lvgl_loop()       │ │
│  │   │                          │   │   │  else                  │ │
│  │   │  매 handler 첫 줄:        │   │   │      (LCD frozen)     │ │
│  │   │  → monitor.markActive()  │   │   ├─ touch_irq_pin read    │ │
│  │   │                          │   │   │  → monitor.notifyTouch() │
│  │   └─ Basic Auth middleware   │   │   ├─ imageApi.loop()       │ │
│  │                              │   │   ├─ configApi.loop()      │ │
│  └──────────────────────────────┘   │   ├─ otaApi.loop()         │ │
│                                     │   └─ mqttEthernet_loop()   │ │
│  ┌─ WebActivityMonitor ────────┐    │                            │ │
│  │  volatile bool active       │    │  freezeLCD()  ── TFT_eSPI │ │
│  │  volatile uint32_t lastTs   │    │  ─ 전체 화면 텍스트:        │ │
│  │  IDLE_TIMEOUT 10000ms       │    │     "웹 접속 중 — 잠시 대기" │ │
│  │  markActive()               │    │                            │ │
│  │  notifyTouch()              │    │  resumeLCD() ── LVGL 복귀  │ │
│  │  shouldFreezeLcd()          │    │                            │ │
│  │  onModeChange(callback)     │    │                            │ │
│  └─────────────────────────────┘    └────────────────────────────┘ │
│         ↑                                                           │
│         │ setOnModeChange([](bool active){                          │
│         │   if (active) freezeLCD(); else resumeLCD();              │
│         │ })                                                        │
│                                                                     │
│  ┌─ Shared SPI Bus (VSPI) ──────────────────────────────────────┐   │
│  │  W5500 (CS=5) + TFT_eSPI (CS=15)                             │   │
│  │  시간 분할 — 동시 점유 없음 (mode != FROZEN 일 때만 TFT)        │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

#### Web 접속 시 (LCD freeze 진입)
```
브라우저 GET /
  → httpd URI handler (core 0)
    → Basic Auth
    → monitor.markActive()
       │  active=true (이미 false 였으면 onModeChange(true) 호출)
       │  lastTs = millis()
       │  (Core 1 main loop 다음 tick 에서 shouldFreezeLcd()=true)
    → INDEX_HTML_GZ (7KB) send (이제 TFT 정지 → SPI 단독, hang 없음)
  ← Browser

(Core 1 main loop)
  if (monitor.shouldFreezeLcd()) {
     onModeChange(true) 시 freezeLCD() 호출됨 (한 번만)
     lvgl_loop() skip
  }
```

#### Web idle 후 LCD 복귀
```
(브라우저 polling 멈춤 또는 timeout)
(Core 1 main loop 매 tick)
  monitor.shouldFreezeLcd():
    if (active && (millis() - lastTs) > IDLE_TIMEOUT) {
       active = false
       onModeChange(false) 호출 → resumeLCD() → lv_obj_invalidate(ui)
    }
    return active && (millis() - lastTs) < IDLE_TIMEOUT
  if (!shouldFreezeLcd()) lvgl_loop()  // 복귀
```

#### LCD touch 우선 (tap-to-acquire)
```
(Core 1 main loop)
  touch_state = read FT6236G via I2C  (I2C 라 SPI 무관)
  if (touch_pressed && monitor.isActive()) {
     monitor.notifyTouch()
        │  active = false
        │  lastTs = 0
        │  onModeChange(false) → resumeLCD()
     // 다음 tick 부터 lvgl_loop() 정상 동작
  }
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| WebActivityMonitor | (없음) | 순수 state 보유 |
| WebServer (v2.3 유지) | WebActivityMonitor (콜백) | 매 handler 진입 시 markActive() |
| main.cpp | WebActivityMonitor, TFT_eSPI, LVGL | freezeLCD/resumeLCD 실행 |
| LVGL / TFT_eSPI | (이전 그대로) | shouldFreezeLcd()=true 동안 lvgl_loop() 호출 안 됨 |
| FT6236G touch (I2C) | (이전 그대로) | I2C 라 SPI 무관 — 항상 read 가능 |

---

## 3. Data Model

### 3.1 WebActivityMonitor (신규)

```cpp
class WebActivityMonitor {
public:
    static constexpr uint32_t IDLE_TIMEOUT_MS = 10000;

    using ModeChangeCb = std::function<void(bool active)>;

    void setOnModeChange(ModeChangeCb cb) { _onChange = cb; }

    // WebServer handler 첫 줄에서 호출 (core 0 task)
    void markActive();

    // main loop 의 touch IRQ 처리에서 호출 (core 1)
    void notifyTouch();

    // main loop 매 tick 호출 — 시간 비교 + state transition
    bool shouldFreezeLcd();

    bool isActive() const { return _active; }

private:
    volatile bool _active = false;
    volatile uint32_t _lastTs = 0;
    ModeChangeCb _onChange = nullptr;
};
```

### 3.2 기존 ControlState / OtaState / UploadState (v2.3 유지)

변경 없음.

---

## 4. API Specification

v2.3 의 14 endpoint 그대로 유지 + Behavior 차이만 있음:

| # | Endpoint | Method | 동작 차이 (v2.3 → v2.4) |
|---|----------|--------|-----|
| 1 | `/` | GET | minimal HTML → **INDEX_HTML_GZ (7KB gzip)** (재활성) |
| 2-13 | `/api/*` | 모두 | 동작 동일, 단 매 진입 시 monitor.markActive() 추가 호출 |
| 14 | `/api/ota` | POST | **partition 변경 후 실제 동작** (Update.h dual partition) |

신규 endpoint 없음.

---

## 5. UI/UX Design

### 5.1 LCD freeze 화면

```
┌─────────────────────────────┐
│                             │
│                             │
│      📡 웹 접속 중           │
│                             │
│     잠시만 대기해 주세요    │
│                             │
│  화면을 터치하면 즉시 복귀  │
│                             │
│                             │
└─────────────────────────────┘
```

- 배경: 검은색
- 텍스트 색: 흰색 / 안내문구 회색
- 폰트: TFT_eSPI 기본 폰트 (LV_FONT 사용 안 함 — LVGL 비활성 상태)
- TFT_eSPI 의 `tft.fillScreen(TFT_BLACK)` + `tft.setCursor()` + `tft.println()` 으로 직접 그림

### 5.2 User Flow

```
[Normal LCD]
   ↓ (브라우저 GET /)
[Web Active] — markActive() → onModeChange(true) → freezeLCD()
   ↓ ("웹 접속 중" 화면 표시, LVGL/TFT_eSPI flush 정지)
   ↓ (polling client 3초 간격 GET → markActive() 매번 갱신)
   ↓ (사용자가 LCD 터치 OR 10초 web idle)
[LCD 복귀] — onModeChange(false) → resumeLCD() → lv_obj_invalidate_all() → lvgl_loop() 재개
```

### 5.3 Component List

| Component | Location | Responsibility |
|-----------|----------|----------------|
| WebActivityMonitor | `src/web/WebActivityMonitor.{h,cpp}` | web active state + 콜백 |
| WebServer (수정) | `src/web/WebServer.cpp` | requireAuth 직후 monitor->markActive() |
| main.cpp (수정) | `src/main.cpp` | freezeLCD / resumeLCD 함수, touch IRQ → notifyTouch, main loop 분기 |
| INDEX_HTML (v2.3 자산) | `src/web/embedded_assets.cpp` | INDEX_HTML_GZ 7KB gzip (재활성) |

### 5.4 Page UI Checklist

WebUI 자체는 v2.3 의 4탭 (Control/Images/Config/Logs) 그대로. 신규 UI 없음.

LCD freeze 화면:
- [ ] 검은 배경 (`tft.fillScreen(TFT_BLACK)`)
- [ ] "📡 웹 접속 중" 큰 텍스트 (TFT_eSPI font 4)
- [ ] "잠시만 대기해 주세요" 부텍스트
- [ ] "화면을 터치하면 즉시 복귀" 안내

---

## 6. Error Handling

| 상황 | 동작 |
|------|------|
| markActive() 호출됐는데 onChange 콜백 미등록 | NULL check + skip (no-op) |
| freezeLCD() 시 TFT 응답 안 함 | retry 1회 + Serial 로그 + main loop 진행 (web 응답 우선) |
| Web 응답 도중 touch 발생 | monitor.notifyTouch() → 다음 main tick 에서 resumeLCD(). 진행 중 응답은 계속 (짧은 SPI 충돌 가능, 응답 끝까지 보냄) |
| onChange 콜백 안에서 reentrancy | static recursion guard (기 진입 중이면 skip) |
| SPI fail 후 W5500 stuck | 기존 ETH retry 로직 동작 (v2.1 보존) |

---

## 7. Security Considerations

- [x] Basic Auth (admin:12345) — v2.3 유지
- [x] 파일명 sanitize — v2.3 유지
- [x] Upload 사이즈 상한 — 이미지 200KB, OTA partition size 까지
- [x] OTA 무결성 — Update.h MD5, dual partition factory fallback
- [ ] HTTPS — 미적용 (LAN 환경 전제)

---

## 8. Test Plan

### 8.1 Test Scope

| Type | Target | Tool | Phase |
|------|--------|------|-------|
| L1: API Tests | 14 endpoint shape + freeze 진입 검증 | curl + serial log | Do |
| L2: PoC 엄격 | brower 6 동시 + 22KB inline + MQTT 동시 | parallel curl + python | Do (Phase 1 gate) |
| L3: LCD 시나리오 | 사용자 직접 검증 | LCD 관찰 + touch | Check |

### 8.2 L1: API Test (v2.3 14건 + 신규 4건)

기존 v2.3 14건 그대로 + 신규:

| # | Check | Expected |
|---|-------|---------|
| 15 | GET / 시 LCD 가 freeze 모드 진입 (Serial 로그 "FREEZE") | 즉시 진입 |
| 16 | 11초 idle 후 LCD 복귀 (Serial 로그 "RESUME") | 자동 복귀 |
| 17 | LCD touch 시 즉시 web_active=false → resume | 다음 tick (< 100ms) |
| 18 | GET / 7165 byte size_download (gzip 정상) | 200 / size 7165 |

### 8.3 L2: PoC 엄격 시나리오 (Phase 1 Gate)

> **fail 시 v2.5 분기 — Plan 명시**

| # | 시나리오 | 통과 조건 |
|---|---------|---------|
| P1 | brower 6 동시 GET / (22KB inline HTML 다운) | 6/6 200, body 완전 (gzip decompressed = 22KB) |
| P2 | 6 동시 × 5회 반복 = 30 요청 burst | 30/30 200, heap 안정 |
| P3 | 30초 sustained: 3 동시 자산 + status + control polling 혼합 | fail 0, uptime monotonic |
| P4 | MQTT publish 1Hz 동시 + 위 30초 부하 | MQTT 메시지 누락 0, web 모두 통과 |
| P5 | Web 부하 중 LCD touch → 즉시 복귀 + web 끊김 자연스러움 | touch < 200ms 내 resume |

### 8.4 L3: LCD 시나리오 (수동)

| # | 시나리오 | 통과 조건 |
|---|---------|---------|
| M1 | LCD freeze 안내 텍스트 명확 표시 | 사용자 인지 가능 |
| M2 | 10초 자동 복귀 | timeout 정확 |
| M3 | Touch 시 즉시 복귀 | 200ms 이내 |
| M4 | Long-click → DeviceManager (v2.1 동작 보존) | LCD active 시 정상 |
| M5 | Sleep / MQTT / 이미지 표시 (v2.1 regression) | 모두 정상 |
| M6 | PNG decode + LCD 렌더 (240×86 PNG) | freeze 도중 decode 안 함, resume 후 렌더 |
| M7 | OTA 업로드 → 단말 reboot → 신 펌웨어 | partition 정상 |

---

## 9. Clean Architecture

### 9.1 Layer Structure

| Layer | Responsibility | Location |
|-------|---------------|----------|
| Presentation (Web) | WebUI 4탭 + URI dispatch | `data/www/`, `src/web/WebServer.*` |
| Application | API handlers (콜백) + Monitor | `src/web/{Image,Config,Control,Ota,Logger,WebActivityMonitor}` |
| Domain | Device state, image config | `src/device/`, `src/images/` |
| Infrastructure | esp_http_server, ETH/MQTT, SPIFFS, Update.h, TFT_eSPI, LVGL | ESP-IDF / Arduino-ESP32 |

### 9.2 Dependency Rules

```
WebServer (handler) ──→ WebActivityMonitor (markActive)
                            │
                            ▼ (callback)
                       main.cpp (freezeLCD / resumeLCD)
                            │
                            ▼
                       TFT_eSPI direct draw + LVGL skip
```

규칙:
- WebActivityMonitor 는 ESP-IDF/Arduino 의존 없음 (순수 state)
- 콜백을 통해 LCD 제어 (의존 inversion)

---

## 10. Coding Convention Reference

### 10.1 Naming

| Target | Rule | Example |
|--------|------|---------|
| C++ class | PascalCase | `WebActivityMonitor` |
| Method | camelCase | `markActive()`, `shouldFreezeLcd()` |
| Constant | UPPER_SNAKE | `IDLE_TIMEOUT_MS` |
| File | PascalCase.{h,cpp} | `WebActivityMonitor.cpp` |

### 10.2 Code Comments

```cpp
// Design Ref: §2.2 — 시간 분할로 SPI 충돌 회피
// Plan SC: FR-01 (Web active mode 진입), FR-04 (LCD 우선)
void WebActivityMonitor::markActive() { ... }
```

---

## 11. Implementation Guide

### 11.1 File Structure

```
RemoteDeck_Touch/
├── platformio.ini                    [수정] partitions = min_spiffs.csv
├── lib/lv_conf.h                     [수정] LV_USE_PNG 0→1
├── src/
│   ├── main.cpp                      [수정] WebActivityMonitor + freeze/resume + main loop 분기
│   ├── web/
│   │   ├── WebActivityMonitor.{h,cpp} [신규]
│   │   ├── WebServer.{h,cpp}         [수정] markActive() hook
│   │   ├── embedded_assets.cpp       [수정 자동] INDEX_HTML_GZ 사용 (재활성)
│   │   └── (그 외 5 모듈)             (v2.3 그대로 — 무수정)
│   └── images/images.cpp             (v2.3 그대로)
├── tools/
│   ├── embed_www.py                  (v2.3 그대로)
│   ├── spiffs_backup.py              [신규]
│   └── spiffs_restore.py             [신규]
└── test/poc/
    └── v24_poc.py                    [신규] brower 6 동시 + 22KB + MQTT
```

### 11.2 Implementation Order

1. [ ] **Phase 1 — module-spi-poc** (WebActivityMonitor + freezeLCD/resumeLCD + WebServer hook + INDEX_HTML_GZ 재활성)
2. [ ] **Phase 2 — module-png-restore** (LV_USE_PNG=1 + LCD freeze 보호 효과 검증)
3. [ ] **Phase 3 — module-ota-partition** (min_spiffs.csv + SPIFFS backup/restore + 실제 OTA)
4. [ ] **Phase 4 — 통합 검증** (Gap Analysis + L1 18건 + L3 M1~M7)

### 11.3 Session Guide

#### Module Map

| Module | Scope Key | Description | Turns |
|--------|-----------|-------------|:---:|
| SPI PoC + WebUI restore | `module-spi-poc` | WebActivityMonitor + freeze/resume + WebServer hook + handleRoot 가 INDEX_HTML_GZ 사용 + L2 P1~P5 통과 | 25-30 |
| PNG restore | `module-png-restore` | LV_USE_PNG=1 + 240×86 PNG decode + LCD 렌더 검증 | 10-15 |
| OTA partition | `module-ota-partition` | min_spiffs.csv + spiffs_backup.py + spiffs_restore.py + Update.h 실제 동작 | 25-35 |
| 통합 검증 | `module-verify` | Gap Analysis + L1 18건 + L3 M1~M7 + 펌웨어 배포 | 15-20 |

#### Recommended Session Plan

| Session | Phase | Scope | Turns | Gate |
|---------|-------|-------|:---:|------|
| Session 1 | Plan + Design | 전체 | ~25 (완료) | ✅ |
| Session 2 | Do | `--scope module-spi-poc` | 25-30 | **L2 P1~P5 통과** (fail → v2.5) |
| Session 3 | Do | `--scope module-png-restore` | 10-15 | PNG round-trip + LCD 렌더 |
| Session 4 | Do | `--scope module-ota-partition` | 25-35 | OTA bin 실제 reboot |
| Session 5 | Check + Report | `--scope module-verify` | 25-30 | Match Rate ≥ 90% |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — Option C Pragmatic, 4 모듈 4 세션 | KDI |
