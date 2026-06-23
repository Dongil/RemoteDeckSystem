---
template: plan
version: 1.3
feature: RemoteDeck_Touch_v2.2
date: 2026-06-23
author: KDI
project: RemoteDeckSystem
based_on: RemoteDeck_Touch v2.1 (156d089, archived 2026-06-22)
predecessors:
  - v1: ea046d3 (BMP decoder + downloadFile fix)
  - v2.1: c44d348 (LAN 스택 통일, archived)
---

# RemoteDeck_Touch v2.2 Planning Document — WebUI 풀세트 + PNG 디코더 (Zero-base WebServer)

> **Summary**: v2.1에서 분리된 WebUI 와 PNG 를 완성. AsyncTCP ↔ W5500 task slot 충돌 학습을 토대로 **WebServer 라이브러리를 제로베이스 재선정** (sync WebServer 또는 esp_http_server). PIO/Arduino-ESP32 최신 업그레이드 가능.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-06-23
> **Status**: Draft
> **Base commit (v2.1 완료점)**: `156d089` fix: DHCP 타임아웃 15s + 재시도
> **운영 안전망**: v2.1 펌웨어가 현장에서 안정 운영 중 → v2.2 시도 자유

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.1 에서 LAN 스택 통일은 완료됐으나 WebUI 가 AsyncTCP task slot 충돌(W5500 + MQTT 환경)로 비활성. 운영자가 단말 자체에서 이미지 교체/OTA/설정 변경 불가, 서버 의존 운영 지속. PNG 콘텐츠 미지원. |
| **Solution** | **AsyncWebServer 포기 → sync WebServer 또는 esp_http_server (ESP-IDF)** 로 교체. v2.1에서 실패 원인 (AsyncTCP task = W5500 SPI poll task와 충돌) 회피. MQTT/W5500 유지하면서 WebUI 안정 동작 목표. PNG는 lv_png_init + LV_USE_FS_STDIO 표준 방식. |
| **Function/UX Effect** | 운영자가 단말 IP로 브라우저 접속해 이미지 업로드/OTA/설정 편집/로그 조회 모두 가능. PNG 자산 사용 → 그래픽 품질 + SPIFFS 압축률 향상. |
| **Core Value** | 단말 자체로 완전 자급운영 (server 없이도 콘텐츠/설정 변경) + 원격 OTA 로 펌웨어 업데이트 가능. v1/v2.1 의 핵심 미해결 과제 종결. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | W5500 + MQTT 환경에서 WebUI 동작 + PNG 콘텐츠 지원으로 단말 자급운영 + 원격 관리 완성 |
| **WHO** | Touch 단말 운영자 (Ethernet 기본), 콘텐츠 담당자, 원격 펌웨어 관리자 |
| **RISK** | sync WebServer 의 대용량 업로드 시 LVGL 지연 / 메모리 부족 / PIO 업그레이드 후 다른 lib 회귀 / OTA partition 변경 / 로그 system 추가로 heap 압박 |
| **SUCCESS** | (1) Ethernet 환경 WebUI 정상 동작 100%, (2) PNG 업로드/표시 ≤5초, (3) OTA 펌웨어 업데이트 성공, (4) v2.1 운영 환경 회귀 ZERO, (5) heap free ≥ 40KB 유지 |
| **SCOPE** | Phase 1 WebServer 라이브러리 PoC (W5500+MQTT 호환 검증) → Phase 2 풀세트 API 구현 → Phase 3 PNG 활성화 → Phase 4 OTA → Phase 5 회귀 검증 |

---

## 1. Overview

### 1.1 Purpose

v2.1 사이클 (commit c44d348) 에서 LAN 스택 통일 (arduino-libs/Ethernet → ETH.h + ETH_PHY_W5500) 은 완료했으나, **AsyncTCP가 W5500 SPI poll task와 동일 task slot을 두고 경쟁** 하여 WebUI 비활성. 이는 **mathieucarbou/AsyncTCP 라이브러리 자체의 task 생성 제약** 이며 build_flags 우회 불가능.

v2.1 보고된 사실:
- W5500 + MQTT 환경: AsyncWebServer `[E][AsyncTCP.cpp:1557] begin(): failed to start task`
- WiFi + MQTT 환경 (사용자 과거 경험): AsyncWebServer 동작 성공

→ 차이의 핵심은 **W5500 SPI driver 가 lwIP 내부에서 동작 task가 AsyncTCP 와 같은 우선순위/슬롯** 을 점유하는 ESP32 환경 특이성.

v2.2 는 **AsyncWebServer 사용을 포기**하고:
- **sync WebServer (Arduino ESP32 내장 `WebServer.h`)** — 가장 간단, main loop에서 `handleClient()` 호출
- 또는 **esp_http_server (ESP-IDF)** — task 자체 관리, 더 안정적이나 API 복잡

으로 교체하여 **W5500 + MQTT + WebServer 3자 공존** 을 달성한다.

또한 PNG 디코더는 lodepng 직접 호출이 아닌 LVGL 표준 (`lv_png_init()` + `LV_USE_FS_STDIO=1` + `S:/` drive) 으로 활성화한다.

### 1.2 Background

- v1 (ea046d3): BMP 디코더 + downloadFile 안정화
- v2.1 (c44d348): LAN 스택 통일, AsyncTCP 충돌 발견 + WebUI/PNG v2.2로 분리, 운영 안정화
- v2.2 (현재): WebUI 풀세트 + PNG 완성

운영 환경:
- v2.1 펌웨어 현재 운영 중 (IP DHCP, MQTT broker 192.168.10.230)
- v2.2 실패해도 v2.1로 즉시 롤백 가능

### 1.3 Related Documents

- v1 Plan/Report: `docs/01-plan/features/RemoteDeck_Touch.plan.md`
- v2.1 Archive: `docs/archive/2026-06/RemoteDeck_Touch_v2.1/`
- PC 참조: `RemoteDeck_PC/src/web/WebServer.{h,cpp}` (AsyncWebServer 패턴, 참고용)

---

## 2. Scope

### 2.1 In Scope

#### Phase 1 — WebServer 라이브러리 PoC + 채택

- [ ] PoC: sync WebServer (ESP32 내장) + W5500 + MQTT 동시 동작 검증
- [ ] PoC: esp_http_server (필요 시) + W5500 + MQTT 동시 동작 검증
- [ ] 채택 결정: sync vs esp_http_server (성능 vs 안정성)
- [ ] PIO/Arduino-ESP32 최신 버전 검토 + 적용
- [ ] platformio.ini lib_deps 정리 (mathieucarbou async 제거, 신규 lib 추가)

#### Phase 2 — WebUI 풀세트 API (PC v2.3.0 수준)

- [ ] **이미지 관리**: `POST /api/images/upload`, `GET /api/images/list`, `DELETE /api/images/{name}`, `GET /api/images/{name}` (미리보기)
- [ ] **상태 모니터링**: `GET /api/status` (heap, SPIFFS, uptime, network, fw_version)
- [ ] **설정 편집**: `GET /api/config`, `POST /api/config` (deviceconfig.json), `GET /api/serverconfig`, `POST /api/serverconfig`
- [ ] **로그 뷰어**: `GET /api/logs` (in-memory ring buffer, JSON return)
- [ ] **OTA**: `POST /api/ota` (multipart upload, esp32 Update.h 사용)
- [ ] **인증**: Basic Auth (admin:12345, deviceconfig.json 에서 변경 가능)
- [ ] **정적 파일**: `/` → index.html, `/style.css`, `/app.js`

#### Phase 3 — PNG 디코더 활성화 (LVGL 표준)

- [ ] `lv_conf.h`: LV_USE_PNG=1, LV_USE_FS_STDIO=1, LV_FS_STDIO_LETTER='S'
- [ ] `setup()` 에서 `lv_png_init()` + LVGL FS driver 자동 등록 확인
- [ ] `images.cpp`: PNG 경로는 `lv_img_set_src(obj, "S:/images/photo.png")` 패턴
- [ ] 기존 BMP 디코더 (RAM 직접 디코드) 와 공존
- [ ] PNG 업로드 → SPIFFS 저장 → 자동 LVGL 재로드 (재부팅 없이)

#### Phase 4 — OTA Handler

- [ ] PC v2.3.0 OTAHandler 패턴 이식 — Update.h 기반 multipart upload
- [ ] 파일명 자동 파싱 (`RemoteDeck_Touch_V{VERSION}_OTA_{YYYYMMDD}.bin`)
- [ ] OTA 성공 시 자동 재부팅
- [ ] OTA 실패 시 fail-safe (이전 펌웨어 유지)

#### Phase 5 — 회귀 검증

- [ ] v2.1 운영 환경 (Ethernet/MQTT/시간/IN/OUT/터치/Long-press/Sleep/BMP) 모두 회귀 없음
- [ ] WebUI 50회 이미지 업로드 stability
- [ ] heap/메모리 안정성 50회 OTA 시도 시뮬레이션

### 2.2 Out of Scope (v2.3로 분리)

- 시간 표시 UI (LCD 위젯 추가)
- WebSocket 실시간 로그 push (HTTP polling으로 충분)
- 인증 강화 (HTTPS, OAuth, 2FA)
- 다국어 UI
- Mobile-responsive 별도 디자인
- 단말 → 단말 직접 통신
- LVGL 9.x 업그레이드

### 2.3 Branch Strategy

- **`v2.2-zero` 별도 브랜치**에서 진행 (zero-base 명칭)
- `main` 은 v2.1 (156d089) 안정 상태 유지
- 회귀 발견 시 main 즉시 롤백 가능
- 단계별 commit (Phase 1~5 별 PoC commit + 통합 commit)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | W5500 + MQTT + WebServer 3자 동시 동작 (task slot 충돌 ZERO) | Critical | Pending |
| FR-02 | Ethernet IP → 브라우저 접속 시 이미지 관리 UI 정상 표시 | High | Pending |
| FR-03 | PNG 업로드 → LCD 5초 이내 갱신 | High | Pending |
| FR-04 | OTA 펌웨어 업데이트 성공 (filename → 자동 version 파싱) | High | Pending |
| FR-05 | deviceconfig/serverconfig 웹 편집 + 저장 + 재부팅 자동 적용 | High | Pending |
| FR-06 | 로그 뷰어 (최근 50건 in-memory ring buffer) | Medium | Pending |
| FR-07 | Basic Auth 보호 (admin:12345 기본, 변경 가능) | High | Pending |
| FR-08 | v2.1 운영 환경 (Ethernet/MQTT/터치/Long-press/Sleep/BMP) 100% 회귀 없음 | Critical | Pending |
| FR-09 | PIO/Arduino-ESP32 최신 버전 + lib 호환 검증 | Medium | Pending |
| FR-10 | RemoteDeck_PC 디자인 토큰 재사용 (다크 + 시안 강조) | Low | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement |
|----------|----------|-------------|
| Memory | heap free ≥ 40KB (목표 v2.1 56KB 대비 약간 감소 허용) | `ESP.getFreeHeap()` 모니터링 |
| Performance | 이미지 업로드 200KB ≤ 10초, OTA 1.5MB ≤ 60초 | 스톱워치 + Serial |
| Stability | 50회 이미지 업로드 + 5회 OTA 후 누수 ZERO | heap 추이 검증 |
| Compatibility | v2.1 기능 회귀 ZERO | 보드 실측 체크리스트 |
| Reliability | sync WebServer 처리 중 LVGL frame drop ≤ 200ms | 시각 검증 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-10 모두 구현 + 실측 검증
- [ ] Ethernet 환경에서 WebUI 풀세트 동작
- [ ] PNG/BMP 각각 5회 이상 업로드 + LCD 즉시 갱신
- [ ] OTA 펌웨어 업데이트 성공 → 재부팅 → 새 버전 확인
- [ ] v2.1 기능 회귀 ZERO (보드 부팅 체크리스트)
- [ ] `docs/02-design/features/RemoteDeck_Touch_v2.2.design.md` 작성
- [ ] `docs/03-analysis/RemoteDeck_Touch_v2.2.analysis.md` Match Rate ≥ 90%

### 4.2 Quality Criteria

- [ ] sync WebServer handleClient() 호출로 인한 LVGL frame drop ≤ 200ms (시각 확인)
- [ ] heap 50회 업로드 후 누수 ZERO
- [ ] OTA 실패 시 이전 펌웨어로 자동 복구
- [ ] WebUI 디자인 일관성 (PC 패턴 재사용)

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| **sync WebServer 가 W5500 + PubSubClient 와도 충돌** | Critical | Medium | Phase 1 PoC 로 사전 검증. 실패 시 esp_http_server fallback |
| **sync WebServer handleClient() 가 LVGL 멈춤** | High | Medium | 청크 단위 처리 + main loop 에서 짧은 quantum 으로 lv_timer_handler 와 round-robin |
| **PIO/Arduino-ESP32 업그레이드 후 다른 lib 회귀** (TFT_eSPI, FT6236G, PubSubClient) | High | Medium | 사전 build_flags 호환성 확인. 회귀 시 이전 버전으로 핀 |
| **OTA Update.h 가 SPIFFS partition 변경 시 데이터 손실** | High | Low | huge_app.csv 유지 (otadata 0xe000 + app0 0x10000 만 변경, spiffs 0x310000 보존) |
| **PNG heap 부족** (v2.1 한계 그대로) | High | Medium | LV_USE_FS_STDIO + lv_png 의 streaming decode 검증, IMAGE_MAX_BYTES 100KB 강제 |
| **로그 ring buffer 가 heap 점유 과다** | Medium | Low | 50건 × 평균 100바이트 = 5KB 한도. JSON 직렬화 시 추가 ~3KB |
| **인증 회피 시도 (운영망 외부 노출)** | Medium | Low | 운영 가이드: 단말은 사내 LAN 전용 + Basic Auth 변경 권장 |
| **`v2.2-zero` 브랜치 main 머지 시 conflict** | Low | Low | v2.1 이후 main 변경 없을 것으로 예상 |
| **사용자 보고된 시간 UI 누락 불만** | Low | Low | v2.3 으로 명확히 분리, Report 에 명시 |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `RemoteDeck_Touch/platformio.ini` | Build config | mathieucarbou async 제거, sync WebServer or esp_http_server lib_deps 변경, PIO 업그레이드 검토 |
| `RemoteDeck_Touch/lib/lv_conf.h` | LVGL config | LV_USE_PNG=1, LV_USE_FS_STDIO=1, LV_FS_STDIO_LETTER='S' |
| `RemoteDeck_Touch/src/web/WebServer.{h,cpp}` | NEW or REWRITE | AsyncWebServer 기반 → sync 기반으로 재작성 |
| `RemoteDeck_Touch/src/web/ImageApi.{h,cpp}` | Modify | 청크 처리 단위 변경 (sync 환경 적응) |
| `RemoteDeck_Touch/src/web/OTAHandler.{h,cpp}` | NEW | PC v2.3.0 패턴 이식 |
| `RemoteDeck_Touch/src/web/ConfigApi.{h,cpp}` | NEW | deviceconfig/serverconfig 편집 endpoint |
| `RemoteDeck_Touch/src/web/Logger.{h,cpp}` | NEW | in-memory ring buffer (50건) |
| `RemoteDeck_Touch/src/images/images.cpp` | Modify | PNG 경로 분기 → `lv_img_set_src("S:/...")` |
| `RemoteDeck_Touch/src/main.cpp` | Modify | webServer.handleClient() 메인 루프 호출, OTA 콜백 등록 |
| `RemoteDeck_Touch/data/www/` | Modify | OTA + 로그 + 설정 편집 UI 추가 |
| `RemoteDeck_Touch/partitions.csv` | Verify | huge_app.csv 유지 또는 OTA 위한 dual-app 검토 |

### 6.2 Current Consumers

| v2.1 Resource | v2.2 변경 영향 |
|---------------|---------------|
| WiFiClient ethClient (PubSubClient) | 영향 없음 — sync WebServer는 별도 Client 사용 |
| MQTT 메시지 처리 | 영향 없음 — task 분리 안 함, main loop 공존 |
| ETH.localIP() / ETH.macAddress() | 영향 없음 |
| HTTPClient downloadFile() | 영향 없음 (fetchImageFiles 그대로) |
| LVGL UI (LCD/터치) | sync handleClient() 가 짧게 점유 — frame drop 우려 |
| Long-press / Sleep / 시간 동기화 / IN/OUT | 영향 없음 |
| BMP 디코더 | 영향 없음 — PNG는 별도 경로로 분기 |

### 6.3 Verification (회귀 체크리스트)

- [ ] Ethernet DHCP/Static IP 정상 획득 (≤15초 + 재시도)
- [ ] MQTT broker 연결 + IN/OUT 토픽 송수신
- [ ] LCD title/photo/name BMP 표시
- [ ] 터치: 재부재 버턴 IN/OUT 토글
- [ ] Long-press 1회 → DeviceManager 진입
- [ ] DeviceManager Sleep 시간 저장/복원
- [ ] Screen saver 진입/해제
- [ ] 자동 재부팅 (rebootTime)
- [ ] heap free ≥ 40KB (목표)

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

Embedded (Arduino/PlatformIO) — 변경 없음.

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| Web Server | AsyncWebServer / sync WebServer / esp_http_server / khoih-prog AsyncWebServer_ESP32_W5500 | **Phase 1 PoC 후 결정** | sync 우선 시도, 실패 시 esp_http_server |
| MQTT 유지 | Keep / Remove | **Keep (사용자 결정)** | 운영 환경 호환성 유지 |
| PNG decoder | lv_png_init + LVGL FS / PNGdec / 제거 | **lv_png_init + LV_USE_FS_STDIO** | LVGL 표준, 사용자 결정 |
| LVGL FS drive | 'S' | **'S'** | `S:/images/photo.png` 패턴 |
| Platform | espressif32@^6.5.0 (v2.1) / pioarduino 53.x (v2.1) / 최신 | **pioarduino 53.x 또는 최신 stable** | Arduino-ESP32 3.x 유지 (이미 v2.1에서 ETH_PHY_W5500 지원) |
| OTA library | Update.h (ESP32 내장) / 별도 lib | **Update.h** | PC v2.3.0 검증 패턴 |
| Auth | Basic Auth | **Basic Auth (admin:12345)** | PC와 동일, v2.3에서 강화 검토 |
| Logger | in-memory ring buffer (50건) | **50건 × ~100B = 5KB** | heap 부담 최소 |

### 7.3 Folder Structure Preview

```
RemoteDeck_Touch/
├── platformio.ini             # 변경: WebServer lib 교체
├── lib/lv_conf.h              # 변경: LV_USE_PNG=1, LV_USE_FS_STDIO=1
├── src/
│   ├── main.cpp               # 변경: webServer.handleClient(), OTA 등록, lv_png_init
│   ├── web/                   # 변경: AsyncWebServer → sync 기반
│   │   ├── WebServer.{h,cpp}  # 재작성
│   │   ├── ImageApi.{h,cpp}   # 청크 처리 sync 적응
│   │   ├── OTAHandler.{h,cpp} # NEW (PC v2.3.0 이식)
│   │   ├── ConfigApi.{h,cpp}  # NEW (config 편집)
│   │   ├── Logger.{h,cpp}     # NEW (ring buffer)
│   │   └── AuthMiddleware.{h,cpp}  # Basic Auth 헬퍼
│   ├── images/images.cpp      # 변경: PNG 경로는 LVGL FS path
│   ├── mqtt/                  # 변경 없음 (MQTT 유지)
│   ├── config/                # 변경 없음
│   ├── device/                # 변경 없음
│   └── utils/                 # 변경 없음
└── data/www/                  # 변경: OTA + 로그 + 설정 편집 UI 추가
    ├── index.html             # 확장: 4개 탭 (Images / OTA / Config / Logs)
    ├── style.css              # 유지
    └── app.js                 # 확장: OTA + Config + Logs API
```

---

## 8. Convention Prerequisites

v2.1 컨벤션 모두 유지. 추가:

- WebServer 핸들러 함수명: `handleXxx(WebServer& server)` 패턴 (sync 기반)
- `_onLog` 콜백 시그니처: `(const char* event, const char* detail)` (PC + v1과 동일)
- Logger ring buffer 항목: `{timestamp, event, detail}` 3-tuple

---

## 9. Next Steps

### v2.2 진행 흐름

1. [ ] `/pdca design RemoteDeck_Touch_v2.2` — 3-옵션 아키텍처 비교 후 Option 채택
2. [ ] `git checkout -b v2.2-zero 156d089` — 별도 브랜치 생성
3. [ ] **Phase 1 PoC**: `/pdca do RemoteDeck_Touch_v2.2 --scope poc`
   - sync WebServer + W5500 + MQTT 최소 코드로 검증
   - 실패 시 esp_http_server PoC
4. [ ] **Phase 2 풀세트 API**: `/pdca do --scope api`
   - ImageApi + ConfigApi + Logger + OTA
5. [ ] **Phase 3 PNG**: `/pdca do --scope png`
   - lv_conf.h 활성화 + images.cpp 분기 + 실측 검증
6. [ ] **Phase 4 OTA**: `/pdca do --scope ota`
   - OTA handler + 펌웨어 빌드 + 실측 업데이트
7. [ ] **Phase 5 회귀**: `/pdca do --scope regression`
   - v2.1 기능 체크리스트 검증
8. [ ] `/pdca analyze RemoteDeck_Touch_v2.2` — Match Rate 측정
9. [ ] `/pdca report RemoteDeck_Touch_v2.2` — 통합 보고서
10. [ ] `/pdca archive RemoteDeck_Touch_v2.2 --summary`
11. [ ] FULL + OTA 펌웨어 빌드 + git 추적 + push

### v2.3 백로그 (또 분리)

| 항목 | 우선순위 |
|------|:--:|
| 시간 표시 UI (LCD 위젯) | High (사용자 보고) |
| WebSocket 실시간 로그 push | Medium |
| 인증 강화 (HTTPS / OAuth) | Medium |
| LVGL 9.x 업그레이드 | Low |
| 다국어 UI | Low |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-23 | 초안 작성 — WebUI 풀세트 + PNG, zero-base WebServer 라이브러리 재선정, v2.3 시간 UI 분리 | KDI |
