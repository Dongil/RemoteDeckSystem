---
template: design
version: 1.3
feature: RemoteDeck_Touch
date: 2026-06-22
author: KDI
project: RemoteDeckSystem
status: Draft
plan_doc: ../../01-plan/features/RemoteDeck_Touch.plan.md
---

# RemoteDeck_Touch Design Document

> **Summary**: RemoteDeck_PC `WebServer/data/www` 패턴을 Touch에 이식 + Touch 특화 `ImageApi` 모듈 신설. PNG 디코더 통합 + LVGL 핫리로드로 재부팅 없는 이미지 교체 달성.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-06-22
> **Status**: Draft
> **Planning Doc**: [RemoteDeck_Touch.plan.md](../../01-plan/features/RemoteDeck_Touch.plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | BMP 교체 불안정 + 재부팅 강제 + 웹 UI 부재로 인한 운영 부담 해소 |
| **WHO** | RemoteDeck_Touch 단말 운영자, 현장 유지보수 인력 |
| **RISK** | ESP32 메모리 한계 (PNG heap, AsyncWebServer RAM 압박, 듀얼 네트워크 자원 경합) |
| **SUCCESS** | 교체 성공률 100% / 재부팅 없이 ≤5초 갱신 / heap free ≥ 50KB |
| **SCOPE** | Phase 1 이미지 파이프라인 안정화 → Phase 2 웹 UI 이식 → Phase 3 핫리로드 + 검증 |

---

## 1. Overview

### 1.1 Design Goals

1. **이미지 교체 안정성 100%** — `downloadFile()`/BMP 파서/SPIFFS write 경로의 모든 실패 모드 제거
2. **재부팅 없는 핫리로드** — 업로드 직후 LVGL이 즉시 새 이미지로 갱신
3. **PC와의 운영 일관성** — RemoteDeck_PC의 `WebServer`/`data/www` 패턴과 인증/로깅 정책 일치
4. **메모리 안전성** — heap free ≥ 50KB 유지, LVGL `img_dsc.data` 누수 0
5. **확장 가능성** — v2(OTA, 로그, 설정 편집) 추가 시 PC와 동일 패턴으로 모듈만 추가

### 1.2 Design Principles

- **PC-Touch 패턴 일치**: WebServer 클래스/Basic Auth/콜백 등록 패턴을 동일하게 유지
- **임베디드 자원 우선**: 추상화 계층 최소화, 가상 함수 회피, 정적 메모리 우선
- **격리된 핫리로드**: 이미지 파이프라인을 `ImageApi`/`images.cpp`에 격리해 LVGL과 명확히 분리
- **Fail-soft**: 업로드/디코딩 실패 시 기존 이미지 유지, 단말 동작 지속

---

## 2. Architecture Options

### 2.0 Architecture Comparison

| Criteria | Option A: Minimal | Option B: Clean | Option C: Pragmatic |
|----------|:-:|:-:|:-:|
| **Approach** | main.cpp에 web 코드 직접 추가 | src/web/ + src/image/ 완전 분리, 디코더 추상화 | PC `WebServer/data/www` 이식 + Touch `ImageApi` 신설 |
| **New Files** | 1 | 8 | 4 |
| **Modified Files** | 3 | 5 | 4 |
| **Complexity** | Low | High | Medium |
| **Maintainability** | Medium | High | High |
| **Effort** | ~3 days | ~7 days | ~4-5 days |
| **Risk** | Low (coupled, main.cpp 비대) | Low (clean, 과도설계) | Low (PC 패턴 검증됨) |
| **Recommendation** | Quick wins, hotfixes | Long-term + 다수 신규 기능 | **Default choice** |

**Selected**: **Option C — Pragmatic** — **Rationale**: RemoteDeck_PC v2.3.0에서 검증된 `WebServer/AsyncWebServer/Basic Auth/data/www` 패턴을 그대로 이식하면 안정성과 PC-Touch 운영 일관성을 동시에 달성할 수 있다. Touch 특화 요소(LVGL 핫리로드, BMP/PNG 디코더)는 `ImageApi`로 격리하여 향후 v2 확장(OTA/로그) 시 PC와 동일 모듈 패턴 적용 가능.

### 2.1 Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     RemoteDeck_Touch (ESP32)                │
│                                                             │
│  ┌────────────┐    ┌───────────────┐    ┌──────────────┐    │
│  │  Browser   │───▶│ AsyncWebServer│───▶│  ImageApi    │    │
│  │  (운영자)  │◀───│ (Basic Auth)  │◀───│  /api/images*│    │
│  └────────────┘    └───────┬───────┘    └──────┬───────┘    │
│                            │                   │            │
│                            │ static            │ Upload     │
│                            ▼                   ▼            │
│                    ┌───────────────┐   ┌────────────────┐   │
│                    │ /data/www/*   │   │ SPIFFS /images/│   │
│                    │ (index.html)  │   │ (PNG/BMP)      │   │
│                    └───────────────┘   └────────┬───────┘   │
│                                                 │ Decode    │
│                                                 ▼           │
│  ┌────────────┐    ┌───────────────┐   ┌────────────────┐   │
│  │   LVGL UI  │◀───│ images_update │◀──│ ImageDecoder   │   │
│  │ (240x320)  │    │ (Hot Reload)  │   │ BMP/PNG → 565  │   │
│  └────────────┘    └───────────────┘   └────────────────┘   │
│                                                             │
│  Existing modules (unchanged):                              │
│  - MQTT (IN/OUT, time sync, reboot)                         │
│  - DeviceManager (config setup screen)                      │
│  - Ethernet (W5500) + WiFi (dual)                           │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow — Image Upload + Hot Reload

```
1. 운영자 브라우저: PNG 파일 선택 → POST /api/images/upload (multipart)
2. AsyncWebServer: Basic Auth 검증 → upload handler 진입
3. ImageApi::handleUpload:
   - 청크 단위(≤1024B) SPIFFS 저장 (/images/{filename})
   - Content-Length 검증, 실패 시 partial 파일 삭제
4. 업로드 final=true 도달:
   - ImageDecoder::decode(filename) 호출 → lv_img_dsc_t 생성
   - 기존 img_photo.data 등 free → 새 dsc 할당
   - lv_img_set_src(ui_*) + lv_obj_invalidate() 호출
5. 브라우저: 201 응답 + 새 이미지 URL 반환
6. LVGL: 다음 lv_timer_handler() 사이클에 화면 갱신 (≤30ms)
```

### 2.3 Dependencies

| Component | Depends On | Purpose |
|-----------|-----------|---------|
| WebServer | AsyncWebServer, AsyncTCP | HTTP 비동기 처리 |
| WebServer | AuthMiddleware | Basic Auth 검증 (PC와 동일 정책) |
| ImageApi | SPIFFS, ImageDecoder | 업로드 저장 + 디코딩 |
| ImageDecoder | lv_png, BMP parser | PNG/BMP → RGB565 변환 |
| images.cpp | ImageDecoder, LVGL | 핫리로드 + LVGL src 갱신 |
| main.cpp | WebServer | server.begin() + 콜백 등록 |

---

## 3. Data Model

### 3.1 Entity: ImageEntry (런타임 메타데이터)

```cpp
struct ImageEntry {
    String name;          // 예: "photo.png"
    size_t size;          // SPIFFS 파일 크기
    uint32_t lastModified; // SPIFFS 메타 (epoch sec)
    String role;          // "title" | "photo" | "name" (imagesconfig.json 매핑)
    bool active;          // 현재 LVGL에 바인딩 상태
};
```

### 3.2 SPIFFS Layout

```
/                                  (SPIFFS root)
├── deviceconfig.json              (기존 유지)
├── serverconfig.json              (기존 유지)
├── imagesconfig.json              (기존 유지 - 이미지 role 매핑)
├── images/                        (NEW: 활성 이미지 영구 저장)
│   ├── title.png / title.bmp
│   ├── photo.png / photo.bmp
│   └── name.png / name.bmp
├── download/                      (deprecated, v2에서 제거 검토)
└── www/                           (NEW: 웹 UI 정적 자산)
    ├── index.html                 (PC `data/www/`에서 이식, 단순화)
    ├── style.css
    └── app.js
```

### 3.3 imagesconfig.json (기존 스키마 유지)

```json
{
  "version": "2.0",
  "buzzer": { /* 기존 동일 */ },
  "images": [
    { "url": "title.png", "x": 0,   "y": 0,   "z": 0, "state": "title" },
    { "url": "photo.png", "x": 0,   "y": 86,  "z": 0, "state": "photo" },
    { "url": "name.png",  "x": 0,   "y": 236, "z": 0, "state": "name"  }
  ]
}
```
> 호환성: 기존 `.bmp` URL도 그대로 동작 (디코더가 확장자로 분기).

---

## 4. API Specification

### 4.1 Endpoint List

| Method | Path | Description | Auth |
|--------|------|-------------|------|
| GET | `/` | 이미지 관리 UI (HTML) | Required |
| GET | `/api/images/list` | 현재 이미지 목록 + 메타 | Required |
| POST | `/api/images/upload` | PNG/BMP 업로드 + 즉시 적용 | Required |
| DELETE | `/api/images/{name}` | 이미지 삭제 (fallback으로 복귀) | Required |
| GET | `/api/images/{name}` | 이미지 raw 다운로드 (미리보기용) | Required |
| GET | `/api/status` | heap/SPIFFS/uptime/network JSON | Required |
| GET | `/api/imagesconfig` | imagesconfig.json 반환 | Required |

### 4.2 Detailed Specification

#### `GET /api/images/list`

**Response 200:**
```json
{
  "images": [
    { "name": "title.png", "size": 12480, "lastModified": 1718937600, "role": "title", "active": true },
    { "name": "photo.png", "size": 38912, "lastModified": 1718937600, "role": "photo", "active": true },
    { "name": "name.png",  "size":  6520, "lastModified": 1718937600, "role": "name",  "active": true }
  ],
  "spiffs_used": 92160,
  "spiffs_total": 1048576
}
```

#### `POST /api/images/upload`

**Request**: `multipart/form-data`
- Field: `file` (binary, PNG 또는 BMP)
- Field: `role` (optional, "title"|"photo"|"name"; 미지정 시 파일명에서 추론)

**Response 201:**
```json
{ "ok": true, "name": "photo.png", "size": 38912, "reloaded": true }
```

**Errors:**
- `400` — 파일 누락, 확장자 불일치
- `401` — Basic Auth 실패
- `413` — `IMAGE_MAX_BYTES`(기본 200KB) 초과
- `500` — SPIFFS write 실패 또는 디코더 실패 (기존 이미지 유지)

#### `DELETE /api/images/{name}`

**Response 200:**
```json
{ "ok": true, "deletedFile": "photo.png", "fallbackTo": "/images/photo.bmp" }
```

#### `GET /api/status`

**Response 200:**
```json
{
  "uptime_sec": 12480,
  "heap_free": 124680,
  "heap_min": 98240,
  "spiffs_used": 92160,
  "spiffs_total": 1048576,
  "network": { "iface": "ethernet", "ip": "192.168.10.97", "mac": "..." },
  "fw_version": "1.0.0-touch",
  "fw_date": "2026-06-22"
}
```

---

## 5. UI/UX Design

### 5.1 Screen Layout (Web UI)

```
┌──────────────────────────────────────────────────┐
│  RemoteDeck_Touch · Image Manager   [logout]     │
├──────────────────────────────────────────────────┤
│  Status: heap=125KB · spiffs=92/1024KB · eth ✓   │
├──────────────────────────────────────────────────┤
│                                                  │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐           │
│  │ title   │  │ photo   │  │ name    │           │
│  │ 320x86  │  │ 320x150 │  │ 320x84  │           │
│  │ [thumb] │  │ [thumb] │  │ [thumb] │           │
│  │ Replace │  │ Replace │  │ Replace │           │
│  └─────────┘  └─────────┘  └─────────┘           │
│                                                  │
│  ┌──────────────────────────────────────────┐    │
│  │ Drag & drop PNG/BMP here, or [Browse]    │    │
│  └──────────────────────────────────────────┘    │
│                                                  │
│  Recent activity (5):                            │
│  · 2026-06-22 10:34  upload photo.png ok         │
│  · 2026-06-22 10:30  upload title.bmp ok         │
└──────────────────────────────────────────────────┘
```

### 5.2 User Flow

```
Open IP in browser → Basic Auth dialog → Image manager page
  → Click "Replace" on photo card OR Drag&Drop file
  → Upload progress bar → "Updated" toast (≤5s)
  → Card thumbnail refreshes (cache-bust query)
  → LVGL on device updates within 30ms
```

### 5.3 Component List

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `WebServer` | `src/web/WebServer.{h,cpp}` | AsyncWebServer 인스턴스, 라우팅, Basic Auth |
| `ImageApi` | `src/web/ImageApi.{h,cpp}` | `/api/images/*` 핸들러 + 업로드 + 핫리로드 트리거 |
| `ImageDecoder` | `src/images/images.{h,cpp}` 확장 | BMP(24/32bit) + PNG → RGB565 변환 |
| `index.html` | `data/www/index.html` | 이미지 관리 UI (PC에서 이식 + 슬림화) |
| `app.js` | `data/www/app.js` | fetch 호출, 드래그&드롭, 진행률 |
| `style.css` | `data/www/style.css` | PC와 동일 토큰 (색/폰트/spacing) |

### 5.4 Page UI Checklist

#### Image Manager Page

- [ ] Header: 제목 "RemoteDeck_Touch · Image Manager" + logout 링크
- [ ] Status bar: heap free, SPIFFS used/total, 활성 인터페이스(eth/wifi) 표시
- [ ] Card 3개: title/photo/name 각 카드에 현재 썸네일 + 크기 + 마지막 업데이트 시각
- [ ] Card 내 버튼: "Replace" → 파일 선택 다이얼로그
- [ ] Drag & Drop 영역: PNG/BMP만 허용, 파일 위로 드래그 시 강조
- [ ] 업로드 진행률 표시 (0~100%)
- [ ] 결과 토스트: 성공/실패 메시지 (3초 자동 사라짐)
- [ ] Recent activity 5건 (시각 + 이벤트 + 결과)

### 5.5 LVGL UI (변경 없음)

기존 `ui_ScreenMain`, `ui_ibtnLogo`, `ui_ImagePhoto`, `ui_ImageName` 객체는 그대로 사용. `images_update()` 함수의 내부 디코더만 교체.

---

## 6. Error Handling

### 6.1 Error Code Definition

| Code | Message | Cause | Handling |
|------|---------|-------|----------|
| 400 | "Bad request" | 파일 누락, 확장자 불일치 | 사용자에게 토스트 메시지 |
| 401 | "Unauthorized" | Basic Auth 실패 | 브라우저 인증 다이얼로그 재표시 |
| 413 | "File too large" | 200KB 초과 | 토스트 메시지 + 업로드 차단 |
| 500 | "Decode failed" | PNG/BMP 디코딩 실패 | 기존 이미지 유지, 토스트 표시, Serial 로그 |
| 507 | "SPIFFS full" | 디스크 부족 | 토스트 + 자동 정리 가이드 |

### 6.2 Error Response Format

```json
{ "ok": false, "error": { "code": 400, "message": "Invalid extension. PNG or BMP only." } }
```

### 6.3 Failure Modes & Fail-Soft

| Scenario | Behavior |
|----------|----------|
| 업로드 중 네트워크 끊김 | partial 파일 자동 삭제, 기존 이미지 유지 |
| PNG 디코딩 heap 부족 | 디코더가 실패 반환 → 기존 lv_img_dsc 유지, 단말 계속 동작 |
| SPIFFS write 실패 | upload handler가 partial 파일 삭제 + 500 응답 |
| 동일 이름 업로드 중복 | 기존 파일 백업(.bak) → 성공 시 삭제, 실패 시 복구 |
| AsyncWebServer 응답 타임아웃 | 클라이언트 재시도 가능 (idempotent) |

---

## 7. Security Considerations

- [x] **Basic Auth** — `/`와 `/api/*` 모두 보호 (PC와 동일, admin:12345 초기값)
- [x] **파일 확장자 화이트리스트** — `.png`, `.bmp`만 허용 (Content-Type도 검증)
- [x] **파일 크기 제한** — `IMAGE_MAX_BYTES` 빌드 플래그(기본 200KB)
- [x] **경로 인젝션 방지** — 업로드 파일명에서 `..`, `/`, `\` 제거 후 `basename`만 사용
- [ ] **HTTPS** — ESP32 자원 한계로 미적용 (운영망 격리 가정, v2 검토)
- [x] **Rate limiting (간이)** — 동시 업로드 1건 제한 (`_uploadInProgress` 플래그)
- [x] **로그인 자격 변경** — 초기값 admin:12345는 deviceconfig.json 에서 변경 가능 (PC와 동일)

---

## 8. Test Plan

### 8.1 Test Scope

| Type | Target | Tool | Phase |
|------|--------|------|-------|
| L1: API Tests | `/api/*` 엔드포인트 | curl + bash 스크립트 | Do |
| L2: UI Action Tests | 웹 UI (브라우저 수동) | 브라우저 수동 + Serial 검증 | Do |
| L3: E2E Scenario Tests | 단말 → 웹 업로드 → LVGL 갱신 | 수동 스톱워치 + Serial | Do |
| L4: Stability | 100회 연속 교체 | bash loop + curl | Check |

### 8.2 L1: API Test Scenarios

| # | Endpoint | Method | Test | Expected | Verification |
|---|----------|--------|------|----------|--------------|
| 1 | `/api/status` | GET | 인증 후 호출 | 200 | `heap_free > 50000`, `uptime_sec > 0` |
| 2 | `/api/status` | GET | 무인증 호출 | 401 | Basic Auth 헤더 누락 시 차단 |
| 3 | `/api/images/list` | GET | 인증 후 호출 | 200 | `.images.length >= 3` |
| 4 | `/api/images/upload` | POST | 200KB 미만 PNG 업로드 | 201 | `.ok=true`, `.reloaded=true` |
| 5 | `/api/images/upload` | POST | 300KB 파일 업로드 | 413 | size limit 적용 확인 |
| 6 | `/api/images/upload` | POST | .txt 업로드 시도 | 400 | 확장자 차단 |
| 7 | `/api/images/photo.png` | DELETE | 인증 후 호출 | 200 | fallback BMP로 복귀 |
| 8 | `/api/images/photo.png` | GET | 인증 후 호출 | 200 | Content-Type 적절 |

### 8.3 L2: UI Action Test Scenarios

| # | Page | Action | Expected Result |
|---|------|--------|----------------|
| 1 | Image Manager | 페이지 로드 | §5.4 체크리스트 모두 표시 |
| 2 | Image Manager | Card "Replace" 클릭 후 PNG 선택 | 진행률 0→100%, 성공 토스트, 단말 LCD 5초 내 갱신 |
| 3 | Image Manager | 영역에 PNG 드래그&드롭 | 강조 효과 + 업로드 시작 |
| 4 | Image Manager | 잘못된 파일(.txt) 드롭 | 400 에러 토스트 |
| 5 | Image Manager | 활성 인터페이스(eth/wifi) 표시 | Status bar에 일치 표시 |

### 8.4 L3: E2E Scenario Test Scenarios

| # | Scenario | Steps | Success Criteria |
|---|----------|-------|-----------------|
| 1 | 첫 사용 | IP 접속 → Auth → 페이지 로드 → 카드 3개 확인 | 모든 UI 요소 표시, status_bar 정확 |
| 2 | 단일 교체 | photo 카드 Replace → PNG 업로드 → LCD 확인 | LCD 단말이 5초 이내 새 이미지 표시, 재부팅 없음 |
| 3 | 다중 교체 | title+photo+name 순차 업로드 | 모두 5초 이내 반영, heap free ≥ 50KB 유지 |
| 4 | 실패 회복 | 300KB 업로드 → 413 → 정상 100KB 업로드 | 기존 이미지 유지, 두 번째 업로드 성공 |
| 5 | 회귀 | 기존 BMP 자산 그대로 동작 | title.bmp/photo.bmp/name.bmp 모두 표시 |
| 6 | MQTT 회귀 | 업로드 중 MQTT IN/OUT 메시지 수신 | room 상태 정상 토글, 업로드 영향 없음 |

### 8.5 L4: 100회 안정성

```bash
for i in $(seq 1 100); do
  curl -u admin:12345 -F "file=@photo_$((i%5)).png" http://<ip>/api/images/upload
  sleep 3
done
# 검증: heap_free 추이 (누수 0), 실패율 0
```

---

## 9. Module Architecture (Embedded — non-Clean-Arch)

> 본 프로젝트는 임베디드 펌웨어로 웹 Clean Architecture는 부분 적용. 모듈 책임만 명확히 분리한다.

### 9.1 Module Responsibilities

| Module | Responsibility | Files |
|--------|----------------|-------|
| **Web Layer** | HTTP 라우팅, 인증, 정적 파일 서빙 | `src/web/WebServer.{h,cpp}` |
| **API Layer** | 이미지 업로드/조회/삭제, 상태 보고 | `src/web/ImageApi.{h,cpp}` |
| **Decoder Layer** | BMP/PNG → RGB565 변환, 메모리 관리 | `src/images/images.{h,cpp}` (확장) |
| **LVGL Adapter** | `lv_img_dsc_t` 갱신, 화면 invalidate | `src/images/images.cpp::images_update()` |
| **Existing Modules** | MQTT/DeviceManager/ConfigManager (변경 없음) | `src/mqtt/`, `src/device/`, `src/config/` |

### 9.2 Dependency Direction

```
main.cpp ──▶ WebServer ──▶ ImageApi ──▶ images.cpp ──▶ LVGL
                │                          │
                └──▶ AuthMiddleware         └──▶ SPIFFS
```

규칙:
- `images.cpp`는 web layer를 알지 못함 (콜백으로만 트리거됨)
- `WebServer`는 LVGL을 직접 호출하지 않음 (ImageApi가 중계)
- `MQTT`/`DeviceManager`는 web layer와 독립

---

## 10. Coding Convention

### 10.1 Naming Conventions (RemoteDeck_PC 추종)

| Target | Rule | Example |
|--------|------|---------|
| 클래스 | PascalCase | `WebServer`, `ImageApi` |
| 함수 | camelCase 또는 snake_case (LVGL 호환) | `handleUpload()`, `images_update()` |
| 상수 | UPPER_SNAKE_CASE | `IMAGE_MAX_BYTES`, `WEB_PORT` |
| 파일 (클래스) | PascalCase.h/cpp | `WebServer.h`, `ImageApi.cpp` |
| 파일 (모듈) | snake_case | `images.cpp`, `lvgl_touch.cpp` |
| SPIFFS 경로 | snake_case 절대경로 | `/images/photo.png` |

### 10.2 Coding Rules

- 한글 주석 허용 (PC와 동일)
- `String` 클래스보다 `const char*` + `snprintf` 선호 (heap 관리)
- LVGL `lv_img_dsc_t.data`는 `malloc`/`free` 한 쌍을 1:1로 관리
- 모든 `_onLog` 콜백은 `(event, detail)` 시그니처 통일 (PC 패턴)

### 10.3 Build Flags

| Flag | Purpose | Default |
|------|---------|---------|
| `IMAGE_MAX_BYTES` | 업로드 크기 제한 | `204800` (200KB) |
| `WEB_PORT` | HTTP listen port | `80` |
| `WEB_AUTH_USER` | 초기 Basic Auth user | `admin` |
| `WEB_AUTH_PASS` | 초기 Basic Auth pass | `12345` |
| `LV_USE_PNG` | LVGL PNG 디코더 활성화 | `1` (lv_conf.h) |

---

## 11. Implementation Guide

### 11.1 File Structure (변경 후)

```
RemoteDeck_Touch/
├── platformio.ini             # +ESPAsyncWebServer, +AsyncTCP, +lv_png 검토
├── partitions.csv             # 검토: SPIFFS 1MB 확보 (현재 huge_app 유지 검토)
├── data/                      # SPIFFS
│   ├── deviceconfig.json
│   ├── serverconfig.json
│   ├── imagesconfig.json
│   ├── images/                # 기본 BMP fallback
│   └── www/                   # NEW: 웹 UI 자산 (PC에서 이식 + 슬림화)
│       ├── index.html
│       ├── style.css
│       └── app.js
└── src/
    ├── main.cpp               # 변경: WebServer init + ImageApi 콜백 등록
    ├── lvgl_touch.{h,cpp}     # 변경 없음
    ├── config/                # 변경 없음
    ├── device/                # 변경 없음
    ├── images/
    │   └── images.{h,cpp}     # 변경: BMP 32bit + PNG + free 추가
    ├── mqtt/                  # 변경 없음
    ├── utils/                 # 변경 없음
    └── web/                   # NEW
        ├── WebServer.{h,cpp}  # PC에서 이식 (라우팅 슬림화)
        └── ImageApi.{h,cpp}   # Touch 신설
```

### 11.2 Implementation Order

1. [ ] **M1**: `platformio.ini` + partitions 검증, lv_conf.h PNG 활성화, 빌드 확인
2. [ ] **M2**: `images.cpp` 확장 — BMP 32bit + top-down + PNG 디코더 통합 + free 명시화
3. [ ] **M3**: `downloadFile()` 재작성 — 단일 file handle, 청크 write, Content-Length 검증
4. [ ] **M4**: `src/web/WebServer.{h,cpp}` 이식 — PC에서 슬림화 (이미지/상태만 유지)
5. [ ] **M5**: `src/web/ImageApi.{h,cpp}` 신설 — upload/list/delete + 핫리로드 콜백
6. [ ] **M6**: `data/www/` 자산 — PC에서 이식 + 이미지 관리 페이지만 유지
7. [ ] **M7**: `main.cpp` 통합 — WebServer.begin() + ImageApi 콜백 등록 + 활성 인터페이스 분기
8. [ ] **M8**: L1/L2/L3 테스트 수행 + L4 100회 안정성

### 11.3 Session Guide

#### Module Map

| Module | Scope Key | Description | Estimated Turns |
|--------|-----------|-------------|:---------------:|
| M1+M2 빌드환경+디코더 | `decoder` | platformio + lv_conf + images.cpp 확장 | 25-30 |
| M3 다운로드 안정화 | `download` | downloadFile() 재작성 + 검증 | 10-15 |
| M4+M5 웹 서버 + API | `web` | WebServer/ImageApi 이식·신설 | 35-45 |
| M6 정적 자산 | `ui` | data/www/ 작성 + 디자인 토큰 | 15-20 |
| M7+M8 통합·테스트 | `integration` | main.cpp 통합 + L1~L4 테스트 | 25-35 |

#### Recommended Session Plan

| Session | Phase | Scope | Turns |
|---------|-------|-------|:-----:|
| Session 1 | Plan + Design | 전체 | 30 (완료) |
| Session 2 | Do | `--scope decoder,download` | 35-45 |
| Session 3 | Do | `--scope web,ui` | 50-65 |
| Session 4 | Do + Check | `--scope integration` | 25-35 |
| Session 5 | Report + Archive | 전체 | 15-20 |

---

## 12. Risk-Specific Designs

### 12.1 PNG Heap 부족 대응

- 최대 디코딩 크기 320×240 = 76,800 px × 2 byte = **153.6KB** RAM 필요
- ESP32 기본 320KB 중 LVGL/Async/SPIFFS 사용량 고려 시 **여유 ≤ 100KB** 우려
- 완화책:
  - PSRAM이 활성화된 보드면 PSRAM 우선 할당 (`heap_caps_malloc(.., MALLOC_CAP_SPIRAM)`)
  - 디코드 직전 `ESP.getFreeHeap()` 체크, 30KB 미만이면 즉시 실패 + 기존 dsc 유지
  - lv_png 사용 시 line-by-line decode 모드 검증

### 12.2 AsyncWebServer ↔ LVGL 동시성

- LVGL은 메인 루프에서 `lv_timer_handler()` 호출, AsyncWebServer는 별도 task
- `images_update()` 호출은 main loop context로 옮김 — ImageApi가 flag(`_pendingReload`)만 set, main loop가 polling 후 호출
- 이미지 디코딩(`malloc/free`)을 web task에서 수행하면 LVGL과 race 가능 — main loop로 옮긴 후 `lv_img_set_src()`

### 12.3 듀얼 네트워크 바인딩

**v1 현재 한계 (2026-06-22 실측 발견)**:
- `arduino-libraries/Ethernet` 은 W5500 을 별도 SPI 드라이버로 다룸 (lwIP 미경유)
- `AsyncTCP` 는 ESP32 lwIP 위에서 동작 → W5500 IP 에서 listen 시 `tcpip_api_call: Invalid mbox` 크래시
- **v1 우회**: `if (wifi_conn) webServer.begin(...)` — Ethernet 환경에서 WebUI 비활성
- 펌웨어 boot 시 Serial 로그: `Web UI disabled: requires WiFi (W5500 + AsyncTCP incompatible — see v2.1 LAN unification)`

**v2.1 해결 계획 — LAN 스택 통일**:
- arduino-libraries/Ethernet 제거 → `<ETH.h>` + `ETH.begin(ETH_PHY_W5500, ...)` (PC v2.3.0 NetManager 패턴)
- W5500 을 ESP32 lwIP 의 Ethernet PHY 로 등록 → AsyncWebServer 가 Ethernet 환경에서도 정상 동작
- 의존: platform 을 pioarduino 53.x (Arduino-ESP32 3.x) 로 upgrade, ArduinoHttpClient → HTTPClient, TFT_eSPI 업데이트 + LCD/터치 회귀 검증

### 12.4 v2.1 백로그 (Plan §9 v2.1 백로그와 일치)

| 항목 | 우선순위 | 의존성 |
|------|:--:|--------|
| LAN 스택 통일 (ETH.h + ETH_PHY_W5500) | High | platform upgrade, TFT_eSPI update |
| PNG 디코더 활성화 (lv_png_init + LVGL FS) | Medium | LV_USE_FS_STDIO 또는 custom FS driver |
| OTA Handler 이식 | Medium | LAN 통일 완료 후 |
| 로그 뷰어 + WebSocket | Low | OTA 이후 |
| deviceconfig/serverconfig 웹 편집 | Low | UI 확장 |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-22 | 초안 작성 — Option C 채택, 8-모듈 구현 가이드 + L1~L4 테스트 플랜 | KDI |
