# RemoteDeck_Touch_v2.3 Completion Report

> **Match Rate**: 86.4% (≥ 90% 기준 미달 — Critical gaps 모두 구조 제약, 사용자 결정으로 Report 진행)
> **Project**: RemoteDeck_Touch
> **Version**: v2.3.0-final
> **Author**: KDI
> **Date**: 2026-06-26
> **Branch**: `v2.3-httpd` (origin push, 13 commits)
> **Status**: Completed

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.1 미구현 (WebUI/PNG/OTA/Control) + v2.2 sync WebServer 가설 폐기 (동시성 본질 불안정) |
| **Solution** | esp_http_server (ESP-IDF native, core 0 task) zero-base 재설계 + v2.2-zero 콜백 시그니처 보존 (Option C). 5 모듈 (WebServer/ImageApi/ConfigApi/Logger/ControlApi/OtaApi) 모두 코드 완성. 운영 단계에서 **SPI 버스 공유 충돌** 발견 (W5500+TFT_eSPI VSPI host 공유) → WebUI/PNG/OTA 비활성, 핵심 API + Control 유지. |
| **Function/UX Effect** | 외부 시스템이 `/api/control` POST 로 IN/OUT 토글 + Short polling 으로 양방향 미러. MQTT 동작 정상. LCD/Touch v2.1 동작 100% 보존. WebUI 풀세트는 v2.4 deferred. |
| **Core Value** | PoC 통과 (esp_http_server + W5500 + MQTT 동시성 검증) + 본질 한계 (SPI 공유) 진단 + 5 모듈 코드 자산 v2.4 재활용 가능. v2.3 → v2.4 인계 사항 명확. |

### 1.3 Value Delivered

| Perspective | Planned | Delivered | Metric |
|-------------|---------|-----------|--------|
| **Problem 해결** | v2.1 미구현 4종 + v2.2 동시성 | API + Control 동작 / WebUI/PNG/OTA deferred | **4 중 1 운영, 3 deferred** |
| **API 안정성** | PoC 풀세트 통과 | B2 config: 30s × 100 req, fail=0 | **100% 통과** |
| **단말 운영 안정성** | LCD regression 0 | uptime monotonic, MQTT 양방향 동작 | **v2.1 동작 100% 보존** |
| **코드 자산** | 5 모듈 + WebUI 4탭 + 빌드 인프라 | 모두 commit + push (v2.3-httpd) | **재활용 가능** |

---

## 1. PRD/Plan/Design → Code 여정 요약

### 1.1 시작
- v2.1 (commit `156d089`): LAN 스택 통일 완료, WebUI/PNG/OTA/Control 미구현
- v2.2 (commit `5fda55c`): sync WebServer 가설 폐기, 동시성 본질 미해결
- v2.3 시작 시 가설: **esp_http_server 가 별도 task + core pinning 으로 동시성 본질 해결 가능**

### 1.2 진행 흐름

| 모듈 | 핵심 결과 |
|------|---------|
| **module-poc** | PoC B2 통과 — sockets 4 + stack 12K + priority 4. 30s × 100 req fail=0 |
| **module-webui** | 4탭 WebUI + PROGMEM embed + binary-safe multipart (Arduino String 0x00 bug fix) + L1 14건 통과 |
| **PNG sub-task** | extern "C" linkage fix + IHDR heap check 안전장치 작성. 단 LCD touch race 로 운영 비활성 |
| **module-ota** | Update.h 기반 OtaApi + /api/ota multipart. 단 huge_app.csv 단일 app partition → 실제 OTA fail (검증에서 발견) |
| **module-control** | Short polling 1s + 0ms 즉시 응답 + ETag + MQTT bridge. 사용자 SPI 진단 통찰로 WebUI 비활성 결정 |

### 1.3 결정적 발견 — SPI 버스 공유 충돌

사용자의 진단 통찰:
- W5500 (ETH) + TFT_eSPI (LCD) 가 **동일 VSPI host + 동일 GPIO** (SCK=18, MOSI=23, MISO=19)
- Arduino-ESP32 의 host-level mutex 가 있지만 large HTTP response 시 SPI transaction 길어짐
- → LVGL flush 가 starvation → main loop hang
- → 작은 응답 (< 1KB) 안전, 큰 응답 (수십 KB) hang

→ WebUI/PNG/OTA 비활성 + 핵심 API + Control 유지 결정. v2.4 sub-task 로 분리.

---

## 2. Plan Success Criteria — Final Status

| ID | Requirement | Status | 비고 |
|----|-------------|:---:|-----|
| FR-01 | esp_http_server core 0 별도 task | ✅ Met | WebServer.cpp core_id=0 |
| FR-02 | PoC 풀세트 통과 | ✅ Met | B2 config 30s × 100 통과 |
| FR-03 | 4탭 WebUI | ⚠️ Partial | 코드 완성, 운영 비활성 (SPI 충돌) |
| FR-04 | PNG 디코더 | ❌ Not Met | LCD touch race + SPI 의심 → 안전판 LV_USE_PNG=0 |
| FR-05 | OTA /api/ota | ❌ Not Met | huge_app.csv 단일 app partition |
| FR-06 | Long polling 10s + ETag | ⚠️ Partial | Short polling 1s + 0ms 로 정정 (단일 task 한계) |
| FR-07 | MQTT 양방향 미러 | ✅ Met | controlApi.notifyState + setMqttPublisher |
| FR-08 | Basic Auth | ✅ Met | mbedtls base64 + 401 |
| FR-09 | LCD/Touch regression | ✅ Met | Long-click / Sleep / 이미지 / MQTT 모두 v2.1 동작 보존 |
| FR-10 | DHCP 15s + 재시도 | ✅ Met | v2.1 코드 그대로 |
| FR-11 | FULL+OTA bin 양산 | ⚠️ Partial | FULL only (OTA partition 없음) |

**Success Rate: 6 Met / 3 Partial / 2 Not Met = 11개 중 7.5/11 = 68%** (정량화)

---

## 3. Key Decisions & Outcomes

| Phase | Decision | Outcome |
|-------|----------|---------|
| [Plan] | esp_http_server (별도 task) | ✅ core 0 pinning 동작, PoC 통과 |
| [Plan] | PoC 엄격 기준 | ✅ B2 config 100/100 통과 |
| [Plan] | OTA bin only (FULL 은 flash.bat) | ⚠️ partition 변경 안 해서 OTA 실제 fail |
| [Plan] | Long polling 10s + ETag | ❌ Short polling 1s + 0ms 정정 |
| [Plan] | Branch `v2.3-httpd` (main 보호) | ✅ origin push 완료 |
| [Design] | Option C Pragmatic (v2.2-zero 콜백 보존) | ✅ ImageApi 무수정, ControlApi/OtaApi 신규 |
| [Design] | 5 모듈 5 세션 | ✅ poc → webui → ota → control → verify |
| [Design] | PoC gate 통과 시만 진행 | ✅ 단계별 진행 |
| [Do] | PNG decode 시도 | ⚠️ extern "C" fix 후 활성 가능, LCD touch race 로 안전판 비활성 |
| [Do] | Logger 50→30 (DRAM) | ✅ DRAM overflow 해소 |
| [Do] | gzip 사전 압축 (22KB→7KB) | ⚠️ SPI 충돌 본질이라 효과 미달 |
| [Do] | WebUI minimal (운영판) | ✅ < 1KB 응답 안정 |

---

## 4. Architecture 회고

### 무엇이 잘 됐나
1. **PoC-first 가 핵심 학습 보호** — module-poc 의 B2 검증 통과로 다음 모듈 진입 정당화. v2.2 의 단발 통과 함정 회피.
2. **Option C 콜백 시그니처 보존** — v2.2-zero 의 ImageApi/ConfigApi/Logger 콜백을 그대로 활용, 라이브러리 교체 비용 최소화.
3. **Branch 전략** — `v2.3-httpd` 분리로 main 운영 펌웨어 (v2.1) 보호. 학습 자산도 보존.
4. **Binary-safe multipart parser** — Arduino String 의 0x00 종료 bug 발견 + memcmp 기반 raw 처리로 재사용 가능 자산.
5. **사용자 통찰 활용** — SPI 공유 진단으로 본질 원인 정확히 파악 + v2.4 sub-task 명확화.

### 어디서 막혔나
1. **OTA partition 사전 확인 부재** — Plan/Design 단계에서 `huge_app.csv` 가 단일 app 임을 인식 못함. 검증 단계에서 발견.
2. **NFR heap 100KB 미충족** — 모듈 추가 시 BSS 누적 (Logger + ConfigApi + OtaApi + ControlApi) 으로 40KB baseline. NFR 정정 필요.
3. **esp_http_server 단일 task 본질 한계** — Long polling 10s 가 동시 다른 요청 block. Short polling 1s → 0ms 로 정정. spec 변경.
4. **SPI 버스 공유 미인지** — W5500+TFT_eSPI VSPI 공유는 H/W 설계 시점부터 존재. v2.1/v2.2 운영 시 발현 안 했던 이유: 대용량 응답 없었음. v2.3 의 large response 시도가 문제 노출.

### 본질 한계 vs 코드 한계
| 한계 | 종류 | v2.3 Iterate 가능? |
|------|:---:|:---:|
| SPI 버스 공유 충돌 | H/W 구조 | ❌ |
| OTA partition 단일 app | partition table | ❌ (SPIFFS wipe 위험) |
| esp_http_server 단일 task | 라이브러리 본질 | ❌ |
| Long polling latency | spec 정정 가능 | ✅ (이미 적용) |
| Logger BSS 크기 | 코드 조정 가능 | ✅ (이미 30개로) |

---

## 5. v2.4 인계 사항

### 최우선 (Critical) — SPI 버스 충돌 해결
| Option | 설명 | 영향 |
|--------|------|------|
| **A** | TFT 27MHz → 10MHz (transaction 시간 1/3) | LCD 약간 느려짐 |
| **B** | TFT_eSPI mutex 명시 + W5500 SPI clock 조정 | 코드 변경 small |
| **C** | TFT 핀 변경 → HSPI 전용 host | H/W rewire 필요 |

→ Option A 가 가장 안전 + 빠름. v2.4 PoC 단계에서 검증.

### 그 다음
1. **WebUI 풀세트 재활성** — handleRoot 가 INDEX_HTML_GZ 사용 (코드 보존됨)
2. **PNG decoder 재활성** — LV_USE_PNG=1 + 큰 PNG decode 시 SPI 충돌 동시 검증
3. **OTA partition 변경** — `huge_app.csv` → `min_spiffs.csv` 또는 custom (app0=1.9M + app1=1.9M + spiffs=400K)
   - 단점: 기존 SPIFFS 데이터 wipe — backup/restore 절차 필요
4. **NFR 정정** — heap baseline 100KB → 40KB (v2.3 실측값 기준)

### 보존 자산 (v2.3-httpd 브랜치)
| 자산 | 위치 |
|------|------|
| 5 모듈 코드 | `RemoteDeck_Touch/src/web/{WebServer,ImageApi,ConfigApi,Logger,ControlApi,OtaApi}.{h,cpp}` |
| WebUI 풀세트 (4탭) | `RemoteDeck_Touch/data/www/{index.html,style.css,app.js}` |
| gzip 빌드 파이프라인 | `RemoteDeck_Touch/tools/embed_www.py` |
| PoC 검증 스크립트 | `RemoteDeck_Touch/test/poc/{run_poc.sh,mqtt_pub.py,control_verify.py,capture_serial.py}` |
| PNG safety check 코드 | `RemoteDeck_Touch/src/images/images.cpp` (IHDR 사전 파싱) |

---

## 6. 운영 모드 (v2.3 최종)

### 활성 기능
- ✅ **LCD + Touch + Long-click** (v2.1 동작 100% 보존)
- ✅ **MQTT 양방향** (room/client publish + room/node_1 subscribe)
- ✅ **API 엔드포인트 13개 운영**:
  - `/api/status` — heap, uptime, IP
  - `/api/control` GET (Short polling) + POST (IN/OUT 토글)
  - `/api/images/list, /<name> GET/DELETE, /upload`
  - `/api/imagesconfig`, `/api/config GET/POST`
  - `/api/log`, `/api/reboot`
- ✅ **GET /** — minimal HTML (안내 + API 링크)

### 비활성 (v2.4 deferred)
- ⏸ WebUI 풀세트 (4탭, 22KB)
- ⏸ PNG 디코더
- ⏸ OTA bin 업로드

### 외부 시스템 연동 예시
```bash
# IN 토글
curl -u admin:12345 -X POST -H 'Content-Type: application/json' \
  -d '{"in":true}' http://192.168.10.122/api/control

# Short polling (1초 응답 또는 변경 즉시)
curl -u admin:12345 'http://192.168.10.122/api/control?since=N'

# 상태 확인
curl -u admin:12345 http://192.168.10.122/api/status
```

---

## 7. Branch 전략

- **main**: v2.1 운영 펌웨어 (156d089) — 단말 운영 가능 (보존)
- **v2.3-httpd**: v2.3 코드 + WebUI minimal 모드 펌웨어 (origin push 완료)
- **단말 적용**: USB flash 권장 (OTA partition 없음)

---

## 8. 다음 단계

```
/pdca archive RemoteDeck_Touch_v2.3 --summary
```

Archive 후 v2.4 plan 시작 권장:
```
/pdca plan RemoteDeck_Touch_v2.4  # SPI 버스 충돌 해결 + WebUI/PNG/OTA 재활성
```

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-26 | Initial — Match Rate 86.4%, v2.4 sub-task 분리, 5 모듈 코드 자산 보존 | KDI |
