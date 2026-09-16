---
template: plan
version: 0.1
feature: RemoteDeck_Touch_v2.8_ServerConfigFetch
date: 2026-09-15
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: main (v2.7 병합 후 이어서)
base: v2.7 (웹 인터페이스 리뉴얼)
status: plan
---

# RemoteDeck_Touch v2.8 — 서버 설정/이미지 불러오기 개선

> **Summary**: LCD 장치설정의 "서버설정 불러오기 / 이미지 불러오기" 버튼(이전 프로젝트 이전부터 존재)을 코드 점검하여 발견한 버그·설계 불일치를 정리. deviceconfig는 서버에서 받지 않고(개별 기기 정보=LCD/웹 전용), serverconfig·이미지만 서버에서 일괄. 포트 버그·확인창·이미지 우선순위 정리.
>
> **Project**: RemoteDeckSystem · **Author**: KDI · **Date**: 2026-09-15 · **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | LCD 장치설정 "서버설정/이미지 불러오기" 버튼이 (1) deviceconfig(개별 기기 IP/deviceID)까지 서버 값으로 덮어써 설정 유실 위험, (2) `configUrl` 대신 `imageUrl` 오용, (3) `httpPort` 파싱만 하고 URL 미반영(항상 80), (4) 성공/실패·재부팅 피드백 없이 무조건 재부팅, (5) 이미지 역할 하드코딩 산재, (6) 서버 다운로드 이미지가 웹 업로드 이미지를 가림. |
| **Solution** | 설계의도(공통 serverconfig·이미지는 서버 일괄, 개별 기기정보는 LCD/웹)에 맞춰: deviceconfig 서버 다운로드 제거, serverconfig 전용화, 포트 반영, 재부팅 확인창+결과 피드백, 역할 고정(title/photo/name), 이미지 로더 우선순위 `/images/`(웹) > `/download/`(서버)로 반전. |
| **Function/UX Effect** | 20실 대량 설치 시 공통 serverinfo·이미지는 버튼 한 번으로 서버에서 일괄 수신(빠른 설치), 개별 IP/설정은 서버가 건드리지 않음. 불러오기 시 확인창·성공/실패 피드백, 웹에서 교체한 이미지가 우선. |
| **Core Value** | 서버 일괄 프로비저닝의 편의는 유지하되, 개별 기기 설정 유실·오작동 위험 제거. 안전한 대량 설치/운영. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 서버 불러오기 버튼이 개별 기기 config까지 덮어써 설정 유실 위험 + 포트/URL 버그 + 피드백 부재 |
| **WHO** | 설치·운영 관리자 (20실 대량 설치, 공통 정보는 서버 일괄) |
| **RISK** | ① deviceconfig 덮어쓰기로 IP/네트워크 유실 ② non-80 서버 접속 실패 ③ 웹 교체 이미지가 서버본에 가려짐 |
| **SUCCESS** | serverconfig·이미지만 서버에서 수신(deviceconfig 보존) + 포트 정상 + 확인창/피드백 + 웹 이미지 우선 (실기) |
| **SCOPE** | 서버 불러오기 로직(main.cpp) + 이미지 로더 우선순위(images.cpp) + LCD 버튼 확인창(DeviceManager/main). 웹 UI·API 변경 없음 |

---

## 1. Overview

### 1.1 Purpose
LCD 장치설정 하단의 "서버설정 불러오기 / 이미지 불러오기" 버튼 동작을 코드 점검 결과에 따라 정리하여, 설계의도(공통 정보 서버 일괄 + 개별 기기 정보 로컬)에 부합하고 버그 없이 안전하게 동작하도록 한다.

### 1.2 Background
- 해당 버튼은 본 프로젝트(2026-06) 이전부터 존재하던 서버 프로비저닝 기능. v2.1에서 HTTPClient 기반으로 다운로드는 재작성됐으나 버튼 로직은 미점검 상태였음.
- **설계의도(사용자 확인)**: 웹 재부재 솔루션에 종속. 20실 설치 시 공통 시스템 정보(serverconfig)와 웹 이미지관리로 id별 저장된 이미지(imagesconfig)는 서버에서 일괄 수신 → 빠른 설치. **개별 기기 정보(deviceconfig=IP/deviceID/네트워크)는 LCD 장치설정 + 웹 인터페이스에서만** 설정.
- v2.7에서 웹 인터페이스가 보강되어 개별 설정은 웹/LCD로 충분.

### 1.3 Related Documents
- [[RemoteDeck_Touch_v2.7_Webinterface]] (웹 UI 리뉴얼) — 직전 작업
- 코드 점검 대상: `src/main.cpp`(fetchServerInfo/fetchImageFiles/downloadFile/sendHttpMessage), `src/images/images.cpp`, `src/device/DeviceManager.cpp`

---

## 2. Scope

### 2.1 In Scope
- [ ] **#1** `fetchServerInfo`: serverconfig만 수신·교체, **deviceconfig 서버 다운로드 제거**
- [ ] **#2** deviceconfig 경로의 `imageUrl` 오용 제거(→ #1로 해소). serverconfig는 공통 고정경로 유지
- [ ] **#3** `downloadFile`·`sendHttpMessage`에서 `httpPort` URL 반영 (non-80 서버 지원)
- [ ] **#4** 불러오기 버튼에 **재부팅 확인창 + 결과(성공/실패) 피드백** (LCD msgbox)
- [ ] **#5** 이미지 **역할 고정**(title/photo/name) — 배열 루프로 정리
- [ ] **#6** 이미지 **단일 경로 `/images/` 통합** (웹 교체·서버 다운로드 공통 저장 → SPIFFS 중복 방지, 마지막 쓰기 우선). in/out(공통·불변)은 펌웨어 임베드 유지
- [ ] **#7** `parseAddress` 깨끗한 host 반환 (트레일링 슬래시/경로 제거) — `server_url="http://ip/"` + `:port` 조립 시 malformed URL 방지 (실기 테스트 중 발견)
- [ ] 실기 검증(서버 불러오기·이미지·확인창)

### 2.2 Out of Scope
- 웹 UI/API 변경 (이번은 LCD 버튼·다운로드 로직·이미지 로더 한정)
- config JSON 스키마 변경 (없음)
- 서버측 파일 구조 변경
- deviceconfig의 서버 관리 (설계상 제외 확정)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | 서버설정 불러오기 = serverconfig.json만 수신·저장, deviceconfig 미수신 | High | Pending |
| FR-02 | 이미지 불러오기 = imagesconfig + 고정 역할(title/photo/name) BMP를 `/download/`에 수신 | High | Pending |
| FR-03 | HTTP 요청 URL에 httpPort 반영 (다운로드/status 전송) | High | Pending |
| FR-04 | 두 버튼에 재부팅 확인창 + 결과 피드백 (성공→Reboot / 실패→Close, 실패 시 재부팅 안 함) | Medium | Pending |
| FR-05 | 이미지 단일 경로 `/images/` (웹 교체·서버 다운로드 공통, 중복 없음). 서버 다운로드도 `/images/`에 저장, 구 `/download/` 정리. in/out은 임베드 유지 | High | Pending |
| FR-06 | blocking HTTP를 LVGL 콜백에서 직접 실행하지 않고 loop에서 처리(UI 안정) | Medium | Pending |
| FR-07 | `parseAddress` 깨끗한 host 반환(경로/슬래시 제거) → `:httpPort` 조립 정상 | High | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement |
|----------|----------|-------------|
| 하위호환 | config 스키마 무변경, 기존 기기 동작 유지 | 실기 |
| 안정성 | deviceconfig 미변경 → 개별 IP/설정 보존 | 실기 (불러오기 후 IP 유지) |
| 견고성 | non-80 포트 서버에서도 다운로드/status 성공 | 포트 지정 서버 테스트(가능 시) |

---

## 4. Success Criteria

### 4.1 Definition of Done
- [ ] 서버설정 불러오기 후 serverconfig만 갱신, deviceconfig(IP/deviceID) 그대로 (실기)
- [ ] 이미지 불러오기 후 고정 역할 이미지 반영, 웹 교체 이미지가 우선 표시 (실기)
- [ ] 확인창·성공/실패 피드백 정상, 실패 시 재부팅 안 함 (실기)
- [ ] `pio run` 빌드 성공

### 4.2 Quality Criteria
- [ ] matchRate ≥ 90%
- [ ] LVGL 콜백 내 blocking 제거로 UI 프리즈/크래시 없음

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| deviceconfig 미수신으로 기존 서버 프로비저닝 흐름 변화 | Medium | Low | 설계의도상 개별정보는 LCD/웹 전용 — 의도된 변경. 문서화 |
| httpPort 반영이 기존(포트없는 IP) 동작 깨뜨림 | High | Low | parseAddress 기본 80 보장 → `:80` 무해. 실기 확인 |
| LVGL msgbox 백드롭 잔존/중첩 | Low | Low | close_async 사용, fetch는 loop에서(콜백 blocking 회피) |
| 이미지 우선순위 반전으로 서버본이 안 보임 | Low | Low | 의도(웹 교체 우선). /images/ 없으면 /download/ fallback 유지 |

---

## 6. Impact Analysis

### 6.1 Changed Resources
| Resource | Type | Change |
|----------|------|--------|
| `src/main.cpp` | Logic | fetchServerInfo(serverconfig 전용, bool), fetchImageFiles(역할 고정, bool), 포트 반영, 확인창/결과창/loop 처리 |
| `src/images/images.cpp` | Logic | try_set 후보경로 우선순위 반전(/images/ 우선) |
| `src/device/DeviceManager.cpp` | Wiring | 버튼 → promptFetch*(확인창) |

### 6.2 Current Consumers
| Resource | Impact |
|----------|--------|
| serverconfig.json | 서버에서 교체(기존과 동일 경로) |
| deviceconfig.json | **서버 다운로드 제거** — 로컬/웹만 |
| /images/, /download/ | 로더 우선순위 반전(웹 우선) |

---

## 7. Architecture Considerations
- 임베디드 단일 스레드 orchestrator. LCD 모드 전용 기능(웹 모드 아님).
- blocking HTTP는 loop에서 실행(LVGL 이벤트 콜백에서 분리) — 플래그(g_pendingFetch) 경유.
- 다운로드는 v2.1 재작성 `downloadFile`(타임아웃/크기상한/부분정리) 재사용.

---

## 8. Convention Prerequisites
- 커밋: `fix/feat(RemoteDeck_Touch_v2.8): 요약` (한글)
- LCD msgbox 라벨은 영문(한글 폰트 subset 글리프 부재 — 기존 관례)
- config 스키마 무변경

---

## 9. Next Steps
1. [ ] Design (Option 선택 + 상세)
2. [ ] Do (구현 — 이미 코드 점검 기반 반영됨)
3. [ ] 실기 검증 → analyze → report → archive

---

## Version History
| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-09-15 | Initial — 서버 불러오기 버그수정 + deviceconfig 분리 + 확인창 + 이미지 우선순위 | KDI |
