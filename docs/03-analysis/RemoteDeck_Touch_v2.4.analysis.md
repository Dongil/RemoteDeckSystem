# RemoteDeck_Touch_v2.4 Gap Analysis Document

> **Summary**: 시간 분할 architecture (WebActivityMonitor + freeze/resume + deferred 콜백) 코드 완성, 그러나 **PoC P1 (단일 GET /) 부터 fail**. 시간 분할 가설 폐기. Plan SC FR-12 정책에 따라 v2.5 분기 결정.
>
> **Project**: RemoteDeck_Touch
> **Version**: v2.4.0-spi (실험)
> **Author**: KDI
> **Date**: 2026-06-26
> **Status**: Check (Match Rate 산출 — PoC fail)
> **Branch**: `v2.4-spi` (origin push 완료, 학습 자산 보존)
> **Plan Doc**: [RemoteDeck_Touch_v2.4.plan.md](../01-plan/features/RemoteDeck_Touch_v2.4.plan.md)
> **Design Doc**: [RemoteDeck_Touch_v2.4.design.md](../02-design/features/RemoteDeck_Touch_v2.4.design.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.3 SPI 충돌 해결 + 보존된 5 모듈/WebUI/PNG/OTA 모두 재활성 |
| **WHO** | 외부 admin (브라우저) / 현장 사용자 (LCD 터치) |
| **RISK** | 시간 분할도 fail → PoC 통과 안 되면 v2.5 분기 (현실화됨) |
| **SUCCESS** | PoC 엄격 통과 + Match Rate ≥ 90% + LCD regression 無 |
| **SCOPE** | Phase 1 SPI PoC → 2 WebUI restore → 3 PNG → 4 OTA partition → 5 검증 |

---

## 1. Strategic Alignment Check

| 검증 항목 | 결과 |
|-----------|:---:|
| Plan 의 핵심 문제 (SPI 충돌 해결) 해결? | ❌ |
| v2.3 자산 재활용 시도? | ✅ INDEX_HTML_GZ 재활성 코드 작성 |
| 시간 분할 architecture 코드 작성? | ✅ WebActivityMonitor + freeze/resume + deferred |
| PoC 통과? | ❌ P1 (단일 GET /) 부터 fail |
| Plan SC FR-12 (PoC fail → v2.5 분기) 정책 적용? | ✅ 즉시 결정 |

### 시간 분할 가설 폐기 사유 (재진단)

```
[Core 0 httpd task]                  [Core 1 main loop]
markActive() (즉시 active=true)        pending_freeze flag 확인
pending_freeze = true                  (10ms 후 다음 tick)
응답 send 시작 (ETH SPI 점유)            freezeLCD() (TFT SPI 점유 시도)
   ↓                                          ↓
   └────── SPI host driver mutex wait ──────┘
                        ↓
       응답 6초 timeout 초과 → client connection drop
```

**본질 원인**:
- ESP32 의 SPI host driver 가 host-level mutex 제공하지만 **transaction wait 시간 무제한**
- ETH 응답 send (수십 KB) 와 TFT freezeLCD (fillScreen + drawString) 가 동시 시도 시 mutex wait
- mutex wait 시간이 누적되어 client (curl 6초 timeout) 초과
- deferred 콜백 (core 0→1 sync) 도 race window 만 줄였을 뿐 mutex contention 해결 안 됨

---

## 2. Plan Success Criteria Status

| ID | Requirement | Priority | Status | Evidence |
|----|-------------|----------|:---:|----------|
| FR-01 | Web active mode 진입 (markActive) | High | ⚠️ Code OK / runtime fail | WebActivityMonitor.cpp 동작, 단 응답 race |
| FR-02 | LCD freeze + 안내 텍스트 | High | ⚠️ Code OK / runtime untested | freezeLCD() 작성, PoC 단계에서 검증 못 함 |
| FR-03 | 10초 idle 자동 복귀 | High | ⚠️ Code OK / runtime untested | shouldFreezeLcd() timeout 로직 |
| FR-04 | LCD touch tap-to-acquire | High | ⚠️ Code OK / runtime untested | notifyTouch() + getTouch polling |
| FR-05 | INDEX_HTML_GZ 재활성 | High | ⚠️ Code OK / runtime fail | handleRoot 변경, 단 GET / 자체 fail |
| FR-06 | 4탭 WebUI 운영 | High | ❌ 검증 불가 | GET / fail 로 진입 불가 |
| FR-07 | PNG decoder 재활성 | High | ❌ Not started | module-png-restore 진입 안 함 |
| FR-08 | OTA partition 변경 | High | ❌ Not started | module-ota-partition 진입 안 함 |
| FR-09 | SPIFFS backup/restore | Medium | ❌ Not started | tools/spiffs_*.py 미작성 |
| FR-10 | OTA 실제 동작 | High | ❌ Not started | partition 변경 안 함 |
| FR-11 | LCD regression 無 | High | ⚠️ untested | PoC 단계에서 확인 못 함 |
| FR-12 | PoC 엄격 통과 (v2.5 분기 기준) | High | ❌ **Not Met → v2.5 분기 정책 적용** | P1 fail |

**Overall FR Score**: Met 0 / Code-only 5 / Not Met 7 (12개 중)
→ 정량화: 0×1.0 + 5×0.3 + 7×0 = **1.5 / 12 = 12.5%** (Functional Depth)

---

## 3. Structural Match (Module Existence)

| Module | Expected (Design §11.1) | Implemented | Status |
|--------|-------------------------|-------------|:---:|
| `src/web/WebActivityMonitor.{h,cpp}` | 신규 | ✅ 작성 (commit 4b1037c) | ✅ |
| `src/web/WebServer.h` 수정 (setActivityMonitor) | 수정 | ✅ | ✅ |
| `src/web/WebServer.cpp` (requireAuth markActive + handleRoot INDEX_HTML_GZ) | 수정 | ✅ | ✅ |
| `src/main.cpp` (freezeLCD/resumeLCD + main loop 분기 + touch poll + deferred) | 수정 | ✅ | ✅ |
| `src/lvgl_touch.h` (extern getTouch) | 수정 | ✅ | ✅ |
| `test/poc/v24_poc.py` | 신규 | ✅ | ✅ |
| `tools/spiffs_backup.py` | 신규 (module-ota-partition) | ❌ Not started | ❌ |
| `tools/spiffs_restore.py` | 신규 (module-ota-partition) | ❌ Not started | ❌ |
| `platformio.ini` partition 변경 | 수정 | ❌ Not started | ❌ |
| `lib/lv_conf.h` LV_USE_PNG=1 | 수정 (module-png-restore) | ❌ Not started | ❌ |

**Structural Match**: 6/10 = **60%**

---

## 4. API Contract

v2.3 의 14 endpoint 그대로 유지 (코드 변경 없음). 단 GET / 시도 시 단말 hang → 운영 검증 불가.

| Aspect | Score |
|--------|:---:|
| 코드 구조 보존 (v2.3 14 endpoint) | 100% |
| 운영 검증 가능 | 0% (PoC fail) |

**Contract Match**: 정량화 어려움. 평균 **0%** (운영 검증 안 됨)

---

## 5. Runtime Verification

### L1 — API Tests
| Test | Result |
|------|--------|
| 단말 부팅 (fw=2.4.0-spi) | ✅ |
| 단말 alive (heap 39KB, uptime monotonic) | ✅ |
| 단일 GET / | ❌ size=0, 6초 timeout |
| 다른 endpoint 검증 | ❌ 단일 GET 도 fail 이라 진입 못 함 |

### L2 — PoC v2.4 Gate (test/poc/v24_poc.py)
| # | 시나리오 | 결과 |
|---|---------|------|
| P1 | brower 6 동시 GET / (22KB inline) | ❌ 6/6 fail |
| P2 | 6 동시 × 5 burst | ❌ 30/30 fail |
| P3 | 30초 sustained | ❌ 15/15 fail |
| P4 | heap 안정 | ❌ status fetch timeout |
| P5 | LCD touch resume | ❌ 검증 불가 (web 진입 자체 fail) |

### L3 — 수동 시나리오
검증 단계 진입 안 함 (PoC fail).

**Runtime Score**: **0%**

---

## 6. Match Rate Calculation

### Formula (static + runtime)

```
Overall = (Structural × 0.15) + (Functional × 0.25) + (Contract × 0.25) + (Runtime × 0.35)
        = (60 × 0.15) + (12.5 × 0.25) + (0 × 0.25) + (0 × 0.35)
        = 9.0 + 3.125 + 0 + 0
        = 12.1%
```

**Match Rate: 12.1%** (PoC fail, 후속 모듈 진입 못 함)

---

## 7. Gap List

### Critical — 가설 폐기 (Iterate 불가)

| ID | Gap | 원인 | v2.4 Iterate? |
|----|-----|------|:---:|
| C-1 | **시간 분할 가설 폐기** (FR-01,02,05) | ESP32 SPI host driver mutex 가 transaction wait 무제한 — race window 회피 불가 | ❌ 본질 한계 |
| C-2 | PoC P1~P4 전체 fail | C-1 의 직접 결과 | ❌ |
| C-3 | module-webui/png/ota/verify 미진입 (FR-06~10) | PoC gate 통과 못 함 | ❌ Plan 정책 |

### Important — 코드 작성됐으나 검증 불가

| ID | Gap | 상태 |
|----|-----|------|
| I-1 | LCD freeze 안내 텍스트 (FR-02) | freezeLCD() 코드 OK, 동작 검증 못 함 |
| I-2 | LCD touch tap-to-acquire (FR-04) | notifyTouch() 코드 OK, 동작 검증 못 함 |
| I-3 | 10초 idle 복귀 (FR-03) | shouldFreezeLcd() 코드 OK, 동작 검증 못 함 |

### Positive Findings — v2.5 재활용 가능

| ID | 발견 | 가치 |
|----|------|------|
| P-1 | **WebActivityMonitor 클래스 자체는 깔끔** | v2.5 의 다른 SPI 해결 방안 (freq 조정 등) 과 조합 가능 |
| P-2 | **deferred 콜백 패턴** (core 0→1 sync) | v2.5 에서 SPI mutex 추가 시 동일 패턴 활용 |
| P-3 | **PoC v24_poc.py 스크립트** | v2.5 PoC 그대로 재사용 가능 (brower 6 동시 + 22KB + sustained + MQTT) |
| P-4 | **ESP32 SPI host driver 한계 명확화** | v2.5 의 접근 방식 정확히 좁힘 (시간 분할 X, freq 조정 / H/W 분리 / 보드 변경) |

---

## 8. Decision Record Verification

| Decision | Followed? | Outcome |
|----------|:---:|---------|
| [Plan] 시간 분할 (사용자 통찰) | ✅ | 코드 완성, runtime fail |
| [Plan] PoC 엄격 (P1~P5) | ✅ | P1 부터 fail 명확히 검출 |
| [Plan] Branch v2.4-spi | ✅ | origin push, 자산 보존 |
| [Plan] **PoC fail 시 v2.5 분기** | ✅ | **즉시 결정** (Plan SC FR-12) |
| [Design] Option C Pragmatic (WebActivityMonitor 단일) | ✅ | 깔끔한 코드 작성 |
| [Design] deferred 콜백 (core 0→1 sync) | ✅ | 적용했으나 race 해소 안 됨 |
| [Design] 5 세션 순차 진행 | ✅ | Session 2 (module-spi-poc) 에서 종료 |

**모든 Plan/Design 결정 정확히 따름**. 단지 핵심 가설 (시간 분할이 SPI 충돌 해결) 자체가 fail.

---

## 9. v2.5 인계 사항

### 시도 후 폐기된 접근법
- ❌ **시간 분할** (v2.4) — ESP32 SPI host driver mutex wait 한계
- ❌ **gzip 압축** (v2.3) — 사이즈 줄여도 race 가능
- ❌ **max_open_sockets 조정** (v2.3) — 4 도 7 도 본질 해결 안 됨
- ❌ **Long polling → Short polling** (v2.3) — handler block 시간만 줄임

### v2.5 의 남은 옵션
| Option | 설명 | 예상 결과 |
|--------|------|----------|
| **A. SPI freq 조정** | TFT 27MHz → 10MHz + W5500 8MHz | transaction 시간 1/3, race window 축소 (해결 보장 X) |
| **B. SPI 명시적 mutex** | TFT_eSPI 호출 전 `spi_device_acquire_bus` 명시 (ESP-IDF API) | mutex contention 더 명확하게 제어 (실효성 불확실) |
| **C. H/W rewire** | TFT 핀 → HSPI (SCK=14, MOSI=13, MISO=12) | SPI host 완전 분리 (보드 수정 필요) |
| **D. ESP32-WROVER (PSRAM)** | PSRAM + 더 큰 메모리 + dual SPI host 자연 분리 | 보드 교체 필요 |
| **E. WebUI 영구 포기** | v2.3-final 의 minimal HTML 모드 운영. API 만 외부 admin 사용 | 즉시 운영 가능 (v2.3 이미 검증됨) |

### v2.4 보존 자산 (v2.5 재활용)
- `v2.4-spi` 브랜치 (origin push, 1 commit)
- WebActivityMonitor.{h,cpp} — SPI fix 와 조합 가능
- v24_poc.py — 동일 PoC 시나리오 재사용
- deferred 콜백 패턴 — SPI mutex 도입 시 함께 사용

### 단말 현재 상태
- **v2.3-final 펌웨어 (`2.3.0-ctrl`) 운영 중** (uptime monotonic, heap 39KB)
- LCD + Touch + MQTT + API 13개 동작 (WebUI minimal)
- v2.4 시도 후 안전 롤백 완료

---

## 10. Checkpoint 5 — Review Decision 권장

Match Rate **12.1%** (< 90% 기준 큰 폭 미달). 그러나 Plan SC FR-12 명시:

> PoC fail 시 v2.5 분기 결정 (시간 분할도 본질 해결 안 됨 명백한 경우)

**즉 Iterate 가 아닌 v2.5 분기가 정답**.

| Option | 평가 |
|--------|------|
| Iterate (자동 fix) | ❌ **본질 한계는 Iterate 불가** (코드는 깔끔, 가설 자체 fail) |
| 그대로 진행 (Report) | ✅ **권장** — Plan 정책 명확. Report + Archive 후 v2.5 plan |

**권장 액션**: 그대로 Report 진행 + v2.5 분기 시작. 단말은 v2.3-final 그대로 운영.

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — Match Rate 12.1%, 시간 분할 가설 폐기, v2.5 5개 옵션 인계 | KDI |
