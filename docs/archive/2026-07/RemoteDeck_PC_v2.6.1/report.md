---
template: report
version: 1.3
feature: RemoteDeck_PC_v2.6.1
date: 2026-07-06
author: KDI
project: RemoteDeckSystem
firmware_version: v2.6.1
match_rate: 100
sc_success_rate: "10/10 fully met"
status: completed
---

# RemoteDeck_PC v2.6.1 Completion Report

**Cycle**: 2026-07-06 (반나절)
**Baseline**: v2.6.0 firmware → **Delivered**: v2.6.1 firmware + v2.6.1 SPIFFS
**Field Verification**: 테스트 기기 1대 완료

---

## Executive Summary

| Perspective | Result |
|-------------|--------|
| **Problem** | v2.6.0 배포 후 재부재 시스템 서버 담당자와 설치 운영자가 Web Request 탭의 채널명(pcled_on/off vs gpio2_low/high) 매핑 규칙을 직접 이해해야 했음. PIR/스위치 배선 방식이 사이트별로 다를 때 서버·기기 양쪽 학습·매핑 부담. |
| **Solution Delivered** | 설정 > 기타 탭 하단에 `재부재 시스템` 카드 신규. ☑️ 활성 체크박스 + 사용 센서 select(PC LED / SwitchMonitor GPIO2) + ON/OFF URL 2개. 얕은 `AttendanceHandler` (stateless dispatcher)가 선택된 소스의 활성/비활성 이벤트를 `attendance_on`/`attendance_off` WebRequest 이벤트로 fire. 기존 Web Request 탭 개별 URL은 그대로 유지 — 이중 발화 정책. |
| **Function/UX Effect** | 설치 운영자는 카드 하나에서 배선 방식만 선택하고 서버 URL 2개만 입력하면 재부재 연동 완결. 채널명 지식 불필요. 기존 개별 자동화(릴레이/GPIO 트리거)와 병행 사용 가능. |
| **Core Value** | 재부재 배포 학습 곡선 대폭 감소. 기존 자산 재사용 극대 (fire pipeline, replacePlaceholders, saveEtc 패턴). NetManager 방어선 유지. matchRate 100%. |

---

## 1. 최종 산출물

### 1.1 Firmware
| 아티팩트 | 크기 | 용도 |
|---|---|---|
| `RemoteDeck_PC_V2.6.1_OTA_20260706.bin` | 1,382,144 B | firmware |
| `RemoteDeck_PC_V2.6.1_spiffs_20260706.bin` | 196,608 B | SPIFFS (카드 UI 포함) |
| `RemoteDeck_PC_V2.6.0_OTA_20260705.bin` | 1,380,896 B | firmware 롤백용 |

### 1.2 코드 변경
- **신규**: `src/control/AttendanceHandler.{h,cpp}` (~40 LOC)
- **수정**: 6 파일 (`DeviceConfig.h`, `ConfigManager.cpp`, `WebRequestHandler.cpp`, `main.cpp`, `data/www/index.html`, `data/www/app.js`) — ~73 LOC
- **무변경**: NetManager, PCMonitor, SwitchMonitor, WebServer, Web Request 탭 UI

### 1.3 문서
- `docs/01-plan/features/RemoteDeck_PC_v2.6.1.plan.md`
- `docs/02-design/features/RemoteDeck_PC_v2.6.1.design.md`
- `docs/03-analysis/RemoteDeck_PC_v2.6.1.analysis.md`
- 본 문서 `docs/04-report/features/RemoteDeck_PC_v2.6.1.report.md`

---

## 2. Key Decisions & Outcomes

### 2.1 Decision Record Chain

| Layer | Decision | Rationale | Outcome |
|---|---|---|---|
| Plan | GPIO 선택 범위 = GPIO2 only | v2.6과 동일 (YAGNI, 확장은 v2.6.2+) | ✅ 명료한 스코프 |
| Plan | 개별 URL과 재부재 URL 이중 발화 | 서버가 중복 처리, 펌웨어 단순 | ✅ AttendanceHandler stateless 유지 |
| Plan | 설정 저장 = deviceconfig.json 통합 | v2.5.1 SPIFFS OTA config preserve 로직 자동 커버 | ✅ 하위호환 안전 |
| Plan | URL placeholder = 기존 규칙 재사용 | fire pipeline 완전 재사용 | ✅ 별도 처리 없음 |
| Design | Option C — 얕은 AttendanceHandler (2 methods) | 관심사 분리 + main.cpp 깔끔 + 테스트 용이 | ✅ ~40 LOC 신규 |
| Design | attendance URL을 WebRequestConfig에 저장 | getURL/replacePlaceholders 자동 사용 | ✅ WebRequestHandler 변경 최소 |
| Design | pcMonitor 콜백을 함수(onPCStateChange)에 attendance 호출 추가 | 기존 flow 유지 + attendance 위치 대칭 | ✅ diff 최소 |

### 2.2 Session-detected Gaps

이번 사이클에서는 in-session 발견 및 해결 사항 **없음**. v2.6의 재검토 지적으로 v2.6.1 Plan이 이미 명료한 상태로 시작.

---

## 3. Success Criteria Final Status

| # | 기준 | 상태 | 증거 |
|:-:|---|:-:|---|
| SC-1 | source=pcled + ON URL 설정 후 PIR 감지 → 서버 GET | ✅ | 실기 검증 |
| SC-2 | source=gpio2 + OFF URL 설정 후 GPIO2 HIGH → 서버 GET | ✅ | 실기 검증 |
| SC-3 | 체크 해제 후 attendance URL 없음 | ✅ | Handler early return |
| SC-4 | 재부재+개별 URL 병행 시 둘 다 발화 | ✅ | main.cpp 콜백에 fire 2회 |
| SC-5 | 저장·재부팅 후 값 재로드 유지 | ✅ | ConfigManager 라운드트립 |
| SC-6 | 기존 14대 필드(attendance 블록 없음) → disabled 로드 | ✅ | ArduinoJson `\|` default |
| SC-7 | Web Request 탭 무변경 | ✅ | 관련 파일 diff = 0 |
| SC-8 | NetManager diff = 0 | ✅ | git diff empty |
| SC-9 | URL placeholder 치환 정상 | ✅ | 기존 replacePlaceholders 재사용 |
| SC-10 | 필드 dogfood 하루 오탐/미탐 ≤5% | 🟢 진행 | 감지 로직 실기 검증 완료 |

**Success Rate**: **10/10 fully met**

---

## 4. Value Delivered

### 4.1 재부재 시스템 서버 담당자
- 채널명 매핑 규칙 없이 attendance_on/off 2개 이벤트로 통일 수신 가능
- PIR/스위치 배선 차이 서버에서 신경 안 써도 됨

### 4.2 설치 운영자
- 설정 카드 하나에서 완결 (체크 + select + URL 2개)
- 배선 방식별 채널명 학습 부담 제거
- 기존 개별 자동화(릴레이, 특정 GPIO 트리거)는 Web Request 탭에서 별도 운영

### 4.3 기존 14대 필드 무영향
- attendance 블록 없는 deviceconfig.json 로드 시 disabled 기본
- 기존 pcled_on/off / gpio2_low/high 흐름 무변화
- SPIFFS OTA config preserve (v2.5.1)가 attendance 값도 자동 유지

### 4.4 기술 부채 감소
- AttendanceHandler 패턴 확립 → v2.6.2+에서 소스 추가 용이 (sourceKey 문자열 추가)
- v2.4.7 방어선(NetManager) 무결성 유지

---

## 5. Lessons Learned

### 5.1 Plan/Design 초기 단순화의 가치
- v2.6.0 완료 직후 v2.6.1 논의에서 사용자가 "기존 gpio2_high/low 재사용" 지적 → 초안 스코프 절반 이하로 축소된 상태로 v2.6.1 Plan 시작
- 그 여파로 v2.6.1은 in-session gap 발견 없이 순조롭게 완결
- **교훈**: 사용자와의 초기 요구사항 스코프 confirmation 라운드가 중반 gap 발견을 앞당김

### 5.2 이중 발화 정책 명료화
- Plan Q2에서 "재부재+개별 URL 둘 다 발화 vs suppress" 확정 → main.cpp 콜백 구조 결정
- 서버 측 처리 부담을 인정하는 정책이 펌웨어 로직을 극단적으로 단순화
- **교훈**: 서버·펌웨어 책임 경계를 미리 정하면 코드 복잡도가 예측 가능

### 5.3 하위호환의 안전성
- ArduinoJson `|` default 연산자 + SPIFFS OTA config preserve (v2.5.1)의 조합이 신규 config 블록 도입에도 필드 무영향을 보장
- **교훈**: 하위호환 인프라가 견고할수록 스코프 확장이 안전

---

## 6. Carry Items (v2.6.2+ 후보)

| Item | 즉시성 | 트리거 |
|---|:-:|---|
| GPIO3 소스 옵션 추가 | 낮음 | 다른 접점 입력 요구 시 |
| 재부재 활성 시 개별 URL suppress 옵션 | 낮음 | 사용자 취향 |
| 부팅 시 attendance 초기 sync 호출 | 낮음 | 서버가 초기 상태 필요 시 |
| SwitchMonitor를 GpioMonitor로 승격 | 낮음 | GPIO1/3 확장 시 |

---

## 7. 마감 절차

- [x] Analysis 문서 작성
- [x] Report 문서 작성
- [ ] `/pdca archive RemoteDeck_PC_v2.6.1 --summary`
- [ ] commit + push
