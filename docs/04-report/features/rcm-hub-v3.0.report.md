---
template: report
version: 1.1
feature: rcm-hub-v3.0
date: 2026-11-10
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_PC (firmware + Web UI)
branch: v3.0-rcm-hub
matchRate: 98
status: completed
---

# RemoteDeck_PC v3.0.0 — RCM 허브 deck 연동 완료 보고

> **Status**: Complete (현장 수동 시험 일부는 허브 서버 완성 후로 이월)
>
> **Project**: RemoteDeckSystem
> **Version**: 3.0.0
> **Author**: KDI
> **Completion Date**: 2026-11-10
> **PDCA Cycle**: #1 (Act 1회)

---

## Executive Summary

### 1.1 Project Overview

| Item | Content |
|------|---------|
| Feature | rcm-hub-v3.0 — SmartDeck(RemoteDeck_PC)을 RCM 통합 API 허브의 `deck`으로 연동 |
| Start Date | 2026-11-09 |
| End Date | 2026-11-10 |
| Duration | 2일 |
| Commits | `d6b66c5` (M1~M4), `e25d44b` (Check/Act + 허브 카드 + OTA 응답), `9c31c28` (SOURCE.md) |
| Upstream | rcm-api PR #1 (`RcmHub::setTap()`) |

### 1.2 Results Summary

```
┌─────────────────────────────────────────────┐
│  Completion Rate: 93%                        │
├─────────────────────────────────────────────┤
│  ✅ Complete:     14 / 15 success criteria   │
│                   (FR 15/15)                 │
│  ⏳ Partial:       1 / 15 (수동 4항목 중 일부)│
│  ❌ Cancelled:     0                         │
└─────────────────────────────────────────────┘
```

### 1.3 Value Delivered

| Perspective | Content |
|-------------|---------|
| **Problem** | RCM 허브 통합 운영을 위해 SmartDeck이 허브에 `deck`으로 붙어야 했고, LED를 확인하지 않는 전원 조작(현장 오조작 원인)과 허브·로컬 기능(스케줄·릴레이1 latch·MQTT) 간 충돌이 있었음 |
| **Solution** | rcm-api 참조 라이브러리를 반입하고 `HubBridge`로 감싼 Option C 구조. 허브 사용 시에만 충돌 기능을 막는 가드(부팅 시 값으로 고정), device_key 가림, OTA 인증, 웹 UI 설정·홈 카드 |
| **Function/UX Effect** | 실기 2대에서 conformance 전 항목 PASS(power·led 포함). 홈 카드에서 연결 상태·세션·통계·송수신 내역 40건을 바로 확인. OTA는 응답 후 재부팅(펌웨어 6.6s, SPIFFS 0.8s에 HTTP 200) |
| **Core Value** | 허브 쪽에서 "PC LED를 확인한 뒤에만 누르는" 전원 제어가 가능해지고, 허브를 쓰지 않는 현장은 기존 동작 그대로(가드가 `hub.enabled`일 때만 작동) |

---

## 1.4 Success Criteria Final Status

| # | 기준 | Status | 근거 |
|---|------|:------:|------|
| SC-1 | FR-01~15 구현 | ✅ | analysis §2, 정적 분석 FR 15/15 Met |
| SC-2 | `pio run` 성공, Flash 여유 ≥15% | ✅ | Flash 76.8%(1,510,152 B, 여유 23.2%), RAM 18.9% |
| SC-3 | conformance `--role deck` 기본 그룹 PASS | ✅ | `.109`·`.193` 모두 basic·guard·relay PASS |
| SC-4 | conformance `--with power,led,reboot` PASS | ✅ | reboot(`.109`), power·led(`.193`, 11/11) |
| SC-5 | 수동 4항목(허브 재기동 ≤10s, 랜선, 4001, 4009) | ⚠️ | 재부팅 후 재접속 3~4s 관찰. 허브 단독 재기동 시간·랜선·4001·4009는 미실시 → 이월 |
| SC-6 | hub.enabled=false 회귀 | ✅ | T-01·T-09 PASS, 가드 4곳 모두 hub 조건 한정(정적 확인). 전체 회귀표는 이월 |
| SC-7 | SPIFFS OTA 설정 보존 | ✅ | T-12, 두 장비 OTA 반복 후 config 동일 |
| SC-8 | Manual 반영 | ✅ | §12 신설, `/api/hub`, OTA 응답 동작 |
| SC-9 | matchRate ≥ 90% | ✅ | 98% |
| SC-10 | 빌드 0 error | ✅ | |
| SC-11 | device_key 평문 미노출, OTA 무인증 401 | ✅ | T-02·T-03·T-11 |
| SC-12 | NetManager 방어선 diff=0 | ✅ | `git diff main -- src/network/Net*` 0줄 |
| SC-13 | 아답터·전원 크로스체크 | ✅ | `.109` 일시 소실은 USB 전원 분리로 확인, 펌웨어 로그 이상 없음 |
| SC-14 | 버전 3.0.0 통일 | ✅ | 배너·`fw_ver`·deck.state `fw` |
| SC-15 | 설정 4곳 왕복 보존 | ✅ | T-05·T-06·T-12 |

**Success Rate**: 14/15 met (93%), 1 partial

## 1.5 Decision Record Summary

| Source | Decision | Followed? | Outcome |
|--------|----------|:---------:|---------|
| [Plan] | 하네스 대신 경량 PDCA | ✅ | 2일 만에 Plan→Report |
| [Plan] | 허브 사용 시 스케줄 릴레이1 건너뜀 | ✅ | `onScheduleAction` 가드 |
| [Plan] | 릴레이1 latch는 허브 사용 시에만 금지 | ✅ | 409 + GUARD 로그, 허브 off면 기존 동작(T-09) |
| [Plan] | 허브 사용 시 MQTT 끔 | ✅ | heartbeat 블로킹 위험 제거 |
| [Plan] | 키 가림 + OTA 인증, UDP 인증은 이월 | ✅ | T-02/03/11 PASS |
| [Design] | Option C: HubBridge + main.cpp 가드 헬퍼 | ✅ | 전역 참조 없는 Deps 주입, 라이브러리는 원본 유지 |
| [Design] | 설정 변경은 재부팅 후 반영 | ✅ (Act에서 보강) | G-02: 가드를 부팅 시 `hubActive`로 고정해 허브 상태와 일치 |
| [Do 중 결정] | 허브 서버 미완성 → rcm-api 참조 허브로 가상 시험 | ✅ | `RDTEST01/02` 방, conformance까지 수행 |
| [Act 중 결정] | 송수신 내역을 위해 라이브러리에 관찰 훅 추가 | ✅ | 사용자 선택. rcm-api PR #1로 업스트림 |

---

## 2. Related Documents

| Phase | Document | Status |
|-------|----------|--------|
| Plan | [rcm-hub-v3.0.plan.md](../../01-plan/features/rcm-hub-v3.0.plan.md) | ✅ Finalized |
| Design | [rcm-hub-v3.0.design.md](../../02-design/features/rcm-hub-v3.0.design.md) | ✅ Finalized |
| Check | [rcm-hub-v3.0.analysis.md](../../03-analysis/rcm-hub-v3.0.analysis.md) | ✅ Complete (98%) |
| Act | 현재 문서 | ✅ |

---

## 3. Completed Items

### 3.1 Functional Requirements

| ID | Requirement | Status | Notes |
|----|-------------|--------|-------|
| FR-01 | lib/RcmHub 반입 + WebSockets | ✅ | 이후 `setTap()` 로컬 추가(업스트림 PR) |
| FR-02 | hub 설정 키 4곳 | ✅ | |
| FR-03 | NTP 뒤 begin 1회, loop 비블로킹 | ✅ | |
| FR-04 | LED 확인 후에만 누름 | ✅ | conformance guard·power |
| FR-05 | LED 변화 즉시 보고, welcome 후 full | ✅ | conformance led |
| FR-06 | 4001/4009/4010 정지 + 사유 표시 | ✅ | 실기 정지 시험은 이월 |
| FR-07 | /api/status hub_* | ✅ | |
| FR-08 | 허브 시 스케줄 릴레이1 건너뜀 | ✅ | |
| FR-09 | 허브 시 latch 거부, pulse 허용 | ✅ | |
| FR-10 | 허브 시 MQTT 끔 | ✅ | |
| FR-11 | device_key 가림 | ✅ | |
| FR-12 | OTA Basic 인증 | ✅ | |
| FR-13 | 버전 3.0.0 통일 | ✅ | |
| FR-14 | 설정 > 기타 RCM-API 카드 | ✅ | |
| FR-15 | 홈 RCM 허브 카드 | ✅ | Act에서 확장: 정보 칸 8개 + 송수신 내역 |

### 3.2 Non-Functional Requirements

| Item | Target | Achieved | Status |
|------|--------|----------|--------|
| Flash 여유 | ≥ 15% | 23.2% | ✅ |
| 재접속 | ≤ 10s | 재부팅 후 3~4s (허브 단독 재기동은 미측정) | ⚠️ |
| 보안 | 키 평문 0, OTA 무인증 401 | 0건 / 401 | ✅ |
| 호환성 | hub off 시 v2.6.2 동작 | T-01·T-09 PASS | ✅ |
| 방어선 | NetManager diff 0 | 0 | ✅ |

### 3.3 Deliverables

| Deliverable | Location | Status |
|-------------|----------|--------|
| 허브 라이브러리(반입) | `RemoteDeck_PC/lib/RcmHub/` + `SOURCE.md` | ✅ |
| 허브 래퍼 | `src/network/HubBridge.h/.cpp` | ✅ |
| 설정·가드·배선 | `src/config/*`, `src/main.cpp` | ✅ |
| API | `/api/status` hub_*, `GET /api/hub`, `/api/relay` 409, `/api/ota` 인증·응답 | ✅ |
| Web UI | `data/www/index.html`, `app.js`, `style.css` | ✅ |
| 문서 | `docs/RemoteDeck_PC_Manual.md` §8.2·§12, Plan/Design/Analysis/Report | ✅ |
| 업스트림 | rcm-api PR #1 | 🔄 리뷰 대기 |

---

## 4. Incomplete Items

### 4.1 Carried Over to Next Cycle

| Item | Reason | Priority |
|------|--------|----------|
| 수동 시험: 허브 단독 재기동 재접속 시간, 랜선 재연결, 4001, 4009 | 사용자 결정으로 Check 종료, 허브 서버 완성 후 현장 dogfood | High |
| T-10 RS485 latch 가드 | RS485 어댑터 없음 | Medium |
| hub off 전체 회귀표(MQTT/RS485/재부재/WebRequest) | 시간 | Medium |
| 훅 추가 후 conformance 재실행 | 장비 포트 변경 필요 | Medium |
| IntegrateController hub 컬럼 | Plan carry | Medium |
| UDP 인증 + IPSetupTool | Plan carry (보안 결정에서 이월) | Medium |
| RemoteDeck_Touch 허브 연동 | Plan carry | Low |
| 정식 허브 전환 시 장비 host/키 재설정 | 현재 가상 허브(192.168.10.84) 설정 | High (전환 시) |

### 4.2 Cancelled/On Hold Items

| Item | Reason | Alternative |
|------|--------|-------------|
| - | - | - |

---

## 5. Quality Metrics

### 5.1 Final Analysis Results

| Metric | Target | Final |
|--------|--------|-------|
| Design Match Rate | 90% | 98% (Check 96% → Act 후) |
| Structural / Functional / Contract | - | 97 / 98 / 97 |
| L1 API | 12항목 | 11 PASS, 1 보류 |
| Conformance | 전 그룹 | PASS (power·led는 PC LED 연결 장비 `.193`) |
| Security Issues | 0 Critical | 0 |

### 5.2 Resolved Issues

| Issue | Resolution | Result |
|-------|------------|--------|
| OTA 응답 미전송(v2.5.1부터) | 복원·재부팅을 `OTAHandler::loop()`로 이관 | ✅ HTTP 200 후 재부팅 |
| G-01 릴레이1 켜기 409가 UI에서 무반응 | `relay()`가 409 안내 | ✅ |
| G-02 저장만 하면 가드와 허브 상태 불일치 | 부팅 시 `hubActive` 고정 | ✅ 실기 확인 |
| G-03 OTA UI가 실패도 "완료" 표시 | `sendOTA()`가 서버 응답으로 판정 | ✅ 실기 확인 |
| "config incomplete" 로그 반복 | 첫 회만 기록 | ✅ |
| 한글 상세가 바이트 단위로 잘려 깨짐 | UTF-8 경계 자르기 | ✅ |

---

## 6. Lessons Learned & Retrospective

### 6.1 What Went Well (Keep)

- rcm-api 참조 허브와 conformance 도구 덕분에 허브 서버 완성 전에도 프로토콜을 실기로 끝까지 검증할 수 있었음.
- 실기를 2대로 나눠 시험(`.109` API·reboot, `.193` PC LED 연결 power·led) — PC 미연결 장비의 power/led FAIL을 펌웨어 문제로 오인하지 않음.
- SPIFFS OTA 시험을 "허브 꺼진 이미지 위로" 수행해, 설정 복원이 실제로 일어났는지 구분 가능하게 함.

### 6.2 What Needs Improvement (Problem)

- OTA 응답 미전송은 v2.5.1부터 있었지만 웹 UI 우회(v2.5.2) 때문에 가려져 있었음. curl 시험에서야 드러남.
- USB 업로드용 SPIFFS(`data/`)에는 기본 설정(node_1, 고정 IP)이 들어 있어, 필드 장비에 그대로 올리면 네트워크를 잃음 — 이번엔 임시 복사본으로 피함.
- 설계에서 UI 오류 처리(409·OTA 실패)를 정의하지 않아 Check에서 갭으로 나옴.

### 6.3 What to Try Next (Try)

- 클라이언트 동작 시험에 curl(우회 없는 경로)을 기본으로 포함.
- `uploadfs` 전에 장비 설정을 백업·병합하는 스크립트(장비 id·네트워크 유지)를 도구로 만들기.
- Design §4 API 계약에 "UI가 각 오류 코드를 어떻게 보여 주는지" 열 추가.

---

## 7. Process Improvement Suggestions

| Phase | Current | Improvement Suggestion |
|-------|---------|------------------------|
| Design | API 상태 코드만 정의 | 클라이언트 표시까지 계약에 포함 |
| Do | 실기 시험 전 커밋 | 가상 허브 시험을 Do 체크리스트에 포함 |
| Check | 수동 4항목이 허브 서버 의존 | 참조 허브로 4001(잘못된 키)·4009(같은 제품번호 2대)를 재현하는 스크립트화 |

---

## 8. Next Steps

### 8.1 Immediate

- [ ] rcm-api PR #1 리뷰·병합 후 `SOURCE.md` 기준 태그 갱신
- [ ] `v3.0-rcm-hub` 브랜치 push·PR → main 병합
- [ ] `build_firmware.bat`로 배포용 `RemoteDeck_PC_V3.0.0_*.bin` 생성

### 8.2 Next PDCA Cycle

| Item | Priority |
|------|----------|
| 정식 허브 연결 + 수동 4항목 현장 시험 | High |
| IntegrateController hub 컬럼 (계약 버전 v3.0.0) | Medium |
| UDP 인증 + IPSetupTool | Medium |

---

## 9. Changelog

### v3.0.0 (2026-11-10)

**Added:**
- RCM 통합 API 허브 `deck` 연동 (`lib/RcmHub`, `HubBridge`)
- 설정 > 기타 「RCM-API 연동」 카드, 홈 「RCM 허브」 카드(상태·통계·송수신 내역 40건)
- `GET /api/hub`, `/api/status` hub_* 6필드

**Changed:**
- 허브 사용 시: 스케줄 릴레이1 건너뜀, 릴레이1 latch 409, MQTT 끔 (부팅 시 값 기준)
- `/api/config`·UDP `GET_CONFIG`에서 `device_key` 가림
- `/api/ota` Basic 인증 필수, 응답 후 재부팅
- 버전 표시를 `config.firmware.version`으로 통일

**Fixed:**
- OTA 성공 응답이 클라이언트에 전달되지 않던 문제 (v2.5.1~)
- 웹 UI OTA가 실패도 "완료"로 표시하던 문제

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2026-11-10 | Completion report created | KDI |
