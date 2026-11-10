---
template: plan
version: 1.3
feature: rcm-hub-v3.0
date: 2026-11-09
author: KDI
project: RemoteDeckSystem
firmware_version: from v2.6.2 → v3.0.0 (target)
---

# rcm-hub-v3.0 Planning Document

> **Summary**: RemoteDeck_PC가 RCM 통합 API 허브에 `deck`(SmartDeck)으로 접속한다. 허브 명령(`pc.powerOn`/`pc.forceOff`)으로 전원 버튼(릴레이1)을 LED 확인 후에만 누르고, PC-LED 변화를 즉시 보고한다. 웹 UI 설정에 「RCM-API 연동」 카드, 홈에 허브 상태 카드를 추가하고, 허브와 충돌하는 기존 전원 경로(스케줄·latch·MQTT 블로킹)를 정리한다. 버전 문자열을 3.0.0으로 통일한다.
>
> **Project**: RemoteDeckSystem
> **Author**: KDI
> **Date**: 2026-11-09
> **Status**: Draft

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | 강의실 전원을 RCM 서버가 통합 관리해야 하는데 RemoteDeck_PC는 허브와 연결할 수단이 없다. 지금 펌웨어는 어떤 경로에서도 버튼을 누르기 전에 LED를 확인하지 않아 "켜진 PC에 ON을 보내 꺼버림", "녹화 중 스케줄 강제 종료" 같은 현장 문제가 생긴다. 또 MQTT connect가 loop()를 막는 구조라 WebSocket heartbeat를 유지할 수 없다. |
| **Solution** | 규격 저장소(`rcm-api`)의 참조 라이브러리 `RcmHub`/`RcmDeck`(컴파일 확인됨)을 `lib/`로 가져와 main.cpp에 연결한다. `hub` 설정 블록을 네 곳(DeviceConfig/ConfigManager/UDP/buildConfigJson)에 추가한다. `hub.enabled`일 때는 로컬 스케줄 전원 동작 비활성, 릴레이1 latch 금지, MQTT 미시작으로 충돌을 차단한다. device_key 가리기와 `/api/ota` 인증을 추가한다. |
| **Function/UX Effect** | 설치자는 설정 > 기타의 「RCM-API 연동」 카드에 host/port/제품번호/장비키를 넣기만 하면 된다. 홈 상단 허브 카드에서 접속 상태와 정지 사유(4001 키 오류, 4009 중복, 4010 버전)를 바로 볼 수 있다. 허브는 LED라는 단일 근거로 강의실 전원 상태를 알고, 이중 안전장치로 잘못 누르는 일이 없어진다. |
| **Core Value** | RCM 통합 운영의 SmartDeck 파트 완성(규격 v1 conformance PASS). 전원 오조작 원인 제거. 기존 MQTT/RS485/재부재 기능은 허브 미사용 시 동작 그대로 유지. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | RCM 허브 통합 운영을 위해 SmartDeck이 허브 `deck`으로 붙어야 하고, LED 미확인 전원 조작(현장 오조작 원인)을 제거해야 함 |
| **WHO** | RCM 서버/허브(명령 발신), 설치 운영자(웹 UI 설정·상태 확인), 기존 MQTT/RS485/재부재 사용 현장(회귀 없어야 함) |
| **RISK** | (1) loop() 블로킹 → heartbeat 끊김(15~30s). (2) 설정 키 4곳 중 하나 누락 → UDP/HTTP 설정 변경 시 hub 값 소실. (3) 허브 미사용 현장 회귀(스케줄/latch/MQTT 동작 변경). (4) 이더넷 GOT_IP 감시 재부팅(20s)과 허브 재접속 겹침. (5) 실기 시험 중 전원/아답터 불안정을 S/W 문제로 오인 |
| **SUCCESS** | `pio run` 성공 · conformance `--role deck` 기본 그룹 + `--with power,led,reboot` 전부 PASS · 수동 4항목(허브 재기동 10s 내 재접속, 랜선 재연결, 4001 정지+화면 표시, 4009 중복 정지) 통과 · hub.enabled=false에서 기존 기능 회귀 0 |
| **SCOPE** | Phase1 펌웨어(라이브러리·설정·연결·/api/status) / Phase2 충돌 정리·보안(스케줄·latch·MQTT·키 가림·OTA 인증·버전 통일) / Phase3 Web UI(설정 카드·홈 카드) / Phase4 실기 conformance + dogfood |

---

## 1. Overview

### 1.1 Purpose

RemoteDeck_PC 펌웨어에 RCM 허브 클라이언트(`deck` 역할)를 넣는다. 허브가 강의실 PC 전원을 켜고 강제 종료할 수 있게 하고, PC 전원 LED 상태를 허브에 실시간으로 보고한다. 이 LED 값이 허브가 강의실 전원 상태를 판단하는 **유일한 근거**다.

### 1.2 Background

- `rcm-api` 저장소(규격 v1 고정, 태그 v1.0.2)가 RCM 웹/서버, HD-CTR 녹화장비, SmartDeck을 하나의 WebSocket API로 묶는다. SmartDeck 파트 인계 문서(`handoff/SMARTDECK.md`)가 작업 지시, 설정 키, 통합 위치, 충돌 목록, 시간 값, 시험 절차를 정해 두었다.
- 참조 라이브러리 `clients/esp32/lib/RcmHub`(RcmHub 연결·인증·재접속 + RcmDeck 전원 보호·보고, 약 540줄)는 같은 플랫폼(pioarduino 53.03.10 / Arduino-ESP32 3.1.0)과 같은 라이브러리(ArduinoJson 6, ESPAsyncWebServer 3.6)로 **컴파일만** 확인됐고 실기 시험은 아직 하지 않았다.
- 현장 조사 결과, 스케줄·MQTT·RS485·HTTP 어느 경로도 버튼을 누르기 전에 LED를 확인하지 않는다.
- 버전 문자열이 세 곳에서 다르다: `data/deviceconfig.json` 2.3.0, `ConfigManager.cpp` 기본값 2.6.2, 부팅 배너 "v2.3". 허브에 보고하는 `fw` 값이 틀릴 수 있다.

### 1.3 Related Documents

- 인계 문서: `D:\SourceCode\Projects\RCM Websocket API\rcm-api\handoff\SMARTDECK.md` (§0~§7)
- 규격: `rcm-api/spec/catalog.json`, `rcm-api/docs/OPS.md`
- 참조 구현: `rcm-api/clients/esp32/lib/RcmHub/`, `rcm-api/clients/esp32/src/main.cpp`
- 검사 도구: `rcm-api/tools/conformance.js`, `rcm-api/tools/device-key.js`
- 이전 사이클: `docs/archive/2026-07/RemoteDeck_PC_v2.6.2/`(재부재 카드 패턴)
- API 문서: `docs/RemoteDeck_PC_Manual.md`

---

## 2. Scope

### 2.1 In Scope

**Phase 1 — 허브 연결 (펌웨어)**
- [ ] `rcm-api/clients/esp32/lib/RcmHub/` → `RemoteDeck_PC/lib/RcmHub/` 복사(원본 커밋/태그 기록)
- [ ] `platformio.ini` `lib_deps`에 `links2004/WebSockets@^2.6.1` 추가
- [ ] `hub` 설정 블록(`enabled, host, port, path, tls, product_id, device_key`)을 네 곳에 추가: `DeviceConfig.h`, `ConfigManager.cpp` load/save/defaults, `onUDPConfig`, `buildConfigJson`
- [ ] `RcmDeckIo` 바인딩: `pcOn`=PCMonitor 디바운스 값, `getRelay/setRelay/pulse`=RelayController(1 기준), `inputs`=GPIO1~3, `ip`, `net`(eth/wifi), `reboot`, `fw`
- [ ] `onConnected`에서 NTP 뒤에 `deck.begin()` 한 번만(`deckStarted` 플래그), `loop()`에서 매 바퀴 `deck.loop()`
- [ ] 부팅 로그에 `RcmHub::selfTest()`(HMAC 벡터) 결과 출력
- [ ] `/api/status`에 `hub_connected`, `hub_stopped`, `hub_stop_reason` 추가(기존 필드 변경 없음)

**Phase 2 — 충돌 정리 · 보안 · 버전**
- [ ] `hub.enabled`이면 로컬 스케줄의 **릴레이1 전원 동작**(on/off/toggle)을 건너뛰고 로그를 남김
- [ ] `hub.enabled`이면 릴레이1 **latch on 거부**(MQTT/RS485 `{cmd:relay,state:on}`, `/api/relay state:on`). pulse는 허용(점검용)
- [ ] `hub.enabled`이면 **MQTT를 시작하지 않음**(begin/loop 건너뜀, 로그 + UI 안내)
- [ ] `device_key`를 `/api/config`, UDP `GET_CONFIG` 응답에서 `"****"`로 가리고, `"****"`가 들어오면 기존 값 유지
- [ ] 펌웨어/SPIFFS OTA 업로드 경로(`/api/ota` 등)에 Basic 인증 추가(웹 UI XHR은 같은 origin이라 브라우저 인증이 자동 첨부됨)
- [ ] 버전 3.0.0으로 통일: `data/deviceconfig.json` `"version"`(단일 기준), `ConfigManager` 기본값, 부팅 배너가 `config.firmware.version`을 출력하도록
- [ ] 운영 원칙 문서화: 허브 사용 시 전원 명령 경로는 허브 하나만 사용(Manual)

**Phase 3 — Web UI (`data/www`)**
- [ ] 설정 > 기타 서브페이지, 「재부재 시스템」 카드 바로 아래 「RCM-API 연동」 카드: 사용 체크, host(IP 권장 안내), port, path, tls, product_id, device_key(password 입력, `****` 표시). 기존 `saveEtc()`로 저장
- [ ] 허브 사용 시 영향 안내 문구(스케줄 전원 동작·MQTT 비활성, 릴레이1 latch 금지)
- [ ] 홈: `hub.enabled`일 때만 표시하는 「RCM 허브」 카드(재부재 카드와 같은 조건부 패턴): 접속 상태 배지, 정지 사유(4001/4009/4010 한글 설명), 제품번호, 허브 주소

**Phase 4 — 검증**
- [ ] `pio run` 빌드 성공, Flash/RAM 사용량 기록
- [ ] conformance 기본 그룹 + `--with power,led,reboot` PASS
- [ ] 수동 4항목 + hub.enabled=false 회귀 점검
- [ ] 라이브러리를 수정했다면 `rcm-api`에 PR

### 2.2 Out of Scope

- IntegrateController 그리드의 허브 상태 표시(다음 버전 carry; 클라는 `/api/status` 신규 필드를 무시해도 회귀 없음)
- RemoteDeck_Touch 허브 연동
- UDP `GET_CONFIG`/`SET_CONFIG`/`REBOOT` 인증(IPSetupTool 동시 변경 필요 → carry)
- 허브 미사용 시 스케줄 LED 가드, 허브 미사용 시 릴레이1 latch 금지(기존 현장 동작 유지 결정)
- 규격(`spec/`)·검사 도구(`tools/conformance.js`) 수정 — 필요 시 rcm-api Issue "규격 변경 요청"
- 정상 종료(녹화장비 에이전트 담당), 켜기 재시도·부팅 확인(허브 담당)

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | `lib/RcmHub` 반입 + `links2004/WebSockets` 의존성 추가, 빌드 성공 | High | Pending |
| FR-02 | `hub` 설정 7개 키를 DeviceConfig/ConfigManager(load·save·defaults)/onUDPConfig/buildConfigJson 네 곳에 반영. 기본 `enabled:false, port:8891, path:"/v1/ws", tls:false` | High | Pending |
| FR-03 | 네트워크 연결 시 NTP 뒤 `deck.begin()` 1회, `loop()`마다 `deck.loop()`. 재접속은 라이브러리 담당 | High | Pending |
| FR-04 | `pc.powerOn`은 LED off일 때만, `pc.forceOff`는 LED on일 때만 누름. 이미 원하는 상태면 `{pressed:false}` 즉시 응답(라이브러리 RcmDeck) | High | Pending |
| FR-05 | LED 변화 시 즉시 `deck.state {pc}` 보고, welcome 직후 `full:true, rev:0` 전체 상태 전송(라이브러리) | High | Pending |
| FR-06 | `hub.closing` 4001/4009/4010 수신 시 재접속 중단, 사유를 `/api/status`와 홈 카드에 노출 | High | Pending |
| FR-07 | `/api/status`에 `hub_connected`(bool), `hub_stopped`(bool), `hub_stop_reason`(문자열/코드) 추가. hub 비활성 시에도 필드 존재(false) | Medium | Pending |
| FR-08 | `hub.enabled`이면 로컬 스케줄의 릴레이1 전원 동작 건너뜀 + `SCHEDULE` 로그 "skipped (hub)" | High | Pending |
| FR-09 | `hub.enabled`이면 릴레이1 latch on 거부(MQTT/RS485/HTTP 공통 경로), pulse 허용. 거부는 로그 + HTTP는 오류 응답 | High | Pending |
| FR-10 | `hub.enabled`이면 MQTT begin/loop 건너뜀. 설정값은 보존 | High | Pending |
| FR-11 | `device_key`를 `/api/config`·UDP `GET_CONFIG`에서 `****`로 가림, `****` 입력 시 기존 값 유지 | High | Pending |
| FR-12 | 펌웨어/SPIFFS OTA 업로드 엔드포인트에 Basic 인증 적용, 웹 UI 업로드는 정상 동작 | Medium | Pending |
| FR-13 | 버전 3.0.0 통일(deviceconfig.json, ConfigManager 기본값, 부팅 배너 → `config.firmware.version`) | Medium | Pending |
| FR-14 | 설정 > 기타 「RCM-API 연동」 카드(재부재 카드 아래), `saveEtc()` 저장, 영향 안내 문구 | High | Pending |
| FR-15 | 홈 「RCM 허브」 카드(hub.enabled 조건부): 접속 배지·정지 사유·제품번호·허브 주소, `/api/status` 주기 갱신 | Medium | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| 응답성 | 허브 사용 시 loop() 1바퀴가 heartbeat를 놓칠 만큼 막히지 않음(전원 누름 중에도 loop 진행) | conformance power 그룹 PASS, 시리얼 로그 |
| 재접속 | 허브 재기동 후 10s 이내 재접속, 랜선 재연결 후 재접속 | 수동 시험 |
| 자원 | app 파티션(0x1E0000 = 1.875MB) 대비 여유 ≥ 15%, free heap 기록 | `pio run` 출력, 부팅 로그 |
| 보안 | device_key 평문이 어떤 조회 응답에도 나오지 않음, OTA 무인증 업로드 401 | curl, UDP GET_CONFIG 확인 |
| 호환성 | hub.enabled=false에서 MQTT/RS485/스케줄/재부재/WebRequest 동작 v2.6.2와 동일 | 회귀 점검표 |
| 방어선 | NetManager(GOT_IP 감시, W5500 SW reset, ETH.begin 재시도) diff=0 | git diff |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01 ~ FR-15 구현
- [ ] `pio run` 성공(경고 신규 0), `build_firmware.bat`로 `RemoteDeck_PC_V3.0.0_*.bin` 생성
- [ ] conformance `--role deck` 기본 그룹(접속·인증, full 상태, 필수 키, ipOk, state.get, 누르지 않기 보호, 릴레이2, 규격 위반 없음) PASS
- [ ] conformance `--with power,led,reboot` PASS
- [ ] 수동 4항목 통과: 허브 재기동 재접속(≤10s), 랜선 재연결(GOT_IP 20s 재부팅과 충돌 없음), 잘못된 키 → 4001 정지 + 홈 표시, 동일 제품번호 2대 → 선행 장비 4009 정지
- [ ] hub.enabled=false 회귀 점검 통과
- [ ] SPIFFS OTA 시 deviceconfig.json(hub 블록 포함)·schedule.json 보존 확인
- [ ] Manual(`docs/RemoteDeck_PC_Manual.md`)에 hub 설정·`/api/status` 필드·운영 원칙 반영

### 4.2 Quality Criteria

- [ ] gap-analysis matchRate ≥ 90%
- [ ] 빌드 0 error
- [ ] 실기 시험 전 아답터·전원 안정성 크로스체크 기록

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| loop() 블로킹(MQTT connect, NTP begin, 동기 HTTP WebRequest)으로 heartbeat 끊김 | High | Medium | 허브 사용 시 MQTT 미시작, deck.begin을 NTP 뒤로. 재부재 WebRequest 동기 호출 시간도 Design에서 확인(타임아웃 상한) |
| 설정 키 4곳 중 누락 → UDP/HTTP 설정 저장 시 hub 값 소실 | High | Medium | Design에 4곳 체크리스트, 저장→재부팅→조회 왕복 시험 |
| 허브 미사용 현장 회귀 | High | Low | 모든 충돌 가드를 `hub.enabled` 조건으로 한정, 회귀 점검표 |
| 이더넷 GOT_IP 감시 재부팅(20s)과 허브 재접속 타이밍 충돌 | Medium | Medium | 랜선 재연결 수동 시험, NetManager 수정 금지 |
| W5500 DNS 불가로 hostname 접속 실패 | Medium | High | host는 IP 권장(UI 안내), Design에서 hostname 입력 처리 방침 결정 |
| 라이브러리 실기 미검증(컴파일만 확인) | Medium | Medium | conformance로 검증, 수정 시 rcm-api에 PR |
| Flash/RAM 증가(WebSockets + 라이브러리) | Low | Low | 현재 1.39MB / 파티션 1.875MB. 빌드 후 사용량 기록 |
| OTA 인증 추가로 기존 업로드 도구가 실패 | Low | Low | .NET 클라이언트에서 `/api/ota` 사용처 없음 확인함. 웹 UI는 같은 origin 인증 |
| 실기 시험 중 전원 불안정을 S/W 문제로 오인 | High | Low | Phase4 시작 전 아답터·전원 크로스체크(feedback_hardware_first) |

---

## 6. Impact Analysis

### 6.1 Changed Resources

| Resource | Type | Change Description |
|----------|------|--------------------|
| `deviceconfig.json` / `DeviceConfig` | Config | `hub` 블록 추가, `firmware.version` 3.0.0 |
| `/api/config` (GET/POST), UDP `GET_CONFIG`/`SET_CONFIG` | API | `hub` 블록 추가, `device_key` 가림 |
| `/api/status` | API | `hub_connected`, `hub_stopped`, `hub_stop_reason` 추가 |
| `onScheduleAction` | Logic | hub.enabled 시 릴레이1 전원 동작 skip |
| 공통 명령 처리(`cmd:relay`), `/api/relay` | Logic/API | hub.enabled 시 릴레이1 latch on 거부 |
| `MQTTHandler` 시작/loop | Logic | hub.enabled 시 미시작 |
| OTA 업로드 엔드포인트 | API | Basic 인증 추가 |
| `data/www` index/app.js | UI | 설정 카드, 홈 카드 |
| `platformio.ini`, `lib/RcmHub` | Build | 의존성·라이브러리 추가 |

### 6.2 Current Consumers

| Resource | Operation | Code Path | Impact |
|----------|-----------|-----------|--------|
| `/api/status` | READ | IntegrateController `RemoteDeckClient` | None(알 수 없는 필드 무시, tolerant 파싱) |
| `/api/status` | READ | 웹 UI 홈(`app.js`), RemoteDeckTest | None(필드 추가만) |
| `/api/config` | READ/UPDATE | 웹 UI 설정(`app.js` loadConfig/saveEtc 등) | Needs verification(hub 블록 왕복, `****` 처리) |
| UDP `GET_CONFIG`/`SET_CONFIG` | READ/UPDATE | IPSetupTool | Needs verification(hub 블록 없는 SET_CONFIG가 hub 값을 지우지 않아야 함) |
| `cmd:relay` latch | UPDATE | MQTT, RS485, `/api/relay`, RemoteDeckTest | hub 사용 시에만 릴레이1 거부(의도된 변경) |
| 스케줄 전원 동작 | UPDATE | ScheduleManager → onScheduleAction | hub 사용 시에만 릴레이1 skip(의도된 변경) |
| MQTT | — | MQTTHandler, RemoteDeck_Touch 연동 현장 | hub 사용 시 비활성(의도된 변경, UI 안내) |
| OTA 업로드 | CREATE | 웹 UI 관리 탭 `uploadOTA`/`uploadFS` | Needs verification(인증 자동 첨부) |
| `firmware.version` | READ | `/api/status` fw, MQTT/WS 이벤트 fw, OTA 파일명 버전 갱신 | None(값만 3.0.0) |

### 6.3 Verification

- [ ] 위 소비자 전체가 변경 후 동작
- [ ] OTA 인증 추가 후 웹 UI 펌웨어/SPIFFS 업로드 정상
- [ ] hub 블록이 없는 구버전 클라이언트 설정 저장이 hub 값을 지우지 않음

---

## 7. Architecture Considerations

### 7.1 Project Level Selection

| Level | Characteristics | Recommended For | Selected |
|-------|-----------------|-----------------|:--------:|
| **Starter** | Simple structure | Static sites | ☐ |
| **Dynamic** | Feature-based modules | Web apps with backend | ☑ |
| **Enterprise** | Strict layer separation | High-traffic systems | ☐ |

> 임베디드 펌웨어 + 내장 웹 UI. 기존 단일 스레드 Arduino 오케스트레이터 구조(main.cpp 배선)를 따른다.

### 7.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| 허브 클라이언트 | 직접 구현 / 참조 라이브러리 반입 | 참조 라이브러리 반입 | 규격 담당이 컴파일 확인, conformance 기준 구현. 다른 펌웨어와 코드 공유 |
| 통합 방식 | 새 핸들러 클래스 / main.cpp 직접 배선 | main.cpp에서 `RcmDeckIo` 배선(Design에서 래퍼 여부 결정) | 기존 패턴: 서브시스템 + main.cpp 콜백 배선 |
| 스케줄 충돌 | 비활성 / LED 가드 / 둘 다 | **hub 사용 시 비활성** | 사용자 결정. 스케줄은 서버 담당 |
| 릴레이1 latch | 항상 금지 / hub 사용 시만 | **hub 사용 시만 금지** | 사용자 결정. 기존 현장 동작 유지 |
| loop 블로킹 | MQTT 타임아웃 단축 / hub 사용 시 MQTT 끔 | **hub 사용 시 MQTT 끔** | 사용자 결정. 가장 안전 |
| 보안 범위 | 키 가림 / +OTA 인증 / 전부 | **키 가림 + /api/ota 인증** | 사용자 결정. UDP 인증은 IPSetupTool 변경 필요 → carry |
| UI 위치 | 별도 서브탭 / 재부재 카드 아래 | 설정 > 기타, 재부재 카드 아래 | 사용자 요청. 기존 `saveEtc()` 재사용 |

### 7.3 Folder Structure

```
RemoteDeck_PC/
├── lib/RcmHub/            # 신규: RcmHub.h/.cpp, RcmDeck.h/.cpp (rcm-api 원본)
├── src/config/            # DeviceConfig.h, ConfigManager.cpp: hub 블록
├── src/main.cpp           # RcmDeckIo 배선, begin/loop, 충돌 가드, /api/status
├── src/web/               # WebServer.cpp, OTAHandler.cpp: OTA 인증, relay 거부 응답
└── data/
    ├── deviceconfig.json  # hub 블록 + version 3.0.0
    └── www/               # index.html, app.js: 설정 카드 + 홈 카드
```

---

## 8. Convention Prerequisites

### 8.1 Existing Project Conventions

- [x] `CLAUDE.md` 프로젝트 규칙(버전 단일 기준, SPIFFS OTA 설정 보존, DNS 불가, reboot flush, 물리계층 우선)
- [ ] ESLint / Prettier / TS — 해당 없음(바닐라 JS, C++)

### 8.2 Conventions to Verify

| Category | Current State | To Define | Priority |
|----------|---------------|-----------|:--------:|
| 설정 키 반영 | 4곳 수동 동기화 | Design 체크리스트 | High |
| 버전 | 3곳 불일치 | `deviceconfig.json` 단일 기준, 코드는 읽기만 | High |
| 커밋 메시지 | `type(component): 요약` | `feat(RemoteDeck_PC): v3.0.0 — RCM 허브 연동 …` | Medium |
| 코드 주석 | Design Ref 주석 | `// Design Ref: §n`, `// Plan SC: …` | Low |

### 8.3 Environment / Field Values

| Item | Purpose | Source |
|------|---------|--------|
| `hub.host` / `port` | 허브 주소(시험: 개발 PC IP / 8899) | 서버 담당 / conformance |
| `hub.product_id` | 짝 녹화장비와 같은 제품번호(`tbl_lecture.l_productid`) | 서버 DB |
| `hub.device_key` | `tools/device-key.js --role deck` 결과 | 서버 담당 / conformance 출력 |

---

## 9. Next Steps

1. [ ] Design 문서 작성(`/pdca design rcm-hub-v3.0`): 3가지 아키텍처안, 설정 4곳 체크리스트, `/api/status` 스키마, UI 와이어, 시험 계획
2. [ ] 실기 준비: SmartDeck + PC(또는 LED 자리 스위치), 아답터 점검, 개발 PC에 rcm-api `npm install`
3. [ ] 구현 시작(`/pdca do rcm-hub-v3.0`)

### Carry Items (다음 버전 후보)

- IntegrateController 허브 상태 컬럼(v3.0.0 계약 소비)
- UDP 명령 인증 + IPSetupTool 대응
- 허브 미사용 시 스케줄 LED 가드 / 릴레이1 latch 전면 금지 재검토
- RemoteDeck_Touch 허브 연동

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-09 | Initial draft (Checkpoint 1/2 결정 반영: 스케줄 비활성, latch hub 시만 금지, MQTT 끔, 키 가림+OTA 인증) | KDI |
