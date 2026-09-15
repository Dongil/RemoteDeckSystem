# RemoteDeck_Touch_v2.3 Design Document

> **Summary**: esp_http_server (ESP-IDF native) 별도 task + core pinning 기반 WebUI/PNG/OTA/Control 일괄 구현. v2.2-zero 콜백 인터페이스 보존 (Option C — Pragmatic).
>
> **Project**: RemoteDeck_Touch
> **Version**: v2.3.0 (target)
> **Author**: KDI
> **Date**: 2026-06-23
> **Status**: Draft
> **Planning Doc**: [RemoteDeck_Touch_v2.3.plan.md](../../01-plan/features/RemoteDeck_Touch_v2.3.plan.md)
> **Branch**: `v2.3-httpd`

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.1 미구현 (WebUI/PNG/OTA/Control) 일괄 구현 + v2.2 실패(동시성 본질 불안정) 해결 |
| **WHO** | 단말 운영자(브라우저) / 배포 담당자(OTA) / 현장 사용자(Control) |
| **RISK** | esp_http_server 도 fail 가능 → PoC 엄격 검증으로 조기 분기 |
| **SUCCESS** | PoC 엄격 통과 + Match Rate ≥ 90% + LCD regression 無 |
| **SCOPE** | Phase 1 PoC → 2 WebUI+PNG → 3 OTA → 4 Control → 5 검증 |

---

## 1. Overview

### 1.1 Design Goals

- **동시성 격리**: WebServer task (core 0) ↔ LVGL/MQTT task (core 1) 물리적 분리 — v2.2 협력적 yield 가설 실패 본질 해결
- **인터페이스 보존**: v2.2-zero 의 `std::function` 콜백 시그니처 그대로 유지 → ImageApi/ConfigApi/Logger 재활용
- **PoC-First**: 라이브러리 교체 직후 풀세트 시나리오 (연속/병렬/혼합) 로 조기 검증 — 단발 OK 함정 회피
- **LCD 보호**: v2.1 운영 펌웨어의 Long-click / Sleep 저장 / 이미지 안전장치 / setup() 순서 100% 보존

### 1.2 Design Principles

1. **YAGNI** — HttpdAdapter / Middleware 추상화는 ESP32 자원 제약에서 비용 > 가치. 라이브러리 교체는 v2.3 1회로 종결.
2. **Single Source of Truth** — Basic Auth 검사는 WebServer 래퍼 1곳에서. 각 *Api 가 직접 헤더 파싱 금지.
3. **C/C++ 경계 명시** — esp_http_server URI handler 는 C function pointer. C++ lambda → static trampoline 패턴 통일.
4. **Fail Fast** — PoC 시나리오 1개라도 fail 시 즉시 v2.4 분기 결정 (sunk cost fallacy 회피, v2.2 학습)

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | Option A: Minimal | Option B: Clean | **Option C: Pragmatic** |
|----------|:-:|:-:|:-:|
| Approach | WebServer.cpp만 교체 | HttpdAdapter / Middleware 추상화 | v2.2-zero 콜백 보존 + 래퍼 1단 |
| New Files | 1 | 7+ | **3** |
| Modified Files | 6 | 8+ | **2** |
| Complexity | Medium | High | **Medium** |
| Maintainability | Medium | High | **High** |
| Effort | Low | High | **Medium** |
| Risk | v2.2-zero 자산 손실 | over-engineering | **Low — 인터페이스 보존** |
| Recommendation | 단순 PoC | 멀티 보드 이식 | **✅ Default choice** |

**Selected**: **Option C — Pragmatic** — **Rationale**: v2.2-zero HTML/CSS/JS + *Api 콜백 시그니처 보존 → 라이브러리 교체 비용 최소화 + 향후 유지보수 명료.

### 2.1 Component Diagram

```
                  ESP32 (Arduino-ESP32 3.x + pioarduino 53.x)
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  ┌─ Core 0 (httpd task) ──────────┐  ┌─ Core 1 (Arduino loop) ─┐ │
│  │                                │  │                         │ │
│  │  esp_http_server (port 80)     │  │  LVGL lv_timer_handler  │ │
│  │   ├─ /        (static SPIFFS)  │  │  Touch (FT6236G)        │ │
│  │   ├─ /api/status               │  │  MQTT loop              │ │
│  │   ├─ /api/images/*  (CRUD)     │  │  DeviceManager          │ │
│  │   ├─ /api/imagesconfig         │  │                         │ │
│  │   ├─ /api/config (deviceCfg)   │  │  ┌─ Shared Resources ─┐ │ │
│  │   ├─ /api/log                  │  │  │  SPIFFS (mutex)    │ │ │
│  │   ├─ /api/control (Long poll)  │◀─┼──┤  imagesconfig.json │ │ │
│  │   └─ /api/ota                  │  │  │  Control state     │ │ │
│  │                                │  │  │  Log ring buffer   │ │ │
│  │  HTTP Auth middleware          │  │  └────────────────────┘ │ │
│  │  (Basic admin:12345)           │  │                         │ │
│  └────────────────────────────────┘  └─────────────────────────┘ │
│                                                                  │
│  ┌─ W5500 SPI Ethernet ──────────────────────────────────────┐   │
│  │  ETH.h + ETH_PHY_W5500  (lwIP socket pool — shared)       │   │
│  │  DHCP 15s + retry  |  static fallback  |  MQTT client     │   │
│  └───────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
                                  ↑
                                  │ port 80 + 1883
            ┌─────────────────────┴─────────────────────┐
            │                                           │
        Browser (admin:12345)                    MQTT broker
        - Images / Config / Logs / Control
```

### 2.2 Data Flow

#### Upload (이미지)
```
Browser POST /api/images/upload (multipart)
  → httpd URI handler (core 0)
    → Basic Auth check → reject 401 if fail
    → ImageApi.onUploadStart(filename, total) → SPIFFS.open("/images/<name>", "w")
    → loop: httpd_req_recv() chunk → ImageApi.onUploadChunk(data, len) → file.write
    → isFinal: file.close + _pendingReload=true
    → response 200 {"ok":true,"reloaded":true}
  ← Browser
(main loop, core 1) ImageApi.loop() detects _pendingReload → images_update() (LVGL safe)
```

#### Control (Long polling 10s + ETag)
```
Browser GET /api/control?since=<etag>
  → httpd URI handler (core 0)
    → if current_etag == since: wait up to 10s on FreeRTOS event group
    → on MQTT state change: xEventGroupSetBits → handler unblocks
    → response: { etag, in: <bool>, out: <bool> }  (or 304 if no change after 10s)
  ← Browser (즉시 다음 폴링)

(core 1) MQTT callback (existing) → ControlState.update(in, out) → xEventGroupSetBits(STATE_CHANGED)
```

#### OTA
```
Browser POST /api/ota (binary OTA bin, 1.72MB)
  → Update.begin(updateSize, U_FLASH)
  → loop: httpd_req_recv() chunk → Update.write(data, len)
  → Update.end(true) → ESP.restart()
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| WebServer (래퍼) | esp_http_server (ESP-IDF), SPIFFS | URI handler 등록, static file serving |
| ImageApi | WebServer (콜백 등록), SPIFFS, images.h | 업로드/조회/삭제, _pendingReload flag |
| ConfigApi | WebServer, SPIFFS | deviceconfig.json + imagesconfig.json |
| Logger | WebServer | ring buffer 50건, 모듈 간 logEvent |
| ControlApi (신규) | WebServer, FreeRTOS event group, MQTT callback | Long polling 10s, ETag |
| OtaApi (신규) | WebServer, Update.h | OTA partition write + reboot |
| main.cpp | 위 모든 모듈 + v2.1 setup 순서 | attach + setup 순서 보존 |

---

## 3. Data Model

### 3.1 ControlState (신규)

```cpp
struct ControlState {
    bool in;      // 재실
    bool out;     // 부재
    uint32_t etag;  // millis() 기반 monotonic counter (변경 시 ++ )
};
```

### 3.2 UploadState (재사용 — v2.2-zero ImageApi)

```cpp
struct UploadState {
    File file;
    String name;             // sanitized basename
    size_t expected, written;
    bool open, ok;
    bool pendingReload;      // main loop trigger
};
```

### 3.3 OtaState (신규)

```cpp
struct OtaState {
    size_t expected, written;
    bool inProgress, ok;
    String message;
};
```

### 3.4 LogEntry (재사용 — v2.2-zero Logger)

```cpp
struct LogEntry {
    uint32_t ts;             // millis()
    char event[24];          // e.g. "IMG_UPLOAD"
    char detail[120];
};
// ring buffer size = 50
```

---

## 4. API Specification

> 모든 `/api/*` 경로는 Basic Auth (admin:12345). 401 시 `WWW-Authenticate: Basic realm="RemoteDeck_Touch"`.

### 4.1 Endpoint List

| # | Method | Path | Description | Auth |
|---|--------|------|-------------|:---:|
| 1 | GET | / | index.html (SPIFFS /www/) | ✓ |
| 2 | GET | /api/status | uptime, heap, spiffs, net, fw | ✓ |
| 3 | GET | /api/images/list | SPIFFS /images/* 목록 | ✓ |
| 4 | GET | /api/images/{name} | raw bmp/png | ✓ |
| 5 | DELETE | /api/images/{name} | 단일 삭제 + reload | ✓ |
| 6 | POST | /api/images/upload | multipart upload | ✓ |
| 7 | GET | /api/imagesconfig | imagesconfig.json | ✓ |
| 8 | GET | /api/config | deviceconfig.json | ✓ |
| 9 | POST | /api/config | deviceconfig 갱신 + apply | ✓ |
| 10 | POST | /api/reboot | 즉시 재부팅 | ✓ |
| 11 | GET | /api/log | ring buffer 50건 | ✓ |
| 12 | GET | /api/control?since={etag} | Long polling 10s | ✓ |
| 13 | POST | /api/control | { in, out } 토글 → MQTT publish | ✓ |
| 14 | POST | /api/ota | OTA bin 업로드 → Update.h → reboot | ✓ |

### 4.2 Key Endpoint Details

#### `GET /api/status`
```json
{
  "uptime_sec": 1234,
  "heap_free": 102400,
  "heap_min": 88000,
  "spiffs_used": 432100,
  "spiffs_total": 2097152,
  "network": { "iface": "ethernet", "ip": "192.168.0.100" },
  "fw_version": "2.3.0", "fw_date": "2026-06-23"
}
```

#### `GET /api/control?since={etag}` (Long polling)
- `since` 미일치: 즉시 200 + 현재 state
- `since` 일치: 최대 10초 대기 (FreeRTOS event group). 변경 발생 시 즉시 200, timeout 시 304.
```json
{ "etag": 42, "in": true, "out": false }
```

#### `POST /api/control`
```json
// Request
{ "in": true }
// Response 200
{ "ok": true, "etag": 43, "in": true, "out": false }
```
→ ControlState 갱신 + MQTT publish `remotedeck/<id>/in` `1`. event group set.

#### `POST /api/ota`
- Content-Type: `application/octet-stream`
- Content-Length: ≤ 1.72MB (OTA partition 크기)
- Streaming write 후 `Update.end(true)` → 응답 `{"ok":true,"reboot":true}` → 1초 후 `ESP.restart()`.
- Failure: 400 + `{"ok":false,"error":"<msg>"}`

### 4.3 Error Response (공통)
```json
{ "ok": false, "error": "<machine-readable>", "msg": "<human-readable>" }
```

| HTTP | error code | 사유 |
|------|------------|------|
| 400 | `bad_request` | 파라미터/사이즈 위반 |
| 401 | `unauthorized` | Basic Auth fail |
| 404 | `not_found` | 리소스 없음 |
| 413 | `too_large` | 사이즈 초과 (이미지 200KB, OTA 1.72MB) |
| 500 | `internal` | SPIFFS/Update.h 실패 |

---

## 5. UI/UX Design

### 5.1 Page Layout (v2.2-zero 재활용)

```
┌──────────────────────────────────────┐
│  RemoteDeck_Touch v2.3.0   admin@..  │
│  [Images] [Config] [Logs] [Control]  │ ← 4탭 (v2.2 4탭 → Control 추가)
├──────────────────────────────────────┤
│                                      │
│  (탭 컨텐츠)                          │
│                                      │
├──────────────────────────────────────┤
│  free heap: 100KB  |  eth: 192...    │
└──────────────────────────────────────┘
```

### 5.2 User Flow

```
브라우저 진입 → Basic Auth → /images (기본탭)
   ↓ Drag&Drop or Click
업로드 → 진행률 → 완료 → 자동 reload → LCD 갱신
   ↓
Control 탭 → IN/OUT 버튼 → POST + MQTT publish → LCD 미러
                                                  ↑ MQTT 외부 변경 → Long poll → UI 갱신
```

### 5.3 Component List

| Component | Location | Responsibility |
|-----------|----------|----------------|
| index.html | data/www/ | 4탭 SPA shell |
| style.css | data/www/ | PC v2.3.0 디자인 토큰 |
| app.js | data/www/ | tab switching + fetch + Long polling |
| WebServer | src/web/WebServer.{h,cpp} | esp_http_server 래퍼 + Auth + URI dispatch |
| ImageApi | src/web/ImageApi.{h,cpp} | 업로드/조회/삭제 (v2.2-zero 콜백 시그니처 보존) |
| ConfigApi | src/web/ConfigApi.{h,cpp} | device/images config |
| Logger | src/web/Logger.{h,cpp} | 50건 ring buffer |
| ControlApi (신규) | src/web/ControlApi.{h,cpp} | Long polling 10s + ETag + MQTT bridge |
| OtaApi (신규) | src/web/OtaApi.{h,cpp} | Update.h 기반 OTA |

### 5.4 Page UI Checklist

#### Images 탭
- [ ] Drag&Drop upload zone (PNG/BMP 화이트리스트)
- [ ] 업로드 진행률 (%)
- [ ] 이미지 카드 grid: 썸네일(SPIFFS), 파일명, 크기(bytes), 삭제 버튼
- [ ] SPIFFS 사용량 progress bar (used/total bytes)
- [ ] imagesconfig.json 매핑 표시 (어떤 이미지가 어떤 슬롯에)

#### Config 탭
- [ ] deviceconfig.json JSON editor (textarea + validate 버튼)
- [ ] Save 버튼 → POST /api/config → 즉시 apply
- [ ] Reboot 버튼 → POST /api/reboot 확인 dialog

#### Logs 탭
- [ ] 50건 ring buffer 표시 (timestamp + event + detail)
- [ ] Refresh 버튼 + 자동 갱신 토글 (10s 주기, off 기본)
- [ ] Clear 버튼

#### Control 탭 (신규)
- [ ] LCD 미러 영역 (240×320 비율 컨테이너 + 현재 이미지 슬롯 표시)
- [ ] IN 버튼 (재실, 토글, 활성 시 강조 색상)
- [ ] OUT 버튼 (부재, 토글, 활성 시 강조 색상)
- [ ] 마지막 갱신 시각 (last etag tick)
- [ ] Long polling 상태 표시 (connected / reconnecting)

#### 공통 푸터
- [ ] free heap bytes
- [ ] eth iface + IP
- [ ] firmware version + date

---

## 6. Error Handling

### 6.1 Error Code Definition

| HTTP | error | 사유 | 처리 |
|------|-------|------|------|
| 400 | `bad_request` | 파라미터/사이즈/포맷 위반 | UI 폼 에러 표시 |
| 401 | `unauthorized` | Basic Auth 실패 | 브라우저 prompt 재요청 |
| 404 | `not_found` | path 없음 | 404 페이지 |
| 413 | `too_large` | 사이즈 초과 | "최대 200KB / 1.72MB" 안내 |
| 500 | `internal` | SPIFFS / Update fail | 재시도 또는 reboot 권장 |

### 6.2 펌웨어 내부 Fail-Soft

| 상황 | 동작 |
|------|------|
| upload 중 heap 부족 | 즉시 close + SPIFFS.remove + 400 응답 |
| OTA 중 power loss | factory partition 자동 부팅 (Update.h 보장) |
| Long polling 동안 socket 부족 | 신규 요청 503 (max_open_sockets 한계) — 클라이언트 재시도 |
| MQTT 끊김 | ControlApi 는 마지막 known state 유지, etag 변경 없음 |

---

## 7. Security Considerations

- [x] Basic Auth (admin:12345) — v2.1 동일 (LAN 내부 사용 전제)
- [x] 파일명 sanitize — basename 추출 + `..` 거부 + 확장자 화이트리스트(.png/.bmp)
- [x] Upload 사이즈 상한 — 이미지 200KB, OTA 1.72MB (Content-Length 검증)
- [x] OTA 무결성 — Update.h MD5 (자동), dual partition 보호
- [ ] HTTPS — 미적용 (ESP32 자원 제약, LAN 환경 전제)
- [ ] Rate limiting — 미적용 (단일 사용자 전제)

> 위협 모델: LAN 내부 신뢰 환경. WAN 노출 시 별도 reverse proxy 필요 (out of scope).

---

## 8. Test Plan (PoC-First)

### 8.1 Test Scope

| Type | Target | Tool | Phase |
|------|--------|------|-------|
| L1: API Tests | 14개 엔드포인트 status/shape | curl + Bash 스크립트 | Do |
| L2: PoC 엄격 시나리오 | 동시성/지속성/메모리 | curl 병렬 + MQTT pub | Do (Phase 1 gate) |
| L3: 수동 시나리오 | LCD 운용 regression | 사람 검증 | Check |

### 8.2 L1: API Test Scenarios

| # | Endpoint | Method | Test | Expected |
|---|----------|--------|------|---------|
| 1 | /api/status | GET | 인증 후 호출 | 200, JSON with heap_free, network.ip |
| 2 | /api/status | GET | 인증 없이 | 401 + WWW-Authenticate |
| 3 | /api/images/list | GET | 인증 후 | 200, `.images[]` 배열 |
| 4 | /api/images/upload | POST | 200KB BMP 업로드 | 200 + `_pendingReload` 후 list 에 반영 |
| 5 | /api/images/upload | POST | 200KB+1 byte | 413 |
| 6 | /api/images/{name} | DELETE | 존재 파일 | 200 ok=true |
| 7 | /api/config | GET | 인증 후 | 200, deviceconfig JSON |
| 8 | /api/config | POST | invalid JSON | 400 bad_request |
| 9 | /api/log | GET | upload 후 | IMG_UPLOAD 엔트리 1건 이상 |
| 10 | /api/control | GET | since=0 | 즉시 200 with etag>0 |
| 11 | /api/control | GET | since=current | 10초 후 304 또는 변경 시 즉시 200 |
| 12 | /api/control | POST | {"in":true} | 200 + etag 증가 + MQTT publish |
| 13 | /api/ota | POST | OTA bin 1.5MB | 200 reboot=true |
| 14 | /api/ota | POST | bin 2MB | 413 |

### 8.3 L2: PoC 엄격 시나리오 (Phase 1 Gate)

> **이 단계 통과 못하면 v2.4 분기 — Plan 명시**

| # | 시나리오 | 통과 조건 |
|---|---------|---------|
| P1 | 연속 upload 10회 (10KB dummy BMP) | 10/10 200, heap_free ≥ 80KB 유지, LCD frame drop 시각 無 |
| P2 | 병렬 GET 5개 (`/api/status` × 5) for 30초 | 모든 응답 200, 응답시간 ≤ 1000ms, ESP 부팅 hang 無 |
| P3 | MQTT 동시 트래픽 (1Hz publish) + GET /api/control × 3 for 1분 | MQTT 메시지 1건도 누락 없음, etag 정상 증가 |
| P4 | 부팅 후 1분간 무요청 idle | heap 안정 (±5KB), LCD 정상 |

스크립트 위치: `RemoteDeck_Touch/test/poc/run_poc.sh` (Do phase 작성)

### 8.4 L3: 수동 LCD Regression (Check phase)

| # | 시나리오 | 통과 조건 |
|---|---------|---------|
| M1 | Long-click 1회 → 설정 진입 | v2.1 동일 (35회 dead-code 재발 無) |
| M2 | Sleep 시간 변경 → 저장 → 재부팅 → 값 유지 | v2.1 regression fix 보존 |
| M3 | 이미지 web upload 후 즉시 LCD 갱신 | _pendingReload → images_update() 동작 |
| M4 | Control 탭 IN 버튼 → LCD 즉시 미러 | MQTT bridge 정상 |
| M5 | DHCP 끊김 → 재연결 15s 내 복귀 | v2.1 동작 보존 |

### 8.5 Seed Data Requirements

| Entity | Count | Note |
|--------|:----:|------|
| /images/*.bmp | 2-3 | v2.1 부터 존재, PoC 에 dummy 추가 |
| imagesconfig.json | 1 | 슬롯 매핑 (v1 부터) |
| deviceconfig.json | 1 | sleep, MQTT, network (v2.1 부터) |

---

## 9. Clean Architecture

> ESP32 임베디드 단일 펌웨어 — bkit Starter 분류. 다만 src/ 내 모듈 경계는 명료히.

### 9.1 Layer Structure

| Layer | Responsibility | Location |
|-------|---------------|----------|
| Presentation (Web) | HTML/CSS/JS + URI dispatch | `data/www/`, `src/web/WebServer.*` |
| Application | API handlers (콜백) | `src/web/{Image,Config,Control,Ota}Api.*`, `src/web/Logger.*` |
| Domain | Device state, image config | `src/device/`, `src/images/` |
| Infrastructure | esp_http_server, ETH/MQTT, SPIFFS, Update.h | ESP-IDF / Arduino-ESP32 lib |

### 9.2 Dependency Rules

```
Web/UI (data/www) ──→ WebServer (래퍼) ──→ *Api (콜백)
                                              │
                                              ▼
                              DeviceManager / images / MQTT / SPIFFS
                                  (core 1 task — main loop)
                                              ▲
                                              │ FreeRTOS event group
                              ControlApi (core 0 httpd task) ──┘
```

규칙:
- `*Api` 는 esp_http_server 의존 금지 (콜백 시그니처 = `std::function<String()>` / `std::function<bool(...)>`)
- WebServer 만 esp_http_server include
- DeviceManager / MQTT 는 web/* 의존 금지 (단방향)

### 9.3 File Import Rules

| From | Can Import | Cannot Import |
|------|-----------|---------------|
| WebServer | esp_http_server, *Api callback types | *Api 구현 디테일 |
| *Api | SPIFFS, ArduinoJson, Domain | esp_http_server |
| ControlApi | freertos/event_groups, MQTT publish 함수 | esp_http_server |
| Domain (device/images) | Arduino 기본 | web/* |

### 9.4 This Feature's Layer Assignment

| Component | Layer | Location |
|-----------|-------|----------|
| index.html / app.js / style.css | Presentation | `data/www/` |
| WebServer | Presentation (URI dispatch) | `src/web/WebServer.*` |
| ImageApi / ConfigApi / ControlApi / OtaApi / Logger | Application | `src/web/` |
| DeviceManager / images | Domain | `src/device/`, `src/images/` |
| esp_http_server / Update.h / SPIFFS / ETH.h | Infrastructure | (ESP-IDF / Arduino) |

---

## 10. Coding Convention Reference

### 10.1 Naming

| Target | Rule | Example |
|--------|------|---------|
| C++ class | PascalCase | `WebServer`, `ImageApi`, `ControlApi` |
| Method | camelCase | `attach()`, `setStatusGetter()`, `handleUpload()` |
| Constant | UPPER_SNAKE | `IMAGE_MAX_BYTES`, `OTA_PARTITION_SIZE`, `LONG_POLL_TIMEOUT_MS` |
| File | PascalCase.{h,cpp} | `WebServer.cpp`, `ControlApi.h` |
| JSON key | snake_case | `heap_free`, `fw_version` |

### 10.2 Include Order

```cpp
// 1. ESP-IDF / Arduino-ESP32
#include <esp_http_server.h>
#include <Arduino.h>
#include <SPIFFS.h>
#include <Update.h>
#include <ArduinoJson.h>

// 2. freertos
#include <freertos/event_groups.h>

// 3. 프로젝트
#include "../images/images.h"
#include "WebServer.h"
```

### 10.3 Design Reference Comments

Plan SC 와 Design §xx 를 코드에 인라인 — v2.2 패턴 계승.

```cpp
// Design Ref: §2.1 — core 0 pinning, LVGL coexist
// Plan SC: FR-01, FR-02
httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
cfg.task_priority = 5;
cfg.core_id = 0;
cfg.max_open_sockets = 7;
```

### 10.4 This Feature's Conventions

| Item | Convention |
|------|-----------|
| URI handler | C-style static trampoline → `WebServer*` capture |
| JSON | ArduinoJson StaticJsonDocument (heap 보호) |
| Error 응답 | `{"ok":false,"error":"<code>","msg":"<human>"}` |
| Auth | WebServer::requireAuth(req) → 401 자동 응답 |

---

## 11. Implementation Guide

### 11.1 File Structure

```
RemoteDeck_Touch/
├── platformio.ini                    [수정] LV_USE_PNG=1, async lib 제거
├── data/www/                         [재활용+조정]
│   ├── index.html                    Control 탭 폴링 2s → 10s
│   ├── style.css                     (v2.2-zero 그대로)
│   └── app.js                        Long polling client
├── src/
│   ├── main.cpp                      [수정] web 모듈 attach, setup 순서 보존
│   ├── images/images.{h,cpp}         [수정] PNG 분기 + 안전장치
│   ├── device/DeviceManager.*        (v2.1 그대로)
│   ├── mqtt/ethernet_mqtt.*          [수정] ControlApi 이벤트 hook 추가
│   └── web/                          [신규/재작성]
│       ├── WebServer.{h,cpp}         [재작성] esp_http_server 래퍼
│       ├── ImageApi.{h,cpp}          [재활용+adapt] upload chunk만 변경
│       ├── ConfigApi.{h,cpp}         (v2.2-zero 그대로)
│       ├── Logger.{h,cpp}            (v2.2-zero 그대로)
│       ├── ControlApi.{h,cpp}        [신규] Long polling 10s + ETag
│       └── OtaApi.{h,cpp}            [신규] Update.h
└── test/poc/
    ├── run_poc.sh                    [신규] L2 시나리오 자동화
    └── mqtt_pub.py                   [신규] P3 트래픽 시뮬레이션
```

### 11.2 Implementation Order

1. [ ] **Phase 1 — PoC** (esp_http_server 부팅 + `/api/status` + LVGL/MQTT coexist + L2 P1~P4 통과)
2. [ ] **Phase 2 — WebUI 풀세트** (ImageApi/ConfigApi/Logger 콜백 attach + PNG)
3. [ ] **Phase 3 — OTA** (OtaApi)
4. [ ] **Phase 4 — Control** (ControlApi Long polling + MQTT bridge)
5. [ ] **Phase 5 — 통합 검증** (L1 14건 + L3 M1~M5 수동)

### 11.3 Session Guide

#### Module Map

| Module | Scope Key | Description | Estimated Turns |
|--------|-----------|-------------|:---:|
| WebServer + PoC | `module-poc` | esp_http_server 래퍼 + /api/status + L2 P1~P4 통과 | 30-40 |
| WebUI 풀세트 + PNG | `module-webui` | ImageApi/ConfigApi/Logger attach + LV_USE_PNG + images.h PNG 분기 | 25-35 |
| OTA | `module-ota` | OtaApi + Update.h + 빌드 스크립트 OTA bin 생성 확인 | 15-20 |
| Control | `module-control` | ControlApi Long polling + ETag + MQTT bridge + Control 탭 UI | 25-30 |
| 통합 검증 | `module-verify` | L1 14건 + L3 M1~M5 + 펌웨어 FULL/OTA 배포 | 15-20 |

#### Recommended Session Plan

| Session | Phase | Scope | Turns | Gate |
|---------|-------|-------|:---:|------|
| Session 1 | Plan + Design | 전체 | ~30 (완료) | ✅ |
| Session 2 | Do | `--scope module-poc` | 30-40 | **L2 P1~P4 통과** → 계속, fail → v2.4 |
| Session 3 | Do | `--scope module-webui,module-ota` | 40-55 | L1 1~9, 13~14 통과 |
| Session 4 | Do | `--scope module-control` | 25-30 | L1 10~12 + M4 통과 |
| Session 5 | Check + Report | `--scope module-verify` | 25-30 | Match Rate ≥ 90% |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-23 | Initial draft — Option C 선택, 5 모듈 5 세션 계획 | KDI |
