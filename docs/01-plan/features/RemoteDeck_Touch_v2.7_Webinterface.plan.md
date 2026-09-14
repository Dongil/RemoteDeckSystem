---
template: plan
version: 0.1
feature: RemoteDeck_Touch_v2.7_Webinterface
date: 2026-09-14
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware web UI)
branch: v2.6-touch-webmode (이어서 → 완료 후 main 병합)
base: v2.6 (웹 설정 모드)
status: plan
---

# RemoteDeck_Touch v2.7 Webinterface Planning Document

> **Summary**: v2.6에서 되살린 Touch 웹 UI를 RemoteDeck_PC 웹 UI 스타일로 리뉴얼 — JSON 통짜 편집을 항목별 폼으로, 관리/상태 메뉴 정비, 전체 일관성 확보.
>
> **Project**: RemoteDeckSystem · **Author**: KDI · **Date**: 2026-09-14 · **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | Touch 웹 UI가 PC와 불일치 — Config가 raw JSON textarea 통짜 편집, 재부팅·OTA가 Config 탭에 산재, 홈/상태·관리 메뉴 부재. 사용성·일관성 낮음. |
| **Solution** | `data/www/{index,app.js,style.css}` 재구성으로 PC 스타일 5탭(상태/제어/설정/관리/로그) + 설정 sub-tab(Device Config / Server Config / 이미지 관리) 항목별 폼 + 관리(재부팅·스케줄·OTA). `embed_www.py`로 임베드, OTA(app)로 배포. |
| **Function/UX Effect** | 브라우저에서 설정을 **항목별로 바로 편집**, 기기관리·재부팅 스케줄·OTA를 PC와 동일 UX로. 이미지 관리(표시·교체)는 유지. |
| **Core Value** | PC/Touch 웹 UX 통일, JSON 수기 편집 제거. 기존 기기는 OTA 업그레이드만으로 신 UI + 설정 보존(하위호환). |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | Touch 웹 UI가 PC와 달라(설정=JSON 통짜) 사용성·일관성 낮음 |
| **WHO** | 관리자(웹 설정 모드에서 브라우저로 설정·OTA·기기관리) |
| **RISK** | ① 재부팅 스케줄=신규 firmware 기능(범위↑) ② 하위호환(구 config 파싱) ③ embed_www.py 재생성 워크플로 ④ flash 크기 |
| **SUCCESS** | PC 스타일 6탭·항목별 편집·저장 정상 + 기존 기기 OTA 후 설정 보존·정상 + 재부팅 스케줄 동작 (전부 실기) |
| **SCOPE** | S1 탭 골격(5탭)+상태/제어/로그 → S2 설정 Device/Server 폼(+serverconfig API) → S3 관리(재부팅+OTA 이동) → S4 재부팅 스케줄(신규) → S5 이미지 관리를 설정 하위로+embed+검증 |

---

## 1. Overview

### 1.1 Purpose
v2.6 웹 설정 모드에서 제공되는 Touch 웹 인터페이스를 RemoteDeck_PC 웹 인터페이스와 동일한 구조·룩앤필로 리뉴얼하여, 설정을 항목별로 직접 편집하고 기기관리/OTA를 일관된 UX로 제공한다.

### 1.2 Background
- v2.6에서 부팅 경계 SPI 배타로 Touch WebUI를 부활(matchRate 100%). 그러나 Config가 raw JSON textarea라 현장 편집이 불편.
- PC는 이미 항목별 폼 + 6탭(홈/제어/스케줄/설정/관리/로그) + 세분 sub-tab 구조로 성숙. Touch를 여기에 맞춰 통일.
- **하위호환 필수**: 필드 14대 등 기존 배포 기기가 OTA 업그레이드 후에도 기존 설정으로 정상 동작해야 함.

### 1.3 Related Documents
- [[RemoteDeck_Touch_v2.6]] (웹 설정 모드 부활) — 본 작업의 기반
- 참고: `RemoteDeck_PC/data/www/{index.html,app.js,style.css}` (목표 스타일)

---

## 2. Scope

### 2.1 In Scope
- [ ] 5탭 재구성(한글): **상태 / 제어 / 설정 / 관리 / 로그**, PC 스타일 네비/카드/색상
- [ ] 상태 탭 신설 (`/api/status`: fw·heap·network·attendance 등)
- [ ] 설정 = **Device Config / Server Config / 이미지 관리** sub-tab, **항목별 폼**(현재 JSON textarea 대체)
- [ ] Server Config용 신규 API `GET/POST /api/serverconfig` (추가 — 하위호환)
- [ ] 설정 저장 = 클라이언트에서 필드→객체 조립 후 통짜 POST (PC 패턴, `/api/config` 계약 유지)
- [ ] **Image Config는 설정에서 제외**
- [ ] 관리 탭: 기기관리(재부팅) + 펌웨어 OTA(Config에서 이동) + **재부팅 스케줄(신규)**
- [ ] 이미지 관리를 **설정 하위 sub-tab**으로 이동 (현재 이미지 표시 + 교체/삭제, `/api/images/*` 재활용)
- [ ] 제어/로그 탭 PC 스타일로 정리
- [ ] `data/www` 편집 → `tools/embed_www.py` 재생성 → `embedded_assets.cpp` 워크플로

### 2.2 Out of Scope
- 전원(릴레이) 스케줄 등 PC의 relay schedule — Touch에 릴레이 없음 (재부팅 스케줄만)
- 웹UI/FS 별도 업데이트(PC의 uploadFS) — Touch 웹UI는 flash 임베드라 firmware OTA로 일괄 배포
- PNG 디코더 재활성 — BMP 유지 (v2.6 결정)
- LCD(TFT) UI 변경 — 본 작업은 웹 UI 한정
- config JSON **스키마 파괴적 변경** — 하위호환 위해 금지 (필드 추가는 default 처리)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | 5탭 네비게이션(상태/제어/설정/관리/로그), 한글, PC 스타일 | High | Pending |
| FR-02 | 상태 탭 — `/api/status` 표시(fw/heap/network/attendance 등) | Medium | Pending |
| FR-03 | 설정 > Device Config 항목별 폼 (deviceID·network·serverURL·sleep/reboot 등) | High | Pending |
| FR-04 | 설정 > Server Config 항목별 폼 (MQTT·연동URL 등) + `GET/POST /api/serverconfig` 신규 | High | Pending |
| FR-05 | 설정 저장 = 필드→객체 조립 후 통짜 POST (config API 계약 유지) | High | Pending |
| FR-06 | Image **Config**(imagesconfig 읽기전용)는 UI에서 제외 — 단 이미지 **관리**(파일 교체)는 설정 하위 sub-tab으로 제공 | High | Pending |
| FR-07 | 관리 > 기기관리(재부팅 버튼) | High | Pending |
| FR-08 | 관리 > 재부팅 스케줄(요일/시간) **신규** — 백엔드 저장+실행(LCD 모드에서 동작) | High | Pending |
| FR-09 | 관리 > 펌웨어 OTA (기존 재활용, Config→관리로 이동) | High | Pending |
| FR-10 | 이미지 관리 — **설정 하위 sub-tab**으로 이동, 현재 이미지 표시 + 교체/삭제 | High | Pending |
| FR-11 | 제어/로그 탭 PC 스타일 정리 (기능 유지) | Medium | Pending |
| FR-12 | **하위호환** — 기존 기기 OTA 업그레이드 후 구 config 파싱·정상 동작 | High | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement |
|----------|----------|-------------|
| 호환성 | 구 deviceconfig/serverconfig.json 무변경 파싱, 신규 필드 default 처리 | 기존 config로 OTA 후 실기 |
| 리소스 | 웹모드 heap 여유(TFT off), embed 후 flash 여유 확인 | build size / 부팅 heap 로그 |
| 아키텍처 | 웹 설정 모드 SPI 배타 유지(서비스 중 TFT 무접근) | v24_poc.py 재확인 |
| 일관성 | PC 디자인 토큰/컴포넌트 재사용 | 육안 + PC와 대조 |

---

## 4. Success Criteria

### 4.1 Definition of Done
- [ ] 5탭(한글) PC 스타일 렌더, 설정 항목별 편집·저장 실기 정상
- [ ] 기존 기기 config로 OTA 업그레이드 후 설정 보존·정상 동작 (FR-12)
- [ ] 재부팅 스케줄 신규 동작 실기 확인 (FR-08)
- [ ] OTA·이미지 관리 정상, 웹모드 부하 PoC 재통과
- [ ] `embed_www.py` 재생성 워크플로 확립·문서화

### 4.2 Quality Criteria
- [ ] `pio run` 빌드 성공 (경고 증가 없음)
- [ ] 웹 UI 로딩/저장 시 SPI 경합 없음(웹모드 배타 유지)
- [ ] matchRate ≥ 90%

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| 재부팅 스케줄=신규 firmware 기능(범위·복잡도↑) | High | High | **별도 모듈(ScheduleManager 유사)로 분리**, S4 단독 단계. 최소 형태(요일+시각 1개)부터. 여의치 않으면 기존 rebootTime(시간) 확장 수준으로 축소 옵션 |
| 하위호환 파손(구 config) | High | Medium | JsonUtils 필드 default(`| ...`) 유지, 스키마 파괴 금지. 구 config 파일로 OTA 후 실기 검증(FR-12) |
| embed_www.py 재생성 누락 → gzip↔소스 불일치 | Medium | Medium | 빌드 전 반드시 재생성, 절차 문서화. 소스(data/www)만 편집 |
| flash 크기 증가(임베드 UI 확대) | Medium | Low | build size 모니터, gzip 유지. 필요 시 CSS/JS 정리 |
| serverconfig API 신규 오류 | Medium | Low | 기존 ConfigApi 패턴 그대로(atomic write), read-only부터 |

---

## 6. Impact Analysis

### 6.1 Changed Resources
| Resource | Type | Change |
|----------|------|--------|
| `data/www/{index.html,app.js,style.css}` | Web source | 전면 재구성(6탭·폼·관리) |
| `src/web/embedded_assets.cpp` | Generated | embed_www.py로 재생성 |
| `src/web/ConfigApi.{h,cpp}` | API | serverconfig GET/POST 추가 |
| `src/web/WebServer.cpp` | Route | `/api/serverconfig`, `/api/schedule`(신규) 라우트 |
| ScheduleManager(신규 or 확장) | Control | 재부팅 스케줄 저장+실행 |
| `src/main.cpp` | Wiring | 스케줄 실행 루프 연동(LCD 모드) |
| `deviceconfig.json`/`serverconfig.json` | Schema | 필드 추가 시 하위호환(default) |

### 6.2 Current Consumers
| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| `/api/config` | READ/WRITE | Touch 웹 UI(Config 탭) | UI 재작성(항목별) — 계약 유지 |
| `serverconfig.json` | READ | main.cpp(MQTT/HTTP), ConfigManager | 신규 쓰기 경로 추가 시 검증 |
| `embedded_assets.cpp` | 서빙 | WebServer handleRoot/style/app | 재생성분으로 교체 |
| `rebootTime`(기존) | 재부팅 | 기존 로직 | 스케줄로 확장/공존 검토 |

### 6.3 Verification
- [ ] 구 config 파일로 OTA 후 전 소비자(MQTT/HTTP/이미지/웹) 정상
- [ ] `/api/config` 계약 불변(기존 파서 회귀 없음)
- [ ] 웹모드 SPI 배타 유지(PoC)

---

## 7. Architecture Considerations

### 7.1 프로젝트 성격
임베디드 펌웨어(ESP32/LVGL) + flash 임베드 웹(esp_http_server). 웹 UI는 `data/www` 소스 → `embed_www.py` → PROGMEM. PC는 SPIFFS 직접 서빙(외부 css/js), Touch는 인라인 gzip — **아키텍처 차이 유지**(Touch는 인라인 방식 그대로).

### 7.2 주요 결정
| 결정 | 선택 | 근거 |
|------|------|------|
| 설정 편집 방식 | 항목별 폼 → 클라 조립 → 통짜 POST | PC 패턴 동일, `/api/config` 계약 유지(하위호환) |
| Server config 노출 | 신규 `GET/POST /api/serverconfig` | 현재 미노출 → 추가(기존 기기 무영향) |
| 재부팅 스케줄 | 신규 모듈 + `/api/schedule` | PC 관리 탭 형태. **범위 최대 항목** — S4 분리 |
| 웹 자산 빌드 | data/www 편집 + embed_www.py 재생성 | gzip 직접 편집 금지 |
| 라벨 | 한글 | PC 일관성(브라우저 렌더라 폰트 무관) |

### 7.3 세션 계획 (design에서 확정)
- **S1**: 5탭 골격(한글 네비·카드·PC 토큰) + 상태 탭(/api/status) + 제어/로그 이관·정리
- **S2**: 설정 Device/Server 항목별 폼 + `/api/serverconfig` 신규(GET→POST) + 저장 조립
- **S3**: 관리 탭 — 기기관리(재부팅) + OTA 이동
- **S4**: 재부팅 스케줄(신규 모듈 + `/api/schedule` + main 루프 연동) ← 최대 리스크, 단독
- **S5**: 이미지 관리를 설정 하위 sub-tab으로 + embed_www.py 재생성 + 빌드/실기 검증 + 하위호환(구 config OTA) 검증

---

## 8. Convention Prerequisites
- 커밋: `feat/docs(RemoteDeck_Touch_v2.7): 요약` (한글)
- 웹 자산: **반드시 data/www 편집 → embed_www.py 재생성** (embedded_assets.cpp 직접 수정 금지)
- API: 기존 ConfigApi atomic write 패턴 준수, 스키마 파괴 금지
- 검증: build 0-warning 유지, 실기(웹 렌더+저장+OTA+스케줄) + 하위호환

---

## 9. Next Steps
1. [ ] `/pdca design RemoteDeck_Touch_v2.7_Webinterface` — 3 architecture option 중 택1, 탭/폼/스케줄 상세 설계
2. [ ] design에서 재부팅 스케줄 backend 상세(스키마·실행) 확정
3. [ ] 구현(S1~S5) → analyze → report → archive

---

## Version History
| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-09-14 | Initial draft — 6탭 리뉴얼, 설정 항목별 폼, 관리(+재부팅 스케줄), 하위호환 | KDI |
