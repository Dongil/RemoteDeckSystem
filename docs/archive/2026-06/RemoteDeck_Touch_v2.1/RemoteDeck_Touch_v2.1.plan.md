---
template: plan
version: 1.3
feature: RemoteDeck_Touch_v2.1
date: 2026-06-22
author: KDI
project: RemoteDeckSystem
based_on: RemoteDeck_Touch v1 (commits 86b49f6..ea046d3)
---

# RemoteDeck_Touch v2.1 Planning Document — LAN 스택 통일 + PNG 디코더

> **Summary**: v1 에서 분리된 백로그 5개 중 **핵심 2개** (LAN 스택 통일 + PNG 디코더) 만 완성. 별도 브랜치 `v2.1-lan` 에서 진행하여 v1 안정 상태(ea046d3) 즉시 롤백 가능.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-06-22
> **Status**: Draft
> **Base commit (v1 완료점)**: `ea046d3` feat(RemoteDeck_Touch): M4-M7 웹 이미지 관리 UI + WiFi-only 가드

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v1 우회로 인해 Ethernet 환경에서 웹 UI 비활성 (`if (wifi_conn) webServer.begin()`). Touch 단말은 Ethernet 운영이 기본이므로 실사용 환경에서 이미지 관리 UI 사용 불가. 또한 v1 PNG 디코더는 LV_USE_PNG=0 으로 비활성 — BMP 파일만 처리. |
| **Solution** | RemoteDeck_PC v2.3.0 패턴 이식: (1) `arduino-libraries/Ethernet` 제거하고 `ETH.h` + `ETH_PHY_W5500` 으로 LAN 스택을 ESP32 lwIP에 통합 → AsyncWebServer 가 Ethernet 위에서 동작. (2) `lv_png_init()` + LVGL FS driver 등록 → 표준 LVGL PNG 디코딩 활성화. |
| **Function/UX Effect** | Ethernet 으로 부팅한 단말의 IP로 브라우저 접속 시 이미지 관리 UI 사용 가능. 운영자가 PNG 파일을 직접 업로드 가능. v1의 모든 기능(BMP 디코더, 핫리로드, MQTT)은 회귀 없이 유지. |
| **Core Value** | 운영자가 단말 1대 + 사무실 PC 만으로 콘텐츠 교체 가능 — WiFi 별도 설정 불필요. PNG 추가로 그래픽 품질 향상 + 압축률 ↑ (SPIFFS 절감). |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v1 우회 (WiFi-only WebUI) 해제로 Ethernet 운영 환경 정상화 + PNG 콘텐츠 지원 |
| **WHO** | RemoteDeck_Touch 운영자 (Ethernet 환경 기본), 콘텐츠 담당자 (PNG 작업 워크플로우) |
| **RISK** | TFT_eSPI 라이브러리 비호환 → LCD/터치 회귀. PNG 디코딩 heap 부족. 동시 두 마이그레이션 (LAN+PNG)의 변경 폭. |
| **SUCCESS** | (1) Ethernet IP로 브라우저 접속 → 웹 UI 정상, (2) PNG 업로드 후 LCD 5초 내 갱신, (3) v1 기능 100% 회귀 없음, (4) heap free ≥ 50KB 유지 |
| **SCOPE** | Phase 1 platform/lib 마이그레이션 → Phase 2 ETH.h 통합 → Phase 3 PNG 활성화 → Phase 4 회귀 검증 |

---

## 1. Overview

### 1.1 Purpose

v1 PDCA 사이클 완료 시점(`ea046d3`)에서 두 가지 제약이 남았다:

1. **AsyncTCP ↔ W5500 비호환**: `arduino-libraries/Ethernet` 은 lwIP 미경유 → AsyncWebServer 가 Ethernet IP에서 listen 불가. v1 우회로 `if (wifi_conn) webServer.begin(...)` 가드를 두어 Ethernet 환경에서 웹 UI 비활성. **실사용 단말은 대부분 Ethernet 환경이므로 핵심 기능이 사실상 비활성 상태**.
2. **PNG 디코더 비활성**: v1에서 lodepng 직접 호출이 LVGL 표준 패턴(`lv_png_init()` + LVGL FS) 과 충돌 → `LV_USE_PNG=0` 으로 비활성화. 현재 BMP만 처리 가능.

v2.1 은 위 두 가지를 **함께** 해결한다. 두 항목 모두 platform/라이브러리 의존성 변화가 큰 작업이므로 한 사이클로 묶어 한 번의 회귀 검증으로 마무리한다.

### 1.2 Background

- RemoteDeck_PC v2.3.0 (commit `cab6764..dee7b43`) 가 ETH.h + ETH_PHY_W5500 패턴으로 검증 완료
- v1 진행 중 LAN 통일을 시도했으나 의존성 사슬 (Arduino-ESP32 3.x → TFT_eSPI 비호환) 발견하여 별도 사이클로 분리
- v1 마지막 보드 실측 (192.168.10.118, commit ea046d3) 에서 BMP 디코더/MQTT/시간 동기화 모두 정상 동작 확인 — 롤백 기준점으로 활용

### 1.3 Related Documents

- v1 Plan: `docs/01-plan/features/RemoteDeck_Touch.plan.md`
- v1 Design: `docs/02-design/features/RemoteDeck_Touch.design.md`
- PC 참조: `RemoteDeck_PC/src/network/NetManager.{h,cpp}`, `RemoteDeck_PC/src/config/PinConfig.h`
- v2.2 백로그 (분리됨): OTA, 로그 뷰어, deviceconfig/serverconfig 웹 편집

---

## 2. Scope

### 2.1 In Scope

- [ ] **L1. Platform 업그레이드**: `platformio/espressif32@^6.5.0` → `pioarduino/platform-espressif32@53.03.10` (PC와 동일)
- [ ] **L2. Ethernet 라이브러리 교체**: `arduino-libraries/Ethernet` 제거, `<ETH.h>` 내장 라이브러리로 전환
- [ ] **L3. ETH_PHY_W5500 통합**: `ETH.begin(ETH_PHY_W5500, 1, PIN_CS, PIN_INT, -1, SPI)` (PC NetManager 패턴 이식)
- [ ] **L4. 클라이언트 통일**: `EthernetClient` → `WiFiClient` (lwIP socket으로 Ethernet/WiFi 공용)
- [ ] **L5. HTTPClient 교체**: `ArduinoHttpClient` 제거, ESP32 내장 `HTTPClient` 로 `downloadFile()` / `sendHttpMessage()` 재작성
- [ ] **L6. TFT_eSPI 라이브러리 업데이트**: Arduino-ESP32 3.x 호환 버전으로 (LCD 드라이버 회귀 검증 필수)
- [ ] **L7. WebServer 가드 제거**: `if (wifi_conn) webServer.begin()` → Ethernet 환경에서도 활성
- [ ] **P1. lv_conf.h**: `LV_USE_PNG=1` 재활성
- [ ] **P2. lv_png_init() 호출**: setup() 에서 LVGL PNG 디코더 등록
- [ ] **P3. LVGL FS driver 등록**: SPIFFS 경로를 LVGL이 `S:` 드라이버로 접근하도록 wrapper 작성 (또는 `LV_USE_FS_STDIO` 활성)
- [ ] **P4. images.cpp PNG 경로**: `lv_img_set_src(obj, "S:/images/photo.png")` 패턴으로 LVGL 표준 디코딩 사용
- [ ] **V1. 회귀 검증**: 기존 BMP 자산 표시, MQTT IN/OUT, 시간 동기화, 자동 재부팅, 터치 입력 모두 정상

### 2.2 Out of Scope (v2.2로 분리)

- OTA 펌웨어 업데이트 (PC v2.3.0 OTAHandler 이식)
- 웹 기반 로그 뷰어 (PC Logger + WebSocket 패턴)
- deviceconfig/serverconfig 웹 편집 UI
- 하드웨어 변경 (W5500 핀, LCD 모듈 등)
- LVGL 버전 업그레이드 (8.3.6 유지)
- MQTT/HTTP 비즈니스 로직 변경

### 2.3 Branch Strategy

- **`v2.1-lan` 별도 브랜치**에서 진행
- `main` 은 v1 완료 시점(`ea046d3`) 안정 상태 유지
- 회귀 발견 시 `main` 으로 즉시 롤백 가능
- 완료 검증 후 `main` 으로 merge

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | Ethernet IP로 브라우저 접속 시 이미지 관리 UI 표시 | High | Pending |
| FR-02 | Ethernet 환경에서 `/api/*` 엔드포인트 모두 정상 응답 | High | Pending |
| FR-03 | PNG 파일 업로드 시 LVGL이 디코딩하여 LCD에 표시 | High | Pending |
| FR-04 | 기존 BMP 자산 (title/photo/name) 회귀 없이 그대로 동작 | High | Pending |
| FR-05 | MQTT IN/OUT, 시간 동기화, 자동 재부팅, 터치 입력 회귀 없음 | High | Pending |
| FR-06 | 듀얼 네트워크 환경(Ethernet+WiFi)에서 우선 인터페이스 결정 (Ethernet 우선) | Medium | Pending |
| FR-07 | downloadFile() / sendHttpMessage() 가 HTTPClient 기반에서 v1과 동일 동작 | High | Pending |
| FR-08 | LCD 색감/속도/터치 응답이 v1 대비 회귀 없음 | High | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| Memory | heap free ≥ 50KB (idle), PNG 디코딩 피크 시 ≥ 20KB | `ESP.getFreeHeap()` 로깅 |
| Memory | flash 사용량 v1 대비 +10% 이내 | pio run 빌드 리포트 비교 |
| Performance | PNG 업로드 → 화면 갱신 ≤ 5초 (100KB PNG) | 스톱워치 + Serial 로그 |
| Stability | 50회 PNG 업로드 회귀 0 | 자동화 스크립트 |
| Compatibility | LCD/터치 v1 대비 회귀 0 | 수동 점검 + 사용 빈도 높은 화면 시각 비교 |
| Build | 빌드 경고 v1 대비 증가 ≤ 5건 | pio run 로그 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] L1~L7 + P1~P4 + V1 모두 완료
- [ ] FR-01 ~ FR-08 검증 통과 (회귀 ZERO)
- [ ] Ethernet 환경 단말에서 브라우저로 PNG/BMP 각 5회 이상 업로드 성공
- [ ] heap free ≥ 50KB 유지 (50회 업로드 후)
- [ ] v1 디자인 토큰/UI 동일 (style.css 변경 없음)
- [ ] `docs/02-design/features/RemoteDeck_Touch_v2.1.design.md` 작성 완료
- [ ] `docs/03-analysis/RemoteDeck_Touch_v2.1.analysis.md` Match Rate ≥ 90%
- [ ] `v2.1-lan` 브랜치 → `main` merge 완료

### 4.2 Quality Criteria

- [ ] LCD 색감/속도 v1 대비 시각적 차이 없음 (또는 사용자 승인)
- [ ] 터치 응답성 v1 대비 회귀 없음
- [ ] heap 사용 추이 50회 업로드 후 안정 (누수 0)
- [ ] 빌드 시간 v1 대비 ≤ 2배

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| **TFT_eSPI 라이브러리 업데이트 후 LCD 색감/속도 회귀** | **High** | **High** | (a) 업데이트 전후 동일 화면 사진 비교, (b) `User_Setup.h` 핀 매핑 재확인, (c) 회귀 시 `v2.1-lan` 브랜치에서 작업 → main 안전, (d) 필요 시 TFT_eSPI 대신 fork 라이브러리(예: `Bodmer/TFT_eSPI` 최신 또는 LGFX) 검토 |
| **TFT_eSPI 업데이트 후 터치 회귀** | High | Medium | FT6236G 별도 라이브러리 사용 중이라 TFT_eSPI 와 독립 — 영향 가능성 낮음. 단 SPI 버스 공유 시 충돌 가능, 핀 설정 재확인 |
| **ArduinoHttpClient → HTTPClient 전환 시 downloadFile 동작 차이** | Medium | Medium | HTTPClient + `getStreamPtr()` 패턴으로 v1과 동등 동작 가능, PC v2.3.0 코드 참조 |
| **lv_png_init + LVGL FS driver 등록 복잡도** | Medium | Medium | LV_USE_FS_STDIO=1 + `LV_FS_STDIO_LETTER='S'` 활성화로 표준 처리. 또는 PC 가 동일 패턴 사용 시 코드 그대로 이식 |
| **PNG heap 부족 (240×320 디코딩)** | High | Medium | (a) 디코드 직전 `ESP.getFreeHeap()` ≥ 30KB 체크 + fail-soft, (b) 최대 이미지 크기 240×320 강제, (c) PSRAM 가능 시 사용 |
| **Arduino-ESP32 3.x 다른 라이브러리 비호환 (FT6236G, PubSubClient 등)** | Medium | Low | 사전 빌드로 사전 확인 (v1 마이그레이션 시도에서 Ethernet/HttpClient/TFT_eSPI 만 문제 — 다른 lib 는 통과했음) |
| **두 마이그레이션 동시 진행 시 디버그 어려움** | Medium | High | (a) commit 단위 분리: L1~L7 먼저 → 빌드/실측 → P1~P4 → 빌드/실측, (b) feature flag 없이 그냥 순차 진행 |
| **`v2.1-lan` 브랜치 main 머지 시 conflict** | Low | Low | v1 완료 후 v2.1 작업 → main 변경 없을 것이므로 거의 없음 |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `RemoteDeck_Touch/platformio.ini` | Build config | platform → pioarduino 53.03.10, arduino-libraries/Ethernet/ArduinoHttpClient 제거, TFT_eSPI 업데이트 명시 |
| `RemoteDeck_Touch/lib/TFT_eSPI/` | Library | 최신 호환 버전으로 교체 (또는 fork) |
| `RemoteDeck_Touch/lib/lv_conf.h` | LVGL config | LV_USE_PNG=1, LV_USE_FS_STDIO=1 (또는 custom FS) |
| `src/mqtt/ethernet_mqtt.{h,cpp}` | Network init | `<Ethernet.h>` → `<ETH.h>`, `Ethernet.init/begin` → `ETH.begin(ETH_PHY_W5500, ...)` |
| `src/main.cpp` | Entry | `EthernetClient` → `WiFiClient`, `Ethernet.localIP()` → `ETH.localIP()`, `HttpClient http(...)` → `HTTPClient http`, downloadFile/sendHttpMessage 재작성, WebServer 가드 제거 |
| `src/images/images.{h,cpp}` | Decoder | LVGL FS path 패턴 (`S:/images/...`)으로 PNG 분기, lv_png_init 호출은 setup() 에서 |
| `src/device/DeviceManager.h` | Header | `<EthernetClient.h>` 제거 (이미 제거됨) |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| `Ethernet.localIP()` | READ | `main.cpp:108`, `ethernet_mqtt.cpp` 다수 | 변경: `ETH.localIP()` |
| `EthernetClient ethClient` | INSTANCE | `main.cpp:30`, `ethernet_mqtt.cpp:8` (extern) | 변경: `WiFiClient ethClient` |
| `HttpClient http(...)` | INSTANCE | `main.cpp:163` | 변경: `HTTPClient http;` (no constructor args) |
| `http.get(path)` | CALL | downloadFile, sendHttpMessage | 변경: `http.begin(url) + http.GET()` |
| `http.responseBody()` | CALL | sendHttpMessage | 변경: `http.getString()` |
| `http.responseStatusCode()` | CALL | downloadFile, sendHttpMessage | 변경: `http.GET()` 반환값 |
| `http.read(buf, len)` | CALL | downloadFile | 변경: `http.getStreamPtr()->readBytes(...)` |
| `lv_img_set_src(obj, &dsc)` | CALL | images.cpp `try_set` | 호환: BMP 경로는 그대로 dsc 방식, PNG 경로는 `"S:/path.png"` 문자열 |
| TFT_eSPI 매크로/함수 | LCD draw | lvgl_touch.cpp, TFT_eSPI 내부 | 변경: 라이브러리 업데이트로 사용법 변경 가능성 — 검증 필요 |
| 기존 BMP 자산 표시 | DECODE | images.cpp read_bmp | 영향 없음 (BMP 디코더 그대로) |
| MQTT/터치/시간 동기화 | RUNTIME | 다수 | 영향 없음 (간접 영향 없음 확인 필요) |

### 6.3 Verification

- [ ] 기존 BMP 3종 표시 회귀 없음 (시각 확인)
- [ ] LCD 색감/속도 v1 대비 동일 (사진 비교)
- [ ] FT6236G 터치 응답 v1 대비 동일
- [ ] MQTT IN/OUT 메시지 처리 영향 없음
- [ ] 자동 재부팅 시각 로직 영향 없음
- [ ] Ethernet 단일 환경 + WiFi 단일 환경 + 듀얼 환경 모두 웹 UI 접근 가능
- [ ] heap 사용량 v1 대비 ±20KB 이내
- [ ] PNG 업로드 + 디코딩 후 LVGL 메모리 누수 0

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

| Level | Selected |
|-------|:--:|
| Embedded (Arduino/PlatformIO) | ✅ |

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| Platform | espressif32@^6.5.0 / pioarduino 53.x | **pioarduino 53.03.10** | PC v2.3.0 과 동일, ETH_PHY_W5500 + Arduino-ESP32 3.x lwIP 통합 |
| W5500 driver | arduino-libraries/Ethernet / ETH.h | **ETH.h (ESP32 내장)** | lwIP 통합으로 AsyncTCP 호환, PC 검증 패턴 |
| HTTP client | ArduinoHttpClient / HTTPClient | **HTTPClient (ESP32 내장)** | Arduino-ESP32 3.x Client API 호환, 추상 클래스 문제 회피 |
| TFT driver | 현재 TFT_eSPI / fork / LGFX | **TFT_eSPI 최신 또는 호환 fork** | 1차 시도, 회귀 시 LGFX 검토 |
| PNG decoder | lodepng 직접 / lv_png_init + LVGL FS | **lv_png_init + LVGL FS** | LVGL 표준, file path 기반 디코딩 |
| LVGL FS driver | LV_USE_FS_STDIO / custom | **LV_USE_FS_STDIO** | 가장 간단, SPIFFS path 직접 사용 가능 |
| Branch | main / 별도 브랜치 | **`v2.1-lan` 별도 브랜치** | 회귀 발생 시 즉시 롤백, main 안정성 유지 |

### 7.3 Folder Structure (v1 유지, 변경점만 표시)

```
RemoteDeck_Touch/
├── platformio.ini         # 변경: platform + lib_deps
├── lib/
│   ├── lv_conf.h          # 변경: LV_USE_PNG=1, LV_USE_FS_STDIO=1
│   └── TFT_eSPI/          # 변경: 라이브러리 업데이트
├── src/
│   ├── main.cpp           # 변경: WiFiClient, HTTPClient, ETH.localIP, gard 제거
│   ├── images/images.{h,cpp}  # 변경: PNG 분기 (LVGL FS path)
│   ├── mqtt/ethernet_mqtt.{h,cpp}  # 변경: ETH.begin(ETH_PHY_W5500)
│   ├── device/DeviceManager.h      # 이미 정리됨 (v1 ea046d3)
│   └── web/                        # 변경 없음 (v1 그대로)
└── data/                  # 변경 없음
```

---

## 8. Convention Prerequisites

### 8.1 v1 컨벤션 그대로 유지

- 한글 주석 허용
- `Design Ref: §{section}` / `Plan SC: {FR-XX}` 주석 패턴 유지
- Basic Auth (admin:12345) 동일
- HTTP API 네이밍 (`/api/images/*`, `/api/status`) 동일

### 8.2 신규 환경 변수 / Build Flag

| Flag | Purpose | Default |
|------|---------|---------|
| `LV_USE_PNG` | LVGL PNG 디코더 활성화 | `1` |
| `LV_USE_FS_STDIO` | SPIFFS path → LVGL FS 매핑 | `1` |
| `LV_FS_STDIO_LETTER` | LVGL FS drive letter | `'S'` |

---

## 9. Next Steps

1. [ ] `/pdca design RemoteDeck_Touch_v2.1` 로 Design 문서 작성 (3-옵션 비교)
2. [ ] `v2.1-lan` 브랜치 생성 + checkout (`git checkout -b v2.1-lan ea046d3`)
3. [ ] `/pdca do RemoteDeck_Touch_v2.1 --scope L1-L5` 로 platform/Ethernet/HTTP 마이그레이션 먼저
4. [ ] 빌드 + 보드 실측 (Ethernet 정상, MQTT 정상, LCD 변경 없음 확인)
5. [ ] `/pdca do RemoteDeck_Touch_v2.1 --scope L6-L7` TFT_eSPI 업데이트 + WebServer 가드 제거
6. [ ] 빌드 + 보드 실측 (LCD/터치 회귀 점검 — 가장 위험한 단계)
7. [ ] `/pdca do RemoteDeck_Touch_v2.1 --scope P1-P4` PNG 디코더 활성화
8. [ ] 빌드 + 보드 실측 (PNG 업로드/표시)
9. [ ] `/pdca analyze RemoteDeck_Touch_v2.1` 로 Match Rate 측정
10. [ ] 회귀 없음 확인 시 `v2.1-lan` → `main` merge
11. [ ] 회귀 발생 시: `main` 의 `ea046d3` 그대로 사용 + v2.1 디버그 지속

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-22 | 초안 작성 — LAN 통일 + PNG 핵심 2개로 스코프 좁힘, 별도 브랜치 전략 명시 | KDI |
