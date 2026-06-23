# RemoteDeck_Touch_v2.3 Planning Document

> **Summary**: v2.1 운영 펌웨어에 미구현으로 남아 있던 WebUI/PNG/OTA/Control 일괄 구현 — v2.2 sync WebServer 가설 폐기 학습을 토대로 esp_http_server (ESP-IDF native) 기반 zero-base 재설계.
>
> **Project**: RemoteDeck_Touch (ESP32 + 240x320 LCD + Touch)
> **Version**: v2.3.0 (target)
> **Author**: KDI
> **Date**: 2026-06-23
> **Status**: Draft
> **Branch**: `v2.3-httpd`

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.1은 LAN/펌웨어 배포는 안정화됐지만 WebUI/PNG/OTA/Control 미구현. v2.2는 sync WebServer 가설(연속 요청 본질적 불안정)로 폐기. |
| **Solution** | esp_http_server (ESP-IDF native, 별도 task + core pinning) 로 WebServer 본질 재설계 + v2.2-zero 브랜치 자산 (HTML/CSS/JS, ImageApi/ConfigApi/Logger 콜백) 재활용. |
| **Function/UX Effect** | 브라우저로 이미지 교체 / 설정 수정 / OTA 펌웨어 업데이트 / Control(LCD 미러 + IN/OUT 토글, MQTT 양방향) 가능. PNG 포맷 지원으로 이미지 작성 자유도 확보. |
| **Core Value** | W5500+MQTT 환경에서 동작 보장된 WebUI 스택 확정 → 단말 운영자가 원격 관리 가능, 펌웨어 갱신 인프라 표준화. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.1 미구현 기능(WebUI/PNG/OTA/Control) 일괄 구현 + v2.2 실패(동시성 불안정) 본질 해결 |
| **WHO** | 단말 운영자 (브라우저로 admin:12345 접속), 펌웨어 배포 담당자 (OTA), 현장 사용자 (Control 탭 + LCD) |
| **RISK** | esp_http_server 도 W5500+MQTT 환경에서 실패할 가능성 — PoC 풀세트 시뮬레이션으로 조기 검증, fail 시 v2.4 분기 |
| **SUCCESS** | PoC 엄격 통과 (연속 upload 10 + 병렬 GET 5 + MQTT 동시 1분 무장애) + Match Rate ≥ 90% + LCD 운용 regression 없음 |
| **SCOPE** | Phase 1: PoC (httpd + /api/status + LVGL coexist) → Phase 2: WebUI 풀세트 + PNG → Phase 3: OTA → Phase 4: Control (Long polling 10s) |

---

## 1. Overview

### 1.1 Purpose

ESP32 + 240×320 LCD + W5500 Ethernet + MQTT 환경에서 안정적으로 동작하는 WebUI 스택을 확정하고, v2.1 운영 펌웨어에 미구현으로 남은 기능 (PNG, OTA, Control) 을 일괄 구현한다.

### 1.2 Background

- **v2.1 (2026-06-22, Match Rate 68%)**: LAN 스택 통일 (ETH.h + ETH_PHY_W5500) + Long-click 진입 + Sleep 저장 완료. WebUI는 AsyncTCP task slot 충돌로 비활성. PNG/OTA/Control/시간 UI 미구현.
- **v2.2 (2026-06-23, Match Rate 49%)**: Arduino 내장 sync WebServer + 협력적 yield 가설 시도. Phase 1 단발 호출 통과 → Phase 2 풀세트 도달 후 연속/병렬 요청 본질적 불안정 발현 → 가설 폐기. Control 탭 추가 시 부팅 hang.
- **v2.3 결단**: WebServer 라이브러리를 **esp_http_server** (ESP-IDF native, 별도 task + core pinning) 로 본질 교체. v2.2 learnings 를 PoC 기준에 반영.

### 1.3 Related Documents

- v2.1 Report: `docs/archive/2026-06/RemoteDeck_Touch_v2.1/RemoteDeck_Touch_v2.1.report.md`
- v2.2 Report: `docs/archive/2026-06/RemoteDeck_Touch_v2.2/RemoteDeck_Touch_v2.2.report.md`
- v2.2 Analysis (가설 폐기 사유): `docs/archive/2026-06/RemoteDeck_Touch_v2.2/RemoteDeck_Touch_v2.2.analysis.md`
- v2.2-zero 브랜치: WebUI HTML/CSS/JS + 모듈 콜백 (재활용 후보)
- RemoteDeck_PC v2.3.0 OTA 패턴: `RemoteDeck_PC/firmware/`

---

## 2. Scope

### 2.1 In Scope

- [ ] **esp_http_server 도입**: WebServer 라이브러리를 ESP-IDF native httpd 로 교체 (별도 task + core pinning)
- [ ] **PoC 엄격 검증**: 연속 upload 10회 + 병렬 GET 5개 + MQTT 동시 트래픽 1분 무장애 통과
- [ ] **WebUI 풀세트**: 이미지 (list/upload/delete/preview) + Config (deviceconfig 편집) + Logs (ring buffer 50건)
- [ ] **PNG 디코더 활성**: `LV_USE_PNG=1` + 메모리 안전장치 (v2.1 BMP 패턴 이식)
- [ ] **OTA 핸들러**: `/api/ota` Update.h 기반, OTA bin 1.72MB 업로드 (FULL bin은 flash.bat 수동 유지)
- [ ] **Control 탭**: LCD 미러 + IN/OUT 토글, **Long polling 10s + ETag** (v2.2 부팅 hang 원인 회피)
- [ ] **v2.2-zero 자산 재활용**: index.html / style.css / app.js + ImageApi/ConfigApi/Logger 콜백 인터페이스 (라이브러리만 esp_http_server 로 교체)
- [ ] **Basic Auth**: admin:12345 유지 (v2.1 동일)
- [ ] **브랜치**: `v2.3-httpd` 에서 작업, main 운영 펌웨어 (v2.1 156d089) 보호

### 2.2 Out of Scope

- ❌ **시간 표시 UI**: 메인 컨트롤 UI 공간 없음 (v2.3 명시적 제외, v2.4+ 검토)
- ❌ WebSocket / SSE: HTTP Long polling 으로 충분
- ❌ 다중 사용자 / RBAC: Basic Auth admin 단일 계정
- ❌ FULL bin 웹 업로드: OTA partition (1.72MB) 만 지원
- ❌ SPIFFS 이미지 partition 웹 갱신: 이미지 개별 업로드로 충분
- ❌ 클라우드 / 외부 브로커: 로컬 MQTT 브로커 기존 유지

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | esp_http_server 가 별도 task (core 0) 에서 동작, LVGL/MQTT loop (core 1) 와 격리 | High | Pending |
| FR-02 | PoC 풀세트 통과: 연속 upload 10회 + 병렬 GET 5개 + MQTT 동시 트래픽 1분 무장애 | High | Pending |
| FR-03 | WebUI 3탭 (Images/Config/Logs) + Control 탭 (총 4탭) | High | Pending |
| FR-04 | PNG 이미지 업로드/표시 (LV_USE_PNG=1, BMP 와 동일 안전장치) | High | Pending |
| FR-05 | OTA 핸들러 `/api/ota` — 1.72MB OTA bin 업로드 → Update.h 적용 → reboot | High | Pending |
| FR-06 | Control 탭 Long polling 10s + ETag 304 — 부팅 시 hang 없음, 자원 점유 최소 | High | Pending |
| FR-07 | MQTT 상태 변경 시 Control 탭 다음 폴링에서 갱신 (LCD 미러 일치) | High | Pending |
| FR-08 | Basic Auth admin:12345 (v2.1 동일) — 모든 /api/* 보호 | Medium | Pending |
| FR-09 | LCD/터치 UI regression 없음 (Long-click 진입, Sleep 저장, 이미지 디코드) | High | Pending |
| FR-10 | DHCP 15s + 재시도 1회 유지 (v2.1 운영값) | Medium | Pending |
| FR-11 | 펌웨어 빌드 FULL (4MB) + OTA (1.72MB) 양산, 자동 버전 파싱 (PC v2.3.0 패턴) | Medium | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| Stability | PoC 엄격 시나리오 1분 무장애 + heap free ≥ 80KB 유지 | Serial monitor + `/api/status` heap 추이 |
| Performance | 이미지 upload 200KB ≤ 5초, /api/status 응답 ≤ 500ms | curl timing |
| Memory | 부팅 후 free heap ≥ 100KB, OTA 진행 중 ≥ 40KB | ESP.getFreeHeap() 로깅 |
| Concurrency | 동시 GET 5개 처리 중 LVGL frame drop 무 | LCD 시각 관찰 + lv_timer_handler log |
| Compatibility | Arduino-ESP32 3.x + pioarduino 53.x + TFT_eSPI 2.5.43 (v2.1 환경) | platformio compile + flash |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-11 전부 구현 완료
- [ ] PoC 엄격 시나리오 무장애 통과 (스크린샷 + serial log 증거)
- [ ] LCD 운용 regression 없음 (수동 검증 1, 2, 3, 4, 5 시나리오)
- [ ] FULL + OTA bin 빌드 성공, flash.bat 로 단말 적용 확인
- [ ] Gap Analysis Match Rate ≥ 90%
- [ ] v2.3-httpd → main merge 후 단말에 OTA 배포

### 4.2 Quality Criteria

- [ ] esp_http_server task가 LVGL task 와 다른 core 에 pinning 됨 (확인: vTaskGetInfo)
- [ ] free heap 100KB 이상 부팅 후 유지
- [ ] 모든 /api/* 엔드포인트 Basic Auth 보호 (curl 401 확인)
- [ ] PNG + BMP 양쪽 디코드 안전장치 (크기 / heap / OLD free) 동작

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| esp_http_server 도 W5500+MQTT 동시성에서 fail | High | Medium | **PoC 엄격 검증**. fail 시 v2.4 분기 (RTOS queue 직렬화 또는 별도 chip) |
| LVGL/touch task starvation (httpd core 점유) | High | Medium | core pinning + httpd task priority 낮춤, lv_timer_handler 우선순위 보장 |
| PNG 디코더 메모리 폭증 (DRAM 부족) | Medium | Medium | MAX_IMAGE_BYTES 154KB 상한 + decode heap 30KB guard (BMP 패턴 재사용) |
| OTA 중 power loss → bricked | High | Low | Update.h 의 dual partition (factory + OTA) 활용. factory 잔존 |
| Control 탭 Long polling 10s 가 ESP32 socket pool 점유 | Medium | Low | httpd_config_t.max_open_sockets = 7 (기본) 충분, polling은 ETag 로 즉시 304 |
| v2.2-zero HTML/CSS/JS 가 esp_http_server response 포맷과 mismatch | Low | Low | 콜백 인터페이스 동일 유지 (String 반환), 라이브러리만 교체 |
| Long-click / Sleep 저장 regression (v2.1 부터의 fix 손실) | High | Low | main.cpp / DeviceManager 직접 수정 최소화, web/* 모듈만 신규 추가 |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `RemoteDeck_Touch/platformio.ini` | Config | mathieucarbou async 제거 → esp_http_server (built-in) 사용. LV_USE_PNG=1 추가. partition huge_app 유지 |
| `RemoteDeck_Touch/src/web/WebServer.{h,cpp}` | New module | AsyncWebServer → esp_http_server 래퍼 신규 작성 (v2.2-zero 콜백 인터페이스 유지) |
| `RemoteDeck_Touch/src/web/ImageApi.{h,cpp}` | Reuse + adapt | v2.2-zero 콜백 시그니처 유지, upload chunk handler 만 httpd req_recv 패턴으로 변경 |
| `RemoteDeck_Touch/src/web/ConfigApi.{h,cpp}` | Reuse | v2.2-zero 그대로 (GET/POST String 반환) |
| `RemoteDeck_Touch/src/web/Logger.{h,cpp}` | Reuse | v2.2-zero 그대로 (ring buffer 50건) |
| `RemoteDeck_Touch/src/web/ControlApi.{h,cpp}` | New | v2.2 hang 회피 — Long polling 10s + ETag, MQTT 이벤트 driven |
| `RemoteDeck_Touch/src/web/OtaApi.{h,cpp}` | New | Update.h 기반, 1.72MB OTA bin 전용 |
| `RemoteDeck_Touch/src/main.cpp` | Modify | 신규 모듈 attach. v2.1 setup() 순서 보존 (SPIFFS→Config→ETH→LCD) |
| `RemoteDeck_Touch/src/images/images.{h,cpp}` | Modify | PNG 분기 추가, 안전장치 BMP 와 공유 |
| `RemoteDeck_Touch/data/www/*` | Reuse + tweak | v2.2-zero HTML/CSS/JS 그대로 + Control 탭 폴링 빈도 2s→10s 조정 |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| MQTT 메시지 (IN/OUT 상태) | READ | ControlApi (신규) Long polling 응답 | Needs verification — 기존 LCD 미러 path 와 race 없는지 |
| SPIFFS /images/* | CREATE | ImageApi.onUploadChunk (httpd 패턴) | Needs verification — chunk 처리 race-free 검증 |
| SPIFFS /imagesconfig.json | READ/WRITE | ConfigApi GET/POST | None (v2.1 동작 그대로) |
| LCD lv_timer_handler | TICK | main loop (core 1) | None — httpd core 0 격리 |
| OTA partition | WRITE | OtaApi.handleUpload + Update.h | Needs verification — partition 자동 선택 |

### 6.3 Verification

- [ ] 모든 consumer 가 esp_http_server task 와 core 격리 확인 (vTaskGetInfo)
- [ ] Basic Auth 변경 없음 (admin:12345)
- [ ] v2.1 setup() 순서 보존 (W5500/TFT_eSPI SPI 충돌 회피)

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

| Level | Characteristics | Recommended For | Selected |
|-------|-----------------|-----------------|:--------:|
| **Starter** | 단일 펌웨어 | 임베디드 단일 단말 | ☑ |
| Dynamic | Feature 모듈 | Web/SaaS | ☐ |
| Enterprise | DI / 계층 분리 | 대규모 | ☐ |

> ESP32 임베디드 단일 펌웨어 — bkit Starter 분류이나 src/web/* 모듈화는 진행.

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| WebServer | AsyncWebServer / sync WebServer / esp_http_server | **esp_http_server** | v2.2 sync 폐기, AsyncTCP task slot 충돌, native httpd 가 별도 task + core pinning 가능 |
| HTTP Polling | 2s 자동 / 10s Long polling+ETag / WebSocket | **Long polling 10s + ETag** | v2.2 부팅 hang 원인 회피 + 사용자 답변 |
| OTA Path | OTA only / OTA+SPIFFS / FULL+OTA | **OTA bin only** | 사용자 답변, FULL 은 flash.bat 유지 |
| PNG Decoder | LV_USE_PNG / 외부 lib | **LV_USE_PNG=1** | LVGL 내장, BMP 와 안전장치 공유 |
| Branch | main 직접 / v2.3-httpd | **v2.3-httpd** | main 운영 펌웨어 (v2.1) 보호 |
| PoC 기준 | 최소 / 중간 / 엄격 | **엄격** | v2.2 단발 통과의 함정 회피 (사용자 답변) |

### 7.3 Module Layout

```
RemoteDeck_Touch/
├── platformio.ini                 (LV_USE_PNG=1, async lib 제거)
├── src/
│   ├── main.cpp                   (web 모듈 attach, v2.1 setup 보존)
│   ├── device/DeviceManager.*     (v2.1 그대로)
│   ├── mqtt/ethernet_mqtt.*       (v2.1 그대로, ControlApi event hook 추가)
│   ├── images/images.*            (PNG 분기 + 안전장치)
│   └── web/                       (신규)
│       ├── WebServer.{h,cpp}      (esp_http_server 래퍼, 콜백 인터페이스 v2.2-zero 유지)
│       ├── ImageApi.{h,cpp}       (v2.2-zero 재활용, upload만 httpd 패턴)
│       ├── ConfigApi.{h,cpp}      (v2.2-zero 그대로)
│       ├── Logger.{h,cpp}         (v2.2-zero 그대로)
│       ├── ControlApi.{h,cpp}     (Long polling 10s + ETag)
│       └── OtaApi.{h,cpp}         (Update.h, OTA bin 전용)
└── data/www/                      (v2.2-zero 재활용 + Control 폴링 10s 조정)
```

---

## 8. Convention Prerequisites

### 8.1 Existing Project Conventions

- [x] `CLAUDE.md` 존재 (프로젝트 루트)
- [ ] CONVENTIONS.md 없음
- [x] platformio.ini build_flags 가 사실상 컨벤션
- [x] Arduino-ESP32 3.x + pioarduino 53.x 고정

### 8.2 Conventions to Verify

| Category | Current State | To Define | Priority |
|----------|---------------|-----------|:--------:|
| Logging | Serial.printf 자유 형식 | 그대로 (esp_http_server 도 Serial 사용) | Low |
| Module 경계 | src/{device,mqtt,images,web} | web/* 신규, v2.2-zero 콜백 시그니처 보존 | High |
| Auth | Basic admin:12345 | 동일 | Medium |
| Error 응답 | JSON `{"ok":false,"error":"..."}` | v2.2-zero 그대로 | Medium |

### 8.3 Environment Variables

| Variable | Purpose | Scope | To Be Created |
|----------|---------|-------|:-------------:|
| (해당 없음 — platformio.ini build_flags 로 컴파일 타임 결정) | - | - | ☐ |

---

## 9. Implementation Phases (PoC-first)

| Phase | 범위 | Gate (다음 phase 진입 조건) |
|-------|------|---------------------------|
| **Phase 1 — PoC** | esp_http_server 부팅 + `/api/status` + LVGL/MQTT coexist | **엄격 PoC 통과**: 연속 upload(dummy) 10 + 병렬 GET 5 + MQTT 동시 1분 무장애. fail 시 v2.4 분기 |
| **Phase 2 — WebUI 풀세트** | 3탭 (Images/Config/Logs) + PNG | 이미지 BMP/PNG upload + delete + preview + config GET/POST + log 50건 ring 동작 |
| **Phase 3 — OTA** | OtaApi + Update.h | 1.72MB OTA bin upload → reboot → 신버전 부팅 + LCD/web 운용 정상 |
| **Phase 4 — Control 탭** | ControlApi Long polling 10s + ETag | 4탭 UI + MQTT 이벤트 미러 + 부팅 hang 없음 |
| **Phase 5 — 최종 검증** | Gap Analysis + LCD regression 시나리오 | Match Rate ≥ 90%, 수동 시나리오 1~5 통과 |

---

## 10. Next Steps

1. [ ] Design 문서 작성 (`/pdca design RemoteDeck_Touch_v2.3`) — 3가지 Architecture Option 비교 + esp_http_server 통합 상세
2. [ ] PoC 시나리오 스크립트 (curl 연속/병렬 + MQTT publisher) 사전 준비
3. [ ] v2.2-zero 브랜치 코드 cherry-pick 대상 식별 (HTML/CSS/JS, ConfigApi, Logger)

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-23 | Initial draft — v2.1/v2.2 인계 통합, PoC 엄격 + esp_http_server + Long polling 결정 | KDI |
