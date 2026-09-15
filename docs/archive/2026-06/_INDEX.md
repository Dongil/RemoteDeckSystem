# Archive Index — 2026-06

PDCA 문서 아카이브 — 2026년 6월 완료 사이클.

## 완료 사이클

| Feature | 기간 | Match Rate | Iterations | Archived | 비고 |
|---------|------|:---:|:---:|----------|------|
| [RemoteDeck_Touch_v2.1](./RemoteDeck_Touch_v2.1/) | 2026-06-22 (1일) | **68%** | 0 | 2026-06-22 | LAN 스택 통일 완료. WebUI/PNG는 v2.2로 분리 |
| [RemoteDeck_Touch_v2.2](./RemoteDeck_Touch_v2.2/) | 2026-06-23 (1일) | **49%** | 0 | 2026-06-23 | sync WebServer 가설 폐기. v2.3 esp_http_server 재설계로 분리. 코드는 v2.2-zero 브랜치 보존 |
| [RemoteDeck_Touch_v2.3](./RemoteDeck_Touch_v2.3/) | 2026-06-23 ~ 2026-06-26 (4일) | **86.4%** | 0 | 2026-06-26 | esp_http_server 5 모듈 완성. SPI 버스 공유 충돌로 WebUI/PNG/OTA 비활성. 핵심 API+Control 운영. 코드는 v2.3-httpd 브랜치 보존 |
| [RemoteDeck_Touch_v2.4](./RemoteDeck_Touch_v2.4/) | 2026-06-26 (1일) | **12.1%** | 0 | 2026-06-26 | 시간 분할 (Web active + LCD freeze) 가설 폐기. ESP32 SPI host mutex wait 한계. v2.5 분기 (freq 조정 / H/W rewire / 보드 변경 / WebUI 영구 포기). 코드는 v2.4-spi 브랜치 보존 |
| [RemoteDeck_Touch_v2.5](./RemoteDeck_Touch_v2.5/) | 2026-06-26 (1일) | **97.1%** | 0 | 2026-06-26 | **WebUI sunset 정식 마감**. WebServer 코드 전체 제거 (25 파일 + 51 line). Flash -95KB (-3.1%), MQTT 양방향 자동 검증. v2.1 LCD/MQTT only 패턴 복귀. Branch `v2.5-sunset` |
| [Integrate_Controller](./Integrate_Controller/) | 2026-06-30 (1일) | **97.9%** | 0 | 2026-06-30 | **신규 PC 통합 컨트롤러**. .NET 8 WinForms + win-x64 SCD single-file (162 MB). RemoteDeck_PC 단말 N대 모니터링/재부팅. DPAPI 자격증명 저장, REST 폴링 + `/api/config` device_id 1회 fetch. 10 컬럼 DataGridView (No/연결/기기 이름/기기 ID/IP/PC/GPIO/FW/Uptime/Last Seen), 우클릭 ContextMenu (브라우저 열기/Reboot/Edit/Delete), `publish.bat` 배포 자동화. 펌웨어 변경 0건. |

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

## RemoteDeck_Touch_v2.4 핵심 학습

**시도**: 시간 분할 (Web active mode + LCD freeze) — WebUI 활성 시 LVGL/TFT 정지 → ETH 가 SPI 단독 점유 (사용자 통찰)

**구현**: WebActivityMonitor (단일 책임) + deferred 콜백 (core 0→1 sync) + freezeLCD/resumeLCD + 10초 idle + LCD touch tap-to-acquire

**결과**: PoC P1 (단일 GET /) **부터 fail**
- 응답 size=0, 6초 timeout
- 단말 alive (uptime monotonic, heap 안정) — esp_http_server task hang
- deferred 콜백 도입 후에도 race 해소 안 됨

**Match Rate 12.1%** — PoC gate fail 로 후속 module-png/ota 진입 안 함

**본질 진단 (재정밀화)**:
- ESP32 의 SPI host driver 가 host-level mutex 제공 — 그러나 **transaction wait 시간 무제한**
- ETH 응답 send (수십 KB) + TFT freezeLCD (fillScreen + drawString) 동시 시도 시 mutex 누적 wait
- mutex wait 시간이 client (curl 6초) timeout 초과
- 시간 분할이 race window 만 줄임 — mutex contention 자체 해결 안 됨

**보존 자산**:
- `v2.4-spi` 브랜치 (origin push 완료) — 시간 분할 시도 코드 전체
- WebActivityMonitor.{h,cpp} (단일 책임 깔끔, v2.5 SPI fix 와 조합 가능)
- v24_poc.py (brower 6 동시 + 22KB inline + sustained + MQTT 시나리오, 재사용 가능)
- deferred 콜백 패턴 (core 0→1 sync, v2.5 SPI mutex 도입 시 함께 사용)
- freezeLCD/resumeLCD/poll_touch_for_resume 정적 함수

**v2.5 인계 (5개 옵션)**:
| Option | 설명 | 권장도 |
|--------|------|:---:|
| A. SPI freq 조정 | TFT 27→10MHz + W5500 8MHz | 중간 |
| B. SPI 명시적 mutex | `spi_device_acquire_bus` ESP-IDF API | 중간 |
| C. H/W rewire | TFT 핀 → HSPI (SCK=14, MOSI=13, MISO=12) | 높음 |
| D. ESP32-WROVER (PSRAM 보드 교체) | dual SPI host 자연 분리 | 높음 |
| **E. WebUI 영구 포기** | v2.3-final minimal 운영 (API only) | **가장 빠름** |

**단말 상태**: v2.3-final 펌웨어 (`2.3.0-ctrl`) 운영 유지 (운영 중단 0초)

## RemoteDeck_Touch_v2.5 핵심 학습

**결정**: v2.4 5개 옵션 중 **E (WebUI 영구 포기)** + 사용자 추가 지시 "webserver 관련 전부 제거" 채택 → **Subtractive Design** sunset cycle

**구현**: 3 commit (Subtractive only — insertion=0)
- `d000f9d` — 디렉토리/파일 일괄 제거 (src/web/ 14 + data/www/ 3 + tools/embed_www.py + test/poc/ 8)
- `116cef5` — main.cpp WebServer 코드 51 line 제거 (#include 6 + 인스턴스 7 + setup/loop/message_process 호출)
- `4e791d9` — platformio.ini CONFIG_HTTPD_MAX_REQ_HDR_LEN/URI_LEN 제거

**Match Rate 97.1%** (Met 10 / Partial 1 / Not Met 0, Critical Gap 0)
- Structural 100% / Functional 95.5% / Contract 100% / Runtime 95%

**Build 결과** (Plan 예상의 2배):
- Flash 1,838,300 → **1,740,984 B (-95KB, -3.1%)**
- RAM 121,184 → 116,920 B (-1.3%)

**자동 검증**:
- HTTP 80 Connection refused (외부 admin = MQTT only 확정)
- ping 1ms, 0% loss
- MQTT 양방향 (paho-mqtt 6 messages @ t=41~42s, room/{device_id} ↔ room/client)

**보존 사항 (v2.1~v2.3 개선)**:
- LAN 스택 통일, Long-click 1회 진입, Sleep 저장/복원, BMP heap guard
- TFT_eSPI 2.5.43, pioarduino 53.x, HTTPClient, DHCP 15s
- MQTT 양방향 (room/{device_id})

**Branch 전략**: `v2.5-sunset` (main / v2.3-httpd 보호, origin push 없음)
- 학습 자산은 `v2.3-httpd` / `v2.4-spi` 브랜치 origin 그대로 보존 → v2.6 (ESP32-S3/WROVER) 시 재시도 가능

**Lessons Learned**:
- **Keep**: Subtractive Design 명시화, 3-cycle PDCA escalation (v2.3 → v2.4 → v2.5), 브랜치 자산 보존
- **Problem**: Plan Flash 추정 2배 오차, M2/M3 LCD 직접 검증 자동화 불가, heredoc `$()` 우회 필요
- **Try**: 사전 `pio run -t size`, LCD touch event injection MQTT 명령, v2.6 ESP32-S3 PoC

**5-cycle 여정 마감**: v2.1 (68%) → v2.2 (49%) → v2.3 (86.4%) → v2.4 (12.1%) → **v2.5 (97.1%)** WebUI 실험 cycle 정식 종료

## Integrate_Controller 핵심 학습

**결정**: 사내 RemoteDeck_PC 단말 다수의 통합 모니터링/재부팅 도구 — Plan 단계에서 **Option C — Pragmatic Models/Services/UI 3-folder** 채택 (Clean Architecture 4-layer는 단일 desktop tool에 과설계 판단)

**구현**: 4 모듈 (sln/csproj/Models/Services + UI + Reboot/Publish) — 1일 4 세션 완성
- Models: `DeviceEntry`, `DeviceStatus`, `DeviceList` (BindingList)
- Services: `DeviceStore` (DPAPI), `RemoteDeckClient` (Singleton HttpClient + Basic Auth + GetStatus/Reboot/GetConfig), `DevicePoller` (PeriodicTimer per-device + CTS)
- UI: `MainForm` (10 컬럼 DataGridView + Toolbar + StatusStrip + Detail panel + ContextMenu), `DeviceEditDialog`, `StatusFormatter`

**Match Rate 97.9%** (Met 9 / Partial 1, Critical Gap 0)
- Structural 100% / Functional 100% / Contract 100% / Runtime 94%

**Build 결과**:
- Single-file SCD exe `IntegrateController.exe` = **162 MB** (win-x64)
- `dotnet build` 경고 0, 오류 0
- 펌웨어 변경 **0건** (기존 REST API `/api/status` `/api/reboot` `/api/config` 그대로 활용)

**Do 중 사용자 요청 반영 (Deviation, 모두 의도된 개선)**:
- `Label` 필드 제거 → device_name/device_id 컬럼으로 대체 (정보량↑)
- 기본 port 80 → 5050 (운영 환경 일치)
- Grid 360px → 420px (15행 표시)
- `GetConfigAsync` 추가 (device_id 1회 fetch)
- "브라우저로 열기" ContextMenu 추가 + `publish.bat` 배포 자동화

**비자명 문제 해결 (3회 hotfix)**:
- **Index out of range (Column ColStatus 미존재)**: `ConfigureGridColumns()` → `DataSource` 순서 뒤집기 + `AutoGenerateColumns = false`
- **BindingList 자동 컬럼 노출 (`AuthPasswordProtected` base64까지 화면 노출 위험)**: 모델 전 속성에 `[Browsable(false)]` + 코드로 `AutoGenerateColumns = false` 강제 + Designer에도 명시 (3중 방어선)
- **reboot timeout 오해**: 펌웨어가 `req->send()` 직후 `ESP.restart()` 호출 → TCP flush 전 재시작 → 클라이언트 timeout. `RebootAsync`가 timeout/`SocketException.ConnectionReset/Aborted/NetworkReset/Shutdown` → **Success + 힌트** 반환으로 보정
- **VS Designer regenerate가 매번 `ColIp` + `AutoGenerateColumns=false` drop**: IP 컬럼을 `MainForm.cs` 의 `AddIpColumn()` 메서드로 분리 → Designer 파일 밖 → 영구 면역
- **선택 시 ● 색/행 배경 색 손실**: `DefaultCellStyle.SelectionBackColor` + `SelectionForeColor` 동시 설정 (StatusFormatter에 색 변종 추가)
- **브라우저 자동 인증 시도 → XHR 캐시 timing race**: URL `user:pass@host/` + 자동 F5 (Win32 `keybd_event`) 모두 실패 → **클린 URL만 열기**로 단순화 (브라우저 Basic Auth 다이얼로그 직접 표시 → 인증 상태 명확)

**보존 자산**:
- 코드 13 파일 `apitestutility_v2/integrate_controller/IntegrateController/`
- `publish.bat` 배포 자동화 + `IntegrateController/publish/IntegrateController.exe` (162 MB)
- 메모리: [[project-rdpc-reboot-flush]] (reboot flush 패턴), [[project-winforms-designer]] (VS Designer + DataGridView 3중 방어선)

**향후 사이클 후보 (선택)**:
- N=10 정식 시연 (mock server 또는 실 단말)
- RemoteDeck_PC 펌웨어 `req->send()` + `delay(200)` + `ESP.restart()` 영구 패치
- device_name UTF-8 한글 깨짐 (펌웨어 측 JSON 직렬화)
- 단위 테스트 (xUnit) — DeviceStore DPAPI roundtrip, RemoteDeckClient ParseStatus
- Toolbar 전역 Poll Interval 토글

**Lessons Learned (단순화는 자동화보다 우위)**:
- 모던 브라우저 자동 인증 자동화 3단계 시도 모두 실패 → 가장 단순한 "클린 URL + 다이얼로그"가 최선
- VS Designer + DataGridView는 Designer regenerate에 적대적 — unbound 컬럼은 코드로 분리
- BindingList 보안 위험 (AuthPasswordProtected 노출 가능) — 모델 정의 시점 `[Browsable(false)]` default 적용

**Branch 전략**: 단일 사이클, 단일 main 브랜치 (별도 분기 없음)
