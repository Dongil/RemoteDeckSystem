# RemoteDeck_Touch_v2.5 Planning Document

> **Summary**: WebServer 코드 전체 제거 + v2.1 LCD/MQTT only 패턴 복귀. 외부 admin 은 MQTT 명령만 사용. v2.4 시간 분할 폐기 이후 깔끔한 sunset 사이클.
>
> **Project**: RemoteDeck_Touch
> **Version**: v2.5.0 (target)
> **Author**: KDI
> **Date**: 2026-06-26
> **Status**: Draft
> **Branch**: `v2.5-sunset`

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.3 SPI 충돌 + v2.4 시간 분할 가설 폐기로 WebUI/PNG/OTA 본질 해결 불가. 단말 펌웨어에 dead code (esp_http_server 5 모듈 + WebUI 자산) 가 잔존 → Flash 56% 사용 + main loop 의 useless callback 호출 + 코드 가독성 저하. |
| **Solution** | **WebServer 코드 전체 제거** (src/web/ 디렉토리 + data/www/ + tools/embed_www.py + test/poc/ web 스크립트 + main.cpp WebServer 코드). v2.1 LCD/MQTT only 패턴 복귀 + v2.1~v2.3 의 모든 개선사항 (LAN 통일 / Long-click / Sleep / BMP 안전장치) 유지. |
| **Function/UX Effect** | 외부 admin 은 **MQTT 명령만** (room/{device_id} publish — v2.1/v2.3 동일 매커니즘). LCD + Touch + MQTT 양방향 정상. Flash ~50KB 절약 + main loop 단순화 + 코드 가독성 회복. WebServer 관련 학습 자산은 v2.3-httpd / v2.4-spi 브랜치 보존. |
| **Core Value** | **WebUI 실험 cycle 정식 마감** — v2.3-v2.4 의 본질 한계 (SPI 공유) 가 코드 제거로 운영 안정성 확정. 향후 H/W 변경 또는 ESP32-S3/WROVER 채택 시 v2.6 cycle 에서 깨끗한 base 위에 재시도 가능. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.3/v2.4 의 본질 한계 정식 수용 — WebServer dead code 제거로 운영 안정화 |
| **WHO** | 단말 사용자 (현장 LCD 터치) + 외부 admin (MQTT publisher) |
| **RISK** | main.cpp 정리 시 의존성 깨짐 — 검증: 빌드 + L1 단말 동작 |
| **SUCCESS** | 빌드 통과 + Flash ~50KB 감소 + LCD/Touch/MQTT v2.1 동작 보존 |
| **SCOPE** | Phase 1 src/web/ + data/www/ + tools/ + test/poc/ 제거 → 2 main.cpp 정리 → 3 빌드/검증 |

---

## 1. Overview

### 1.1 Purpose

v2.3 (Match Rate 86.4%, SPI 충돌 발견) + v2.4 (Match Rate 12.1%, 시간 분할 폐기) 의 본질 한계 정식 수용 → WebServer 코드 dead weight 제거. 단말 운영 펌웨어를 **v2.1 패턴 (LCD + Touch + MQTT only) + v2.1~v2.3 의 모든 개선사항** 로 정리.

### 1.2 Background

- **v2.1 (Match Rate 68%, archived)**: LAN 스택 통일 + Long-click 1회 진입 + Sleep 저장 + BMP 안전장치
- **v2.3 (86.4%, archived)**: esp_http_server 5 모듈 완성 / SPI 충돌로 WebUI/PNG/OTA 비활성, minimal HTML 운영
- **v2.4 (12.1%, archived)**: 시간 분할 가설 폐기, 단말 v2.3-final 롤백
- **v2.5 결단**: 사용자 선택 — **WebServer 코드 전체 제거** (E + 추가). dead code 정리 + 운영 안정성 확정

### 1.3 Related Documents

- v2.3 archive: `docs/archive/2026-06/RemoteDeck_Touch_v2.3/`
- v2.4 archive: `docs/archive/2026-06/RemoteDeck_Touch_v2.4/`
- 학습 자산 브랜치: `v2.3-httpd` (origin), `v2.4-spi` (origin)

---

## 2. Scope

### 2.1 In Scope

- [ ] **src/web/ 디렉토리 전체 삭제** (WebServer.{h,cpp}, ImageApi.{h,cpp}, ConfigApi.{h,cpp}, Logger.{h,cpp}, ControlApi.{h,cpp}, OtaApi.{h,cpp}, embedded_assets.{h,cpp})
- [ ] **data/www/ 디렉토리 전체 삭제** (index.html, style.css, app.js)
- [ ] **tools/embed_www.py 삭제**
- [ ] **test/poc/ 의 WebServer 관련 스크립트 삭제** (v24_poc.py, control_verify.py, capture_serial.py, run_poc.sh, mqtt_pub.py, README.md 등)
- [ ] **main.cpp WebServer 관련 코드 제거**:
  - `#include "web/*"` 모두 제거
  - WebServer / ImageApi / ConfigApi / Logger / OtaApi / ControlApi / TouchAuth 인스턴스 제거
  - setup() 의 attach/begin/setMqttPublisher 호출 제거
  - loop() 의 imageApi/configApi/otaApi.loop() 호출 제거
- [ ] **platformio.ini 정리** — WebServer 관련 build_flags 잔존 시 제거 (CONFIG_HTTPD_* 등)
- [ ] **빌드 통과 + Flash 감소 확인** (~50KB 절약 예상)
- [ ] **단말 부팅 + LCD/Touch/MQTT 검증** — v2.1 동작 그대로
- [ ] **MQTT 외부 admin 검증** — room/client publish 로 IN/OUT 토글 동작
- [ ] **Branch `v2.5-sunset`** — main 운영 펌웨어 (v2.3-final) 보호

### 2.2 Out of Scope

- ❌ WebUI 재구현 시도 (v2.5 는 완전 제거)
- ❌ HTTP API 대체 (MQTT 만 사용)
- ❌ SPI freq / mutex / rewire (v2.4 archive 의 옵션 A/B/C/D 모두 deferred)
- ❌ PNG decoder 재활성 (LV_USE_PNG=0 유지)
- ❌ OTA partition 변경 (huge_app.csv 유지, USB flash 만)
- ❌ 시간 표시 UI (v2.3/v2.4 명시 제외 유지)
- ❌ git history 정리 (v2.3-httpd / v2.4-spi 브랜치 그대로 보존)

### 2.3 v2.5 이후 보존 사항 (v2.1~v2.3 개선)

- ✅ LAN 스택 통일 (ETH.h + ETH_PHY_W5500) — v2.1
- ✅ Long-click 1회 진입 (35회 dead-code 버그 fix) — v2.1
- ✅ Sleep 시간 저장/복원 — v2.1
- ✅ 이미지 디코드 안전장치 (BMP, 30KB heap guard) — v2.1
- ✅ TFT_eSPI 2.5.43 + build_flags 이식 — v2.1
- ✅ pioarduino 53.x (Arduino-ESP32 3.x) — v2.1
- ✅ HTTPClient (ESP32 내장, downloadFile/sendHttpMessage) — v2.1
- ✅ DHCP 15s + 재시도 — v2.1
- ✅ setup() 순서 (SPIFFS→Config→ETH→LCD) — v2.1
- ✅ MQTT 양방향 (room/{device_id}) — v2.1

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | src/web/ 디렉토리 전체 삭제 (8 파일) | High | Pending |
| FR-02 | data/www/ 디렉토리 전체 삭제 (3 파일) | High | Pending |
| FR-03 | tools/embed_www.py 삭제 | Medium | Pending |
| FR-04 | test/poc/ web 관련 스크립트 삭제 | Medium | Pending |
| FR-05 | main.cpp WebServer 관련 코드 모두 제거 | High | Pending |
| FR-06 | platformio.ini WebServer 잔존 build_flags 정리 | Medium | Pending |
| FR-07 | 빌드 통과 + Flash ~50KB 감소 확인 | High | Pending |
| FR-08 | 단말 부팅 + LCD + Touch + MQTT 정상 동작 | High | Pending |
| FR-09 | LCD regression 無 (Long-click, Sleep, 이미지, MQTT 양방향) | High | Pending |
| FR-10 | MQTT 외부 admin 검증 (room/client IN/OUT publish) | High | Pending |
| FR-11 | Branch v2.5-sunset 분리 | Medium | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement |
|----------|----------|------------|
| **Flash 사이즈** | v2.3-final 1.83MB → v2.5 ~1.78MB (-50KB 예상) | platformio build 출력 |
| **RAM 사이즈** | v2.3-final 36.9% → v2.5 ~33-34% (-3% 예상) | platformio build 출력 |
| **부팅 시간** | v2.1 동일 (~7s) | Serial log 또는 LED |
| **운영 안정성** | uptime monotonic + heap 안정 | LCD 또는 serial |
| **Compatibility** | Arduino-ESP32 3.x + pioarduino 53.x + TFT 2.5.43 | platformio compile |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-11 전부 완료
- [ ] 빌드 통과 (RAM/Flash 감소 확인)
- [ ] 단말 부팅 + LCD 메인 화면 표시
- [ ] Long-click → DeviceManager 진입 정상 (v2.1)
- [ ] Sleep 시간 변경 → 재부팅 → 값 유지 (v2.1)
- [ ] MQTT room/{device_id} 수신 → LCD ibtnRoom IN/OUT 갱신
- [ ] MQTT room/client publish (LCD IN/OUT 클릭) 정상
- [ ] Gap Analysis Match Rate ≥ 90%
- [ ] v2.5-sunset → main merge

### 4.2 Quality Criteria

- [ ] 단말 24시간 운영 후 heap leak 없음 (가능하면 검증)
- [ ] Flash sector erase 후 fresh boot 정상
- [ ] LCD 동작 v2.1 100% 동일

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| main.cpp 정리 시 의존성 깨짐 (compile error) | High | Medium | 의존성 순서 정리 (web 제거 → main 수정 → 빌드 반복) |
| ImageApi.h 의 buildStatusJson 이 다른 곳에서 사용되는 경우 | Medium | Low | grep 검증 후 제거 |
| HTTPClient `http` 변수 사용처 확인 (downloadFile/sendHttpMessage 는 v2.1 부터 존재) | Medium | Low | http 전역 그대로 유지 (web 과 무관) |
| 잘못된 파일 제거로 LCD/MQTT 동작 영향 | High | Low | git checkout 으로 복구 가능 + 단계별 commit |
| 단말 flash 후 mqttEthernet_init() fail (v2.4 와 동일 환경) | Low | Low | v2.1 코드 그대로 — 검증된 상태 |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `RemoteDeck_Touch/src/web/` | Delete dir | 8 파일 전체 (WebServer/ImageApi/ConfigApi/Logger/ControlApi/OtaApi/WebActivityMonitor/embedded_assets) |
| `RemoteDeck_Touch/data/www/` | Delete dir | 3 파일 (index.html, style.css, app.js) |
| `RemoteDeck_Touch/tools/embed_www.py` | Delete | gzip 압축 스크립트 (v2.5 더 이상 필요 X) |
| `RemoteDeck_Touch/test/poc/` | Partial delete | v24_poc.py / control_verify.py / capture_serial.py / run_poc.sh / mqtt_pub.py / README.md 제거. dummy.bmp 등 잔존 시 제거 |
| `RemoteDeck_Touch/src/main.cpp` | Modify | web/* include + 인스턴스 + setup attach + loop() 호출 모두 제거 |
| `RemoteDeck_Touch/src/lvgl_touch.h` | Revert | v2.4 의 extern getTouch 제거 (v2.3 상태로 복귀) |
| `RemoteDeck_Touch/platformio.ini` | Modify | CONFIG_HTTPD_* build_flags 정리 (있으면 제거) |

### 6.2 Current Consumers (확인 필요)

| Resource | 사용처 | Impact |
|----------|--------|--------|
| ImageApi.h | main.cpp 외 ? | grep 검증 |
| ControlApi.h | main.cpp message_process | 제거 시 controlApi.notifyState 호출 제거 |
| HTTPClient `http` 전역 | main.cpp downloadFile/sendHttpMessage | 유지 (web 무관) |
| MQTT 콜백 mqtt_ReceivedCallback | message_process | 유지 |
| DeviceManager / images.cpp | main.cpp + lvgl_touch | 유지 |

### 6.3 Verification

- [ ] grep `web/` 으로 모든 의존성 확인 후 제거
- [ ] grep `ImageApi|ConfigApi|Logger|OtaApi|ControlApi|WebServer|WebActivityMonitor` 으로 잔존 참조 확인
- [ ] 빌드 → main.cpp 컴파일 에러 없음
- [ ] 빌드 → linker undefined reference 없음

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

| Level | Selected |
|-------|:--------:|
| Starter (임베디드 단일 펌웨어) | ☑ |

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| **WebServer 처리** | 영구 비활성 (v2.4 E) / 코드 제거 (v2.5) | **코드 전체 제거** | 사용자 선택. dead code 정리 |
| 외부 admin 메커니즘 | HTTP API / MQTT / 둘 다 / 없음 | **MQTT 만** | 사용자 답변 |
| Branch 전략 | main 직접 / v2.5-sunset 신규 | **v2.5-sunset** | main 운영 펌웨어 (v2.3-final) 보호 |
| v2.3-httpd / v2.4-spi 브랜치 | merge / 보존 / 삭제 | **보존** | 학습 자산 + 향후 v2.6 base 가능성 |
| HTTPClient `http` 전역 | 유지 / 제거 | **유지** | v2.1 부터 downloadFile/sendHttpMessage 에서 사용 (web 무관) |

### 7.3 Folder Structure (제거 후)

```
RemoteDeck_Touch/
├── platformio.ini                  [정리] CONFIG_HTTPD_* 제거
├── lib/lv_conf.h                   (그대로)
├── data/                           [정리] www/ 제거, deviceconfig.json 등 SPIFFS data 유지
├── src/
│   ├── main.cpp                    [수정] WebServer 관련 코드 모두 제거
│   ├── lvgl_touch.{h,cpp}          [정리] extern getTouch 제거 (v2.3 복귀)
│   ├── images/                     (그대로 — BMP 안전장치)
│   ├── device/                     (그대로)
│   ├── mqtt/                       (그대로)
│   ├── config/                     (그대로)
│   └── utils/                      (그대로)
├── tools/                          [정리] embed_www.py 제거 (있으면)
└── test/poc/                       [정리] 모든 web 관련 스크립트 제거
```

`src/web/` 디렉토리 완전 제거.

---

## 8. Convention Prerequisites

### 8.1 Existing Project Conventions

- [x] `CLAUDE.md` 존재
- [x] platformio.ini build_flags
- [x] Arduino-ESP32 3.x + pioarduino 53.x 고정
- [x] v2.1 setup() 순서 (SPIFFS→Config→ETH→LCD)

### 8.2 v2.5 컨벤션

- 코드 제거는 commit 단위로 분리 (src/web/ → data/www/ → tools/ → test/poc/ → main.cpp → platformio.ini)
- 각 단계 후 grep 으로 잔존 참조 확인
- 빌드 통과 후 단말 검증

---

## 9. Implementation Phases

| Phase | 범위 | Gate |
|-------|------|-----|
| **Phase 1 — 디렉토리/파일 제거** | src/web/ + data/www/ + tools/embed_www.py + test/poc/ web 관련 | 디렉토리 비어있음 |
| **Phase 2 — main.cpp 정리** | include + 인스턴스 + setup + loop + lvgl_touch.h revert | grep 잔존 참조 0 |
| **Phase 3 — 빌드 + Flash 감소 확인** | platformio.ini 정리 + 빌드 | 빌드 통과 + Flash -50KB 확인 |
| **Phase 4 — 단말 검증** | flash + LCD + MQTT 동작 + L3 시나리오 | LCD M1~M5 통과 |
| **Phase 5 — Gap Analysis + Report + Archive** | Match Rate 산출 + v2.5 마무리 | Match Rate ≥ 90% |

---

## 10. Next Steps

1. [ ] Design 문서 작성 (`/pdca design RemoteDeck_Touch_v2.5`) — 3 Architecture Options + 제거 순서 상세
2. [ ] v2.5-sunset 브랜치 생성 (v2.3-httpd 에서 분기)

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — WebServer 전체 제거 + v2.1 LCD/MQTT only 패턴 복귀 | KDI |
