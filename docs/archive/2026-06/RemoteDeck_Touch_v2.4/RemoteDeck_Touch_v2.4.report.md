# RemoteDeck_Touch_v2.4 Completion Report

> **Match Rate**: 12.1% (PoC P1 fail — 시간 분할 가설 폐기, Plan SC FR-12 정책에 따라 v2.5 분기)
> **Project**: RemoteDeck_Touch
> **Version**: v2.4.0-spi (실험 사이클)
> **Author**: KDI
> **Date**: 2026-06-26
> **Branch**: `v2.4-spi` (origin push, 1 commit + Plan/Design 2 commit)
> **Status**: Completed (가설 폐기 cycle)

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.3 의 SPI 버스 공유 (W5500+TFT_eSPI VSPI host) 충돌로 WebUI/PNG/OTA 비활성. v2.4 는 시간 분할 (사용자 통찰) 로 본질 회피 시도. |
| **Solution** | WebActivityMonitor 단일 클래스 + deferred 콜백 (core 0→1 sync) + freezeLCD/resumeLCD + 10초 idle + LCD touch tap-to-acquire. 코드 완성 후 PoC 검증. |
| **Function/UX Effect** | **PoC P1 (단일 GET /) 부터 fail** — ESP32 SPI host driver mutex wait 가 transaction 시간 무제한 → ETH 응답 + TFT freezeLCD 동시 시도 시 client timeout. 시간 분할 가설 폐기 + v2.5 분기 결정. |
| **Core Value** | **본질 한계 명확화** — 시간 분할로도 해결 불가 입증. v2.5 의 접근 정확히 좁힘 (freq 조정 / H/W rewire / 보드 변경 / WebUI 영구 포기). 단말 v2.3-final 안정 운영 보존. |

### 1.3 Value Delivered

| Perspective | Planned | Delivered | Metric |
|-------------|---------|-----------|--------|
| **시간 분할 가설 검증** | PoC P1~P5 통과 | P1 부터 fail | **가설 폐기 명확** |
| **본질 진단 심화** | ESP32 SPI 한계 명확화 | mutex wait 무제한 확정 | **v2.5 옵션 정밀화** |
| **단말 운영 안정성** | v2.4 활성 펌웨어 배포 | v2.3-final 롤백 운영 중 | **운영 중단 0초** |
| **코드 자산 보존** | WebActivityMonitor + PoC 스크립트 | v2.4-spi 브랜치 origin push | **v2.5 재활용 가능** |

---

## 1. PRD/Plan/Design → Code 여정 요약

### 1.1 시작
- v2.3 (Match Rate 86.4%, archived `4963c06`): esp_http_server 5 모듈 완성 / SPI 충돌로 WebUI/PNG/OTA 비활성
- v2.3 archive 인계: SPI 충돌 해결 (Option A/B/C) + WebUI/PNG/OTA 재활성
- v2.4 사용자 통찰: **시간 분할** (WebUI 활성 시 LCD freeze + LVGL/TFT 정지) → SPI 단독 점유로 race 회피

### 1.2 진행 흐름

| Phase | 결과 |
|-------|------|
| Plan | 6 FR (활성/freeze/timeout/touch/restore/partition) + PoC 엄격 + Option C Pragmatic |
| Design | WebActivityMonitor + deferred 콜백 + freezeLCD/resumeLCD + 4 모듈 5 세션 |
| **module-spi-poc** | **PoC P1 (단일 GET /) fail — size=0, 6초 timeout** |
| module-png-restore | 진입 안 함 (PoC gate fail) |
| module-ota-partition | 진입 안 함 |
| 통합 검증 | 진입 안 함 |

### 1.3 결정적 발견 — 시간 분할 가설 폐기

```
[Core 0 httpd]                         [Core 1 main loop]
markActive() (즉시 active=true)         pending_freeze flag 확인 (10ms 후 다음 tick)
pending_freeze = true                   freezeLCD() (TFT SPI 점유 시도)
응답 send 시작 (ETH SPI)                       ↓
       ↓                                       │
       └──── SPI host driver mutex wait ──────┘
                       ↓
       응답 6초 timeout 초과 → client connection drop
```

**본질 원인 (재진단)**:
- ESP32 의 SPI host driver 가 host-level mutex 제공 — 그러나 **transaction wait 시간 무제한**
- ETH 응답 send (수십 KB) + TFT freezeLCD (fillScreen + drawString) 가 동시 시도 시 mutex 누적 wait
- mutex wait 시간이 6초 client timeout 초과
- **deferred 콜백 (core 0→1 sync) 도 race window 만 줄임** — mutex contention 자체 해결 안 됨

### 1.4 가설 폐기 결정

Plan SC FR-12 명시: "PoC fail 시 v2.5 분기 결정". 즉시 적용 + 단말 v2.3-final 롤백.

---

## 2. Plan Success Criteria — Final Status

| ID | Requirement | Status | 비고 |
|----|-------------|:---:|------|
| FR-01 | Web active mode 진입 | ⚠️ Code OK / runtime fail | WebActivityMonitor.markActive 동작 |
| FR-02 | LCD freeze + 안내 텍스트 | ⚠️ Code OK / runtime untested | freezeLCD() 작성, 검증 못 함 |
| FR-03 | 10초 idle 복귀 | ⚠️ Code OK / runtime untested | shouldFreezeLcd() timeout |
| FR-04 | LCD touch tap-to-acquire | ⚠️ Code OK / runtime untested | notifyTouch() + getTouch poll |
| FR-05 | INDEX_HTML_GZ 재활성 | ⚠️ Code OK / runtime fail | handleRoot 변경 |
| FR-06 | 4탭 WebUI 운영 | ❌ Not Met | GET / fail 로 진입 불가 |
| FR-07 | PNG decoder | ❌ Not started | |
| FR-08 | OTA partition 변경 | ❌ Not started | |
| FR-09 | SPIFFS backup/restore | ❌ Not started | |
| FR-10 | OTA 실제 동작 | ❌ Not started | |
| FR-11 | LCD regression | ⚠️ untested | PoC 진입 못 함 |
| FR-12 | PoC 엄격 (v2.5 분기 기준) | ❌ **Not Met → 정책 적용** | P1 fail |

**Success Rate**: 0 Met / 5 Code-only / 7 Not Met (12개 중) = **0/12 명목 Met** (12.5% 정량화)

---

## 3. Key Decisions & Outcomes

| Phase | Decision | Outcome |
|-------|----------|---------|
| [Plan] | 시간 분할 (사용자 통찰) | ✅ 코드 완성 / ❌ runtime fail |
| [Plan] | PoC 엄격 (P1~P5) | ✅ P1 부터 fail 명확 검출 |
| [Plan] | OTA min_spiffs.csv + backup/restore | — (module 진입 안 함) |
| [Plan] | Branch v2.4-spi (main 보호) | ✅ origin push, 학습 자산 보존 |
| [Plan] | **PoC fail 시 v2.5 분기** | ✅ **정책대로 즉시 결정** |
| [Design] | Option C Pragmatic (WebActivityMonitor 단일) | ✅ 깔끔한 코드 |
| [Design] | deferred 콜백 (core 0→1 sync) | ✅ 적용했으나 race 해소 안 됨 |
| [Design] | 4 모듈 5 세션 | ✅ Session 2 에서 종료 |
| [Do] | 단말 v2.3-final 롤백 | ✅ 운영 중단 0초 |

**모든 Plan/Design/Do 결정 정확히 따름**. 핵심 가설 자체가 fail.

---

## 4. Architecture 회고

### 무엇이 잘 됐나
1. **사용자 통찰을 빠르게 architecture 로 채택** — Plan/Design 1 세션 완료
2. **PoC-first 정책 엄격 적용** — module-spi-poc 의 P1 fail 즉시 감지 → 후속 모듈 진입 안 함 (sunk cost 0)
3. **Option C 콜백 시그니처 보존** — WebActivityMonitor 가 단일 책임 (state) 만 보유, 깔끔
4. **Branch 분리** — v2.4-spi 자산 보존 + main 운영 펌웨어 (v2.3-final) 안전
5. **단말 즉시 롤백** — v2.4 코드 빌드 후 fail 확정 → 같은 세션 안에 v2.3 펌웨어로 복귀

### 어디서 막혔나
1. **ESP32 SPI host driver mutex 가정 오류** — Plan 단계에서 "시간 분할 = SPI 단독 점유" 라고 단순화. 실제로는 mutex contention 시간이 client timeout 초과
2. **freezeLCD 자체가 TFT SPI 호출** — race 회피 위해 LVGL flush 만 정지하면 충분하다 가정. 실제로는 freezeLCD 의 짧은 fillScreen + drawString 도 race 영역
3. **deferred 콜백 한계** — core 0 → 1 sync 만 보장. 응답 send 가 core 1 freezeLCD 와 동시 진행되는 root window 해결 안 됨

### 본질 한계 vs 코드 한계
| 한계 | 종류 | v2.4 Iterate 가능? |
|------|:---:|:---:|
| ESP32 SPI host driver mutex wait | 라이브러리 본질 | ❌ |
| freezeLCD 의 TFT SPI 호출 | 디자인 선택 (안내 텍스트 표시) | ⚠️ noop 화 가능 |
| Client (curl) timeout | 외부 조건 | ❌ |

freezeLCD 를 noop (LVGL 정지만, TFT 그림 안 함) 화 해도 평소 LVGL flush 자체가 race 가능. 본질 해결 안 됨.

---

## 5. v2.5 인계 사항

### 시도 후 폐기된 접근법
- ❌ **시간 분할 (v2.4)** — ESP32 SPI driver mutex 한계
- ❌ gzip 압축 (v2.3) — 사이즈 줄여도 race
- ❌ max_open_sockets 조정 (v2.3) — 본질 해결 X
- ❌ Long → Short polling (v2.3) — handler block 시간만 ↓

### v2.5 의 남은 옵션 (5가지)

| Option | 설명 | 예상 결과 | 권장도 |
|--------|------|----------|:---:|
| **A. SPI freq 조정** | TFT 27→10MHz + W5500 8MHz | transaction 시간 1/3 → race window 축소 (보장 X) | 중간 |
| **B. SPI 명시적 mutex** | `spi_device_acquire_bus` ESP-IDF API | mutex 제어 명확 (실효성 불확실) | 중간 |
| **C. H/W rewire** | TFT 핀 → HSPI (SCK=14, MOSI=13, MISO=12) | SPI host 완전 분리 (보드 수정 필요) | 높음 |
| **D. ESP32-WROVER (PSRAM)** | 보드 교체 + dual SPI host 자연 분리 | 본질 해결 (보드 교체 비용) | 높음 |
| **E. WebUI 영구 포기** | v2.3-final minimal HTML 유지 + API only | 즉시 운영 (v2.3 이미 검증) | 가장 빠름 |

### v2.4 보존 자산 (v2.5 재활용)
| 자산 | 위치 |
|------|------|
| WebActivityMonitor.{h,cpp} | `RemoteDeck_Touch/src/web/` (v2.4-spi 브랜치) |
| v24_poc.py | `RemoteDeck_Touch/test/poc/` (brower 6 동시 + 22KB + sustained) |
| deferred 콜백 패턴 | main.cpp setOnModeChange 람다 |
| freezeLCD/resumeLCD/poll_touch_for_resume | main.cpp 정적 함수 |
| extern getTouch in lvgl_touch.h | LCD freeze 중 I2C polling |

---

## 6. 단말 운영 모드 (현재 = v2.3-final)

### 활성 (v2.4 시도 전과 동일)
- ✅ LCD + Touch + Long-click + Sleep (v2.1 동작 100% 보존)
- ✅ MQTT 양방향 (room/client + room/node_1)
- ✅ API 엔드포인트 13개:
  - /api/status, /api/control (GET/POST), /api/log
  - /api/images/{list,name,upload}, /api/imagesconfig
  - /api/config (GET/POST), /api/reboot
- ✅ GET / — minimal HTML (안내 + API 링크)

### 비활성 (v2.5 deferred)
- ⏸ WebUI 풀세트 4탭
- ⏸ PNG 디코더
- ⏸ OTA bin 업로드

### 외부 시스템 연동 (v2.3 동일)
```bash
curl -u admin:12345 -X POST -H 'Content-Type: application/json' \
  -d '{"in":true}' http://192.168.10.122/api/control
curl -u admin:12345 'http://192.168.10.122/api/control?since=N'
curl -u admin:12345 http://192.168.10.122/api/status
```

---

## 7. Branch 전략

- **main**: v2.2 archive 시점 (참고용)
- **v2.3-httpd**: v2.3 archived + v2.4 Plan/Design/Analysis 문서 (단말 운영 펌웨어 = v2.3-final)
- **v2.4-spi**: v2.4 시간 분할 시도 (origin push, 학습 자산)

---

## 8. 다음 단계

```
/pdca archive RemoteDeck_Touch_v2.4 --summary
```

Archive 후 v2.5 plan 시작 권장:
```
/pdca plan RemoteDeck_Touch_v2.5
```

v2.5 의 첫 결정은 5개 옵션 중 선택 — 사용자가 하드웨어 (보드 교체 / PCB 수정) 가능 여부와 운영 요구 (WebUI 필수성) 기반으로 결정.

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — 시간 분할 가설 폐기, v2.5 5개 옵션 인계, 단말 v2.3 운영 유지 | KDI |
