---
template: plan
version: 1.3
feature: RemoteDeck_Touch
date: 2026-06-22
author: KDI
project: RemoteDeckSystem
firmware_version: TBD (현재 main.cpp 주석 기준 2025.02.20 빌드)
---

# RemoteDeck_Touch 리팩토링 Planning Document

> **Summary**: ESP32 + 240×320 LCD/터치 출퇴근 단말의 이미지 교체 안정성 문제 해결과, RemoteDeck_PC와 유사한 웹 인터페이스(이미지 관리 우선) 추가 리팩토링.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-06-22
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | BMP 이미지 교체가 안정적으로 동작하지 않음 (파일 close/reopen 패턴, 24bit BMP 한정, 재부팅 강제). RemoteDeck_PC 대비 웹 UI 부재로 현장에서 이미지 교체가 어려움. |
| **Solution** | (1) 이미지 다운로드 경로 단일 스트림 write로 재작성 + PNG 디코더 추가, (2) RemoteDeck_PC의 AsyncWebServer 패턴을 이식해 이미지 업로드/미리보기/즉시 교체 웹 UI 구축, (3) `images_update()` 핫리로드로 재부팅 제거. |
| **Function/UX Effect** | 운영자가 웹 브라우저에서 PNG/BMP를 드래그&드롭으로 올리면 단말이 재부팅 없이 즉시 새 이미지로 갱신. 24bit 외 32bit BMP, PNG도 지원. 다운로드 실패율과 깨진 이미지 표시 감소. |
| **Core Value** | 현장 출장 없이 디스플레이 콘텐츠를 원격/즉시 변경 가능 — 운영 비용/응답 시간 절감. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | BMP 교체 불안정 + 재부팅 강제 + 웹 UI 부재로 인한 운영 부담 해소 |
| **WHO** | RemoteDeck_Touch 단말 운영자 (이미지 콘텐츠 교체 담당), 현장 유지보수 인력 |
| **RISK** | ESP32 메모리 한계 — PNG 디코딩 시 heap 부족, AsyncWebServer 추가로 RAM 압박, 듀얼 네트워크(W5500+WiFi) 동시 사용 시 자원 경합 |
| **SUCCESS** | (1) 이미지 교체 성공률 100%, (2) 재부팅 없이 5초 이내 화면 갱신, (3) RemoteDeck_PC 웹 UI 디자인 토큰 재사용, (4) heap free ≥ 50KB 유지 |
| **SCOPE** | Phase 1: 이미지 파이프라인 안정화 / Phase 2: 웹 UI 이식(이미지 관리) / Phase 3: 핫리로드 + 통합 검증 |

---

## 1. Overview

### 1.1 Purpose

RemoteDeck_Touch는 사무실/출입공간에 설치된 ESP32 기반 출퇴근(IN/OUT) 표시 단말이다. 콘텐츠 BMP(title/photo/name)는 서버 URL에서 다운로드해 SPIFFS에 저장하고 LVGL로 표시한다. 현재 이미지 교체 절차가 다음과 같이 불안정하다:

- `downloadFile()`이 2바이트(writeCounter%2)마다 `file.close()` 후 `FILE_APPEND` 재오픈 — SPIFFS 트랜잭션 비용 + 무결성 위험.
- BMP 파서가 24bit BGR888만 지원, 32bit/top-down/압축 BMP 처리 불가.
- 이미지 교체 후 `ESP.restart()`로 강제 재부팅 — 운영자 UX 저하, 네트워크 재연결 부담.
- 웹 UI가 없어 운영자가 서버에 직접 BMP를 업로드해야 함(외부 인프라 의존).

이 리팩토링은 (1) 이미지 다운로드/디코딩 파이프라인 안정화, (2) RemoteDeck_PC와 같은 단말 자체 웹 UI 추가(이미지 관리 우선), (3) 재부팅 없는 핫리로드를 달성한다.

### 1.2 Background

- RemoteDeck_PC가 v2.3.0에서 AsyncWebServer + OTA + 웹 로그 + 설정 UI 체계로 안정화됨 (commit `cab6764`~`dee7b43`).
- RemoteDeck_Touch는 동일 시스템군이나 별도 펌웨어로 운영 중이며 웹 UI가 비활성(`main.cpp:14, 38-39`에서 주석 처리).
- 현장 보고: "이미지 교체가 원활하지 않음" — 가장 빈번한 운영 이슈.

### 1.3 Related Documents

- 참조 PC 펌웨어: `RemoteDeck_PC/src/web/`, `RemoteDeck_PC/data/` (AsyncWebServer 구성)
- 참조 Release Notes: `docs/RemoteDeck_PC_v2.3.0_ReleaseNotes.md`
- 현재 Touch 코드 진입점: `RemoteDeck_Touch/src/main.cpp`, `images/images.cpp`
- 다음 단계 Design 문서: `docs/02-design/features/RemoteDeck_Touch.design.md` (작성 예정)

---

## 2. Scope

### 2.1 In Scope

- [ ] 이미지 다운로드 함수(`downloadFile`) 안정화 — close/reopen 제거, 단일 스트림 write, Content-Length 검증
- [ ] BMP 파서 확장 — 24bit + 32bit 지원, top-down/bottom-up 양방향 처리, 메모리 안전성 강화
- [ ] PNG 디코더 추가 — LVGL `lv_png` 또는 PNGdec 라이브러리 통합, 디코드 결과 캐싱
- [ ] 단말 자체 AsyncWebServer 구축 — `/`(이미지 관리 UI), `/api/images/upload`, `/api/images/list`, `/api/images/delete`
- [ ] 이미지 업로드 후 `images_update()` 즉시 호출 — 재부팅 없이 LVGL 화면 갱신
- [ ] Basic Auth 인증 — RemoteDeck_PC와 동일 정책 (admin:12345 초기 비밀번호)
- [ ] heap/SPIFFS 사용량 모니터링 엔드포인트 — `/api/status`
- [ ] RemoteDeck_PC 웹 UI HTML/CSS 디자인 재사용 — 일관된 운영 경험

### 2.2 Out of Scope

- OTA 펌웨어 업데이트 (v2 로 연기)
- 웹 기반 로그 뷰어 (v2 로 연기)
- deviceconfig/serverconfig 웹 편집 (v2 로 연기 — 기존 URL 다운로드 유지)
- MQTT/HTTP 비즈니스 로직 변경 (재실(IN/OUT), 시간 동기, 자동 재부팅 등 기존 동작 유지)
- 새로운 LCD 드라이버, 새로운 터치 컨트롤러
- 하드웨어 변경 (W5500 + WiFi 듀얼 구성 그대로 유지)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | 단말 IP로 웹 브라우저 접속 시 이미지 관리 UI를 표시 | High | Pending |
| FR-02 | 사용자가 PNG/BMP 파일을 업로드하면 SPIFFS `/images/`에 저장 | High | Pending |
| FR-03 | 업로드 완료 직후 LVGL 화면이 재부팅 없이 새 이미지로 갱신 | High | Pending |
| FR-04 | BMP 파서가 24bit/32bit, top-down/bottom-up 모두 처리 | High | Pending |
| FR-05 | PNG(8bit 알파 포함)를 RGB565로 디코딩하여 표시 | High | Pending |
| FR-06 | `downloadFile()` 재작성 — 단일 file handle, 청크 단위 write, Content-Length 검증 | High | Pending |
| FR-07 | 웹 UI에서 현재 이미지 목록과 크기/수정시각을 표시 | Medium | Pending |
| FR-08 | 웹 UI에서 이미지 삭제 가능 (fallback `/images/`만 남도록) | Medium | Pending |
| FR-09 | Basic Auth로 모든 `/api/*` 보호 | Medium | Pending |
| FR-10 | `/api/status` — heap/SPIFFS/uptime/network 상태 JSON 반환 | Medium | Pending |
| FR-11 | 업로드 실패 시 사용자에게 에러 메시지 + 단말 재부팅 없음 | High | Pending |
| FR-12 | Ethernet 또는 WiFi 중 활성화된 인터페이스에서 모두 웹 UI 접근 가능 | Medium | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| Performance | 이미지 업로드 후 화면 갱신까지 ≤ 5초 (240×320 PNG ≤ 100KB) | 스톱워치 + Serial 로그 타임스탬프 |
| Memory | heap free ≥ 50KB (idle), PNG 디코딩 피크 시 ≥ 20KB | `ESP.getFreeHeap()` 모니터링 |
| Stability | 100회 연속 이미지 교체 시 실패 0건 | 자동화 스크립트 + Serial 검증 |
| Compatibility | 기존 BMP 자산 그대로 동작 (회귀 없음) | 기존 BMP 3종 표시 검증 |
| Security | `/api/*` 모두 Basic Auth, HTTP만 지원(HTTPS 미적용) | curl 무인증 호출 → 401 확인 |
| Usability | 브라우저 드래그&드롭 업로드, 진행률 표시 | 수동 UX 점검 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-12 모두 구현 및 Serial/웹 로그로 검증
- [ ] 단말에 펌웨어 업로드 후 PNG/BMP 각각 5회 이상 교체 성공 (100/100 자동 테스트로 확장)
- [ ] RemoteDeck_PC 웹 UI 와 디자인 일관성 (CSS, 헤더, 인증 흐름)
- [ ] `docs/02-design/features/RemoteDeck_Touch.design.md` 작성 완료
- [ ] `docs/03-analysis/RemoteDeck_Touch.analysis.md` Match Rate ≥ 90%
- [ ] 현장 1대 시범 배포 후 운영자 피드백 수렴

### 4.2 Quality Criteria

- [ ] 빌드 경고 0건 (PlatformIO `-Wall`)
- [ ] heap 사용량 회귀 없음 (현 펌웨어 대비 ±20KB 이내)
- [ ] 이미지 교체 후 LVGL 메모리 누수 0 (`lv_mem_monitor` 출력 검증)
- [ ] 100회 반복 업로드 + 재시작 안정성 테스트 통과

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| PNG 디코딩이 heap 부족으로 실패 | High | Medium | PSRAM 확인 후 사용, 디코드 결과를 LVGL `lv_img_dsc_t`로 캐싱 + 사용 후 free, 최대 이미지 크기 제한(예: 320×240 이하) |
| AsyncWebServer 추가로 RAM 압박 | High | Medium | 라이브러리는 ESPAsyncWebServer (PC에서 검증됨) 재사용, 업로드 버퍼는 청크 단위 (≤ 1024 bytes), 동시 연결 제한 |
| 듀얼 네트워크(W5500+WiFi)에서 웹 서버 바인딩 충돌 | Medium | Medium | RemoteDeck_PC v2.3 코드 참조 — 활성 인터페이스의 `IPAddress`로만 listen, 둘 다 활성 시 Ethernet 우선 |
| SPIFFS 용량 부족 (partitions.csv에서 spiffs 영역) | Medium | Low | 현재 huge_app 사용 중 → spiffs 별도 파티션 확보 필요, 1MB 이상 권장. Plan/Design 단계에서 partition 변경 결정 |
| LVGL 핫리로드 시 이전 `lv_img_dsc_t` 데이터 해제 누락 → 메모리 누수 | High | High | `images_update()` 재호출 전에 기존 `img_photo.data` 등 free, malloc 이력 추적 헬퍼 도입 |
| RemoteDeck_PC 코드 재사용 시 동일 변수/매크로 충돌 | Low | Medium | 코드 복사 대신 namespace/prefix 적용, 공통 부분만 lib/ 로 분리 검토 |
| Basic Auth 초기 비밀번호 (admin:12345) 노출 | Medium | High | 운영 가이드 문서에 변경 절차 명시, 초기 접속 시 변경 권장 메시지 표시 (v2 추진) |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `RemoteDeck_Touch/src/main.cpp` | Entry point | 웹서버 init/loop 추가, `images_update()` 호출 지점 추가 |
| `RemoteDeck_Touch/src/images/images.cpp` | Image decoder | BMP 32bit 지원, PNG 디코더 추가, 핫리로드 시 free 처리 |
| `RemoteDeck_Touch/platformio.ini` | Build config | ESPAsyncWebServer + AsyncTCP + PNG 라이브러리 추가, partitions 재검토 |
| `RemoteDeck_Touch/partitions.csv` | Partition layout | SPIFFS 영역 확보 (현재 huge_app.csv → custom으로 변경 가능성) |
| `RemoteDeck_Touch/data/` | SPIFFS contents | 웹 UI HTML/CSS/JS 자산 추가 (RemoteDeck_PC `data/`에서 복사 변형) |
| 기존 `downloadFile()` | HTTP downloader | 재작성 — close/reopen 제거 |
| 기존 `read_bmp_from_spiffs()` | BMP parser | 32bit 지원, top-down 처리 안정화 |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| `images_update()` | CALL | `main.cpp:119` (setup), `fetchImageFiles()` (재부팅 전) | 변경: setup() 외에 웹 업로드 콜백에서도 호출 |
| `downloadFile()` | CALL | `main.cpp` fetchServerInfo/fetchImageFiles 3회 | 변경: 단일 file handle 패턴으로 시그니처 유지 |
| `ConfigManager::loadImagesConfig()` | CALL | setup() + fetchImageFiles() | 영향 없음 (호출 그대로) |
| `lv_img_set_src()` | CALL | images.cpp 6곳 | 변경: 호출 전 기존 데이터 free |
| `ui_ibtnLogo`, `ui_ImagePhoto`, `ui_ImageName` (LVGL) | UPDATE | images.cpp | 영향 없음 |
| MQTT IN/OUT 토픽 처리 | RECEIVE | `mqtt_ReceivedCallback` | 영향 없음 (격리 유지) |
| `ESP.restart()` | CALL | fetchImageFiles 끝, ethernetInfo_Changed 등 | 변경: 이미지 교체 경로에서만 제거, 설정 변경 경로는 유지 |

### 6.3 Verification

- [ ] 기존 BMP 3종 (title/photo/name) 표시 회귀 없음
- [ ] MQTT IN/OUT 메시지 처리 변경 없음 (격리 확인)
- [ ] 자동 재부팅(`rebootTime`) 로직 영향 없음
- [ ] Ethernet/WiFi 단일 활성화 시 모두 웹 UI 접근 가능
- [ ] 듀얼 활성화 시 충돌 없음 (Ethernet 우선)

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

| Level | Characteristics | Recommended For | Selected |
|-------|-----------------|-----------------|:--------:|
| **Embedded (Arduino/PlatformIO)** | ESP32 + LVGL, SPIFFS, FreeRTOS, AsyncWebServer | IoT 단말 펌웨어 | ✅ |
| Starter/Dynamic/Enterprise (웹) | N/A | — | ☐ |

> bkit의 기본 3-Level(Starter/Dynamic/Enterprise)은 웹 프로젝트 전제이며, 본 프로젝트는 임베디드 펌웨어이므로 Embedded 컨텍스트로 진행.

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| Web Server | WebServer (sync) / ESPAsyncWebServer | **ESPAsyncWebServer** | RemoteDeck_PC v2.3에서 검증됨, 비동기 업로드 핸들러 지원 |
| Image Decode | BMP only / +PNG (lv_png) / +PNG (PNGdec) | **lv_png 우선, fallback PNGdec** | LVGL 통합 용이, RAM 효율 검증 후 PNGdec 대체 가능 |
| Image Hot Reload | 재부팅 / `images_update()` 재호출 | **images_update() 재호출** | UX 우선 + 기존 LVGL 객체 재사용 가능, free 누수만 주의 |
| Network Priority | WiFi / Ethernet / Dual | **Dual (Ethernet 우선)** | 현재 동작 유지, 듀얼 환경에서 Ethernet 안정성 우선 |
| Auth | None / Basic Auth / JWT | **Basic Auth** | RemoteDeck_PC와 동일 정책, ESP32 자원 부담 최소 |
| Config Web Edit | Yes / No | **No (v2)** | 스코프 좁히기, 기존 URL 다운로드 유지 |
| Partition Layout | huge_app.csv / custom (SPIFFS 확장) | **custom (검토 후 결정)** | 웹 UI 자산 + 이미지 캐싱 위해 SPIFFS 확장 필요 |

### 7.3 Folder Structure Preview

```
RemoteDeck_Touch/
├── platformio.ini           # +ESPAsyncWebServer, +PNG, +partitions
├── partitions.csv           # NEW (또는 huge_app 재검토)
├── data/                    # SPIFFS 자산 (현재 deviceconfig/serverconfig/imagesconfig + 기본 BMP)
│   ├── deviceconfig.json
│   ├── serverconfig.json
│   ├── imagesconfig.json
│   ├── images/              # 기본 BMP fallback
│   └── www/                 # NEW: 웹 UI 자산 (PC에서 이식)
│       ├── index.html
│       ├── style.css
│       └── app.js
└── src/
    ├── main.cpp             # 변경: 웹서버 init/loop, 핫리로드 콜백
    ├── lvgl_touch.{h,cpp}   # 그대로
    ├── config/              # 그대로
    ├── device/              # 그대로
    ├── images/
    │   ├── images.{h,cpp}   # 변경: BMP 32bit + PNG + free 추가
    │   └── ImageDecoder.{h,cpp}  # NEW (선택): 디코더 추상화
    ├── mqtt/                # 그대로
    ├── utils/               # 그대로
    └── web/                 # NEW
        ├── WebServerHandler.{h,cpp}
        ├── ImageApi.{h,cpp}
        └── AuthMiddleware.{h,cpp}
```

---

## 8. Convention Prerequisites

### 8.1 Existing Project Conventions

- [x] `CLAUDE.md` 프로젝트 루트에 존재 (untracked)
- [x] PlatformIO `platformio.ini` 컨벤션 (PC/Touch 양쪽 동일)
- [ ] `docs/01-plan/conventions.md` (미존재)
- [x] RemoteDeck_PC 코드 스타일 — 한글 주석 허용, snake_case 함수명 + camelCase 변수 혼용

### 8.2 Conventions to Define/Verify

| Category | Current State | To Define | Priority |
|----------|---------------|-----------|:--------:|
| **Naming** | RemoteDeck_PC 스타일 추종 | 신규 web/ 모듈도 동일 스타일 적용 | High |
| **Folder structure** | src/{config,device,images,mqtt,utils} | + src/web/ 추가 | High |
| **Error handling** | Serial.println 위주 | `_onLog` 콜백 패턴(PC v2.3) 도입 검토 | Medium |
| **HTTP API** | 없음 | RemoteDeck_PC `/api/*` 네이밍 재사용 (`/api/images`, `/api/status`) | High |
| **Auth** | 없음 | RemoteDeck_PC와 동일 Basic Auth (admin:12345) | High |

### 8.3 Build/Runtime Variables

| Variable | Purpose | Scope | To Be Created |
|----------|---------|-------|:-------------:|
| `WEB_AUTH_USER` / `WEB_AUTH_PASS` | Basic Auth 자격 | Runtime (deviceconfig.json) | ☐ |
| `WEB_PORT` | HTTP listen port (기본 80) | Build flag | ☐ |
| `IMAGE_MAX_BYTES` | 업로드 크기 제한 | Build flag | ☐ |

---

## 9. Next Steps

1. [ ] `/pdca design RemoteDeck_Touch` 로 Design 문서 작성 — 3가지 아키텍처 옵션 비교 후 선택
2. [ ] partition layout 검증 (`huge_app.csv` 유지 가능 여부 vs custom partition)
3. [ ] lv_png 빌드 가능성 검증 (lv_conf.h 설정)
4. [ ] RemoteDeck_PC `src/web/` 와 `data/www/` 파일 목록 추출 → 재사용 범위 확정
5. [ ] 구현은 모듈 단위 세션 분할 (Image pipeline → Web server → Hot reload → 검증)
6. [ ] 펌웨어 1차 빌드 후 단말 1대 시범 배포 → 100회 교체 회귀 테스트

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-22 | 초안 작성 — 이미지 파이프라인 리팩토링 + 웹 UI(이미지 관리 우선) 도입 | KDI |
