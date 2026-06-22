---
template: design
version: 1.3
feature: RemoteDeck_Touch_v2.1
date: 2026-06-22
author: KDI
project: RemoteDeckSystem
status: Draft
plan_doc: ../../01-plan/features/RemoteDeck_Touch_v2.1.plan.md
base_commit: ea046d3
branch: v2.1-lan
---

# RemoteDeck_Touch v2.1 Design Document — LAN 통일 + PNG 디코더

> **Summary**: PC v2.3.0 NetManager 패턴을 Touch ethernet_mqtt.cpp 안에 인라인 이식 (클래스 신설 X) + lv_png_init() + LV_USE_FS_STDIO=1로 표준 LVGL PNG 디코딩 활성. 별도 브랜치 `v2.1-lan` 에서 진행.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-06-22
> **Status**: Draft
> **Planning Doc**: [RemoteDeck_Touch_v2.1.plan.md](../../01-plan/features/RemoteDeck_Touch_v2.1.plan.md)
> **Base commit**: `ea046d3` (v1 안정 상태)
> **Branch**: `v2.1-lan` (회귀 시 main 롤백)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v1 우회(WiFi-only) 해제 + PNG 지원으로 Ethernet 운영 환경 정상화 |
| **WHO** | Touch 단말 운영자 (Ethernet 기본), 콘텐츠 담당자 |
| **RISK** | TFT_eSPI 업데이트 후 LCD/터치 회귀 (High), PNG heap, 동시 두 마이그레이션 변경 폭 |
| **SUCCESS** | Ethernet 웹 UI 동작 + PNG 업로드 + v1 기능 100% 회귀 없음 + heap ≥ 50KB |
| **SCOPE** | platform 마이그레이션 → ETH.h 통합 → PNG 활성화 → 회귀 검증 |

---

## 1. Overview

### 1.1 Design Goals

1. **AsyncWebServer Ethernet 동작**: W5500을 ESP32 lwIP의 Ethernet PHY로 등록하여 AsyncTCP가 정상 listen
2. **LVGL 표준 PNG 디코딩**: lv_png_init() + LV_USE_FS_STDIO=1로 SPIFFS 파일을 `S:/path.png` 로 직접 디코딩
3. **v1 회귀 ZERO**: BMP/MQTT/터치/시간 동기화/자동 재부팅 모두 동일 동작
4. **PC 패턴 일관성**: NetManager 클래스 신설 없이도 PC와 동일 시그니처(`ETH.begin(ETH_PHY_W5500, ...)`) 사용
5. **롤백 가능성**: `v2.1-lan` 별도 브랜치 진행, 회귀 시 main(`ea046d3`)으로 즉시 복귀

### 1.2 Design Principles

- **PC 검증 패턴 우선**: 새 추상화 도입 금지, PC NetManager.cpp 코드 그대로 인라인 차용
- **commit 단위 분리**: L1~L5 / L6~L7 / P1~P4 / V1 — 각 단계 후 빌드+실측 검증
- **LVGL 표준 사용**: lodepng 직접 호출 금지, lv_png_init() + LVGL FS API 만 사용
- **Touch 기존 구조 보존**: ethernet_mqtt.cpp 단일 파일에 ETH.h 로직 인라인 (PC처럼 NetManager 클래스 신설 안 함)

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | Option A: Minimal | Option B: Clean | Option C: Pragmatic |
|----------|:-:|:-:|:-:|
| **Approach** | lodepng 직접 + ETH.h 인라인 | NetManager + ImageDecoder + LvFsAdapter 분리 | PC NetManager 패턴 인라인 + lv_png_init + LV_USE_FS_STDIO |
| **New Files** | 0 | 4 | 0 |
| **Modified Files** | 5 | 7 | 5 |
| **Complexity** | Low | High | Medium |
| **Maintainability** | Medium (ethernet_mqtt 비대) | High (모듈 분리) | High (PC 패턴 + 모듈 보존) |
| **Effort** | ~2-3 days | ~5-7 days | ~3-4 days |
| **Risk** | Medium-High (v1 lodepng 실패 반복) | Low (검증 모듈) | Low (PC 검증 + 변경 최소화) |
| **Recommendation** | Quick fix only | Long-term + 다수 신규 모듈 시 | **Default choice** |

**Selected**: **Option C — Pragmatic** — **Rationale**: v1 시도에서 lodepng 직접 호출(`#include "extra/libs/png/lodepng.h"`)이 undefined reference 발생 → LVGL 표준 (`lv_png_init()`)로 전환 필수. PC NetManager 클래스 분리는 Touch 규모에 과도설계 (Touch는 ethernet_mqtt.cpp 단일 모듈로 충분). 따라서 PC NetManager의 핵심 로직(ETH.begin + ETH event 처리)만 ethernet_mqtt.cpp에 인라인 이식.

### 2.1 Component Diagram

```
┌────────────────────────────────────────────────────────────────┐
│                      ESP32 (Arduino-ESP32 3.x)                 │
│                                                                │
│  ┌──────────┐    ┌────────────────────┐                        │
│  │ Browser  │───▶│ AsyncWebServer     │                        │
│  │ (Eth/WiFi)│◀───│ (WebServer.cpp)   │                        │
│  └──────────┘    └────────┬───────────┘                        │
│                           │                                    │
│                           │ AsyncTCP                           │
│                           ▼                                    │
│  ┌────────────────────────────────────────┐                    │
│  │     ESP32 lwIP (통합 socket stack)     │                    │
│  └──────┬─────────────────────────────┬───┘                    │
│         │                             │                        │
│   ETH.h │ ETH_PHY_W5500       WiFi.h  │                        │
│         ▼                             ▼                        │
│  ┌─────────────────┐         ┌──────────────────┐              │
│  │ W5500 (SPI Eth) │         │ ESP32 WiFi       │              │
│  └─────────────────┘         └──────────────────┘              │
│                                                                │
│  HTTPClient ── (lwIP socket) ──→ ETH/WiFi 양쪽 동작            │
│                                                                │
│  ┌────────────────────────────────────────┐                    │
│  │ LVGL 8.3.6                             │                    │
│  │  ├ lv_png decoder (LV_USE_PNG=1)        │                   │
│  │  └ LV_USE_FS_STDIO=1, drive='S'        │                    │
│  └───────────────┬────────────────────────┘                    │
│                  │ lv_img_set_src(obj, "S:/images/x.png")      │
│                  ▼                                             │
│            SPIFFS /images/*.png                                │
└────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow — PNG Hot Reload (v2.1)

```
1. 브라우저 → POST /api/images/upload (PNG, multipart)
2. AsyncWebServer (Ethernet IP에서 listen) → ImageApi::handleUpload
3. ImageApi: SPIFFS /images/photo.png 에 청크 저장 (v1과 동일)
4. final=true 도달 시 _pendingReload = true (v1과 동일)
5. main loop의 imageApi.loop() 폴링 → images_update() 호출
6. images_update():
   - title/photo/name 각 role 에 대해 try_set() 호출
   - PNG 경로면 lv_img_set_src(obj, "S:/images/photo.png") 호출
   - LVGL이 LV_USE_FS_STDIO 통해 SPIFFS 직접 read → lv_png decoder가 RGB565 변환
7. LVGL 다음 lv_timer_handler() 사이클에 화면 갱신
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| ETH.h | Arduino-ESP32 3.x core | W5500을 lwIP PHY로 등록 |
| WiFiClient | ESP32 lwIP socket | ETH/WiFi 공용 TCP client |
| HTTPClient | ESP32 internal | downloadFile/sendHttpMessage (ArduinoHttpClient 대체) |
| AsyncWebServer | mathieucarbou/ESPAsyncWebServer 3.x | PC와 동일 lib (Arduino-ESP32 3.x 호환) |
| TFT_eSPI | 최신 호환 버전 | LCD 드라이버 (회귀 검증 필수) |
| lv_png + LV_USE_FS_STDIO | LVGL 8.3.6 내장 | PNG 디코딩 + SPIFFS path 매핑 |

---

## 3. Data Model — 변경 없음

v1의 ImageEntry 구조, SPIFFS layout, imagesconfig.json 스키마 모두 그대로 유지.

LVGL FS drive letter 추가:
```cpp
// lv_conf.h
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'S'    // SPIFFS path: "S:/images/photo.png"
```

---

## 4. API Specification — 변경 없음

v1의 7개 엔드포인트(`GET /`, `/api/status`, `/api/images/list`, `POST /api/images/upload`, `DELETE/GET /api/images/{name}`, `/api/imagesconfig`) 그대로. 단 동작 환경이 WiFi-only → Ethernet + WiFi 모두 확장.

---

## 5. UI/UX — 변경 없음

`data/www/index.html`, `style.css`, `app.js` 모두 v1 그대로 유지. PNG 업로드는 클라이언트 측에서 이미 지원 (accept=".png,.bmp" 도 v1에 포함됨).

---

## 6. Error Handling — v1 + 추가

v1 에러 코드 그대로 + 새 항목:

| Code | Message | Cause | Handling |
|------|---------|-------|----------|
| 500 | "PNG decode failed" | lv_png_init 실패 또는 heap 부족 | 기존 BMP 유지, Serial 로그, toast |
| 500 | "LVGL FS read failed" | SPIFFS path가 LVGL `S:` driver로 매핑 안 됨 | LV_USE_FS_STDIO 설정 검증 |

---

## 7. Security — 변경 없음

v1과 동일 (Basic Auth + 확장자 화이트리스트 + 200KB 제한).

---

## 8. Test Plan

### 8.1 Test Scope

| Type | Target | Tool |
|------|--------|------|
| L1: API Tests | `/api/*` 엔드포인트 (Ethernet IP) | curl |
| L2: UI Tests | 브라우저에서 PNG/BMP 업로드 | 수동 |
| L3: Hardware Regression | LCD 색감, 터치 응답, 시간 동기화 | 수동 + 사진 비교 |
| L4: Memory Stability | 50회 PNG 업로드 후 heap | curl loop + Serial |

### 8.2 L1: API Test Scenarios (Ethernet 환경)

| # | Endpoint | Test | Expected |
|---|----------|------|----------|
| 1 | `/api/status` | Ethernet IP 로 GET | 200, `network.iface=ethernet`, `network.ip=<eth_ip>` |
| 2 | `/api/images/upload` | PNG 100KB 업로드 | 201, `.ok=true`, LCD 5초 내 갱신 |
| 3 | `/api/images/list` | PNG 업로드 후 조회 | photo.png 포함 |
| 4 | `/api/images/photo.png` | GET 미리보기 | 200, Content-Type: image/png |
| 5 | `/api/images/photo.png` | DELETE | 200, fallback BMP로 복귀 |

### 8.3 L3: Hardware Regression Checklist

- [ ] LCD title.bmp 색감 v1 대비 동일
- [ ] LCD photo.bmp 색감 v1 대비 동일
- [ ] 터치 입력 응답 (재부재 버턴) v1과 동일
- [ ] 화면보호기 진입/해제 정상
- [ ] 상단 로고 long click → 35회 → DeviceManager 진입 정상
- [ ] MQTT IN/OUT 메시지 → UI 토글 정상
- [ ] NTP 시간 동기화 → 06:00 자동 재부팅 (시뮬레이션)

### 8.4 L4: Memory Stability

```bash
# Ethernet IP 기준
for i in $(seq 1 50); do
  curl -u admin:12345 -F "file=@photo_test.png" http://192.168.10.118/api/images/upload
  curl -u admin:12345 -s http://192.168.10.118/api/status | jq .heap_free
  sleep 3
done
# 기준: heap_free 추이 안정 + 누수 0
```

---

## 9. Module Architecture — v1 모듈 유지

v1과 동일한 모듈 구조:
- `src/web/WebServer.{h,cpp}` — 변경 없음 (그대로 동작)
- `src/web/ImageApi.{h,cpp}` — 변경 없음
- `src/mqtt/ethernet_mqtt.{h,cpp}` — **변경**: ETH.h 인라인 이식
- `src/main.cpp` — **변경**: WiFiClient + HTTPClient + 가드 제거
- `src/images/images.{h,cpp}` — **변경**: PNG 경로는 LVGL FS path 패턴
- `lib/lv_conf.h` — **변경**: LV_USE_PNG=1, LV_USE_FS_STDIO=1
- `lib/TFT_eSPI/` — **변경**: 최신 호환 버전으로 업데이트

---

## 10. Coding Convention — v1 유지

- 한글 주석 허용
- `Design Ref: §{section}` / `Plan SC: {FR-XX}` 주석 패턴
- ETH 이벤트 콜백은 람다보다 static 함수 권장 (PC NetManager 패턴 따름)

---

## 11. Implementation Guide

### 11.1 File Structure (변경 후)

```
RemoteDeck_Touch/
├── platformio.ini             # 변경: platform pioarduino 53.x + lib 교체
├── lib/
│   ├── lv_conf.h              # 변경: LV_USE_PNG=1, LV_USE_FS_STDIO=1
│   └── TFT_eSPI/              # 변경: 호환 버전 (또는 fork)
├── src/
│   ├── main.cpp               # 변경: WiFiClient + HTTPClient + 가드 제거 + lv_png_init()
│   ├── mqtt/
│   │   ├── ethernet_mqtt.h    # 변경: #include <ETH.h>
│   │   └── ethernet_mqtt.cpp  # 변경: ETH.begin(ETH_PHY_W5500, ...) 인라인
│   ├── images/
│   │   ├── images.h           # 변경: PNG path 시그니처 추가 (선택)
│   │   └── images.cpp         # 변경: try_set 의 PNG 경로는 lv_img_set_src(obj, "S:/...png")
│   └── web/, device/, config/, utils/  # 변경 없음
└── data/                      # 변경 없음
```

### 11.2 Implementation Order (commit 단위 분리)

1. [ ] **C1 — Platform 마이그레이션**: platformio.ini 만 변경하고 빌드 시도
   - lib_deps 에서 arduino-libraries/Ethernet + ArduinoHttpClient 제거
   - platform = pioarduino 53.03.10 + AsyncWebServer mathieucarbou 3.x 추가
   - 결과: 빌드 실패 예상 (다음 단계에서 코드 수정)
2. [ ] **C2 — ethernet_mqtt.cpp ETH.h 이식**: PC NetManager 핵심 로직 인라인
   - `<Ethernet.h>` → `<ETH.h>`
   - `Ethernet.init/begin` → `ETH.begin(ETH_PHY_W5500, 1, W5500_CS_GPIO, INT_GPIO, -1, SPI)`
   - Static IP는 `ETH.config()`, DHCP는 ETH 이벤트가 자동 처리
   - 결과: ethernet_mqtt 빌드 통과, main.cpp는 아직 실패
3. [ ] **C3 — main.cpp 클라이언트 통일**: `EthernetClient ethClient` → `WiFiClient ethClient`
   - `Ethernet.localIP()` → `ETH.localIP()` 모든 호출처
   - 결과: main.cpp 빌드 통과, HttpClient 호출처만 남음
4. [ ] **C4 — HTTPClient 교체**: ArduinoHttpClient → ESP32 HTTPClient
   - `HttpClient http(ethClient, httpUrl, 80)` → `HTTPClient http;`
   - downloadFile/sendHttpMessage 재작성 (http.begin/GET/getStreamPtr/end)
   - 결과: 전체 빌드 통과 (TFT_eSPI 만 에러 가능)
5. [ ] **C5 — TFT_eSPI 업데이트**: 가장 위험한 단계
   - 호환 버전 확인 후 lib/TFT_eSPI 교체
   - User_Setup.h 핀 매핑 보존 확인
   - 빌드 통과 + **보드 업로드 + LCD 색감 검증** (사진 비교)
   - 회귀 시 즉시 stop, 다른 fork 시도 또는 LGFX 검토
6. [ ] **C6 — WebServer 가드 제거**: `if (wifi_conn)` → `if (ethernet_conn || wifi_conn)`
   - Ethernet 환경에서 webServer.begin(80) 동작 확인
   - L1 API 테스트 수행
7. [ ] **C7 — LV_USE_PNG + LV_USE_FS_STDIO 활성화**: lv_conf.h 만 수정
   - 빌드 통과 (lodepng.c, FS 어댑터가 LVGL 라이브러리 컴파일에 포함됨)
8. [ ] **C8 — main.cpp lv_png_init() 호출**: setup() 의 lvgl_touch_init() 직후
   - `extern "C" void lv_png_init(void);` 선언
   - LV_USE_FS_STDIO 가 자동 등록되므로 별도 등록 호출 불필요
9. [ ] **C9 — images.cpp PNG 분기**: try_set() 에서 PNG path 만 LVGL FS 사용
   - PNG 후보 발견 시 `lv_img_set_src(obj, "S:/images/photo.png")` (path 문자열)
   - BMP 후보는 v1 그대로 decode_image_from_spiffs → lv_img_set_src(obj, &dsc)
   - 결과: PNG 업로드 → LCD 표시 가능
10. [ ] **C10 — V1 회귀 검증**: §8.3 hardware regression checklist 완료
   - LCD 색감, 터치, MQTT, 자동 재부팅, 화면보호기 모두 정상
   - L4 50회 PNG 업로드 stability 통과

### 11.3 Session Guide

#### Module Map

| Module | Scope Key | Steps | Estimated Turns |
|--------|-----------|-------|:---------------:|
| platform + 라이브러리 교체 | `L1-L5` | C1+C2+C3+C4 | 35-45 |
| TFT_eSPI 업데이트 (위험) | `L6-L7` | C5+C6 | 20-30 (LCD 회귀 검증 포함) |
| PNG 활성화 | `P1-P4` | C7+C8+C9 | 20-25 |
| 회귀 검증 + L4 | `V1` | C10 | 15-20 |

#### Recommended Session Plan

| Session | Phase | Scope | Turns |
|---------|-------|-------|:-----:|
| Session 1 | Plan + Design | 전체 | 25 (완료) |
| Session 2 | Do | `--scope L1-L5` (platform + lib 교체) | 35-45 |
| Session 3 | Do | `--scope L6-L7` (TFT_eSPI ← 위험) | 20-30 |
| Session 4 | Do | `--scope P1-P4` (PNG 활성화) | 20-25 |
| Session 5 | Check + Report | `--scope V1` 검증 + 보고 | 25-35 |

**모든 세션은 `v2.1-lan` 브랜치에서 진행**. main 변경 없음.

---

## 12. Risk-Specific Designs

### 12.1 TFT_eSPI 회귀 대응 (가장 위험)

- **사전 준비**: v1 ea046d3 시점의 LCD 사진 확보 (각 화면 별)
- **검증 시점**: C5 직후 즉시 보드 업로드 + 사진 비교
- **회귀 발견 시**:
  1. lib/TFT_eSPI 의 User_Setup.h 와 lvgl_touch.cpp 의 핀 정의 일치 재확인
  2. fork 버전 시도: `bodmer/TFT_eSPI@^2.5` 또는 `lovyan03/LovyanGFX`
  3. 그래도 실패 시 `v2.1-lan` 브랜치 폐기 후 main(`ea046d3`) 유지

### 12.2 PNG heap 부족 대응

- v1의 `MIN_DECODE_HEAP=30KB` 가드는 lv_png_init 사용 시 자동 적용 안 됨 (LVGL 내부에서 alloc)
- 우회: 업로드 시점에 SPIFFS 파일 크기 확인 → 100KB 초과 시 즉시 reject (현재 IMAGE_MAX_BYTES=200KB 이미 적용)
- LVGL decode 실패 시 자동 fallback: `lv_img_set_src` 가 실패해도 LVGL 객체는 깨지지 않음 → 빈 화면 → BMP fallback 로직 동작

### 12.3 ETH 이벤트 비동기 → 동기 흐름 변환

PC NetManager 는 ARDUINO_EVENT_ETH_GOT_IP 이벤트로 IP 획득 알림. Touch ethernet_mqtt.cpp 는 동기 폴링 패턴 유지를 위해:

```cpp
ETH.begin(ETH_PHY_W5500, 1, W5500_CS_GPIO, INT_GPIO, -1, SPI);
unsigned long start = millis();
while (ETH.localIP() == IPAddress(0,0,0,0) && millis() - start < 10000) {
    delay(200);
}
```

이 폴링 방식은 setup() 안에서만 동작 — loop()에서는 ETH 가 자동 유지됨.

### 12.4 v2.2 백로그 (또 분리)

| 항목 | 우선순위 | 의존성 |
|------|:--:|--------|
| OTA Handler 이식 (PC v2.3.0) | High | v2.1 완료 |
| 로그 뷰어 + WebSocket | Medium | v2.1 완료 |
| deviceconfig/serverconfig 웹 편집 | Medium | UI 확장 |
| 알파 채널 PNG 합성 (배경색 블렌딩) | Low | v2.1 PNG 안정 후 |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-22 | 초안 작성 — Option C 채택, commit C1~C10 단위 분할 + TFT_eSPI 회귀 사전 대응 | KDI |
