---
template: design
version: 0.1
feature: RemoteDeck_Touch_v2.8_ServerConfigFetch
date: 2026-09-15
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: main
plan: docs/01-plan/features/RemoteDeck_Touch_v2.8_ServerConfigFetch.plan.md
status: design
selected_option: C (Pragmatic)
---

# RemoteDeck_Touch v2.8 — 서버 설정/이미지 불러오기 개선 (Design)

> **Summary**: LCD "서버설정/이미지 불러오기" 코드 점검 결과 정리. deviceconfig 서버 다운로드 제거(개별 기기정보=LCD/웹 전용), serverconfig·이미지만 서버 일괄, httpPort 반영, 재부팅 확인창+피드백, 이미지 역할 고정, 로더 우선순위 웹>서버.
> **Planning Doc**: [plan](../../01-plan/features/RemoteDeck_Touch_v2.8_ServerConfigFetch.plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 서버 불러오기가 개별 기기 config 덮어씀 + 포트/URL 버그 + 피드백 부재 |
| **WHO** | 설치·운영 관리자 (20실 대량 설치) |
| **RISK** | ① deviceconfig 덮어쓰기 ② non-80 접속 실패 ③ 웹 교체 이미지 가려짐 |
| **SUCCESS** | serverconfig·이미지만 서버 수신(deviceconfig 보존) + 포트 정상 + 확인창/피드백 + 웹 이미지 우선 |
| **SCOPE** | main.cpp(fetch/download/loop) + images.cpp(로더) + DeviceManager(버튼). 웹/API/스키마 무변경 |

---

## 1. Overview

### 1.1 Design Goals
- 공통 정보(serverconfig)·이미지는 서버 일괄, 개별 기기정보(deviceconfig)는 LCD/웹 전용으로 명확히 분리
- 다운로드/전송 URL의 포트 정확성
- 불러오기 UX: 확인 → 실행 → 결과 피드백 → (성공 시) 재부팅
- 웹에서 교체한 이미지가 서버 일괄본보다 우선

### 1.2 Design Principles
- **No schema/API change**: config JSON·엔드포인트 불변. 기존 기기 무영향
- **Reuse**: v2.1 `downloadFile`(타임아웃/크기상한/부분정리) 재사용
- **UI 안정**: blocking HTTP는 LVGL 이벤트 콜백이 아닌 main loop 에서

---

## 2. Architecture Options

| Criteria | A: 최소(포트·이미지우선만) | B: 전면 재설계(다운로드 서브시스템) | C: Pragmatic |
|----------|:-:|:-:|:-:|
| 포트 버그(#3) | ✅ | ✅ | ✅ |
| 이미지 우선순위(#6) | ✅ | ✅ | ✅ |
| deviceconfig 분리(#1) | ✅ | ✅ | ✅ |
| 확인창/피드백(#4) | ❌ | ✅(과함) | ✅(msgbox 재사용) |
| 역할 고정(#5) | 부분 | ✅ | ✅ |
| 복잡도/리스크 | Low/요구 미충족 | High | Low~Med |

**Selected: C (Pragmatic)** — 기존 msgbox 패턴(webmode 확인창)·downloadFile 재사용으로 확인창/피드백까지 최소 리스크로 충족. 다운로드 서브시스템 재설계(B)는 과투자.

### 2.1 Flow

```
[LCD 장치설정]
  "서버설정 불러오기" → promptFetchServerInfo() ── 확인창(OK/Cancel)
  "이미지 불러오기"   → promptFetchImageFiles()  ──┘   │ OK
                                                        ▼ (g_pendingFetch=1|2)
  [main loop] runPendingFetch() ─ fetchServerInfo()/fetchImageFiles() (blocking)
                                    │
                                    ▼ 결과창  성공→[Reboot]  실패→[Close](재부팅 안 함)
```

---

## 3. Data Model / 동작

### 3.1 서버 수신 대상 (설계의도 반영)
| 대상 | 서버 수신 | 경로 | 비고 |
|------|:--:|------|------|
| serverconfig.json | ✅ | `/iot_device/serverconfig.json` (공통, device_id 무관) | 공통 시스템 정보 |
| imagesconfig.json | ✅ | `imageUrl`(`/iot_device/[id]/`) + `imagesconfig.json` | 버전/정보 |
| title/photo/name.bmp | ✅ | `imageUrl` + `<role>.bmp` → `/download/<role>.bmp` | 역할 고정 |
| **deviceconfig.json** | ❌ | — | **개별 기기정보 = LCD 장치설정 + 웹 전용** |

### 3.2 이미지 단일 경로 (images.cpp `try_set`)
```
후보(역할별): /images/<role>.png → /images/<role>.bmp → (없으면) 앱 임베드 기본
```
- **단일 경로 `/images/`**: 웹 교체·서버 다운로드 **모두 `/images/<role>.bmp` 같은 파일**에 저장 → SPIFFS 중복 없음, **마지막 쓰기 우선**(웹 교체하면 웹, 서버 불러오기 하면 서버).
- 서버 다운로드(fetchImageFiles)는 `/images/<role>.bmp` 저장 + 구 `/download/<role>.*`·잔여 `/images/<role>.png` 정리(그림자/낭비 방지).
- 이전 `/download/` 별도 경로는 폐기 — `/images/` 기본이미지에 가려 서버 다운로드가 반영 안 되던 문제 해소.
- **in/out(재실/부재)**: 개인별로 바뀌지 않는 공통 이미지 → **펌웨어 임베드 에셋**(`ui_img_in_png`/`ui_img_out_png`) 사용. SPIFFS 무관, 정리 대상 아님(항상 표시).

### 3.3 URL 포트 (downloadFile / sendHttpMessage)
```
http://<httpUrl>:<httpPort><path>   // parseAddress 기본 80 → :80 무해, non-80 지원
```

---

## 4. API Specification
변경 없음. 서버측 파일(serverconfig/imagesconfig/*.bmp) GET만 사용(기존).

---

## 5. UI/UX (LCD msgbox)
- 확인창: title "Server Config"/"Image Config", "Load ... from server and reboot?", [OK][Cancel]
- 결과창(성공): "Loaded from server. Reboot to apply." [Reboot]
- 결과창(실패): "Load failed. Check server. No change." [Close]
- 라벨 영문(폰트 subset 관례). blocking fetch는 loop에서 → 콜백 내 blocking 없음.

---

## 6. Error Handling
| 상황 | 처리 |
|------|------|
| 다운로드 실패(서버 불통/404) | fetch* false 반환 → 결과창 "실패, 변경 없음", 재부팅 안 함 |
| serverconfig 파싱 실패 | temp 삭제 후 false |
| 이미지 일부 실패 | 성공분만 저장, anyOk 기준 결과 표시 |

---

## 7. Implementation Guide

### 7.1 변경 파일
```
src/main.cpp
  - fetchServerInfo()  : bool, serverconfig 전용 (deviceconfig 다운로드 삭제)
  - fetchImageFiles()  : bool, imagesconfig + ROLES[]{title,photo,name} 루프 → /images/<role>.bmp 저장
                         + 구 /download/<role>.*·잔여 /images/<role>.png 정리
  - downloadFile()/sendHttpMessage() : URL 에 :httpPort 추가
  - g_pendingFetch + fetchConfirm_cb/fetchResult_cb/runPendingFetch + promptFetch* 
  - loop(): runPendingFetch() 호출(LCD 모드, lvgl_loop 직후)
src/utils/TypeUtils.cpp
  - parseAddress(): scheme 제거 후 첫 '/' 이후(경로/트레일링 슬래시) 잘라 깨끗한 host 반환
                    ("http://ip/" → "ip") — :httpPort 조립 시 malformed URL 방지
src/images/images.cpp
  - try_set(): 단일 경로 /images/<role>.{png,bmp} (기존 /download/ 후보 제거)
src/device/DeviceManager.cpp
  - btnLoadMqtt_Click → promptFetchServerInfo(); btnLoadImages_Click → promptFetchImageFiles()
```

### 7.2 Module Map
| Module | Scope Key | Description |
|--------|-----------|-------------|
| 서버 fetch 로직 | `module-fetch` | fetchServerInfo/fetchImageFiles bool 화 + 포트 |
| 확인창/피드백 | `module-dialog` | prompt/confirm/result + loop 처리 |
| 이미지 우선순위 | `module-imgorder` | try_set 후보 반전 |

---

## 8. Test Plan

### 8.1 실기 시나리오
| # | 시나리오 | 성공 |
|---|---------|------|
| 1 | 서버설정 불러오기 → 확인 → serverconfig 갱신, **deviceconfig(IP/deviceID) 유지** | FR-01 |
| 2 | 이미지 불러오기 → 고정 역할 이미지 반영 | FR-02 |
| 3 | 서버 불통 시 → "실패" 피드백, 재부팅 안 함 | FR-04 |
| 4 | 웹에서 이미지 교체 후 → 서버 /download/ 잔존해도 웹 /images/ 우선 표시 | FR-05 |
| 5 | (가능 시) non-80 포트 서버에서 다운로드 성공 | FR-03 |

---

## 9. Version History
| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-09-15 | Initial — Option C. 서버 불러오기 정리(deviceconfig 분리/포트/확인창/역할고정/이미지우선) | KDI |
