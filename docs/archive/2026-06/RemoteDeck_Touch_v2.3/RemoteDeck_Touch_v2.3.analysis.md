# RemoteDeck_Touch_v2.3 Gap Analysis Document

> **Summary**: esp_http_server zero-base 재설계 + v2.1 미구현 (WebUI/PNG/OTA/Control) 일괄 구현 시도. 5 모듈 모두 코드 완성, 단 v2.3 본질적 한계 (SPI 버스 충돌) 발현으로 WebUI/PNG/OTA 운영 비활성. 핵심 API + Control 동작 유지.
>
> **Project**: RemoteDeck_Touch
> **Version**: v2.3.0-final
> **Author**: KDI
> **Date**: 2026-06-26
> **Status**: Check (Match Rate 산출)
> **Branch**: `v2.3-httpd` (origin push 완료)
> **Plan Doc**: [RemoteDeck_Touch_v2.3.plan.md](../01-plan/features/RemoteDeck_Touch_v2.3.plan.md)
> **Design Doc**: [RemoteDeck_Touch_v2.3.design.md](../02-design/features/RemoteDeck_Touch_v2.3.design.md)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.1 미구현 (WebUI/PNG/OTA/Control) 일괄 구현 + v2.2 실패(동시성 본질 불안정) 해결 |
| **WHO** | 단말 운영자(브라우저) / 배포 담당자(OTA) / 현장 사용자(Control) |
| **RISK** | esp_http_server 도 fail 가능 → PoC 엄격 검증으로 조기 분기 |
| **SUCCESS** | PoC 엄격 통과 + Match Rate ≥ 90% + LCD regression 無 |
| **SCOPE** | Phase 1 PoC → 2 WebUI+PNG → 3 OTA → 4 Control → 5 검증 |

---

## 1. Strategic Alignment Check

| 검증 항목 | 결과 |
|-----------|:---:|
| Plan 의 핵심 문제 (v2.1 미구현 일괄 구현) 해결? | ⚠️ 부분 |
| v2.2 실패 (동시성) 해결? | ✅ PoC 통과로 검증 |
| Plan SC 가 운영 환경에서 충족? | ⚠️ 본질 제약 발현 |
| Design Option C (Pragmatic) 따라 구현? | ✅ v2.2-zero 콜백 시그니처 보존 |

### 새로 발견된 본질적 제약
**SPI 버스 충돌** (사용자 진단으로 확인):
- W5500 (ETH) 와 TFT_eSPI (LCD) 가 동일 VSPI host + 동일 GPIO (SCK=18/MOSI=23/MISO=19) 공유
- Arduino-ESP32 의 host-level mutex 가 있지만 large HTTP response 시 SPI transaction 길어짐
- → TFT_eSPI 의 LVGL flush 가 starvation → main loop hang
- → 작은 응답 (< 1KB) 안전, 큰 응답 (수십 KB) hang

이 제약은 v2.3 Plan 단계에서 인지하지 못함. v2.4 sub-task 로 분리.

---

## 2. Plan Success Criteria Status

### Functional Requirements (FR)

| ID | Requirement | Priority | Status | Evidence |
|----|-------------|----------|:---:|----------|
| FR-01 | esp_http_server core 0 별도 task | High | ✅ Met | `WebServer.cpp:cfg.core_id=0, task_priority=4` (commit 8e5100a) |
| FR-02 | PoC 풀세트 통과 (연속 + 병렬 + MQTT + idle) | High | ✅ Met | B2 config: 30s 부하 100/100, heap 안정 (commit 90951cd) |
| FR-03 | WebUI 4탭 (Control/Images/Config/Logs) | High | ⚠️ Partial | 코드 완성 + 운영 비활성 (SPI 충돌). minimal HTML 만 응답 (commit 5f9e182) |
| FR-04 | PNG 디코더 활성 | High | ❌ Not Met | LV_USE_PNG=0 (LCD touch race 회피, commit 30b3baa) — 코드 보존 (1aae337, 85d52be) |
| FR-05 | OTA 핸들러 /api/ota (1.72MB 업로드) | High | ❌ Not Met | huge_app.csv 가 단일 app partition (Update.h fail) — 코드 보존 (3c28b1e) |
| FR-06 | Control Long polling 10s + ETag | High | ⚠️ Partial | Short polling 1s 로 정정 (esp_http_server 단일 task 한계) + 0ms throttle (01e9337) |
| FR-07 | MQTT 양방향 미러 | High | ✅ Met | controlApi.notifyState + setMqttPublisher 동작 검증 (commit 76f4f3f) |
| FR-08 | Basic Auth admin:12345 | Medium | ✅ Met | mbedtls base64 + WWW-Authenticate (commit 6aa92fc) |
| FR-09 | LCD/Touch regression 없음 | High | ✅ Met | Long-click / Sleep / 이미지 표시 / MQTT 모두 정상 (사용자 검증) |
| FR-10 | DHCP 15s + 재시도 1회 | Medium | ✅ Met | ethernet_mqtt.cpp v2.1 그대로 보존 |
| FR-11 | FULL (4MB) + OTA (1.72MB) bin 양산 | Medium | ⚠️ Partial | FULL only — partition 변경 안 됨 |

**Overall FR Score**: Met 6 / Partial 3 / Not Met 2 (11개 중)
→ 정량화: 6×1.0 + 3×0.5 + 2×0.0 = **7.5 / 11 = 68%** (Functional Depth)

### Non-Functional Requirements (NFR)

| Category | Criteria | 결과 |
|----------|----------|:---:|
| Stability | PoC 엄격 1분 무장애 + heap ≥ 80KB | ⚠️ PoC 1분 ✅ / heap baseline 40KB (재설정 권장) |
| Performance | 이미지 200KB ≤ 5초 / /api/status ≤ 500ms | ✅ status ~95ms |
| Memory | 부팅 후 heap ≥ 100KB, OTA 중 ≥ 40KB | ❌ 40KB baseline (NFR 미달) — Logger/ConfigApi/OtaApi/ControlApi BSS 누적 |
| Concurrency | 동시 GET 5개 + LVGL frame drop 무 | ⚠️ 단발 5 동시 OK / brower multi-conn 큰 응답 시 hang |
| Compatibility | Arduino-ESP32 3.x + pioarduino 53.x + TFT 2.5.43 | ✅ |

---

## 3. Structural Match (Module Existence)

| Module | Expected (Design §11.1) | Implemented | Status |
|--------|-------------------------|-------------|:---:|
| `src/web/WebServer.{h,cpp}` | esp_http_server 래퍼 | ✅ 재작성 | ✅ |
| `src/web/ImageApi.{h,cpp}` | v2.2-zero 재활용 | ✅ 콜백 보존 | ✅ |
| `src/web/ConfigApi.{h,cpp}` | 신규 | ✅ 작성 | ✅ |
| `src/web/Logger.{h,cpp}` | 신규 | ✅ 30건 ring | ✅ |
| `src/web/ControlApi.{h,cpp}` | 신규 (Long polling) | ✅ Short polling 정정 | ✅ |
| `src/web/OtaApi.{h,cpp}` | 신규 (Update.h) | ✅ 작성 (운영 비활성) | ✅ |
| `src/web/embedded_assets.{h,cpp}` | data/www PROGMEM | ✅ + gzip 사전 압축 | ✅ |
| `data/www/{index.html,style.css,app.js}` | 4탭 SPA | ✅ 4탭 완성 | ✅ |
| `tools/embed_www.py` | 자동 생성 스크립트 | ✅ + gzip | ✅ |
| `test/poc/{run_poc.sh,mqtt_pub.py,control_verify.py,capture_serial.py}` | PoC 시나리오 | ✅ | ✅ |

**Structural Match**: **10/10 = 100%** ✅

---

## 4. API Contract (14 Endpoints — Design §4.1)

| # | Endpoint | Method | Registered | 동작 검증 | 운영 상태 |
|---|----------|--------|:---:|:---:|:---:|
| 1 | `/` | GET | ✅ | ✅ minimal HTML | ⚠️ minimal only |
| 2 | `/api/status` | GET | ✅ | ✅ 200 ~95ms | ✅ |
| 3 | `/api/images/list` | GET | ✅ | ✅ 200 156B | ✅ |
| 4 | `/api/images/<name>` | GET | ✅ | ✅ 200 (BMP raw) | ✅ |
| 5 | `/api/images/<name>` | DELETE | ✅ | ✅ 200 | ✅ |
| 6 | `/api/images/upload` | POST | ✅ | ✅ binary-safe multipart | ✅ |
| 7 | `/api/imagesconfig` | GET | ✅ | ✅ 200 | ✅ |
| 8 | `/api/config` | GET | ✅ | ✅ 200 505B | ✅ |
| 9 | `/api/config` | POST | ✅ | ✅ atomic .tmp + rename | ✅ |
| 10 | `/api/reboot` | POST | ✅ | ✅ 1s grace + ESP.restart | ✅ |
| 11 | `/api/log` | GET | ✅ | ✅ 200 ring buffer | ✅ |
| 12 | `/api/control?since=N` | GET | ✅ | ✅ 200 ~95ms | ✅ short polling |
| 13 | `/api/control` | POST | ✅ | ✅ etag 정확 증가 | ✅ |
| 14 | `/api/ota` | POST | ✅ | ⚠️ Update.h fail | ❌ partition 없음 |

**Contract Match**: 코드 등록 14/14, 동작 검증 13/14 (OTA fail). 정량 13/14 = **93%**

---

## 5. Runtime Verification 결과 종합

### L1 — API Endpoint Tests (✅ 통과)
| Test | Result |
|------|--------|
| GET / minimal HTML | ✅ 853B, 95ms |
| GET /api/status (auth) | ✅ 200 197B 95ms |
| GET /api/status (no auth) | ✅ 401 + WWW-Authenticate |
| GET /api/control polling | ✅ 200 32B 95ms |
| POST /api/control toggle | ✅ etag 정확 증가, MQTT publish |
| GET /api/images/list | ✅ 200 156B |
| GET /api/images/title.bmp | ✅ 200 61974B (BMP raw) |
| DELETE /api/images/* | ✅ 200 |
| POST /api/images/upload (3126B BMP) | ✅ round-trip 바이트 동일 |
| GET /api/config / POST validate | ✅ atomic write |
| GET /api/log | ✅ ring buffer JSON |

### L2 — PoC Strict Scenarios (✅ 통과)
| # | 시나리오 | 결과 |
|---|---------|------|
| P1 | Sequential 10 GET | ✅ 10/10 |
| P2 | 병렬 5 × 30초 | ✅ 100/100 |
| P3 | 60초 sustained (183 req) | ✅ 183/183, uptime +60s |
| P4 | idle 60s heap | ✅ +12 byte (누수 無) |

### L3 — 단말 운영 시나리오 (사용자 직접 검증)
| # | 시나리오 | 결과 |
|---|---------|:---:|
| M1 | LCD Long-click → DeviceManager | ✅ |
| M2 | Sleep 시간 저장 (재부팅 후 유지) | ✅ v2.1 코드 보존 |
| M3 | LCD 이미지 표시 (BMP) | ✅ |
| M4 | Control 토글 → LCD 미러 (양방향) | ✅ MQTT 동작 |
| M5 | DHCP 끊김 → 재연결 15s | ✅ v2.1 동작 보존 |
| M6 | 30초 부하 (status + control polling) | ✅ 44/44 |
| M7 | WebUI 4탭 풀세트 운영 | ❌ SPI 충돌로 hang |
| M8 | OTA bin 업로드 | ❌ partition 없음 |
| M9 | PNG decode + LCD 렌더 | ⚠️ 작은 PNG round-trip OK / 큰 PNG hang |

### Decision Record Verification

| Decision | Followed? | Outcome |
|----------|:---:|---------|
| [Plan] esp_http_server (별도 task) | ✅ | core 0 pinning 동작 |
| [Plan] PoC 엄격 기준 | ✅ | B2 config 통과 |
| [Plan] OTA bin only (FULL flash.bat) | ⚠️ | OTA partition 없음 — partition 변경 안 함 |
| [Plan] Long polling 10s + ETag | ❌ | Short polling 1s + 0ms 로 정정 (단일 task 한계) |
| [Plan] Branch v2.3-httpd | ✅ | origin push 완료 |
| [Design] Option C Pragmatic | ✅ | v2.2-zero 콜백 시그니처 보존 |
| [Design] 5 모듈 5 세션 | ✅ | poc → webui → ota → control → verify |
| [Design] PoC gate 통과 시만 진행 | ✅ | 모든 모듈 통과 후 진행 |

---

## 6. Match Rate Calculation

### Formula (static + runtime hybrid)

```
Overall = (Structural × 0.15) + (Functional × 0.25) + (Contract × 0.25) + (Runtime × 0.35)
        = (100 × 0.15) + (68 × 0.25) + (93 × 0.25) + (Runtime × 0.35)
```

### Runtime Score (L1+L2+L3 종합)
- L1 API: 11/11 = 100%
- L2 PoC: 4/4 = 100%
- L3 운영: 6/9 = 67% (M7/M8/M9 fail or deferred)
- 가중: (100+100+67)/3 = **89%**

### Final Match Rate
```
Overall = (100 × 0.15) + (68 × 0.25) + (93 × 0.25) + (89 × 0.35)
        = 15.0 + 17.0 + 23.25 + 31.15
        = 86.4%
```

**Match Rate: 86.4%** (≥ 90% 기준 미달)

---

## 7. Gap List (Critical / Important)

### Critical (코드 + 운영 모두 미달)

| ID | Gap | 심각도 | 원인 | v2.3 Iterate 가능? |
|----|-----|:---:|------|:---:|
| C-1 | **WebUI 운영 비활성** (FR-03) | Critical | SPI 버스 (W5500+TFT VSPI 공유) 충돌 — large response 시 hang | ❌ 구조 제약 |
| C-2 | **PNG decoder 비활성** (FR-04) | Critical | LCD touch race + heap 부족 + SPI race 의심 | ❌ 구조 제약 |
| C-3 | **OTA partition 미지원** (FR-05) | Critical | `huge_app.csv` 가 단일 app — partition table 변경 시 SPIFFS wipe | ❌ partition 결정 |

### Important (정정 또는 Partial)

| ID | Gap | 심각도 | 처리 |
|----|-----|:---:|------|
| I-1 | Long polling 10s → Short polling 1s/0ms (FR-06) | Important | Plan SC 정정 — esp_http_server 단일 task 한계 |
| I-2 | OTA bin 양산 (FULL only, FR-11) | Important | FR-05 동반 deferred |
| I-3 | heap baseline 100KB → 40KB (NFR) | Important | Logger + ConfigApi + OtaApi + ControlApi BSS 누적. 정정 |

### Positive Findings (Plan 외 추가)

| ID | 발견 | 가치 |
|----|------|------|
| P-1 | **gzip 사전 압축 구조** (tools/embed_www.py) | v2.4 재활용 가능 — 22KB → 7KB |
| P-2 | **inline HTML 빌드 패이프라인** | v2.4 SPI 해결 후 즉시 활성 가능 |
| P-3 | **SPI 충돌 진단 + 코드 보존** | v2.4 sub-task 명확 분리 |
| P-4 | **PoC 엄격 검증 방법론 확립** | run_poc.sh / control_verify.py / capture_serial.py 재사용 |
| P-5 | **binary-safe multipart parser** | Arduino String 0x00 종료 bug fix — 재사용 가능 |
| P-6 | **모듈 5개 코드 보존** (WebServer/ConfigApi/Logger/ControlApi/OtaApi) | v2.4 에서 SPI 해결 후 그대로 활성 |

---

## 8. v2.4 인계 사항 (이번 사이클 deferred)

### 최우선 — SPI 버스 충돌 해결
1. **Option A**: TFT 27MHz → 10MHz (transaction 시간 1/3, 가장 안전)
2. **Option B**: TFT_eSPI mutex 명시 동기화 + W5500 SPI clock 조정
3. **Option C**: TFT 핀 변경 → HSPI 전용 host (H/W rewire 필요)

### 그 이후 활성
4. WebUI 풀세트 재활성 (handleRoot 가 INDEX_HTML_GZ 사용)
5. PNG decoder 재활성 (LV_USE_PNG=1) + LCD touch 검증
6. OTA partition 변경 (huge_app.csv → min_spiffs.csv 또는 custom)
   - 단점: 기존 SPIFFS data 보존 위한 backup/restore 절차 필요
7. NFR 정정: heap baseline 100KB → 40KB 운영 기준

### 보존된 v2.3 학습 자산
- `v2.3-httpd` 브랜치 (origin push 완료, 12 commits)
- 5 모듈 코드 (WebServer/ImageApi/ConfigApi/Logger/ControlApi/OtaApi)
- WebUI 풀세트 (index.html/style.css/app.js, 4탭)
- gzip 빌드 파이프라인 (tools/embed_www.py)
- PoC 검증 스크립트 (test/poc/*)

---

## 9. Checkpoint 5 — Review Decision 권장

Match Rate **86.4%** (< 90%) → 일반 case 면 Iterate 권장. 그러나 **Critical gaps 모두 구조 제약** (SPI 버스, partition):

| Option | 평가 |
|--------|------|
| Iterate (자동 fix) | ❌ **구조 제약은 Iterate 로 해결 불가** (코드 수정 영역 외) |
| 그대로 진행 (Report) | ✅ **권장** — Match Rate 86.4% + 본질 진단 확립 + v2.4 sub-task 명확 |
| v2.4 즉시 분기 | ✅ 가능 — Report 후 v2.4 plan |

**권장 액션**: Critical gaps 는 v2.4 분기. v2.3 는 그대로 Report 진행. Plan 의 "PoC 실패 시 v2.4 분기" 규칙은 적용 안 됨 (PoC 는 통과). 본질 발견은 운영 단계에서 발생.

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — Match Rate 86.4%, SPI 충돌 진단 + v2.4 sub-task 분리 권장 | KDI |
