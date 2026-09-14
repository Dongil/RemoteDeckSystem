---
template: design
version: 0.1
feature: RemoteDeck_Touch_v2.6
date: 2026-09-11
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: v2.6-touch-webmode
base: v2.3-httpd
plan: docs/01-plan/features/RemoteDeck_Touch_v2.6.plan.md
status: design
---

# RemoteDeck_Touch v2.6 Design Document — 웹 설정 모드 (부팅 경계 SPI 배타)

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | LCD 터치 외 설정/OTA 수단 부재로 유지보수 불편. 웹UI 동시구동은 SPI 충돌로 불가(v2.2~2.4). |
| **WHO** | 현장 사용자(LCD) + 관리자(가끔 웹) |
| **핵심 결정** | 동시 사용 포기 → **부팅당 SPI 소비자 1개만 초기화** (모드 전환) |
| **SUCCESS** | 웹 모드 PoC(SC-1) 통과 + OTA/설정 동작 + LCD 무회귀 + 안티브릭 |
| **SCOPE** | boot 분기 + 모드 플래그 + 웹모드 loop + 종료/타임아웃 + 정적 안내화면 |

---

## 1. Overview

### 1.1 Design Goals
- LCD 터치 모드와 웹 설정 모드를 **부팅 경계로 완전 분리** (동시 미초기화)
- 웹 모드에서 RemoteDeck_PC 수준 설정/로그/제어 + OTA (v2.3 httpd 5모듈 재활성)
- **브릭 불가**: 어떤 비정상 종료에서도 다음 부팅은 LCD 모드로 복귀

### 1.2 Design Principles
- **Exclusive SPI ownership per boot** — TFT(TFT_eSPI)와 W5500은 SPI 버스(SCK18/MOSI23/MISO19) 공유. 한 부팅에서 둘 중 하나만 init → 경합 대상 소멸.
- **One-shot consumed flag** — 웹 모드 플래그는 부팅 시 즉시 소비(=false 저장). 웹 모드는 본질적으로 1회성.
- **Reuse over rewrite** — v2.3 httpd 자산(WebServer/Config/Control/Image/Ota/Logger/embedded_assets/PoC) 재활용.

---

## 2. Architecture

### 2.0 Options Comparison

| Option | 설명 | 평가 |
|--------|------|:---:|
| **A. 재부팅 경계 (권장)** | 플래그 저장 → 재부팅 → boot 분기에서 한쪽만 init | ✅ 단순·안전, 사용자 요구와 일치, 경합 원천 차단 |
| B. 라이브 전환(무재부팅) | TFT/LVGL deinit + SPI 해제 후 웹 기동 | ❌ teardown race, TFT_eSPI/LVGL 정리 리스크 (v2.4류 위험) |

→ **Option A 채택.**

### 2.1 Boot-Mode Branch (핵심 구조)

```
setup():
  Serial + SPIFFS + ConfigManager.load(deviceConfig,...)      # 공통
  parse serverURL
  mqttEthernet_init()  (ETH/W5500 SPI 점유)                    # 공통 (두 모드 다 네트워크 필요)

  if (deviceConfig.webConfigMode):        ── WEB MODE ──
      deviceConfig.webConfigMode = false                       # (1) 즉시 소비 → 저장 (안티브릭)
      ConfigManager.saveDeviceConfig(deviceConfig)
      [정적 안내 1회] TFT 최소 init → "웹 설정 모드 / http://<IP>:80" drawString 1회 → 이후 TFT 무접근
      attach: imageApi/configApi/webLogger/otaApi/controlApi
      webServer.begin(80, &touchAuth)
      webModeStartMs = millis()
      # loop(): 웹 모듈 loop + 무활동 타임아웃 watchdog. lv_timer_handler/터치 폴링 호출 안 함.

  else:                                    ── LCD MODE ──
      lvgl_touch_init(240,320); ui_init(); (+ long-press cb)
      # webServer.begin() 호출 안 함
      # loop(): lv_timer_handler + 터치 + MQTT (v2.5 수준, 웹 미구동)
```

**불변식(Invariant)**: 웹서버가 요청을 서비스하는 동안 **TFT SPI transaction 0건**. 정적 안내는 webServer.begin() *이전*에 1회만 그리고, 웹 모드 loop에서는 lv_timer_handler를 절대 호출하지 않는다. (v2.4가 실패한 지점은 "서비스 중 동시 TFT transaction"이었음 — 여기선 존재하지 않음.)

### 2.2 Mode Transition (State)

```
 [LCD MODE] ──(설정화면 "웹 인터페이스 사용")──▶ flag=true,save,restart
     ▲                                                    │
     │                                              [reboot]
     │                                                    ▼
     │                                        [WEB MODE boot: flag 소비→false,save]
     │                                                    │
     └──(restart)──┬── 웹 "완료·재부팅" endpoint ──────────┤
                   ├── 무활동 10분 타임아웃 watchdog ───────┤
                   └── 전원 재인가/워치독 (flag 이미 false) ─┘
```

### 2.3 Dependencies
- 기존: TFT_eSPI, LVGL/ui, ETH.h(W5500), esp_http_server, PubSubClient, ArduinoJson, SPIFFS, Update(OTA)
- 신규 라이브러리 **0** (전부 재활용/기존)

---

## 3. Data Model

### 3.1 DeviceConfig 확장
```cpp
// src/config/DeviceConfig.h
class DeviceConfig {
  ...
  bool webConfigMode = false;   // v2.6: true면 다음 부팅을 웹 설정 모드로 (부팅 시 소비)
};
```

### 3.2 deviceconfig.json 스키마 (하위호환)
```json
{ "...": "...", "webConfigMode": false }
```
- 필드 부재(구버전 config) → `false` 기본. ConfigManager.load 에서 `containsKey` 가드.

### 3.3 웹 모드 런타임 상태 (RAM only)
| 변수 | 용도 |
|------|------|
| `webModeStartMs` | 무활동 타임아웃 기준 (마지막 요청 시 갱신) |
| `lastWebActivityMs` | 각 endpoint 진입 시 갱신 (watchdog reset) |

---

## 4. Mode / API Specification

### 4.1 진입 (LCD → WEB)
- 트리거: 장치 설정 화면(`DeviceManager::showDeviceSet`)의 "웹 인터페이스 사용" 버튼
- 동작: `deviceConfig.webConfigMode=true` → `ConfigManager.saveDeviceConfig` → `ESP.restart()`

### 4.2 웹 모드 endpoint (v2.3 재활용 + 신규 1)
| Endpoint | 출처 | 비고 |
|---|---|---|
| `/`, `/api/status`, config/control/image/log endpoints | v2.3 httpd 5모듈 | 재활용 |
| `POST /api/ota` (chunk upload) | OtaApi (v2.3) | **설정 보존** 추가 (§4.4) |
| **`POST /api/exit-webmode`** | 신규 | flag 이미 false → 단순 `ESP.restart()` (웹UI "완료·재부팅" 버튼) |

- 포트 `:80` (기존 유지, URL 간결). 인증 `admin/12345` (`TouchAuth` 재사용).

### 4.3 종료 (WEB → LCD)
1. 웹 "완료·재부팅" (`/api/exit-webmode`) → restart
2. 무활동 10분(`WEB_IDLE_TIMEOUT_MS=600000`) watchdog → restart
3. 전원 재인가/브라운아웃 → flag 이미 소비되어 false → LCD 부팅

### 4.4 OTA 설정 보존 (RemoteDeck_PC v2.5.1 패턴 재사용)
- SPIFFS OTA는 파티션 통째 덮어씀 → OTA 전 `deviceconfig.json`/`serverconfig.json`/`imagesconfig.json` backup, 후 restore. (참고: [[project_spiffs_ota_preserve]])
- 펌웨어(app) OTA는 SPIFFS 무영향 — 구분 처리.

---

## 5. UI/UX Design

### 5.1 LCD 진입점
- 장치 설정 화면에 "웹 인터페이스 사용" 항목(버튼/스위치). 선택 시 확인 후 재부팅.

### 5.2 웹 모드 정적 안내 화면 (OQ1 결정 = 정적 1회)
- webServer.begin 직전 TFT에 1회 렌더: `웹 설정 모드` + `http://<IP>:80` + `admin / 12345` + `완료하려면 웹에서 재부팅`.
- 이후 TFT 무접근(웹 모드 loop에 lv_timer_handler 없음) → 경합 없음.
- **Fallback**: PoC에서 잔여 경합이 관측되면 정적화면도 생략(완전 off)로 후퇴 (SC-6로 판정).

### 5.3 웹 UI
- v2.3 embedded_assets(index/app.js/style) 재활용 — 상태·설정·제어·로그·OTA 탭. "완료·재부팅" 버튼 추가.

---

## 6. Error Handling / Fail-Soft (안티브릭)

| 상황 | 처리 |
|------|------|
| 웹 모드 진입 후 방치 | 10분 타임아웃 → 자동 LCD 복귀 |
| 웹 모드 중 전원 손실 | flag 이미 false → LCD 부팅 |
| ETH 미연결(웹모드) | IP 없으면 안내화면에 오류 + 30초 후 재부팅(LCD) |
| config save 실패 | 진입 취소(플래그 미저장) → LCD 유지 |
| OTA 실패 | Update.abort + 기존 펌웨어 유지 + 웹 에러 응답 |

---

## 7. Security
- 웹 모드 전 endpoint `TouchAuth`(admin/12345) 인증. LCD 모드에서는 웹 포트 미개방(begin 미호출)이라 표면 최소.

---

## 8. Test Plan (PoC-First)

### 8.1 L2 — PoC Gate (SC-1, 최우선) — `test/poc/v24_poc.py` 재사용
| # | 시나리오 | v2.4 | v2.6 기대 |
|---|---------|:---:|:---:|
| P1 | 6 동시 GET / | ❌ | ✅ (TFT 미구동) |
| P2 | 6동시 ×5 burst | ❌ | ✅ |
| P3 | 30초 sustained | ❌ | ✅ |
| P4 | heap 안정 + status | ❌ | ✅ |
- **판정**: P1~P4 통과 시 가설 입증 → 후속 진행. 실패 시 §5.2 fallback(완전 off) 재시도 후에도 실패면 가설 재검토.

### 8.2 L1 — API/기능
- OTA 업로드→재부팅→버전 확인 (SC-2), 설정 변경 persist→LCD 복귀 반영 (SC-3), exit-webmode/타임아웃 복귀.

### 8.3 L3 — LCD Regression (SC-4)
- LCD 모드 부팅에서 터치/MQTT/이미지/롱프레스 진입 기존대로. 웹서버 미구동 확인(포트 closed).

### 8.4 왕복/안정 (SC-5)
- LCD→web→LCD 3회, heap 안정, flag 소비 정상.

### 8.5 SC-6 (transaction 0)
- 웹 모드 부팅 시리얼 로그로 TFT flush 호출 0건(서비스 구간) 확인.

---

## 9. Clean Architecture
- Layer: config(DeviceConfig/ConfigManager) → boot orchestrator(main.cpp) → web(httpd 모듈) / ui(LVGL). 모드 분기는 orchestrator(main.cpp) 책임, 각 모듈은 모드 무지(mode-agnostic).
- 의존 규칙: web 모듈은 ui/LVGL 미참조(웹 모드에서 LVGL 미init 이므로 링크/런타임 안전).

---

## 10. Coding Convention
- Design Reference 주석: `// v2.6: <의도>` (RemoteDeck_PC 관례와 동일).
- 버전 스탬프: `imageApi.setFirmwareInfo("2.6.0-webmode", ...)` + deviceconfig version.

---

## 11. Implementation Guide

### 11.1 File Structure
| 파일 | 변경 | 내용 |
|------|:---:|------|
| `src/config/DeviceConfig.h` | 수정 | `webConfigMode` 필드 |
| `src/config/ConfigManager.{cpp,h}` | 수정 | load/save + 하위호환 가드 |
| `src/main.cpp` | 수정 | setup() boot 분기 + 웹모드 loop + flag 소비 + 정적화면 + 타임아웃 watchdog |
| `src/device/DeviceManager.{cpp,h}` (또는 ui) | 수정 | "웹 인터페이스 사용" 버튼 + handler |
| `src/web/OtaApi.{cpp,h}` | 수정 | SPIFFS OTA 설정 backup/restore |
| `src/web/ConfigApi.cpp` (또는 신규) | 수정 | `/api/exit-webmode` |
| `src/web/embedded_assets.*` | 수정 | "완료·재부팅" 버튼 |
| `web/WebServer/ImageApi/ControlApi/Logger` | 재활용 | v2.3 그대로 |
| `test/poc/v24_poc.py` | 재활용 | SC-1 gate |

### 11.2 Implementation Order (PoC-first gate)
1. **S1 (Gate)**: DeviceConfig+ConfigManager 플래그 → main.cpp boot 분기(웹모드=TFT init 스킵, LCD모드=web 미구동) → **PoC SC-1**. ❌면 중단/재검토.
2. S2: 정적 안내화면 + 무활동 타임아웃 + exit-webmode.
3. S3: OTA 설정 보존 + 설정 변경 persist 검증.
4. S4: LCD 진입 버튼(DeviceManager) 연결.
5. S5: 왕복/regression 검증 → analysis/report/archive.

### 11.3 Session Guide
- S1 통과가 전체 성패. 최소 변경으로 boot 분기만 먼저 세우고 v24_poc.py 를 돌린다.

---

## Open Questions 결정
| # | 질문 | 결정 | 근거 |
|---|------|------|------|
| OQ1 | 웹모드 LCD 표시 | **정적 1회 안내 표시** (SPI 경합 관측 시에만 완전 off 후퇴) | 사용자 확정 (2026-09-11) — 웹 설정모드 표시 |
| OQ2 | 웹 포트 | **:80 유지** | 사용자 확정 (2026-09-11) — touch 기존 80 유지 |
| OQ3 | 무활동 타임아웃 | **10분** | 권장값 (변경 가능) |
| OQ4 | 인증 | **admin/12345 재사용** | 권장값 (기존 TouchAuth) |
| OQ5 | 진입 UI 위치 | **장치 설정 화면 내** | 권장값 (사용자 요구 "장치설정으로 들어가서") |

---

## Version History
| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-09-11 | Initial — Option A(부팅 경계) 확정, one-shot flag 안티브릭, PoC-first S1 gate | KDI |
