---
template: design
version: 0.1
feature: RemoteDeck_Touch_v2.7_Webinterface
date: 2026-09-14
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware web UI)
branch: v2.6-touch-webmode
plan: docs/01-plan/features/RemoteDeck_Touch_v2.7_Webinterface.plan.md
status: design
selected_option: C (Pragmatic)
---

# RemoteDeck_Touch v2.7 Webinterface Design Document

> **Summary**: Touch 웹 UI를 RemoteDeck_PC 스타일 5탭 + 설정 항목별 폼으로 리뉴얼. Option C(Pragmatic) — API 계약 유지 + serverconfig/schedule 신규, 단일 주간 재부팅 스케줄.
> **Planning Doc**: [plan](../../01-plan/features/RemoteDeck_Touch_v2.7_Webinterface.plan.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | Touch 웹 UI가 PC와 달라(설정=JSON 통짜) 사용성·일관성 낮음 |
| **WHO** | 관리자(웹 설정 모드에서 브라우저로 설정·OTA·기기관리) |
| **RISK** | ①재부팅 스케줄=신규 firmware 기능 ②하위호환 ③embed_www.py 워크플로 ④flash 크기 |
| **SUCCESS** | 5탭·항목별 편집·저장 + 기존 기기 OTA 후 설정보존 + 재부팅 스케줄 (전부 실기) |
| **SCOPE** | S1 골격+상태/제어/로그 → S2 설정폼+serverconfig → S3 관리(재부팅+OTA) → S4 재부팅 스케줄 → S5 이미지 설정하위+embed+검증 |

---

## 1. Overview

### 1.1 Design Goals
- Touch 웹 UI를 RemoteDeck_PC와 동일한 5탭 + sub-tab + 항목별 폼 구조/룩앤필로 통일
- 설정을 raw JSON에서 항목별 편집으로 전환하되 **API 계약·config 스키마 하위호환 유지**
- 재부팅 스케줄을 최소·안전 형태로 신설 (NTP 기반, 단일 주간 스케줄)

### 1.2 Design Principles
- **No breaking change**: `/api/config` 계약·config JSON 스키마 불변, 신규는 추가만 (기존 14대 OTA 무영향)
- **Reuse over rewrite**: 기존 API(config/status/images/control/log/ota) 재활용, 프론트 재구성 중심
- **Source-driven assets**: `data/www` 편집 → `embed_www.py` 재생성 (gzip 직접 편집 금지)
- **Web/LCD 배타 유지**: v2.6 부팅경계 불변 (웹모드 서비스 중 TFT 무접근)

---

## 2. Architecture Options

### 2.0 Comparison

| Criteria | A: Minimal | B: Clean(풀 패리티) | C: Pragmatic |
|----------|:-:|:-:|:-:|
| 웹 UI 재구성(5탭+폼) | ✅ | ✅ | ✅ |
| Server Config API | 통짜 | 항목검증 | 통짜 |
| 재부팅 스케줄 | 기존 rebootTime 재사용(스케줄 아님) | 주간 다중 스케줄(ScheduleManager+schedule.json) | 단일 주간 스케줄(요일+시각, NTP) |
| New Files | ~1 | ~3 | ~1-2 |
| Modified | ~5 | ~8 | ~6 |
| Complexity | Low | High | Medium |
| Risk | Low(요구 미충족) | High(신규 서브시스템) | Low~Medium |

**Selected**: **C (Pragmatic)** — Rationale: 사용자가 "재부팅 스케줄 신설" 선택 → A 배제. B의 다중 스케줄 서브시스템은 과함(릴레이 없는 Touch에 과투자). C는 실제 주간 스케줄을 최소·안전하게 신설하면서 하위호환·범위 균형.

### 2.1 Component Diagram

```
[Browser]  ── HTTP(:80, 웹모드) ──▶  [esp_http_server (core0)]
  5 tabs                                 │
  상태/제어/설정/관리/로그               ├─ ImageApi   /api/status, /api/images/*
                                         ├─ ConfigApi  /api/config, /api/imagesconfig,
                                         │             +/api/serverconfig(신규), +/api/schedule(신규)
                                         ├─ ControlApi /api/control
                                         ├─ Logger     /api/log
                                         └─ OtaApi     /api/ota, /api/reboot
                                              │
                              [SPIFFS] deviceconfig.json / serverconfig.json / imagesconfig.json / images/
                              [main loop (core1, LCD모드)] rebootSchedule NTP 검사 → ESP.restart()
```

### 2.2 Data Flow (설정 저장 — PC 패턴)

```
폼 로드: GET /api/config, /api/serverconfig → 필드에 분배
저장:    필드 → 클라에서 whole object 조립 → POST /api/config | /api/serverconfig
         → 서버 ArduinoJson 검증 + atomic tmp-write+rename → 저장
스케줄:  GET/POST /api/schedule → deviceconfig.rebootSchedule (load-merge-save)
```

### 2.3 Dependencies
| Component | Depends On | Purpose |
|-----------|-----------|---------|
| 웹 UI(app.js) | 기존 API + serverconfig/schedule 신규 | 항목별 편집 |
| rebootSchedule 실행 | NTP 시간(기존 동작 확인됨) | 주간 스케줄 판정 |
| embedded_assets.cpp | data/www + embed_www.py | 임베드 |

---

## 3. Data Model

### 3.1 기존 config (스키마 불변 — 폼 필드 매핑)
- **DeviceConfig** (deviceconfig.json): deviceID, networkConfig{usingEthernet, wifiSSID/Passwd/MAC, usingStatic, staticIP/Gateway/Subnet/PrimaryDNS/SecondaryDNS/MAC}, serverURL, rebootTime, sleepTime, versionInfo, webConfigMode(v2.6)
- **ServerConfig** (serverconfig.json): version, configUrl, imageUrl, statusUrl, mqttConfig{url,user,passwd,port,keepalive,pubTopic,subTopic,pingTopic}, usingHttpRequest, sleepTime

### 3.2 신규 — rebootSchedule (deviceconfig.json 내 추가, 하위호환)
```jsonc
"rebootSchedule": {
  "enabled": false,          // 부재 시 false (구 기기 하위호환)
  "days": [0,1,2,3,4,5,6],   // 0=일 … 6=토 (선택 요일)
  "hour": 4,                 // 0-23
  "minute": 0                // 0-59
}
```
- JsonUtils deserialize: `enabled = doc["rebootSchedule"]["enabled"] | false` 등 default 가드 → **구 deviceconfig.json 파싱 안전**.
- 기존 `rebootTime`(int)는 유지(공존). v2.7 스케줄이 우선; rebootTime 처리 로직 변경 없음(하위호환).

### 3.2b 신규 — nightOff (야간 화면 끄기, deviceconfig.json 내 추가, 하위호환)
```jsonc
"nightOff": {
  "enabled": false,          // 부재 시 false (구 기기 하위호환)
  "startHour": 22, "startMinute": 0,
  "endHour": 6,   "endMinute": 0   // 자정 넘김(22:00~06:00) 지원
}
```
- **스크린세이버(sleepTime)와 별개** — sleepTime은 무활동 기반, nightOff는 시간대 기반.
- 실행(main.cpp LCD loop, NTP): now가 [start,end) 야간 구간이면 LCD 백라이트 off. 자정 wrap 처리(start>end면 반전).
- **터치 동작**: 야간 구간에 화면이 off여도 터치 시 기존 스크린세이버 wake 경로로 잠깐 켜졌다(짧은 timeout) 다시 off. → 야간에도 급하면 확인 가능.
- 백라이트 off 메커니즘은 기존 스크린세이버 경로 재사용(Do 단계에서 BL 핀/디스플레이 off 확정).

### 3.3 실행 로직 (main.cpp, LCD 모드 loop)
```
매 분 1회: NTP now 조회 → enabled && (now.weekday ∈ days) && now.hour==hour && now.minute==minute
        → 로그 남기고 ESP.restart()  (중복 방지: 같은 분 1회만)
```
> 웹 모드에서는 실행 안 함(웹 모드는 일시적 세션). LCD 모드 정상 운영 중에만 스케줄 동작.

---

## 4. API Specification

### 4.1 Endpoint List
| Method | Path | 상태 | 설명 |
|--------|------|:-:|------|
| GET | `/`, `/style.css`, `/app.js` | 재활용 | 정적(임베드) — 내용 교체 |
| GET | `/api/status` | 재활용 | 상태 탭 |
| GET/POST | `/api/config` | 재활용(계약불변) | Device Config (whole object) |
| GET | `/api/imagesconfig` | 재활용 | (읽기전용, UI 노출 제외) |
| **GET/POST** | **`/api/serverconfig`** | **신규** | Server Config (whole object) |
| **GET/POST** | **`/api/schedule`** | **신규** | rebootSchedule (deviceconfig 내 필드 load-merge-save) |
| GET/POST | `/api/images/*`, list, upload, DELETE | 재활용 | 이미지 관리(설정 하위) |
| GET/POST | `/api/control` | 재활용 | 제어 탭 |
| GET | `/api/log` | 재활용 | 로그 탭 |
| POST | `/api/reboot`, `/api/ota` | 재활용 | 관리 탭 |

### 4.2 신규 상세

#### `GET /api/serverconfig` → serverconfig.json (전체)
#### `POST /api/serverconfig` (Request: 전체 ServerConfig JSON)
- 서버: ArduinoJson 파싱 + 크기 제한 + `ConfigManager::saveServerConfig` (atomic). 응답 `{"ok":true}` / 400.
- **하위호환**: 누락 필드는 기존 값/기본 유지(파싱 default).

#### `GET /api/schedule` → `{enabled,days,hour,minute}`
#### `POST /api/schedule` (Request: `{enabled,days,hour,minute}`)
- 서버: deviceconfig 로드 → rebootSchedule 병합 → 저장. 응답 `{"ok":true}` / 400.

### 4.3 공통 에러
`400` 검증 실패(JSON/범위), `401` 인증(TouchAuth admin/12345). 기존 패턴 유지.

---

## 5. UI/UX Design

### 5.1 Layout (PC 토큰 재사용)
```
[header: RemoteDeck Touch + fw/상태]
[nav: 상태 | 제어 | 설정 | 관리 | 로그]   ← active=cyan underline
[page.active]
  설정: [sub-tabs: Device Config | Server Config | 이미지 관리]
        [.card > .cfg-group(label+input) ...]  [저장 버튼]
```
- 색상/카드/폼: PC `style.css` 토큰(#1a1a2e/#16213e/#0ff 등) 준수. Touch 고유 컴포넌트(toast/drop-zone/img-card) 유지.

### 5.2 User Flow
```
웹모드 진입(v2.6) → 브라우저 접속 → 탭 이동 → 설정 항목 수정 → 저장(toast) → (관리)재부팅/스케줄/OTA
```

### 5.4 Page UI Checklist

#### 상태 (Status)
- [ ] Display: 펌웨어 버전 / heap free·min / network(ip·mac·ethernet|wifi) / uptime
- [ ] Display: attendance(present/absent, /api/status 필드 있으면)

#### 제어 (Control)
- [ ] Button: 재실/부재(in/out) 토글 (`/api/control` POST)
- [ ] Display: 현재 상태 (long-poll `since`)

#### 설정 > Device Config
- [ ] Input: deviceID
- [ ] Toggle: usingEthernet (ethernet/wifi)
- [ ] Input: wifiSSID, wifiPasswd
- [ ] Toggle: usingStatic; Input: staticIP/Gateway/Subnet/PrimaryDNS/SecondaryDNS
- [ ] Input: serverURL
- [ ] Dropdown: sleepTime (스크린세이버, No/1/2/3/4/5/10/20/30/60)
- [ ] Night Off(야간 화면 끄기): Toggle enabled + 시작(HH:MM) + 종료(HH:MM) — 스크린세이버와 별개
- [ ] Button: 저장 (whole deviceconfig POST /api/config)

#### 설정 > Server Config
- [ ] Input: mqtt url/user/passwd/port/keepalive/pubTopic/subTopic/pingTopic
- [ ] Input: configUrl/imageUrl/statusUrl
- [ ] Toggle: usingHttpRequest
- [ ] Button: 저장 (whole serverconfig POST /api/serverconfig)

#### 설정 > 이미지 관리
- [ ] Card: 현재 이미지(title/photo/name) 썸네일+파일명+크기
- [ ] Button: 교체(replace)/삭제(delete) per role
- [ ] Dropzone: 업로드(PNG/BMP, ≤200KB) → `/api/images/upload`

#### 관리 (Admin)
- [ ] Button: 즉시 재부팅 (`/api/reboot`)
- [ ] Form: 재부팅 스케줄 — Toggle enabled + 요일 체크박스(일~토) + hour/minute → 저장 `/api/schedule`
- [ ] OTA: 펌웨어 .bin file input + 업로드 버튼 + progress bar (`/api/ota`)

#### 로그 (Log)
- [ ] List: 로그 엔트리(ts/event/detail) (`/api/log`)

---

## 6. Error Handling
| Code | 상황 | 처리 |
|------|------|------|
| 400 | JSON/범위 검증 실패 | toast 에러, 저장 취소 |
| 401 | 인증 실패 | 브라우저 basic auth 재요청 |
| 연결 끊김 | OTA/reboot 중 | 기존 "reboot 가능성, 30초 후 새로고침" 안내 유지 |

---

## 7. Security
- 전 endpoint TouchAuth(admin/12345) — 기존 유지. 웹모드에서만 포트 개방(v2.6).

---

## 8. Test Plan

### 8.2 L1 — API
| # | Endpoint | Test | Expected |
|---|----------|------|----------|
| 1 | GET /api/serverconfig | 조회 | 200, serverconfig 필드 |
| 2 | POST /api/serverconfig | 전체 저장 | 200 {ok:true}, 재조회 반영 |
| 3 | POST /api/serverconfig | 잘못된 JSON | 400 |
| 4 | GET/POST /api/schedule | 스케줄 왕복 | 200, 저장 반영 |
| 5 | POST /api/config | 기존 계약 회귀 | 200 (기존과 동일) |

### 8.3 L2 — UI (수동/브라우저)
| # | Page | Action | Expected |
|---|------|--------|----------|
| 1 | 설정>Device | 필드 수정 후 저장 | toast 성공, 재조회 반영 |
| 2 | 설정>Server | MQTT 필드 저장 | 반영 |
| 3 | 설정>이미지 | 교체/삭제 | 썸네일 갱신 |
| 4 | 관리 | 스케줄 저장 | /api/schedule 반영 |
| 5 | 관리 | OTA/재부팅 | 정상 |

### 8.4 L3 — 실기 시나리오
| # | 시나리오 | 성공 |
|---|---------|------|
| 1 | 웹모드 진입 → 5탭 렌더 → 설정 저장 → LCD 복귀 후 반영 | 설정 persist |
| 2 | **구 config 기기 OTA 업그레이드** → 기존 설정 보존·정상 | 하위호환(FR-12) |
| 3 | 재부팅 스케줄 설정 → 해당 시각 자동 재부팅 | FR-08 실기 |
| 4 | 웹모드 부하 PoC(v24_poc.py) | 여전히 통과(SPI 배타 유지) |

---

## 9. Clean Architecture (firmware 계층)
| Layer | 책임 | 위치 |
|-------|------|------|
| Presentation | 웹 UI (탭/폼/스케줄) | `data/www/*` → embedded_assets |
| Application | API 핸들러 | `src/web/{ConfigApi,ImageApi,ControlApi,OtaApi,Logger}` |
| Domain/Config | config 구조·직렬화 | `src/config/*`, `src/utils/JsonUtils` |
| Infra | SPIFFS, NTP, esp_http_server, 부팅분기(main) | `src/main.cpp`, ConfigManager |
- 규칙: rebootSchedule 실행은 main(Infra/orchestrator)에서, 웹 핸들러는 config만 read/write.

---

## 10. Coding Convention
- 웹 자산: **data/www 편집 → tools/embed_www.py 재생성** (embedded_assets.cpp 직접 수정 금지)
- API: 기존 ConfigApi atomic write 패턴 준수
- Design Ref 주석: `// v2.7: …`
- 커밋: `feat(RemoteDeck_Touch_v2.7): …`

---

## 11. Implementation Guide

### 11.1 File Structure (변경/신규)
```
RemoteDeck_Touch/
├─ data/www/{index.html, app.js, style.css}   ← 전면 재구성 (편집 대상)
├─ src/web/embedded_assets.cpp                 ← embed_www.py 재생성
├─ src/web/ConfigApi.{h,cpp}                   ← +serverconfig, +schedule 핸들러
├─ src/web/WebServer.cpp                       ← +/api/serverconfig, +/api/schedule 라우트
├─ src/config/DeviceConfig.h                   ← +rebootSchedule 구조
├─ src/utils/JsonUtils.cpp                     ← rebootSchedule (de)serialize(하위호환)
├─ src/main.cpp                                ← 스케줄 NTP 실행(LCD loop)
└─ tools/embed_www.py                          ← 재생성 도구(기존)
```

### 11.3 Session Guide

#### Module Map
| Module | Scope Key | Description | Turns |
|--------|-----------|-------------|:-----:|
| 탭 골격+상태/제어/로그 | `module-shell` | 5탭 네비·PC 토큰 + 상태/제어/로그 이관 | 6-8 |
| 설정 폼 + serverconfig | `module-settings` | Device/Server 항목별 폼(+nightOff 야간끄기 필드) + /api/serverconfig | 8-10 |
| 관리(재부팅+OTA) | `module-admin` | 재부팅 버튼 + OTA 이동 | 3-4 |
| 시간기반 실행 | `module-schedule` | rebootSchedule + /api/schedule + **nightOff 백라이트 실행** + main NTP loop | 8-10 |
| 이미지+embed+검증 | `module-image-embed` | 이미지 설정하위 + embed + 실기/하위호환 | 5-7 |

#### Recommended Session Plan
| Session | Scope | 비고 |
|---------|-------|------|
| S1 | `module-shell` | 골격부터, embed→실기 렌더 확인 |
| S2 | `module-settings` | 항목별 편집 핵심 |
| S3 | `module-admin` | |
| S4 | `module-schedule` | 최대 리스크 — 단독, 실기 |
| S5 | `module-image-embed` | 하위호환(구 config OTA) 검증 |

---

## Version History
| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-09-14 | Initial — Option C, 5탭+설정폼, serverconfig/schedule 신규, 단일 주간 재부팅 스케줄, 하위호환 | KDI |
