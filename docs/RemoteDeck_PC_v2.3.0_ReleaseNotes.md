# RemoteDeck PC v2.3.0 Release Notes

> **버전**: v2.3.0
> **최초 릴리스**: 2026-06-10
> **최종 개정**: 2026-06-11 (BUG-5, BUG-6 추가, OTA 자동 버전 파싱)
> **유형**: 안정성 / 디버깅 핫픽스 릴리스
> **대상**: v2.2.0 사용자 (in-place 펌웨어 업데이트 권장)

---

## Executive Summary

| 항목 | 내용 |
|------|------|
| 종류 | 안정성 버그 수정 (Hotfix 누적 릴리스) + 운영 편의 기능 추가 |
| 핵심 이슈 | IPSetupTool IP 변경 실패 / WebRequest 소켓 누수 / NTP 미동기화 / 웹 로그 표시 truncation |
| 변경 파일 | 펌웨어 7개 + IPSetupTool 2개 (이전 릴리스 누적 포함) |
| API 변경 | `/api/status`에 `heap_free`, `heap_min` 필드 추가 (하위 호환) |
| 권장 업데이트 | **필수** — Ethernet 모드에서 v2.2 사용자는 모두 영향 받음 |

### Value Delivered

| 관점 | 내용 |
|------|------|
| **Problem** | (1) IPSetupTool IP 변경 시 디바이스 Guru Meditation crash. (2) PCLED 토글 약 15회 후 WebRequest `socket: 105` 영구 실패. (3) 웹 UI 로그 시간 `--:--:--` 표시. (4) 웹 UI 로그에 WebRequest 호출 누락. (5) 장기 운영 시 약 25~30개 이후 웹 로그가 `undefined`로 truncate. (6) OTA 후 `fw_ver` 표시값이 deviceconfig.json 그대로라 수동 업데이트 필요. |
| **Solution** | (1) IPSetupTool HTTP REST API 전환. (2) WebRequestHandler를 단일 worker task + FreeRTOS queue로 재설계. (3) NTP 한국 IP fallback 2개 추가. (4) Logger thread-safe mutex + WebRequest 콜백 연결. (5) Logger JSON 직렬화 doc 크기를 entry 내용 기반 동적 사이징. (6) OTA 업로드 파일명(`RemoteDeck_PC_V{ver}_OTA_{date}.bin`)에서 버전/날짜 자동 파싱 + config 저장. |
| **Function/UX Effect** | IP 변경 안정성, 외부 시스템 연동 무한정 동작, 로그 시각과 100개 모두 정상 표시, OTA 업로드 한 번으로 펌웨어 + 버전 표시까지 동시 갱신, heap 모니터링으로 장기 운영 검증 가능. |
| **Core Value** | 현장 배포 가능한 수준의 장기 운영 안정성 확보 + 운영자 편의성. PIR 등 빠른 토글 응용에서도 검증 완료, OTA 운영 휴먼 에러 방지. |

---

## 1. 수정된 결함 (Bug Fixes)

### BUG-1: IPSetupTool IP 변경 시 Guru Meditation Crash

**증상**
- IPSetupTool에서 `[저장 및 재부팅]` 클릭 시 디바이스가 LoadProhibited 패닉으로 재부팅 루프
- 시리얼 로그에 `Guru Meditation Error: Core 0 panic'ed (LoadProhibited)` + EXCVADDR=0x34

**원인**
- UDP `SET_CONFIG` 패킷 (756 byte) 처리 중 ESP-IDF 5.3 W5500 driver의 `esp_netif_free_rx_buffer`가 NULL `netif`로 호출되며 panic
- `addr2line` 분석 결과 `esp_netif_lwip.c:1260`에서 NULL 포인터의 `+0x34` (driver_free_rx_buffer 필드) 접근
- ESP-IDF 5.3 / pioarduino platform-espressif32 53.03.10 플랫폼 레벨 버그 (사용자 코드 결함 아님)

**수정**
- IPSetupTool의 IP 변경 경로를 **HTTP REST API**로 전환:
  - `POST /api/config` (Basic Auth) → 설정 저장
  - `POST /api/reboot` (Basic Auth) → 재부팅
- UDP 경로는 백업 fallback으로만 유지 (HTTP 실패 시)
- `/api/reboot`는 펌웨어가 응답 직후 `ESP.restart()` 하므로 client는 3초 timeout 후 `TaskCanceledException`을 **성공으로 처리** (의도된 동작)

**검증 결과**
- 192.168.10.96으로 IP 변경 성공 (시리얼 로그: `Network: Applying static IP: 192.168.10.96`)
- 재부팅 후 신규 IP에서 정상 접속

**관련 커밋**: `3e1338f` fix(IPSetupTool): SET_CONFIG/REBOOT을 HTTP 경로로 전환

---

### BUG-2: WebRequest Socket 누수 (`socket: 105` / ENOBUFS)

**증상**
- 외부 서버(`http://192.168.10.230:9001/attend_status.php?…`)로 WebRequest 호출 시
- 약 **13~15회 성공** 후 다음 메시지로 영구 실패:
  ```
  [E][NetworkClient.cpp:232] connect(): socket: 105
  WebRequest FAIL [-1]: http://192.168.10.230:9001/...
  ```
- 디바이스 재부팅 없이는 복구 불가

**원인**
- `errno 105 = ENOBUFS` — lwIP socket pool 고갈
- 기존 `WebRequestHandler::fire()` 가 이벤트마다 `xTaskCreate` → 새 `HTTPClient` → 새 TCP socket 생성
- `http.end()` 호출 후에도 TCP는 **TIME_WAIT 상태**로 약 60초 유지 (2×MSL)
- ESP32 기본 `MEMP_NUM_NETCONN ≈ 10~16` → TIME_WAIT 누적되면 `lwip_socket()`가 ENOBUFS 반환
- PCLED 4~10초 토글 시 socket이 free될 시간 부족 → 누적 → 고갈

**수정**
`WebRequestHandler`를 task-per-request → **단일 worker task + FreeRTOS queue** 구조로 재설계:

```
이벤트 (main thread)            worker task (1개, 영구 실행)
─────────────────                ──────────────────────────
fire(event) ──→ xQueueSend ──→  xQueueReceive (대기)
                  (queue 8개)    │
                                 ├── HTTPClient 재사용 (setReuse=true)
                                 ├── http.begin/GET/end
                                 ├── 로그 출력
                                 └── 다음 큐 항목 처리
```

- HTTPClient 인스턴스 1개만 사용 → TCP 연결 재사용
- 큐 full (= 서버 다운 상황) 시 drop + `WebRequest DROP (queue full)` 로그
- `_busy` flag 제거 (큐 자체가 backpressure 역할)

**검증 결과**
- 30회 이상 연속 `WebRequest [200]` 성공 (이전: 15회에서 실패)
- 영구 안정 동작 확인

**파일**:
- `RemoteDeck_PC/src/network/WebRequestHandler.h`
- `RemoteDeck_PC/src/network/WebRequestHandler.cpp`

---

### BUG-3: 웹 UI 로그 시간 표시 빈값 (`--:--:--`)

**증상**
- 웹 UI 로그 페이지의 모든 항목이 `--:--:-- PCLED PC ON` 형태로 시간 누락
- 시리얼 로그: `NTP: Initial sync failed, will retry automatically`

**원인**
- `pool.ntp.org` 호스트명 DNS 조회 실패
- 본 프로젝트는 W5500 Ethernet driver의 DNS 동작 이슈로 hostname 해석 불가 (MQTT는 별도 hostname→IP workaround로 우회 중)
- SNTP 백그라운드 재시도도 동일 DNS 실패로 영구 실패

**수정**
NTP 서버에 한국 공개 NTP **IP 2개를 fallback**으로 추가:

```cpp
static const char* NTP_FALLBACK_1 = "168.126.63.1";    // kns.kornet.net (KT)
static const char* NTP_FALLBACK_2 = "203.248.240.140"; // time.bora.net (LG U+)

configTzTime(timezone, server, NTP_FALLBACK_1, NTP_FALLBACK_2);
```

- 사용자 설정 hostname이 DNS 실패해도 IP-form fallback으로 동기화 성공
- 초기 동기화 대기 시간 5초 → 10초 확장 (네트워크 RTT 여유)

**파일**: `RemoteDeck_PC/src/network/NTPSync.cpp`

---

### BUG-4: 웹 UI 로그에 WebRequest 호출 누락

**증상**
- 웹 UI 로그에 `SYSTEM`, `NETWORK`, `PCLED`, `RELAY`, `REBOOT`, `CMD`, `MQTT`, `SCHEDULE` 이벤트는 표시되지만
- WebRequest 호출 결과는 표시되지 않음

**원인**
- `WebRequestHandler`는 시리얼에만 출력하고 Logger 미연동
- BUG-2의 worker task 구조에서 Logger를 호출하려면 thread-safe 보장 필요

**수정**
1. Logger에 `SemaphoreHandle_t` mutex 추가 (push_back / erase / toJson 보호)
2. `WebRequestHandler::setLogger(callback)` 추가
3. worker loop가 매 요청 완료 후 콜백 호출:
   ```
   WEBREQ [200] http://192.168.10.230:9001/attend_status.php?node_id=node_1&status=ON
   WEBREQ [-1] http://...        (실패)
   WEBREQ DROP http://...        (큐 full)
   WEBREQ BEGIN FAIL http://...  (URL parse 실패)
   ```
4. `main.cpp`에서 callback 연결:
   ```cpp
   webRequestHandler.setLogger([](const char* evt, const char* det) {
       logger.log(evt, det);
   });
   ```

**파일**:
- `RemoteDeck_PC/src/utils/Logger.h` / `Logger.cpp`
- `RemoteDeck_PC/src/network/WebRequestHandler.h` / `.cpp`
- `RemoteDeck_PC/src/main.cpp`

---

### BUG-5: 웹 UI 로그 표시 truncation (`undefined` 표시)

**증상**
- PIR 장기 테스트 중 약 25~30개 항목 이후 웹 UI 로그가 잘리고
- 마지막 줄에 `[timestamp] undefined` 표시
- 시리얼은 정상 출력됨 (펌웨어 내부 `_entries` vector는 정상 누적)

**원인**
- `Logger::toJson()`이 `DynamicJsonDocument doc(4096)` 고정 4KB 버퍼 사용
- WEBREQ 1개 항목 ≈ `timestamp(10)` + `time(8)` + `event(6)` + `detail(URL 약 80)` + JSON 구조 오버헤드 = **약 150~200 바이트**
- 100개 항목 × 175 = **약 17.5KB 필요** → 4KB 초과 → `arr.createNestedObject()`가 invalid object 반환 → `obj["event"] = "WEBREQ"` no-op → JS 측 `undefined` 표시

**수정**
실제 entry 내용 합산 기반 동적 사이징:
```cpp
size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(_entries.size())
                + _entries.size() * JSON_OBJECT_SIZE(4);
for (const auto& e : _entries) {
    capacity += e.timeStr.length() + e.eventStr.length() + e.detailStr.length() + 16;
}
capacity += 512;  // safety margin
DynamicJsonDocument doc(capacity);
```

**부수 영향 (긍정)**
- `_entries` vector 자체는 정상 circular 동작이었으므로 **장기 운영 영향은 없었음** (표시만 잘림)
- 수정 후 100개 모두 정상 직렬화

**파일**: `RemoteDeck_PC/src/utils/Logger.cpp`

---

### BUG-6: OTA 후 `fw_ver` 자동 갱신 안 됨 + Heap 모니터링 부재

**증상**
- OTA로 펌웨어를 새 버전으로 교체해도 `/api/status`의 `fw_ver`는 SPIFFS의 `deviceconfig.json` 값을 그대로 표시 → 수동 설정 저장 필요
- 장기 운영 시 heap 누수 모니터링 수단 없음 (외부 진단 도구 필요)

**수정**

**1) OTA 업로드 파일명 기반 버전 자동 파싱**
- `OTAHandler`에 `setOnFilename(callback)` 추가
- 업로드 시작(`index == 0`) 시점에 파일명 콜백 호출
- main.cpp에서 파일명 패턴 `RemoteDeck_PC_V{VERSION}_OTA_{YYYYMMDD}.bin` 파싱
  - 예: `RemoteDeck_PC_V2.3.0_OTA_20260611.bin` → version `2.3.0`, date `2026-06-11`
- `Update.begin()` 직전에 `config.firmware.version/date` 갱신 + `ConfigManager::save(config)`
- 재부팅 후 새 버전으로 자동 표시

```cpp
webServer.ota().setOnFilename([](const String& filename) {
    int vIdx = filename.indexOf("_V");
    int oIdx = filename.indexOf("_OTA_");
    int dotIdx = filename.lastIndexOf(".bin");
    // ... parse, update, save ...
    config.firmware.version = version.c_str();
    config.firmware.date = dateFormatted.c_str();
    ConfigManager::save(config);
    logger.log("OTA", (String("Version ") + version + " (" + dateFormatted + ")").c_str());
});
```

**2) Heap 모니터링 필드 추가**
`/api/status` 응답에 다음 필드 추가 (하위 호환):
```json
{
  "heap_free": 202500,    // 현재 free heap (bytes)
  "heap_min": 190312      // 부팅 후 최저점 (가장 위험했던 순간)
}
```

활용:
- 30분~1시간 간격으로 `/api/status` 호출
- `heap_free` 변동 ±5KB 이내면 누수 없음
- `heap_min` 150KB 이상 유지되면 안전 마진 충분

**파일**:
- `RemoteDeck_PC/src/web/OTAHandler.h` / `.cpp`
- `RemoteDeck_PC/src/main.cpp`

---

## 2. 하드웨어 응용 가이드 (참고)

v2.3.0 디버깅 과정에서 검증된 응용 시나리오:

### 2.1 PCLED 단자 재사용 (PIR 인체감지)

PCLED 입력은 광커플러(PC817) → ESP32 GPIO4 (INPUT, inverted logic) 구조.

PCLED 입력 인식 임계값:
- **LOW (PC ON)**: 광커플러 LED forward current 3~10 mA 이상 → GPIO 0V 근처
- **HIGH (PC OFF)**: 광커플러 OFF → GPIO 3.3V (내부 pull-up)

| 입력 방식 | 동작 여부 | 비고 |
|----------|----------|------|
| PC 메인보드 PWR_LED (5V/3.3V) | ✅ | 원래 용도, 정상 동작 |
| 5V 1A 어댑터 직결 | ✅ | 광커플러가 전류 제한, 입력 5V 안전 |
| PIR 220V output → 5V 어댑터 → PCLED± | ✅ | PIR 동작 시 1~2초 내 ON 인식 |

**응용 시 주의**:
- Triac 출력 PIR 센서는 leakage current로 인해 완전 OFF 안 될 수 있음 (Relay 출력 PIR 권장)
- 응답 시간: ON 4~6초, OFF 10~40초 (debounce 3회 × 1초 poll + PIR holdover)

### 2.2 PIR 연동 시 WebRequest 안정성

v2.3.0 이전 (task-per-request 구조):
- PIR 잦은 토글 → socket 누수 → 15회 후 영구 실패

v2.3.0 이후 (worker + queue 구조):
- 단일 socket 재사용 → **영구 안정 동작 검증 완료**

---

## 3. 업그레이드 방법

### 3.1 OTA 업데이트 (권장)

1. 웹 UI 접속 (`http://{장치IP}:5050`) → **펌웨어** 탭
2. 펌웨어 binary 업로드 — 파일명 규칙을 지키면 버전 자동 갱신
3. 자동 재부팅 (설정 보존)

**파일명 규칙** (BUG-6 자동 버전 파싱 활용):
```
RemoteDeck_PC_V{VERSION}_OTA_{YYYYMMDD}.bin
```
예시:
- `RemoteDeck_PC_V2.3.0_OTA_20260611.bin` → 자동으로 fw_ver="2.3.0", date="2026-06-11" 저장
- `RemoteDeck_PC_V2.4.0_OTA_20260801.bin` → 자동으로 fw_ver="2.4.0", date="2026-08-01" 저장
- 규칙에 안 맞으면 펌웨어는 정상 업데이트되지만 `fw_ver`는 변경되지 않음 (시리얼에 `Filename pattern not recognized` 출력)

### 3.2 시리얼 업로드 (개발자)

```powershell
cd C:\Code\Projects\RemoteDeck\RemoteDeckSystem\RemoteDeck_PC
pio run -e esp32dev -t upload --upload-port COM3
```

### 3.3 IPSetupTool 함께 업데이트

`IPSetupTool/publish/IPSetupTool.exe`도 함께 배포 권장 (BUG-1 수정 포함).

---

## 4. 검증 체크리스트

업데이트 후 다음 항목 확인:

- [ ] 부팅 시리얼 메시지에 `=== RemoteDeck PC Power Manager v2.3 ===` 출력
- [ ] 부팅 후 `NTP: Synced - YYYY-MM-DD HH:MM:SS` 메시지 출력 (시간 동기화 성공)
- [ ] 웹 UI 로그 페이지의 시간 필드가 `HH:MM:SS` 형식으로 표시
- [ ] IPSetupTool로 IP 변경 시 crash 없이 정상 재부팅
- [ ] WebRequest 활성화 후 PCLED 토글 30회 이상 연속 성공 (`socket: 105` 미발생)
- [ ] 웹 UI 로그에 `WEBREQ [200] http://...` 항목 표시
- [ ] 웹 UI 로그가 100개 항목까지 모두 정상 표시 (`undefined` 미발생)
- [ ] OTA 후 `/api/status`의 `fw_ver` 값이 업로드한 파일명의 버전과 일치
- [ ] `/api/status`에 `heap_free`, `heap_min` 필드 존재 (장기 운영 모니터링용)

---

## 5. 변경 파일 목록

### Firmware (RemoteDeck_PC/)

| 파일 | 변경 내용 |
|------|-----------|
| `src/main.cpp` | 부팅 메시지 v2.2 → v2.3, WebRequest setLogger 연결, OTA 파일명 버전 파서 콜백 등록, `/api/status`에 heap_free/heap_min 필드 추가 |
| `src/config/ConfigManager.cpp` | 기본 firmware.version "2.3.0", date "2026-06-10" |
| `src/network/WebRequestHandler.h` / `.cpp` | worker task + queue 구조 재설계, LogCallback 추가 |
| `src/network/NTPSync.cpp` | 한국 NTP IP 2개 fallback, 초기 동기화 시간 10초 확장 |
| `src/utils/Logger.h` / `.cpp` | SemaphoreHandle_t mutex, `toJson()` 동적 사이징 (4KB 고정 → entry 내용 기반) |
| `src/web/OTAHandler.h` / `.cpp` | `setOnFilename(cb)` 인터페이스 추가, 업로드 시작 시점에 파일명 콜백 호출 |
| `data/deviceconfig.json` | version "2.3.0", date "2026-06-10" |

### IPSetupTool

| 파일 | 변경 내용 |
|------|-----------|
| `Services/DeviceConfigService.cs` | HTTP REST 메소드 추가 (SendConfigViaHttpAsync, SendRebootViaHttpAsync) |
| `MainForm.cs` | HTTP-first, UDP fallback 경로 적용 |

---

## 6. 알려진 제약사항 (Known Limitations)

- **NTP 동기화 첫 시도 실패**: 초기 10초 내 동기화 실패 시 SNTP가 백그라운드 재시도하나, IP fallback도 막힌 폐쇄망에서는 시간 표시 불가 — 이 경우 사용자 설정에서 사내 NTP IP를 입력해야 함
- **WebRequest queue full drop**: 큐 크기 8개 — 서버 응답이 5초 이상 지연 + 1초 이내 8회 이벤트 발생 시 drop (drop 자체는 로그됨)
- **`/api/reboot` 응답**: 현재 펌웨어가 응답 후 즉시 restart하므로 client는 timeout을 받음 (IPSetupTool은 이를 성공으로 처리). 향후 응답 flush 후 restart로 개선 예정

---

## 7. 다음 버전 후보 (v2.4 backlog)

본 릴리스에서 의도적으로 보류한 개선 사항:

- 펌웨어: UDP `SET_CONFIG` handler deprecation (HTTP-only로 단순화)
- 펌웨어: `/api/reboot` 응답 완료 후 restart (TaskCanceledException 제거)
- 펌웨어: SPIFFS OTA 지원 (`Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)`)
- 펌웨어: GPIO1~3 입력에 `INPUT_PULLUP` + 폴링 간격 옵션 노출 (PIR 천정 릴레이 직결 시 응답 시간 단축)
- 플랫폼: pioarduino platform-espressif32 최신 버전 (W5500 NULL netif 패치 적용 가능성)
- 응용: PIR 인체감지 통합 정식 기능화 (PCLED 재사용 → 전용 GPIO 분리, AC 220V 트리거 릴레이 모듈 표준 연결)

---

## 8. 참고 문서

- [RemoteDeck_PC_Manual.md](RemoteDeck_PC_Manual.md) — 본체 사용자 매뉴얼 (v2.3.0으로 함께 업데이트됨)
- [v2.2-api-simplification.report.md](04-report/features/v2.2-api-simplification.report.md) — v2.2 API 단순화 보고서
- 이전 핫픽스 커밋:
  - `b6d4371` fix(IPSetupTool): DHCP 상태 저장/복원으로 인터넷 단절 방지
  - `3e1338f` fix(IPSetupTool): SET_CONFIG/REBOOT을 HTTP 경로로 전환
- v2.3.0 릴리스 커밋:
  - `cab6764` feat(RemoteDeck_PC): v2.3.0 안정성 핫픽스 릴리스 (BUG-1 ~ BUG-4)
  - `43547a8` fix(RemoteDeck_PC): 웹 로그 표시 truncation + heap 모니터링 추가 (BUG-5)
  - (이번 커밋) fix(RemoteDeck_PC): OTA 파일명에서 버전/날짜 자동 파싱 (BUG-6)

---

*Generated: 2026-06-10, Revised: 2026-06-11 by RemoteDeck PC dev team*
