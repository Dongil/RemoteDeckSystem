---
template: analysis
version: 0.1
feature: rcm-hub-v3.0
date: 2026-11-10
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_PC (firmware + Web UI)
branch: v3.0-rcm-hub
plan: docs/archive/2026-11/RemoteDeck_PC_v3.0/plan.md
design: docs/archive/2026-11/RemoteDeck_PC_v3.0/design.md
phase: check
matchRate: 98
---

# RemoteDeck_PC v3.0.0 — RCM 허브 deck 연동 Gap Analysis (Check)

> **Summary**: FR-01~15 전부 구현. 가상 허브(rcm-api 참조 허브 v1.0.3) + 실기 2대(192.168.10.109 `node_2`, 192.168.10.193 `node_1` PC LED 연결)로 L1 API 11/12, conformance(basic·guard·relay·power·led·reboot) 전 항목 PASS. 실기 중 발견한 OTA 응답 미전송(v2.5.1부터 있던 문제)을 수정. Important 갭 3건(G-01~03)도 Act에서 수정·실기 확인 → **matchRate 98%**.

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | RCM 허브 통합 운영을 위해 SmartDeck이 허브 `deck`으로 붙어야 하고, LED 미확인 전원 조작(현장 오조작 원인)을 제거해야 함 |
| **WHO** | RCM 서버/허브(명령 발신), 설치 운영자(웹 UI 설정·상태 확인), 기존 MQTT/RS485/재부재 사용 현장(회귀 없어야 함) |
| **RISK** | loop() 블로킹, 설정 키 4곳 누락, 허브 미사용 현장 회귀, GOT_IP 재부팅과 재접속 겹침, 전원/아답터 불안정 오인 |
| **SUCCESS** | `pio run` 성공 · conformance 기본 + `--with power,led,reboot` PASS · 수동 4항목 · hub.enabled=false 회귀 0 |
| **SCOPE** | Phase1 펌웨어 / Phase2 충돌 정리·보안 / Phase3 Web UI / Phase4 실기 conformance |

---

## 1. 검증 방식

| 층 | 방법 | 결과 |
|----|------|------|
| L0 빌드 | `pio run` | SUCCESS — Flash 76.3% (1,499,936 / 1,966,080, 여유 23.7%), RAM 16.8% |
| 정적 | gap-detector (설계 §3.3·§4·§5·§6.4 ↔ 코드 ↔ app.js) | Structural 97 / Functional 93 / Contract 92 |
| L1 API | curl + UDP 스크립트, 실기 `.109` | T-01~12 중 11 PASS, T-10 보류 |
| L3 프로토콜 | `rcm-api/tools/conformance.js --role deck` | `.109`: basic·guard·relay·reboot PASS / `.193`: power·led 포함 전 항목 PASS |
| L4 현장 | F-00~06 | 일부만 관찰(아래 §4), 사용자 판단으로 이번 Check에서 종료 |

허브 서버가 아직 완성되지 않아 **rcm-api 참조 허브**(`hub/server.js`, 로컬 `hub/config.json`, 방 `RDTEST01`/`RDTEST02`)로 대체 시험.

---

## 2. Functional Requirements 대비

| ID | 요구사항 | 구현 | 실기 | 근거 |
|----|----------|:--:|:--:|------|
| FR-01 | lib/RcmHub 반입 + WebSockets 의존성 | ✅ | ✅ | `platformio.ini` lib_deps, `lib/RcmHub/SOURCE.md`. 부팅 로그 `HUB: HMAC self test OK` |
| FR-02 | hub 키 4곳 반영 | ✅ | ✅ | DeviceConfig/ConfigManager/onUDPConfig/buildConfigJson. T-05·T-06·T-12에서 값 보존 |
| FR-03 | NTP 뒤 begin 1회, loop마다 deck.loop | ✅ | ✅ | 부팅 로그 `NTP: Synced` → `HUB: started` → `[rcm] ready` |
| FR-04 | LED 확인 후에만 누름 | ✅ | ✅ | conformance guard PASS, power(powerOn/forceOff) PASS(`.193`) |
| FR-05 | LED 변화 즉시 보고, welcome 후 full | ✅ | ✅ | conformance `full=true rev=0`, led(손으로 누름 off→on) PASS |
| FR-06 | 4001/4009/4010 정지 + 사유 노출 | ✅ | ⚠️ | 코드·UI 매핑 확인(목업 Playwright). 실기 4001/4009 수동 시험은 미실시 |
| FR-07 | /api/status hub_* 6필드 | ✅ | ✅ | T-01(off), 연결 시 `hub_connected:true`, `hub_host`, `hub_product_id` |
| FR-08 | 허브 시 스케줄 릴레이1 건너뜀 | ✅ | — | `main.cpp` onScheduleAction. 실기 시간 트리거 시험 없음 |
| FR-09 | 허브 시 릴레이1 latch 거부, pulse 허용 | ✅ | ✅ | T-07/T-07b 409, T-08 200, T-09(허브 off) 200 |
| FR-10 | 허브 시 MQTT 끔 | ✅ | ✅ | `mqtt_connected:false`, begin 미호출 |
| FR-11 | device_key 가림 | ✅ | ✅ | T-02, T-03, T-04(재부팅 후 재접속) |
| FR-12 | OTA Basic 인증 | ✅ | ✅ | T-11 401·재부팅 없음, 인증 업로드 정상 |
| FR-13 | 버전 3.0.0 통일 | ✅ | ✅ | 배너 `v3.0.0`, `fw_ver`, deck.state `fw:"3.0.0"`, OTA 파일명 버전 기록 |
| FR-14 | 설정 > 기타 RCM-API 카드 | ✅ | ✅ | §6.4 체크리스트 15/15, 실기 index.html 서빙 확인 |
| FR-15 | 홈 RCM 허브 카드 | ✅ | ✅ | 목업 상태 4종 + 실기 서빙 |

---

## 3. Success Criteria

| 기준 | 상태 | 근거 |
|------|:--:|------|
| FR-01~15 구현 | ✅ | §2 |
| `pio run` 성공, Flash 여유 ≥15% | ✅ | 여유 23.7% |
| conformance 기본 그룹 PASS | ✅ | `.109`·`.193` 모두 |
| conformance `--with power,led,reboot` PASS | ✅ | reboot(`.109`), power·led(`.193` PC LED 연결 장비). `.109`의 power/led FAIL은 PC 미연결 때문 |
| 수동 4항목(허브 재기동 ≤10s, 랜선 재연결, 4001, 4009) | ⚠️ | 허브 재기동 후 재접속은 관찰(`.193` 재부팅 후 4s, `.109` 재부팅 후 3s). 허브만 재기동한 경우의 시간·랜선·4001·4009는 미실시 |
| hub.enabled=false 회귀 | ⚠️ | T-01·T-09 PASS. MQTT/RS485/재부재/WebRequest 전체 회귀표는 미실시 |
| SPIFFS OTA 설정 보존 | ✅ | T-12: 허브 꺼진 이미지 위로 허브 켜진 설정 복원, schedule 보존 |
| Manual 반영 | ✅ | §12 신설, API·문제해결·OTA 응답 동작 |
| NetManager 방어선 diff=0 | ✅ | `git diff main -- src/network/Net*` 0줄 |
| 아답터·전원 크로스체크 | ⚠️ | `.109`가 한때 네트워크에서 사라졌으나 USB 재연결 후 정상, 펌웨어 로그 이상 없음 |

---

## 4. 런타임 결과 상세

### 4.1 L1 API (`.109`, 허브 연결 상태)
T-01 ✅ · T-02 ✅ · T-03 ✅ · T-04 ✅ · T-05 ✅ · T-06 ✅(IPSetupTool은 HTTP 저장 경로) · T-07 ✅ · T-08 ✅ · T-09 ✅ · T-10 ⏭(RS485 어댑터 없음) · T-11 ✅ · T-12 ✅

### 4.2 Conformance
- `.109` (`--with power,led,reboot`): 11 PASS / 2 FAIL(power·led — PC 미연결로 LED 무변화) / 2 SKIP
- `.193` 1차 `--only power`: 9 PASS / 1 SKIP(PC 켜짐) — forceOff PASS
- `.193` 2차 `--only power,led`: **11 PASS / 0 FAIL** — powerOn, forceOff, 손으로 누름 보고 off→on
- 허브 쪽 규격 위반 0건, 장비 프레임 규격 위반 0건, heartbeat 중 예기치 않은 끊김 0건

---

## 5. 실기 중 발견·수정 (Check 단계 변경)

| 항목 | 내용 | 조치 |
|------|------|------|
| OTA 응답 미전송 | v2.5.1부터 응답 콜백 안에서 `delay(1000); ESP.restart()` → 응답이 flush되지 않아 curl이 timeout까지 대기(웹 UI는 v2.5.2 `upload.onload` 우회로 가려짐) | `OTAHandler::loop()`로 복원·재부팅 이관(1.5s 후), `main.cpp` loop에서 호출. `.109`에서 펌웨어 OTA HTTP 200 6.6s, SPIFFS OTA 200 0.8s, 설정 보존 확인. `.193`에도 적용. Manual §8.2 갱신 |

설계 문서에는 없는 변경이므로 Report에 Design 편차로 기록.

---

## 6. Gap 목록

### 6.1 Important (신뢰도 ≥80%)

| # | Gap | 근거 | 제안 |
|---|-----|------|------|
| G-01 | 허브 사용 중 홈 「릴레이1 켜기」 버튼이 409를 받아도 UI가 아무 반응 없음 | `app.js` relay()가 응답 미확인 / `index.html` 릴레이 버튼 | 409면 안내 표시, 또는 `hub_enabled`일 때 latch 버튼 비활성 |
| G-02 | 가드(스케줄·latch·MQTT)가 실시간 `config.hub.enabled`를 읽어, 저장만 하고 재부팅 전이면 허브 상태와 가드가 어긋남(설계 §1.2 "재부팅 후 반영"과 불일치) | `main.cpp` 가드 4곳 ↔ onUDPConfig 즉시 반영 | 부팅 시 1회 캡처한 값(`hubActive`)으로 가드 판정 |
| G-03 | 웹 UI OTA가 서버 응답을 보지 않음 — 401·`{ok:false}`도 "업로드 완료" 표시. 이제 서버가 응답을 보내므로 판정 가능 | `app.js` uploadOTA/uploadFS `upload.onload` | `xhr.onload`에서 status/ok 확인, 응답 없을 때만 기존 reload 폴백 |

### 6.1.1 Act 결과 (2026-11-10, 사용자 결정: 지금 모두 수정)

| # | 수정 | 실기 확인 |
|---|------|----------|
| G-01 | `app.js` relay(): 409면 안내창, 그 밖의 실패도 표시 | `.109` 브라우저: 릴레이1 켜기 → 409 → 안내창 |
| G-02 | `main.cpp` `hubActive`를 setup에서 1회 확정, 가드 4곳(latch·스케줄·MQTT·허브 시작)이 이 값 사용 | 허브 끔 저장(재부팅 안 함) 후 릴레이1 on → 여전히 409, 허브 연결 유지 |
| G-03 | `app.js` 공통 `sendOTA()`: 200+ok면 성공·재로드, 401·실패면 안내만(재로드 안 함). 응답 없는 구버전 장비는 업로드 완료 20s 뒤 성공 처리 | 잘못된 파일 업로드 → `업로드 실패` 안내, 재부팅 없음(uptime 유지) |

빌드: Flash 76.3%(1,499,948 B), RAM 16.8%. 두 장비 모두 펌웨어+SPIFFS OTA(HTTP 200) 후 설정 동일·허브 재연결.

### 6.2 Minor

- "config incomplete" 로그가 네트워크 재연결마다 반복
- `/api/status` 문서 768B — hub_stop_reason이 길면 hub_* 필드가 잘릴 여지(1024 권장)
- 웹 UI에서 저장된 device_key를 비울 방법 없음(빈칸 = 유지)
- HMAC self test 결과가 웹 로그에는 남지 않음(Serial만)
- OTA 완료 후 재부팅까지 1.5s 동안 설정 저장 시 유실 가능(실사용 영향 낮음)

---

## 7. Match Rate

| 축 | 점수 | 가중치 |
|----|:--:|:--:|
| Structural | 97 | 0.15 |
| Functional | 93 | 0.25 |
| Contract | 92 | 0.25 |
| Runtime (실행 항목 통과율; T-10·F 항목 일부 미실시) | 100 | 0.35 |
| **Overall (Check)** | **96** | |
| **Overall (Act 후, Functional 93→98, Contract 92→97)** | **98** | |

정적만 계산 시 Check 93%. 품질 게이트(≥90%) 통과.

---

## 8. Carry Items

- 수동 4항목 중 미실시분(허브 단독 재기동 시간, 랜선 재연결, 4001, 4009) — 허브 서버 완성 후 현장 dogfood에서
- T-10 RS485 latch 가드 실기 확인
- hub.enabled=false 전체 회귀표(MQTT/RS485/재부재/WebRequest)
- IntegrateController hub 컬럼, UDP 인증 + IPSetupTool, RemoteDeck_Touch 허브 연동 (Plan carry)
- rcm-api: 로컬 시험용 `hub/config.json`(RDTEST01/02)은 git 무시 파일, 정식 허브 전환 시 장비 host/키 재설정 필요
