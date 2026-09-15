# RemoteDeck_Touch_v2.4 Planning Document

> **Summary**: v2.3 의 SPI 버스 충돌을 **시간 분할 (Web active mode + LCD freeze)** 으로 본질 회피. v2.3 보존 자산 (5 모듈 + WebUI 4탭 + gzip) 재활성 + PNG + OTA partition 변경.
>
> **Project**: RemoteDeck_Touch
> **Version**: v2.4.0 (target)
> **Author**: KDI
> **Date**: 2026-06-26
> **Status**: Draft
> **Branch**: `v2.4-spi` (계획)

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.3 의 SPI 버스 공유 (W5500+TFT_eSPI VSPI host) 충돌로 큰 응답 시 hang. WebUI/PNG/OTA 비활성 상태. |
| **Solution** | **시간 분할 (Time Multiplexing)** — HTTP 요청 감지 시 LCD freeze + 안내 텍스트 표시. WebUI 동안 LVGL/TFT 비활성 (SPI 단독 점유). 10초 idle 또는 LCD touch 시 LCD 복귀. v2.3 의 모든 코드 자산 (5 모듈 + WebUI 4탭 + gzip) 그대로 재활성. |
| **Function/UX Effect** | 외부 admin 이 브라우저로 WebUI 풀세트 (Control/Images/Config/Logs) 사용 가능 + LCD 에는 "웹 접속 중" 명확 안내. 현장 사용자 LCD 터치 시 즉시 LCD 복귀 (web 일시 끊김). PNG/OTA 풀세트 동작. |
| **Core Value** | SPI freq 조정 / mutex / H/W rewire 모두 회피하면서 본질 해결. v2.3 코드 자산 100% 재활용. 동시성 손실은 사용 패턴 (admin vs 현장 분리) 으로 자연 해소. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.3 SPI 충돌 해결 + 보존된 5 모듈/WebUI/PNG/OTA 모두 재활성 |
| **WHO** | 외부 admin (브라우저, WebUI 풀세트) / 현장 사용자 (LCD 터치, IN/OUT 토글) |
| **RISK** | LCD freeze 시 현장 사용자 혼란 — 명확 안내 텍스트 + Touch 우선 정책으로 완화. PoC 시나리오 fail 시 v2.5 분기 |
| **SUCCESS** | PoC 엄격 통과 (brower 6 conn + 22KB inline 5회 + MQTT 동시 1분) + Match Rate ≥ 90% + LCD regression 無 |
| **SCOPE** | Phase 1 module-spi-poc (시간 분할 + PoC gate) → 2 module-webui-restore (PROGMEM gzip 재활성) → 3 module-png-restore → 4 module-ota-partition (SPIFFS backup/restore) → 5 통합 검증 |

---

## 1. Overview

### 1.1 Purpose

v2.3 cycle 에서 본질로 진단된 **SPI 버스 공유 충돌** (W5500 ETH + TFT_eSPI LCD 가 동일 VSPI host) 을 **시간 분할** 로 해결. 두 device 가 동시에 SPI 점유하지 않도록 mutually exclusive 모드 운영. v2.3 보존 자산 (5 모듈 코드 + WebUI 4탭 + gzip 빌드 파이프라인) 을 그대로 재활성.

### 1.2 Background

- **v2.3 (Match Rate 86.4%, archived `4963c06`)**: esp_http_server 5 모듈 코드 완성 + PoC 통과. 운영 단계에서 SPI 충돌 발견 → WebUI/PNG/OTA 비활성. 핵심 API + Control 유지.
- **v2.3 archive 인계 사항**: SPI 충돌 해결 (Option A/B/C) + WebUI/PNG/OTA 재활성 + partition 변경 + NFR 정정.
- **v2.4 결단**: 사용자 통찰 — **하드웨어 modification 없이 + freq 조정 없이 + mutex 없이** 시간 분할로 본질 해결.

### 1.3 Related Documents

- v2.3 Report: `docs/archive/2026-06/RemoteDeck_Touch_v2.3/RemoteDeck_Touch_v2.3.report.md`
- v2.3 Analysis: `docs/archive/2026-06/RemoteDeck_Touch_v2.3/RemoteDeck_Touch_v2.3.analysis.md`
- v2.3-httpd 브랜치 (보존 자산): origin push 완료, 15 commits

---

## 2. Scope

### 2.1 In Scope

- [ ] **Web active mode**: HTTP 요청 감지 시 진입, 마지막 요청 timestamp + 10초 idle 후 자동 종료
- [ ] **LCD freeze**: 전체 화면 안내 텍스트 ("웹 접속 중 — 잠시 대기") + LVGL touch 무시 + TFT_eSPI flush 비활성
- [ ] **LCD touch 우선**: Touch IRQ 감지 시 즉시 web_active=false → LCD 복귀 (web 응답 일시 끊김 허용)
- [ ] **WebUI 풀세트 재활성**: handleRoot 가 INDEX_HTML_GZ (gzip 7KB) 사용. 4탭 (Control/Images/Config/Logs) 운영
- [ ] **PNG decoder 재활성**: LV_USE_PNG=1 + LCD freeze 모드 덕분에 race 해소 기대
- [ ] **OTA partition 변경**: `huge_app.csv` → `min_spiffs.csv` 또는 custom (app0+app1 + SPIFFS)
- [ ] **SPIFFS backup/restore 절차**: deviceconfig + imagesconfig + /images/* 모두 backup → flash → restore
- [ ] **PoC 엄격 검증**: brower 6 동시 connection + 22KB inline HTML 5회 + 30초 sustained + MQTT 동시 1분
- [ ] **v2.3 자산 100% 재활용**: 새 모듈 작성 거의 없음 (Web active flag + LCD freeze 로직만 신규)
- [ ] **Branch**: `v2.4-spi` 새 브랜치, main (v2.3-final) 운영 펌웨어 보호

### 2.2 Out of Scope

- ❌ SPI frequency 조정 (시간 분할로 회피)
- ❌ TFT_eSPI mutex 명시 (시간 분할로 불필요)
- ❌ TFT 핀 H/W rewire (시간 분할로 불필요)
- ❌ WebSocket / SSE (Short polling 1s 충분)
- ❌ 다중 사용자 / RBAC (Basic Auth admin:12345 유지)
- ❌ 시간 표시 UI (v2.3 명시 제외 유지)
- ❌ FULL bin 웹 업로드 (OTA partition 만)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | Web active mode 진입 — HTTP 요청 받을 때마다 web_active=true + last_web_ts 갱신 | High | Pending |
| FR-02 | LCD freeze — LVGL flush 중단 + 전체 화면 안내 텍스트 ("웹 접속 중 — 잠시 대기") | High | Pending |
| FR-03 | Web idle 10초 → 자동 LCD 복귀 (main loop 에서 millis 비교) | High | Pending |
| FR-04 | LCD touch 우선 — Touch IRQ 감지 시 web_active=false + 즉시 LCD 복귀 | High | Pending |
| FR-05 | WebUI 풀세트 재활성 — handleRoot 가 INDEX_HTML_GZ (gzip 7KB) 사용 | High | Pending |
| FR-06 | 4탭 (Control/Images/Config/Logs) 모두 운영 (v2.3 코드 보존됨) | High | Pending |
| FR-07 | PNG decoder 재활성 — LV_USE_PNG=1 + LCD freeze 보호 효과 검증 | High | Pending |
| FR-08 | OTA partition 변경 — app0 1.9MB + app1 1.9MB + SPIFFS 400KB | High | Pending |
| FR-09 | SPIFFS backup/restore 자동화 — deviceconfig + imagesconfig + /images/* | Medium | Pending |
| FR-10 | OTA /api/ota 실제 동작 — 새 partition 으로 Update.h 정상 | High | Pending |
| FR-11 | LCD regression 無 — Long-click / Sleep / 이미지 / MQTT 모두 v2.1 동작 보존 | High | Pending |
| FR-12 | PoC 엄격 통과 — brower multi-conn + 22KB inline + MQTT 동시 | High | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement |
|----------|----------|------------|
| **Stability** | brower 6 동시 GET / + GET /api/control polling 30초 fail=0 | curl parallel + uptime monotonic |
| **Performance** | LCD freeze 전환 ≤ 100ms, 복귀 ≤ 200ms | Serial 로그 timing |
| **Memory** | heap baseline ≥ 35KB (v2.3 정정값), heap_min ≥ 30KB | /api/status |
| **UX** | LCD freeze 안내 텍스트 사용자 인지 + 터치 우선 정책 명확 | 사용자 검증 |
| **Compatibility** | Arduino-ESP32 3.x + pioarduino 53.x + TFT 2.5.43 | platformio compile |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-12 전부 구현 완료
- [ ] PoC 엄격 시나리오 무장애 통과 (brower multi-conn + 22KB inline + MQTT)
- [ ] LCD regression 없음 (Long-click, Sleep, 이미지, MQTT 시나리오 통과)
- [ ] WebUI 4탭 풀세트 운영 (Control IN/OUT + Images upload + Config 편집 + Logs)
- [ ] PNG decoder 동작 — 240×86 PNG 업로드 + LCD 렌더링 정상
- [ ] OTA bin 업로드 동작 — 단말 자동 재부팅 + 신 펌웨어 동작
- [ ] Gap Analysis Match Rate ≥ 90%
- [ ] v2.4-spi → main merge 후 단말 배포

### 4.2 Quality Criteria

- [ ] Web active 진입/종료 timing 검증 (Serial 로그)
- [ ] LCD freeze 시 LVGL frame drop 없음 (TFT 점유 안 함이라 자명)
- [ ] LCD touch 우선 정책 race-free (web_active=false → 다음 main loop tick 에서 LCD 복귀)
- [ ] OTA partition 변경 후 dual app 정상 (Update.h 동작 검증)

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| **시간 분할도 fail** (HTTP 응답 도중 LCD touch race) | High | Low | LCD touch 우선 정책 즉시 web_active=false → 다음 HTTP 응답까지만 SPI 충돌 가능. 짧은 응답만 영향 |
| LCD freeze 시 사용자 혼란 ("단말 죽었음 오해") | Medium | Medium | 전체 화면 안내 텍스트 명확 + 10초 timeout 짧음 + Touch 한 번에 즉시 복귀 |
| SPIFFS backup/restore 절차 실수 → 데이터 손실 | High | Medium | backup 자동화 스크립트 + restore 검증 + step-by-step 가이드 |
| Partition table 변경 시 boot loader 깨짐 | High | Low | 표준 min_spiffs.csv 사용 + esptool full erase 후 flash |
| WebUI 사용 시간 길어지면 MQTT 메시지 누락 | Medium | Low | MQTT main loop (core 1) 는 LCD freeze 와 무관, web_active 와도 무관 |
| LCD/MQTT 동작 회귀 (LCD freeze 로직 추가) | High | Low | v2.1 setup 순서 + DeviceManager 진입 등 100% 보존, freeze 는 main loop 의 lvgl_loop() 만 영향 |
| brower 의 multi-conn 부하 (6+ 동시) 시간 분할도 fail | High | Low | LCD freeze 동안 ETH 가 SPI 단독 점유 — 동시성 제약 해소 기대 (PoC 검증) |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `RemoteDeck_Touch/src/main.cpp` | Modify | web_active flag + last_web_ts + freezeLCD/resumeLCD 함수 + main loop 분기 |
| `RemoteDeck_Touch/src/web/WebServer.cpp` | Modify | requireAuth 직후 모든 handler 에서 web_active 시그널 보냄 |
| `RemoteDeck_Touch/src/web/WebServer.h` | Modify | webActivityCallback signature 추가 |
| `RemoteDeck_Touch/lib/lv_conf.h` | Modify | LV_USE_PNG 0 → 1 (재활성) |
| `RemoteDeck_Touch/platformio.ini` | Modify | partition: huge_app.csv → min_spiffs.csv 또는 custom |
| 신규 partition 파일 (필요 시) | New | `partitions_v24.csv` — app0+app1+SPIFFS 균형 |
| `RemoteDeck_Touch/tools/spiffs_backup.py` | New | curl 로 SPIFFS data 일괄 backup |
| `RemoteDeck_Touch/tools/spiffs_restore.py` | New | partition flash 후 backup 복원 |
| `RemoteDeck_Touch/test/poc/v24_poc.py` | New | brower 6 동시 + 22KB inline + MQTT 동시 시나리오 |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| LVGL `lv_timer_handler` | TICK | `lvgl_loop()` from `main.cpp` loop | Needs verification — web_active 면 skip |
| LCD touch (FT6236G) | IRQ | LVGL indev driver | Needs verification — touch 감지 시 web_active=false 우선 |
| MQTT loop (core 1) | run | `mqttEthernet_loop()` | None — LCD freeze 무관, 그대로 동작 |
| ImageApi.loop()` (_pendingReload) | tick | main loop | Needs verification — web_active 동안에도 호출, images_update 는 LCD freeze 중이라 차이 없음 |
| ConfigApi.loop() / OtaApi.loop() | reboot trigger | main loop | None |
| SPIFFS /images/* /deviceconfig.json | READ/WRITE | 모든 *Api | OTA partition 변경 후 SPIFFS 영역 축소 — backup/restore 필수 |

### 6.3 Verification

- [ ] main loop 에서 `if (web_active && !lcd_priority) skip lvgl_loop` 분기 검증
- [ ] WebServer handler 모두 첫 줄에 `markWebActive()` 호출 검증
- [ ] LCD touch (IRQ pin 6, FT6236G I2C) 감지 시 즉시 `web_active=false` 검증
- [ ] OTA partition 변경 후 dual app boot 검증 (Update.begin 성공)

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

| Level | Selected |
|-------|:--------:|
| Starter (임베디드 단일 펌웨어) | ☑ |

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| **SPI 충돌 해결** | freq / mutex / rewire / **시간 분할** | **시간 분할** | 사용자 통찰. H/W 변경 없음 + 코드 minimal |
| Web active mode | flag + timestamp / state machine / RTOS task | **flag + timestamp** | 가장 단순. main loop tick 으로 충분 |
| LCD freeze 시 안내 | 전체 텍스트 / 아이콘 / 단순 freeze | **전체 텍스트** | 사용자 답변. 명확 |
| LCD touch 우선 정책 | 즉시 종료 / grace 연장 / 무시 | **즉시 종료** | 사용자 답변. tap-to-acquire |
| Timeout | 5s / 10s / 30s / manual | **10초** | 사용자 답변. polling 3s throttle 와 호환 |
| WebUI 활성화 | INDEX_HTML_GZ 재활성 / 새로 작성 | **INDEX_HTML_GZ 재활성** | v2.3 자산 그대로 재사용 |
| OTA partition | huge_app 유지 / min_spiffs / custom | **min_spiffs.csv** | ESP-IDF 표준 + SPIFFS 128KB 가 deviceconfig+imagesconfig 만이라면 충분 |
| SPIFFS backup | 수동 / 자동 스크립트 | **자동 스크립트** | tools/spiffs_backup.py + restore.py |
| Branch | main 직접 / v2.4-spi 신규 | **v2.4-spi** | main 운영 펌웨어 보호 |
| PoC 기준 | 최소 / 중간 / 엄격 | **엄격** | 사용자 답변. v2.3 hang 재현 시도 |

### 7.3 모듈 구조

```
RemoteDeck_Touch/
├── platformio.ini                     [수정] partitions = min_spiffs.csv
├── lib/lv_conf.h                      [수정] LV_USE_PNG 0→1
├── src/
│   ├── main.cpp                       [수정] web_active flag + LCD freeze 로직
│   ├── web/WebServer.{h,cpp}          [수정] markWebActive() 호출 hook
│   ├── (그 외 5 모듈)                 (v2.3 그대로 — 코드 무수정)
│   └── images/images.cpp              (v2.3 그대로 — PNG safety check 보존)
├── tools/
│   ├── embed_www.py                   (v2.3 그대로 — gzip 파이프라인)
│   ├── spiffs_backup.py               [신규] curl 자동 backup
│   └── spiffs_restore.py              [신규] partition flash 후 restore
└── test/poc/
    └── v24_poc.py                     [신규] brower 6 동시 + 22KB inline + MQTT
```

---

## 8. Convention Prerequisites

기존 v2.3 컨벤션 그대로 유지 — 모든 모듈/네이밍/Auth/Error 포맷 동일.

### 8.1 Existing Project Conventions

- [x] `CLAUDE.md` 존재
- [x] platformio.ini build_flags 가 사실상 컨벤션 (TFT_eSPI build_flags 이식 등)
- [x] Arduino-ESP32 3.x + pioarduino 53.x 고정
- [x] v2.3 의 `src/web/*` 콜백 시그니처 (Option C Pragmatic)

### 8.2 신규 컨벤션 (v2.4)

| Category | Rule |
|----------|------|
| Web active flag | `volatile bool web_active`, `volatile uint32_t last_web_ts` (main.cpp 전역) |
| Activity hook | WebServer handler 첫 줄: `requireAuth(req); markWebActive();` |
| LCD freeze 함수 | `void freezeLCD()` + `void resumeLCD()` (main.cpp static) |
| Timeout | `#define WEB_IDLE_TIMEOUT_MS 10000` |

---

## 9. Implementation Phases (PoC-first)

| Phase | 범위 | Gate |
|-------|------|-----|
| **Phase 1 — module-spi-poc** | web_active + LCD freeze + WebServer hook + PoC v24 시나리오 | **PoC 엄격 통과**: brower 6 동시 + 22KB inline + MQTT 동시 1분 무장애. fail 시 v2.5 분기 |
| **Phase 2 — module-webui-restore** | handleRoot 가 INDEX_HTML_GZ 사용 (1줄 변경) + 4탭 전체 검증 | 모든 4탭 동작 + 큰 응답 hang 無 |
| **Phase 3 — module-png-restore** | LV_USE_PNG=1 + PNG decode + LCD 렌더 검증 | 240×86 PNG round-trip + LCD 표시 정상 |
| **Phase 4 — module-ota-partition** | partition 변경 + SPIFFS backup/restore + OTA bin 실제 동작 | OTA bin 업로드 → 단말 자동 reboot → 신 펌웨어 부팅 |
| **Phase 5 — 통합 검증** | Gap Analysis + LCD regression | Match Rate ≥ 90%, M1~M9 모두 통과 |

---

## 10. Next Steps

1. [ ] Design 문서 작성 (`/pdca design RemoteDeck_Touch_v2.4`) — 3 Architecture Options + 모듈 dep graph
2. [ ] v2.4-spi 브랜치 생성 (v2.3-httpd 에서 분기)
3. [ ] tools/spiffs_backup.py 사전 작성 (PoC 단계 진입 전 단말 backup)

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — 시간 분할 architecture + v2.3 자산 100% 재활용 + PoC 엄격 | KDI |
