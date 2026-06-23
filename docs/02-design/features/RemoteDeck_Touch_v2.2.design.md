---
template: design
version: 1.3
feature: RemoteDeck_Touch_v2.2
date: 2026-06-23
author: KDI
project: RemoteDeckSystem
status: Draft
plan_doc: ../../01-plan/features/RemoteDeck_Touch_v2.2.plan.md
base_commit: 156d089 (v2.1 final)
branch: v2.2-zero
---

# RemoteDeck_Touch v2.2 Design Document — sync WebServer + 협력적 yield

> **Summary**: ESP32 Arduino 내장 sync WebServer 사용 + 업로드 청크 사이 lv_timer_handler() 호출로 LVGL 지연 최소화. task 분리 안 함 → W5500+MQTT+WebServer 3자 task slot 충돌 회피.
>
> **Architecture**: Option C — Pragmatic
> **Branch**: `v2.2-zero` (base `156d089`)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | W5500+MQTT 환경 WebUI + PNG 콘텐츠 지원으로 단말 자급운영 + 원격 관리 완성 |
| **WHO** | Touch 단말 운영자, 콘텐츠 담당자, 원격 펌웨어 관리자 |
| **RISK** | sync handleClient() LVGL 지연 / OTA partition / PIO 업그레이드 회귀 / heap 압박 |
| **SUCCESS** | Ethernet WebUI 100% / PNG ≤5초 / OTA 성공 / v2.1 회귀 ZERO / heap ≥40KB |
| **SCOPE** | Phase 1 PoC → 2 API → 3 PNG → 4 OTA → 5 회귀 검증 |

---

## 1. Overview

### 1.1 Design Goals

1. **task 충돌 ZERO** — sync WebServer로 별도 task 생성 안 함
2. **LVGL 지연 최소화** — 업로드 청크 사이 협력적 yield (lv_timer_handler 호출)
3. **PC v2.3.0 API 호환** — `/api/images/*`, `/api/ota`, `/api/config`, `/api/log`, `/api/status` 동일 네이밍
4. **PNG LVGL 표준** — lv_png_init + LV_USE_FS_STDIO, `S:/images/photo.png` 패턴
5. **v2.1 회귀 ZERO** — Ethernet/MQTT/터치/Long-press/Sleep/BMP 모두 보존

### 1.2 Design Principles

- **No new task, no conflict**: sync WebServer = main loop driven, no FreeRTOS task creation
- **Cooperative multitasking**: 업로드 청크 처리 → lv_timer_handler() → 다음 청크 → repeat
- **PC API mirror**: PC v2.3.0 endpoint 시그니처를 Touch에 그대로 이식 (운영 일관성)
- **Module isolation**: ImageApi/ConfigApi/Logger/OTAHandler 분리 → 독립 테스트/디버그

---

## 2. Architecture

### 2.0 Architecture Selection

**Selected**: Option C — Pragmatic (sync WebServer + 협력적 yield)

**Rationale**: v2.1에서 mathieucarbou/AsyncWebServer가 W5500 환경에서 task 생성 실패. PC v2.3.0과 같은 패턴 재시도는 불필요한 위험. sync WebServer는 별도 task 없이 main loop에서 동작하므로 W5500 SPI poll task와 MQTT keep-alive task 사이에 task slot 경합이 일어나지 않는다. LVGL 지연은 청크 사이 협력적 yield로 50ms 이내로 통제.

### 2.1 Component Diagram

```
┌────────────────────────────────────────────────────────────────┐
│              ESP32 (Arduino-ESP32 3.x, latest PIO)             │
│                                                                │
│  ┌──────────┐       ┌──────────────────────────┐               │
│  │  Browser │──────▶│  sync WebServer (built-in)│              │
│  │          │◀──────│  handleClient() in loop()│               │
│  └──────────┘       └──────────┬───────────────┘               │
│                                │                               │
│        ┌──────────────────────┬┴┬─────────────────┬─────────┐  │
│        ▼                      ▼ ▼                 ▼         ▼  │
│   ┌─────────┐    ┌────────────┐  ┌──────────┐  ┌────────┐  ┌──┐│
│   │ImageApi │    │ ConfigApi  │  │  Logger  │  │OTAHandl│  │..││
│   │/upload  │    │ /config    │  │ /log     │  │ /ota   │  │  ││
│   │/list    │    │ /serverconf│  │ ring 50  │  │Update.h│  │  ││
│   └────┬────┘    └─────┬──────┘  └─────┬────┘  └────┬───┘  └──┘│
│        │ SPIFFS         │ SPIFFS       │ in-RAM    │ partition │
│        ▼                ▼              ▼           ▼           │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │              SPIFFS (huge_app.csv)                      │  │
│   │  /images, /www, deviceconfig, serverconfig, imagesconfig│  │
│   └─────────────────────────────────────────────────────────┘  │
│                                                                │
│  main loop() round-robin:                                      │
│    lv_timer_handler()  ← LVGL paint, touch                     │
│    webServer.handleClient()  ← HTTP 1 request per call         │
│    mqttEthernet_loop()  ← MQTT keep-alive                      │
│    imageApi.loop()  ← pending reload trigger                   │
│                                                                │
│  Existing (v2.1 그대로):                                       │
│    ETH + WiFiClient + PubSubClient + HTTPClient                │
│    LVGL + FT6236G touch + TFT_eSPI                             │
│    DeviceManager (Long-press / Sleep / IN-OUT)                 │
└────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow — Image Upload with cooperative yield

```
1. Browser → POST /api/images/upload (multipart, 100KB PNG)
2. sync WebServer 가 main loop 의 handleClient() 호출에서 진입
3. multipart 파서가 8KB 청크 단위로 body read
4. 청크 받을 때마다:
   a. SPIFFS file.write(chunk)
   b. lv_timer_handler() 호출 ← 협력적 yield
   c. 다음 청크 read
5. 업로드 완료 시 ImageApi::onUploadFinal:
   a. _pendingReload = true
   b. 200 OK 응답 즉시
6. 다음 loop iteration의 imageApi.loop() 가 _pendingReload 감지
   → images_update() 호출 → LCD 5초 내 갱신
7. (브라우저는 응답 받고 후속 fetch로 썸네일 갱신)
```

### 2.3 Dependencies

| Component | Library / Source | Purpose |
|-----------|------------------|---------|
| sync WebServer | ESP32 Arduino 내장 (`<WebServer.h>`) | HTTP 서버 (main loop driven) |
| Update.h | ESP32 Arduino 내장 | OTA 펌웨어 적용 |
| lv_png + LV_USE_FS_STDIO | LVGL 8.3.6 내장 | PNG 디코딩 + SPIFFS 매핑 |
| ETH.h + WiFiClient | v2.1 그대로 | LAN 통신 |
| PubSubClient | v2.1 그대로 | MQTT |
| HTTPClient | v2.1 그대로 | downloadFile/sendHttpMessage |
| TFT_eSPI | v2.1 그대로 (2.5.43) | LCD |
| FT6236G | v2.1 그대로 | 터치 |
| LVGL 8.3.6 | v2.1 그대로 | UI |
| **PIO platform** | 최신 stable (pioarduino 53.x 또는 latest) | 빌드 환경 |

---

## 3. Data Model — 변경 없음

v2.1 ImageEntry 구조, SPIFFS layout 그대로 유지. lv_conf.h 만 추가:

```c
// lib/lv_conf.h
#define LV_USE_PNG 1
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'S'    // SPIFFS path → "S:/images/photo.png"
```

### Logger ring buffer

```cpp
struct LogEntry {
    uint32_t timestamp;  // millis()
    char event[16];      // e.g. "IMG_UPLOAD"
    char detail[80];     // free-form
};

static LogEntry kRingBuffer[50];
static uint8_t kRingHead = 0;
static uint8_t kRingCount = 0;
```

---

## 4. API Specification

### 4.1 Endpoint List (PC v2.3.0 미러)

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/` | 이미지 관리 + OTA + 설정 + 로그 통합 UI | Required |
| GET | `/style.css`, `/app.js` | 정적 자산 | Required |
| GET | `/api/status` | heap/spiffs/network/fw | Required |
| GET | `/api/images/list` | 이미지 목록 + 메타 | Required |
| POST | `/api/images/upload` | PNG/BMP 업로드 + 즉시 핫리로드 | Required |
| GET | `/api/images/{name}` | 이미지 raw (썸네일 미리보기) | Required |
| DELETE | `/api/images/{name}` | 이미지 삭제 → fallback BMP | Required |
| GET | `/api/imagesconfig` | imagesconfig.json | Required |
| GET | `/api/config` | deviceconfig.json | Required |
| POST | `/api/config` | deviceconfig 업데이트 | Required |
| GET | `/api/serverconfig` | serverconfig.json | Required |
| POST | `/api/serverconfig` | serverconfig 업데이트 | Required |
| GET | `/api/log` | 최근 50건 로그 (JSON) | Required |
| POST | `/api/ota` | OTA 펌웨어 업로드 + 자동 reboot | Required |
| POST | `/api/reboot` | 수동 재부팅 | Required |

### 4.2 Detailed Specs

#### `POST /api/images/upload`

multipart/form-data, field `file` (PNG/BMP), max 200KB.
Cooperative yield: 매 청크 처리 후 `lv_timer_handler()` 호출.

Response 201:
```json
{ "ok": true, "name": "photo.png", "size": 102400, "reloaded": true }
```

#### `POST /api/ota`

multipart/form-data, field `firmware` (.bin), 자동 `Update.begin/write/end`.

Filename 파싱: `RemoteDeck_Touch_V{VERSION}_OTA_{YYYYMMDD}.bin` → version/date 추출 후 deviceConfig.firmwareInfo 저장.

Response 200 (성공 시 1초 후 자동 재부팅):
```json
{ "ok": true, "version": "2.2.0", "date": "2026-06-23" }
```

#### `GET /api/log`

```json
{
  "logs": [
    { "ts": 1234567, "event": "IMG_UPLOAD", "detail": "photo.png ok" },
    ...
  ]
}
```

---

## 5. UI/UX

### 5.1 Screen Layout — 4 탭 통합 UI

```
┌────────────────────────────────────────────────┐
│  RemoteDeck_Touch · v2.2     [logout]          │
├────────────────────────────────────────────────┤
│  [Images] [Config] [OTA] [Logs]   ← Tab nav    │
├────────────────────────────────────────────────┤
│                                                │
│  (Tab content)                                 │
│                                                │
└────────────────────────────────────────────────┘
```

### 5.2 Page UI Checklist

#### Images Tab (v2.1 유지)
- [ ] 3개 카드 (title/photo/name) + 썸네일 + 교체/삭제 버튼
- [ ] Drag&Drop 영역, 200KB 제한
- [ ] 진행률 표시

#### Config Tab
- [ ] deviceconfig 편집 폼 (deviceID, serverURL, sleep, reboot time, network)
- [ ] serverconfig 편집 폼 (MQTT broker, topics, imageUrl, statusUrl)
- [ ] 저장 버튼 → POST `/api/config` 또는 `/api/serverconfig`
- [ ] "재부팅 필요" 표시 (네트워크 설정 변경 시)
- [ ] Auth 변경 (user/pass)

#### OTA Tab
- [ ] 현재 버전 표시 (firmware version + date)
- [ ] 파일 선택 (`.bin`, max 2MB)
- [ ] 업로드 버튼 → POST `/api/ota`
- [ ] 진행률 + 완료 후 자동 재부팅 알림

#### Logs Tab
- [ ] 최근 50건 로그 표시 (시간/이벤트/상세)
- [ ] 5초마다 자동 새로고침
- [ ] 수동 새로고침 버튼
- [ ] Clear 버튼 (로컬만)

---

## 6. Error Handling

| Code | 응답 | 사용처 |
|------|------|--------|
| 200 | `{"ok":true,...}` | 성공 |
| 201 | `{"ok":true,"reloaded":true}` | 이미지 업로드 + 갱신 |
| 400 | `{"ok":false,"error":"bad request"}` | 확장자/필드 오류 |
| 401 | Basic Auth 다이얼로그 | 미인증 |
| 413 | `{"ok":false,"error":"too large"}` | 200KB / 2MB 초과 |
| 500 | `{"ok":false,"error":"..."}` | SPIFFS/OTA/Update 실패 |
| 503 | `{"ok":false,"error":"OTA in progress"}` | 동시 OTA 차단 |

---

## 7. Security

v2.1과 동일 Basic Auth + 확장자 화이트리스트. 추가:

- OTA: `.bin` 만 허용, 2MB 한도
- POST /api/config 검증: JSON 파싱 후 필드별 sanity check (length, IP 형식)
- 동시 OTA 차단: `_otaInProgress` 플래그

---

## 8. Test Plan

### 8.1 Phase 별 검증

| Phase | 검증 항목 | Tool |
|-------|----------|------|
| 1 PoC | sync WebServer + W5500 + MQTT 동시 동작 (해피 패스) | curl + 시리얼 로그 |
| 2 API | 9개 endpoint 모두 200 응답 | curl 자동화 스크립트 |
| 3 PNG | PNG 240×320 업로드 + LCD 5초 내 갱신 | 수동 + 사진 비교 |
| 4 OTA | 1.5MB .bin 업로드 → 재부팅 → 새 버전 표시 | OTA upload + 재부팅 후 /api/status |
| 5 회귀 | v2.1 체크리스트 모두 통과 | 보드 실측 |

### 8.2 L1 API Tests (curl)

```bash
# 200/401 check
curl -u admin:12345 http://<ip>/api/status     # 200
curl http://<ip>/api/status                     # 401

# Upload
curl -u admin:12345 -F "file=@photo.png" http://<ip>/api/images/upload  # 201
curl -u admin:12345 -F "file=@big_300kb.bmp" http://<ip>/api/images/upload  # 413

# Config
curl -u admin:12345 http://<ip>/api/config                             # 200 JSON
curl -u admin:12345 -X POST -d '{"deviceID":"node_2"}' http://<ip>/api/config  # 200

# OTA
curl -u admin:12345 -F "firmware=@RemoteDeck_Touch_V2.2.0_OTA_20260623.bin" http://<ip>/api/ota  # 200
```

### 8.3 L3 E2E Scenario

| # | Scenario | Steps | Success |
|---|----------|-------|---------|
| 1 | First boot | Power on → Ethernet/MQTT → ScreenMain | 모든 v2.1 기능 정상 |
| 2 | Image replace | Browser → upload PNG → LCD 갱신 | 5초 이내, LVGL frame drop ≤50ms |
| 3 | Config edit | Browser → Config 탭 → deviceID 변경 → 저장 | /api/config POST 200, 재부팅 후 새 deviceID |
| 4 | OTA update | Browser → OTA 탭 → .bin 업로드 → 재부팅 | 새 버전 /api/status에 반영 |
| 5 | Log query | Browser → Logs 탭 | 최근 이벤트 50건 표시 |
| 6 | Regression | 6시간 운영 후 heap/MQTT/터치 | heap ≥40KB, MQTT 정상, 터치 정상 |

---

## 9. Module Architecture

```
src/web/
├── WebServer.{h,cpp}        — sync WebServer 래퍼 + setup() + handleClient()
├── ImageApi.{h,cpp}         — /api/images/* (v2.1 재작성, sync 적응)
├── ConfigApi.{h,cpp}        — /api/config, /api/serverconfig
├── Logger.{h,cpp}           — in-memory ring buffer 50건
├── OTAHandler.{h,cpp}       — /api/ota (Update.h 기반)
└── AuthMiddleware.{h,cpp}   — Basic Auth 헬퍼

main.cpp:
  loop() {
    delay(5);
    lv_timer_handler();
    webServer.handleClient();   ← NEW
    mqttEthernet_loop();
    mqttHandler.loop();         (wifi conn 시)
    imageApi.loop();            ← v2.1 그대로 (pending reload)
  }
```

---

## 10. Coding Convention

v2.1 그대로 + 추가:

- WebServer 핸들러: `void handleXxx() { ... server.send(200, ...); }` (sync 패턴)
- 협력적 yield 헬퍼: `static inline void yieldToLvgl() { lv_timer_handler(); }`
- Upload 청크 마다 yieldToLvgl() 호출 (PC AsyncWebServer 와 다른 점)

---

## 11. Implementation Guide

### 11.1 File Structure (변경 후)

```
RemoteDeck_Touch/
├── platformio.ini             # 변경: AsyncWebServer 제거, PIO 업그레이드
├── lib/lv_conf.h              # 변경: LV_USE_PNG=1, LV_USE_FS_STDIO=1
├── src/
│   ├── main.cpp               # 변경: webServer.handleClient() + lv_png_init() 호출
│   ├── web/                   # 변경: AsyncWebServer → sync 재작성
│   ├── images/images.cpp      # 변경: PNG 경로 분기 → LVGL FS path
│   ├── mqtt/                  # 변경 없음
│   ├── config/                # 변경 없음
│   ├── device/                # 변경 없음
│   └── utils/                 # 변경 없음
└── data/www/                  # 변경: 4 탭 통합 UI
    ├── index.html             # 확장: nav + 4 panels
    ├── style.css              # PC v2.3.0 디자인 토큰 유지
    └── app.js                 # 확장: OTA + Config + Logs API
```

### 11.2 Implementation Order (Commit C1~C12)

| Phase | Commit | 내용 |
|-------|--------|------|
| **1 PoC** | C1 | platformio.ini: AsyncWebServer/AsyncTCP 제거, PIO 업그레이드 검토 |
| | C2 | src/web/WebServer.{h,cpp}: sync WebServer 최소 구현 (/api/status 만) |
| | C3 | main.cpp: webServer.handleClient() 통합 + 보드 PoC 검증 |
| **2 API** | C4 | src/web/AuthMiddleware + ImageApi (이미지 endpoint) |
| | C5 | src/web/ConfigApi (config + serverconfig endpoint) |
| | C6 | src/web/Logger (ring buffer + /api/log) |
| | C7 | data/www/ 4 탭 UI 작성 |
| **3 PNG** | C8 | lv_conf.h LV_USE_PNG=1 + LV_USE_FS_STDIO=1 |
| | C9 | main.cpp lv_png_init() 호출 + images.cpp PNG path 분기 |
| **4 OTA** | C10 | src/web/OTAHandler + Update.h 통합 + 파일명 파서 |
| | C11 | OTA UI panel + 보드 실측 (FULL → OTA 업데이트) |
| **5 회귀** | C12 | v2.1 체크리스트 + 50회 안정성 + 펌웨어 빌드 |

### 11.3 Session Guide

| Module | Scope Key | Turns | 위험도 |
|--------|-----------|:-----:|:--:|
| PoC (C1-C3) | `poc` | 25-35 | **🔴 최고** (sync+W5500+MQTT 검증 — 실패 시 esp_http_server pivot) |
| API (C4-C7) | `api` | 40-55 | 중 |
| PNG (C8-C9) | `png` | 15-20 | 중 (LVGL FS driver 검증) |
| OTA (C10-C11) | `ota` | 20-30 | 중 (Update.h + 펌웨어 실측) |
| 회귀 (C12) | `regression` | 20-25 | 낮 |

#### Recommended Session Plan

| Session | Phase | Scope | Turns |
|---------|-------|-------|:-----:|
| Session 1 | Plan + Design | 전체 | 30 (완료) |
| Session 2 | Do PoC | `--scope poc` | 30-40 |
| Session 3 | Do API | `--scope api` | 45-60 |
| Session 4 | Do PNG + OTA | `--scope png,ota` | 40-55 |
| Session 5 | Check + Report | `--scope regression` | 30-40 |

---

## 12. Risk-Specific Designs

### 12.1 sync WebServer + W5500 + MQTT 충돌 사전 검증 (Phase 1 PoC)

`/pdca do --scope poc` 단계에서 다음 minimum code 로 PoC:

```cpp
WebServer http(80);

void setup() {
    // ... v2.1 setup 그대로 (ETH, MQTT, LVGL) ...
    http.on("/api/status", [](){ http.send(200, "application/json", "{\"ok\":true}"); });
    http.begin();
}

void loop() {
    lv_timer_handler();
    http.handleClient();
    mqttEthernet_loop();
}
```

- 보드 부팅 → curl `/api/status` → 200 응답 확인
- 동시에 MQTT IN/OUT 메시지 + LCD 표시 모두 정상이면 PoC 통과
- 실패 시 즉시 esp_http_server 시도

### 12.2 OTA 1.5MB 업로드 LVGL 멈춤 대응

업로드 핸들러에서 청크(8KB) 사이 yield:

```cpp
HTTPUpload& upload = http.upload();
if (upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
    lv_timer_handler();   // 협력적 yield
}
```

LVGL은 약 30Hz refresh 목표 → 33ms 사이에 1회 yield 필요. 청크 8KB / 460Kbps = ~14ms. 청크 사이 lv_timer_handler 호출하면 LCD frame drop 거의 없음.

### 12.3 PIO 업그레이드 회귀 방지

- v2.1과 동일 platform 유지 (pioarduino 53.03.10) 또는 minor 업그레이드만 (53.0x)
- 주요 lib (TFT_eSPI, FT6236G, PubSubClient, ArduinoJson) 버전 핀
- 단순 stable 유지 — 새 PIO 기능 활용 시도 안 함

### 12.4 v2.3 백로그 (또 분리)

- 시간 표시 UI (사용자 보고 item 4)
- WebSocket 실시간 로그 push
- HTTPS / OAuth 인증 강화

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-23 | 초안 — Option C 채택 (sync + cooperative yield), C1~C12 commit 단위 분할, Phase 1 PoC 위험 명시 | KDI |
