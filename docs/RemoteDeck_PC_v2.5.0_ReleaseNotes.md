# RemoteDeck_PC v2.5.0 / v2.5.1 Release Notes

## v2.5.1 (2026-07-03) — SPIFFS OTA 설정 보존

**중요**: v2.5.0의 SPIFFS OTA는 파티션 전체를 덮어써서 `/deviceconfig.json`(네트워크·MQTT·계정·WebRequest URL)과 `/schedule.json`(릴레이/재부팅 스케줄)도 초기화되는 문제가 있었음. 필드 대량 배포 불가.

v2.5.1은 SPIFFS 대상 OTA 진행 시 다음 순서로 설정 파일 보존:
1. Update.begin(U_SPIFFS) 직전에 `/deviceconfig.json`, `/schedule.json`을 RAM으로 백업
2. 새 SPIFFS 이미지 Update.write() 처리
3. Update.end(true) 후 `SPIFFS.end()` → `SPIFFS.begin(true)` 재마운트
4. RAM 백업본을 원위치에 재기록
5. `ESP.restart()`

또한 OTAHandler 파일명 판정 규칙 완화:
- v2.5.0: `.spiffs.bin` `.fs.bin` `_fs.bin` `-fs.bin` 확장자 **strict endsWith 검사** (파일명 규칙 이탈 시 침묵)
- v2.5.1: `spiffs` `_fs.` `-fs.` `.fs.` 를 **substring 검색** — `RemoteDeck_PC_V2.5.1_20260703.spiffs.bin` 뿐 아니라 `_spiffs_20260703.bin` 같은 위치도 정상 판정
- 시리얼에 판정된 대상 파티션과 파일명을 명시 출력

**OTA artifacts (v2.5.1)**:
- `RemoteDeck_PC_V2.5.1_OTA_20260703.bin` (1,379,968 B, +1,728 B vs v2.5.0)
- `RemoteDeck_PC_V2.5.1_20260703.spiffs.bin` (196,608 B)

**필드 배포 절차** (v2.4.7 → v2.5.1):
1. 웹 UI '펌웨어' 탭 → firmware bin 업로드 → 재부팅 → v2.5.1 firmware 실행 (SPIFFS는 아직 v2.4.7, 설정 파일 그대로)
2. '펌웨어' 탭 → spiffs.bin 업로드 → OTAHandler v2.5.1 로직으로 설정 백업/복원 → 재부팅 → 관리 탭 표시 + 기존 설정 유지

---

## v2.5.0 (2026-07-03, 초기 릴리스 — v2.5.1로 대체됨)

**Release Date**: 2026-07-03
**Baseline**: v2.4.7 (2026-07-01, cold-boot recovery final)

**OTA artifacts** (관리 탭에서 2단계로 업로드):
- `RemoteDeck_PC_V2.5.0_OTA_20260703.bin` (1,378,240 B, +2,000 B vs v2.4.7) — **펌웨어**
- `RemoteDeck_PC_V2.5.0_spiffs_20260703.bin` (196,608 B) — **웹 UI(SPIFFS)**

---

## What's New

### 1. 관리 탭 재편성 (Web UI)
상단 탭 `펌웨어` → `관리` 로 개명. 카드 스택으로 다음 두 카드 배치:
- **기기 관리**: 즉시 재부팅 버튼 + 재부팅 스케줄 등록·조회·삭제
- **펌웨어 업데이트**: 기존 OTA UI 유지 (동일 기능, 위치만 이동)

### 2. 재부팅 스케줄 (ScheduleManager 확장)
`Schedule.action` 문자열에 `"reboot"` 값 추가. 기존 `on/off/toggle` 릴레이 스케줄과 동일 저장소(`/schedule.json`) 공유, MAX_SCHEDULES=8 상한 공유. 일치 시각에 `ESP.restart()`로 즉시 재부팅.

- 스케줄 탭에서 reboot 항목은 `🔁 재부팅` 아이콘으로 구분 표시
- 관리 탭에서는 reboot 스케줄만 필터링 표시

### 3. 부팅 후 상태 sync (`WebRequestHandler::syncCurrentStates`)
부팅 완료(`NetManager::isConnected() == true`) + 5초 stabilization 이후 1회 실행. GPIO1/2/3, PCLED 각 채널의 현재 실측값에 대응하는 WebRequest URL이 설정되어 있으면 호출.

- 대상: `gpio1/2/3_high|low`, `pcled_on|off`
- 미대상: 릴레이 (명령 성격)
- 실패 시 부팅·이후 로직에 영향 없음 (best-effort)
- 이벤트 로그에 `BOOT_SYNC` 카테고리로 기록

### 4. SPIFFS OTA endpoint 확장 (필수 인프라 개선)
기존 `POST /api/ota` 는 app 파티션만 갱신했었음. v2.5.0부터 **파일명 확장자로 대상 파티션 자동 판정**:
- `*.bin`, `firmware.bin` → app 파티션 (기존 동작)
- `*.spiffs.bin`, `*_fs.bin`, `*.fs.bin`, `*-fs.bin` → SPIFFS 파티션 (웹 UI)

관리 탭에 `웹 UI 업데이트` 카드 추가 (기존 펌웨어 업데이트 카드 아래). 파일명 규칙과 불일치 시 확인 dialog로 오배포 방지. 이 확장으로 웹 UI 변경을 물리적 접근 없이 원격 배포 가능해짐.

### 5. IntegrateController 로그 뷰 (C# WinForms)
기기 상세 패널 하단에 로그 ListView 추가. 선택된 기기의 최근 이벤트를 시간 역순 100건 표시.

- 폴링: `GET /api/log`, 기기별 5초 주기
- 시작 시점 stagger: 300ms × 인덱스 (14대 기기까지 5초 안에 균등 분산)
- 저장: 기기별 500건 (dedup key = `timestamp|event|detail`)
- UI: ListView + SplitContainer (코드 기반 UI, VS Designer 미의존)

---

## Success Criteria (Plan §3.3)

| # | 기준 | 상태 |
|:-:|---|:-:|
| SC-1 | 관리 탭 즉시 재부팅 동작 | 필드 확인 대기 |
| SC-2 | 재부팅 스케줄 ±1분 정확도 | 필드 확인 대기 (dogfood 1주) |
| SC-3 | 기존 릴레이 스케줄 무손실 | 필드 확인 대기 |
| SC-4 | IntegrateController 5초 이내 로그 로드 | 필드 확인 대기 |
| SC-5 | 로그 latency ≤ 5s | 필드 확인 대기 |
| SC-6 | 부팅 후 15초 이내 GPIO/PCLED sync | 필드 확인 대기 |
| SC-7 | URL 미설정 채널 skip | 필드 확인 대기 |
| SC-8 | NetManager diff = 0 | ✅ 검증 완료 |
| SC-9 | Flash +≤10KB, heap ≥100KB | Flash ✅ (+1,208 B) / Heap 필드 확인 |
| SC-10 | 필드 dogfood 1주 | 예정 |

전체 필드 검증 체크리스트: [`RemoteDeck_PC_v2.5.0_VerificationChecklist.md`](RemoteDeck_PC_v2.5.0_VerificationChecklist.md)

---

## v2.4.7 방어선 유지 (Non-Regression)

v2.4.7에서 확정된 콜드 부팅 최소 방어선은 v2.5.0에서 코드 diff **0줄**로 원형 유지:

- `NetManager.h/cpp` 무변경 (pre-init delay 500ms, W5500 SW reset, ETH.begin retry 3, GOT_IP watchdog 20s)
- v2.4.0~v2.4.6에서 제거된 미검증 개입(brownout disable, MAC stagger, NVS 추적) **재도입 금지 유지**

콜드 부팅 실제 원인은 아답터 불안정 전류로 확정되어 있으며 (2026-07-03 필드 확정, [[project-rdpc-coldboot-adapter]]), v2.5.0 펌웨어는 그 결론을 흔들지 않습니다.

---

## Migration

v2.4.7 → v2.5.0 은 **OTA 무중단 업그레이드**. 다음 하위호환 유지:

- `/schedule.json` 스키마: `action` 값 확장뿐, 기존 `on/off/toggle` 스케줄 무손실 로드
- `POST /api/schedule`: 기존 payload 형식 유지 (action="reboot"이면 relay=0 자동)
- `GET /api/log`, `POST /api/reboot`: 응답 shape 무변경

롤백 필요 시 v2.4.7 bin 재플래시 (`RemoteDeck_PC_V2.4.7_OTA_20260701.bin`).

---

## Files Changed

| Component | Files | Change |
|-----------|-------|:------:|
| ScheduleManager (reboot 지원) | `src/control/ScheduleManager.{h,cpp}` | +20 |
| WebServer schedule endpoint | `src/main.cpp` (scheduleSetter) | +3 |
| WebRequestHandler::syncCurrentStates | `src/network/WebRequestHandler.{h,cpp}` | +26 |
| main.cpp boot sync + reboot wiring | `src/main.cpp` | +26 |
| Web UI 관리 탭 | `data/www/{index.html,app.js,style.css}` | +107 |
| Version stamp v2.5.0 | `src/config/ConfigManager.cpp` | +2 |
| OTAHandler SPIFFS 대상 분기 | `src/web/OTAHandler.cpp` | +16 |
| Web UI 업데이트 카드 | `data/www/{index.html,app.js}` | +40 |
| IntegrateController LogEntry | `Models/LogEntry.cs` | **new** |
| IntegrateController RemoteDeckClient.GetLogsAsync | `Services/RemoteDeckClient.cs` | +65 |
| IntegrateController LogPoller | `Services/LogPoller.cs` | **new** |
| IntegrateController MainForm 통합 | `UI/MainForm.cs` | +90 |
| **총 diff** | ~13 files, +450 LOC | |
| **불변** | `NetManager.h/cpp` | diff = 0 ✅ |
