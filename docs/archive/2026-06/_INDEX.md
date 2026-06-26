# Archive Index — 2026-06

PDCA 문서 아카이브 — 2026년 6월 완료 사이클.

## 완료 사이클

| Feature | 기간 | Match Rate | Iterations | Archived | 비고 |
|---------|------|:---:|:---:|----------|------|
| [RemoteDeck_Touch_v2.1](./RemoteDeck_Touch_v2.1/) | 2026-06-22 (1일) | **68%** | 0 | 2026-06-22 | LAN 스택 통일 완료. WebUI/PNG는 v2.2로 분리 |
| [RemoteDeck_Touch_v2.2](./RemoteDeck_Touch_v2.2/) | 2026-06-23 (1일) | **49%** | 0 | 2026-06-23 | sync WebServer 가설 폐기. v2.3 esp_http_server 재설계로 분리. 코드는 v2.2-zero 브랜치 보존 |
| [RemoteDeck_Touch_v2.3](./RemoteDeck_Touch_v2.3/) | 2026-06-23 ~ 2026-06-26 (4일) | **86.4%** | 0 | 2026-06-26 | esp_http_server 5 모듈 완성. SPI 버스 공유 충돌로 WebUI/PNG/OTA 비활성. 핵심 API+Control 운영. 코드는 v2.3-httpd 브랜치 보존 |

## RemoteDeck_Touch_v2.1 요약

**핵심 성과**:
- arduino-libraries/Ethernet → ETH.h + ETH_PHY_W5500 (PC v2.3.0 패턴)
- TFT_eSPI 2.4.61 → 2.5.43 (Arduino-ESP32 3.x 호환)
- ArduinoHttpClient → ESP32 내장 HTTPClient (downloadFile/sendHttpMessage 재작성)
- setup() 순서 재배치로 W5500/TFT_eSPI SPI 충돌 해결

**Plan 외 추가 개선 (Positive findings)**:
- Long-click 35회 누적 → 1회 long-press 진입 (v1 dead-code 버그 동반 수정)
- DeviceManager Sleep 시간 저장/복원 (v1 regression fix)
- 이미지 디코드 메모리 안전장치 (크기/heap 검증, OLD 선행 free, 진단 로깅)
- downloadFile Content-Length 200KB 상한
- FULL (4MB) + OTA (1.72MB) 펌웨어 git 추적 + flash.bat 인프라

**v2.2 인계 사항**:
- AsyncTCP task slot 충돌 (WebUI 비활성)
- LV_USE_PNG=0 (PNG 디코더 미활성)
- OTA Handler / 로그 뷰어 / deviceconfig 웹 편집
- 시간 표시 UI

**Branch 전략**: `v2.1-lan` → `main` merge (commit `c44d348`)
**펌웨어**: `RemoteDeck_Touch/firmware/RemoteDeck_Touch_V2.1.0_{FULL|OTA}_20260622.bin`

## 문서 구성

### RemoteDeck_Touch_v2.1 (성공 + 분리)
- `RemoteDeck_Touch_v2.1.plan.md` — Plan (LAN 스택 통일 + PNG)
- `RemoteDeck_Touch_v2.1.design.md` — Design (Option C Pragmatic)
- `RemoteDeck_Touch_v2.1.analysis.md` — Gap Analysis (68%)
- `RemoteDeck_Touch_v2.1.report.md` — 통합 보고서

### RemoteDeck_Touch_v2.2 (실패 가설 + 학습 보존)
- `RemoteDeck_Touch_v2.2.plan.md` — Plan (WebUI 풀세트 + PNG, zero-base)
- `RemoteDeck_Touch_v2.2.design.md` — Design (Option C sync WebServer)
- `RemoteDeck_Touch_v2.2.analysis.md` — Gap Analysis (49%, sync 가설 폐기)
- `RemoteDeck_Touch_v2.2.report.md` — 통합 보고서 + v2.3 인계 사항

## RemoteDeck_Touch_v2.2 핵심 학습

**시도**: ESP32 Arduino 내장 sync WebServer + 협력적 yield 로 W5500+MQTT 환경 WebUI 구현
**결과**: Phase 1 PoC (단발 `/api/status`) 성공 → Phase 2 풀세트 도달 후 **연속/병렬 요청 처리 본질적 불안정** 발현
**증상**: `request handler not found` 반복, 이미지 디코드 fail 패턴, Control 탭 추가 시 부팅 hang
**결정**: 가설 폐기 + v2.1 운영 유지 + v2.3 esp_http_server 재설계 (별도 task + core pinning)

**보존 자산**:
- `v2.2-zero` 브랜치 (origin push 완료) — sync WebServer 시도 코드 전체
- 격리 진단 방식 (`#if WEB_SERVER_DISABLED_DEBUG`) — hang 디버깅 방법론

**v2.3 인계**:
1. WebServer = esp_http_server (ESP-IDF native, 별도 task)
2. PoC = 풀세트 케이스 시뮬레이션 (단발 검증의 한계 학습)
3. PNG / OTA / Control 탭 / 시간 UI (v2.1 사용자 보고)

## RemoteDeck_Touch_v2.3 핵심 학습

**시도**: esp_http_server (ESP-IDF native, core 0 task) zero-base 재설계 + 5 모듈 (WebServer / ImageApi / ConfigApi / Logger / ControlApi / OtaApi) 완성

**결과**: PoC 통과 후 module-webui/ota/control 모두 코드 완성 → 운영 단계에서 **SPI 버스 공유 충돌** 발견
- W5500 (ETH) + TFT_eSPI (LCD) 가 동일 VSPI host + 동일 GPIO (SCK=18/MOSI=23/MISO=19) 공유
- 작은 응답 (< 1KB) OK / 큰 응답 (수십 KB) 시 LVGL flush starvation → hang
- WebUI/PNG/OTA 비활성, 핵심 API + Control 유지

**Match Rate 86.4%** (FR 11개 중 Met 6 / Partial 3 / Not Met 2)

**보존 자산**:
- `v2.3-httpd` 브랜치 (origin push 완료, 15 commits) — 5 모듈 코드 전체 + WebUI 4탭 + gzip 빌드 파이프라인
- PoC 검증 스크립트 (run_poc.sh, mqtt_pub.py, control_verify.py, capture_serial.py)
- binary-safe multipart parser (Arduino String 0x00 bug fix)
- PNG IHDR 사전 heap check 안전장치 코드

**v2.4 인계**:
1. **SPI 버스 충돌 해결 (최우선)**
   - Option A: TFT 27MHz → 10MHz (추천, 가장 안전)
   - Option B: TFT_eSPI mutex 명시 + W5500 SPI clock 조정
   - Option C: TFT 핀 변경 → HSPI 전용 host (H/W rewire 필요)
2. 해결 후 INDEX_HTML_GZ 재활성 (WebUI 풀세트)
3. PNG decoder 재활성 + LCD touch 검증
4. OTA partition 변경 (huge_app.csv → min_spiffs.csv 또는 custom)
5. NFR 정정 (heap baseline 100KB → 40KB)
