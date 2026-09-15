---
template: analysis
version: 0.1
feature: RemoteDeck_Touch_v2.7_Webinterface
date: 2026-09-15
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware web UI)
branch: v2.6-touch-webmode
plan: docs/01-plan/features/RemoteDeck_Touch_v2.7_Webinterface.plan.md
design: docs/02-design/features/RemoteDeck_Touch_v2.7_Webinterface.design.md
phase: check
matchRate: 97
---

# RemoteDeck_Touch v2.7 Webinterface — Gap Analysis (Check)

> **Summary**: v2.6 웹 UI를 RemoteDeck_PC 스타일 5탭 + 항목별 폼으로 리뉴얼. Design(Option C) 대비 구현·실기 검증 결과 **matchRate 97%** — 전 기능(FR-01~13) 구현·실기 완료, 경미한 검증 잔여 2건(웹모드 부하 PoC 재실행·pre-v2.7 실기기 마이그레이션).

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | Touch 웹 UI가 PC와 달라(설정=JSON 통짜) 사용성·일관성 낮음 |
| **WHO** | 관리자(웹 설정 모드에서 브라우저로 설정·OTA·기기관리) |
| **RISK** | ①재부팅 스케줄=신규 기능 ②하위호환 ③embed_www.py 워크플로 ④flash |
| **SUCCESS** | 5탭·항목별 편집·저장 + 기존 기기 OTA 후 설정보존 + 재부팅 스케줄 (전부 실기) |
| **SCOPE** | S1 골격 → S2 설정폼+serverconfig → S3 관리 → S4 재부팅 스케줄 → S5 이미지+embed+검증 |

---

## 1. 검증 방식

CLAUDE.md 규약대로 임베디드는 **자동 테스트 하네스 없음 → (a) `pio run` 클린 빌드 + (b) 실기기 필드 검증**을 done 기준으로 한다. 본 사이클은 COM3 실기기로 각 단계(S1~S5) 및 추가 정리작업을 사용자가 직접 확인함.

---

## 2. Functional Requirements 대비

| ID | Requirement | 구현 | 실기 | 근거 |
|----|-------------|:--:|:--:|------|
| FR-01 | 5탭 네비(상태/제어/설정/관리/로그), 한글, PC 스타일 | ✅ | ✅ | `data/www/index.html` nav.tabs, 스크린샷 |
| FR-02 | 상태 탭 `/api/status` 표시 | ✅ | ✅ | `ImageApi::buildStatusJson`(device_id/mqtt/time 추가), renderStatusTab |
| FR-03 | 설정>Device Config 항목별 폼 | ✅ | ✅ | index.html #sub-device, loadDeviceConfig/saveDeviceConfig |
| FR-04 | 설정>Server Config 폼 + `GET/POST /api/serverconfig` 신규 | ✅ | ✅ | WebServer.cpp:169-170 route, #sub-server |
| FR-05 | 저장 = 필드→객체 조립 후 통짜 POST (계약 유지) | ✅ | ✅ | postConfig(load-merge-post), `/api/config` 불변 |
| FR-06 | Image Config 제외 + 이미지 관리는 설정 하위 | ✅ | ✅ | imagesconfig UI 미노출, #sub-image |
| FR-07 | 관리>기기관리(재부팅) | ✅ | ✅ | btnReboot → /api/reboot |
| FR-08 | 관리>재부팅 스케줄(신규) 저장+실행 | ✅ | ✅ | /api/schedule + main.cpp rebootSchedule (S4a "설정시간에 재부팅됨") |
| FR-09 | 관리>펌웨어 OTA (Config→관리 이동) | ✅ | ✅ | otaUpload; 진행률·연결끊김=성공 처리 수정 확인 |
| FR-10 | 이미지 관리 설정 하위 + 표시/교체/삭제 | ✅ | ✅ | 썸네일 표시(쿼리스트링 파싱 수정), 교체/삭제 실기 |
| FR-11 | 제어/로그 탭 PC 스타일 정리 | ✅ | ✅ | 제어 정상작동 확인, 로그 탭 |
| FR-12 | 하위호환 — 기존 기기 OTA 후 구 config 정상 | ✅ | ◐ | JsonUtils default 가드 + reboot_time 마이그레이션. 벤치 기기 반복 OTA로 config persist 확인. pre-v2.7 실기기 마이그레이션은 미검증(잔여) |
| FR-13 | 야간 화면 끄기(신규) | ✅ | ✅ | nightOff + lvgl_touch night_active (S4b "야간끄기 잘 작동됨") |

**FR 충족: 13/13 구현, 12.5/13 실기(FR-12 부분)**

---

## 3. Success Criteria (Plan §4)

| 기준 | 상태 | 비고 |
|------|:--:|------|
| 5탭 PC 스타일 렌더, 항목별 편집·저장 실기 | ✅ Met | 전 탭 실기 확인 |
| 기존 기기 config OTA 후 보존·정상 (FR-12) | ◐ Partial | 로직·가드 완비, config persist 실기 확인. pre-v2.7 실기기 검증만 잔여 |
| 재부팅 스케줄 신규 동작 실기 (FR-08) | ✅ Met | S4a |
| OTA·이미지 관리 정상 + 웹모드 부하 PoC 재통과 | ◐ Partial | OTA·이미지 ✅. v24_poc.py 부하 PoC 이번 사이클 미재실행(웹모드 아키텍처 v2.6 불변) |
| embed_www.py 워크플로 확립·문서화 | ✅ Met | 전 단계 사용, CLAUDE.md 문서화 |
| **품질**: `pio run` 빌드 성공(경고 증가 없음) | ✅ Met | 반복 빌드 SUCCESS |
| **품질**: 웹모드 SPI 배타 유지 | ✅ Met | 웹모드 이미지 서빙·저장 정상, LVGL 미접근 가드 |

---

## 4. API Contract (Design §4 ↔ 서버 ↔ 클라)

| Endpoint | Design | 서버 라우트 | 클라 호출 | 결과 |
|----------|:--:|:--:|:--:|:--:|
| GET/POST /api/serverconfig | 신규 | ✅ WebServer.cpp:169-170 | ✅ load/saveServerConfig | 일치 |
| GET/POST /api/schedule | 신규 | ✅ WebServer.cpp:171-172 | ✅ load/saveSchedule | 일치 |
| GET/POST /api/config | 계약불변 | ✅ | ✅ | 회귀 없음 |
| /api/status, /api/images/*, /api/control, /api/log, /api/reboot, /api/ota | 재활용 | ✅ | ✅ | 일치 |

3-way 계약 일치 100%.

---

## 5. 계획 외 추가 구현 (품질 강화 delta)

실기 검증 중 발견·해결한 항목 (FR-12/품질 강화):

1. **썸네일 표시 버그 수정** — `/api/images/<name>?t=` 캐시버스터 쿼리를 서버(`parseImageNameFromUri`)에서 제거. 기존엔 `sanitizeImageName` 확장자 검사 실패 → 400 → 썸네일 깨짐.
2. **이미지 카드 실제 크기(100%) 표시** — `.thumb` 고정높이 제거.
3. **Sleep(스크린세이버) 단일 소스 통합** — LCD 장치설정이 `serverConfig.sleepTime`(웹 미반영) 대신 `deviceConfig.sleepTime` 읽도록. 웹↔LCD 불일치 해소.
4. **재부팅 완전 일원화** — Plan은 rebootTime "공존"이었으나, LCD 죽은 위젯(ui_dropReboot) + 레거시 일일 재부팅 로직 제거 → rebootSchedule 단일화. 레거시 `reboot_time>0` config는 1회 자동 마이그레이션(하위호환 강화, 매일 그 시각 유지).
5. **간이 NTP(MQTT tick) 유효성 가드** — 누락/이상 tick이 `currentTime`을 1970으로 오염시키던 잠재 버그 차단. 재부팅 스케줄/야간끄기 시각 정확도 강화. `currentTime` 소프트클록도 무조건화(매 루프 보간).

> RemoteDeck_PC의 NTP(SNTP `configTzTime`)와 비교 검토 완료: 폐쇄망 특성상 Touch의 서버-tick 방식이 타당(공용 NTP 라우팅 불가). 스케줄 로직은 PC `ScheduleManager`와 동일 패턴.

---

## 6. Match Rate

정적(자동 서버 테스트 없음) 기준 가중:
`Overall = Structural×0.2 + Functional×0.4 + Contract×0.4`

| 축 | 점수 | 근거 |
|----|:--:|------|
| Structural (파일/라우트/컴포넌트 존재) | 100% | 전 파일·라우트·UI 컴포넌트 존재 |
| Functional (로직 완결·placeholder 없음·실기) | 95% | FR 13/13 구현, FR-12 실기 부분(잔여 2건) |
| Contract (Design API ↔ 서버 ↔ 클라) | 100% | serverconfig/schedule/config 3-way 일치 |

**Overall matchRate = 20 + 38 + 40 = 97%** (≥ 90% 게이트 통과)

---

## 7. Carry Items (잔여)

| # | 항목 | 심각도 | 처리 |
|---|------|:--:|------|
| C-1 | pre-v2.7(구 config) **실기기** OTA 마이그레이션 검증 (reboot_time→rebootSchedule) | Low | 필드 배포 시 1대 확인 권장. 로직·벤치 검증 완료 |
| C-2 | 웹모드 부하 PoC(`v24_poc.py`) 재실행 | Low | v2.6에서 통과·아키텍처 불변. 배포 전 스모크 권장 |

두 항목 모두 **구현 결함이 아닌 검증 완결성** 사안 → 게이트 통과, Report 진행.

---

## 8. 결론

- matchRate **97% ≥ 90%** → **Check 통과**, Report 단계로 진행.
- 전 기능 실기 검증 완료(FR-12 실기기 마이그레이션·부하 PoC만 배포 전 스모크로 잔여).
- 계획 대비 재부팅 일원화·NTP 하드닝·설정 일관성 등 품질 delta 추가 확보.
