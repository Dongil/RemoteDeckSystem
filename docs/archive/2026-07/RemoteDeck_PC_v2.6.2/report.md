---
template: report
version: 1.3
feature: RemoteDeck_PC_v2.6.2
date: 2026-07-06
author: KDI
project: RemoteDeckSystem
firmware_version: v2.6.2
match_rate: 100
sc_success_rate: "12/12 fully met"
status: completed
---

# RemoteDeck_PC v2.6.2 Completion Report

**Cycle**: 2026-07-06 (반나절, 2 fix 라운드 포함)
**Baseline**: v2.6.1 firmware + v2.6.1 SPIFFS → **Delivered**: v2.6.2 firmware + v2.6.2 SPIFFS
**Field Verification**: 테스트 기기 실기 검증 완료 (사용자 fix 7건 in-session 반영 후 "검증 완료했어")

---

## Executive Summary

| Perspective | Result |
|-------------|--------|
| **Problem** | v2.6.1 설정 UX는 완결됐지만 홈에서 재부재 실시간 모니터링 불가, GPIO2 상태 새로고침 필요, 부팅 시 attendance 초기 상태 미전송, 외부 API로 재부재 상태 조회 수단 없음. |
| **Solution Delivered** | 홈 최상단 조건부 `재부재 시스템` 카드(센서 뱃지·현재 상태·이력) 신규. AttendanceHandler에 링버퍼[8] + toJson + syncOnBoot + StateGetters + Logger 브릿지 + onFireResult 확장. WebRequestHandler에 이벤트별 result 콜백. WebServer에 `/api/attendance/history` 신규. /api/status에 attendance 미니 블록. `상태 모니터` 카드 개편 (이름·순서·PC LED dot). switchMonitor.onChange에서 broadcastStatus. 색상 규칙 통일 (재실=초록/부재=빨강/성공=초록/실패=빨강). |
| **Function/UX Effect** | 감시자는 홈 접속 즉시 재부재 상태·이력을 확인 (시분초 시각, O/X 성공·실패 아이콘, 최근 5건). GPIO2 실시간 반영. 외부 자동화 시스템은 `/api/status` attendance 필드 또는 `/api/attendance/history` 로 상태 조회. 부팅 직후 서버 정합성 확보. |
| **Core Value** | 재부재 UX 완결(설정→모니터링) + 통합 표면 정비. NetManager 방어선 유지. matchRate 100%. |

---

## 1. 최종 산출물

### 1.1 Firmware
| 아티팩트 | 크기 | 용도 |
|---|---|---|
| `RemoteDeck_PC_V2.6.2_OTA_20260706.bin` | 1,387,728 B | firmware |
| `RemoteDeck_PC_V2.6.2_spiffs_20260706.bin` | 196,608 B | SPIFFS (홈 UI 카드 포함) |
| `RemoteDeck_PC_V2.6.1_OTA_20260706.bin` | 1,382,144 B | firmware 롤백용 |

### 1.2 코드 변경
- **확장**: `AttendanceHandler.{h,cpp}` (v2.6.1 dispatcher 위에 링버퍼·toJson·syncOnBoot·Logger 브릿지·onFireResult 얹기)
- **확장**: `WebRequestHandler.{h,cpp}` (event 추적 + result callback)
- **확장**: `WebServer.{h,cpp}` (/api/attendance/history 라우트)
- **수정**: `main.cpp` (setStateGetters, setTimeGetter, setLoggerBridge, setResultCallback, buildStatusJson attendance, switchMonitor broadcast, syncOnBoot 부팅)
- **홈 UI**: `data/www/{index.html, app.js, style.css}` (재부재 카드 + 상태 모니터 개편 + 색상 통일 + O/X + 시분초)
- **버전 스탬프**: `ConfigManager.cpp` 2.6.1 → 2.6.2
- **무변경**: NetManager, PCMonitor, SwitchMonitor

### 1.3 문서
- `docs/01-plan/features/RemoteDeck_PC_v2.6.2.plan.md`
- `docs/02-design/features/RemoteDeck_PC_v2.6.2.design.md` (Option C)
- `docs/03-analysis/RemoteDeck_PC_v2.6.2.analysis.md`
- 본 문서

---

## 2. Key Decisions & Outcomes

### 2.1 Decision Record Chain

| Layer | Decision | Rationale | Outcome |
|---|---|---|---|
| Plan | 이력 저장 = 링버퍼 8건 + /api/attendance/history | 서버측 데이터 소유, Logger 무관 | ✅ 깔끔한 filter, 5s 폴링 |
| Plan | 외부 API = /api/status 필드 추가 | 기존 파서 자연 재사용 | ✅ IntegrateController 회귀 없음 |
| Plan | GPIO 실시간 = WebSocket broadcast 강화 | 폴링보다 즉응성 · 서버 부하 최소 | ✅ switchMonitor.onChange 1줄 |
| Design | Option C — AttendanceHandler 확장 | 클래스 신설보다 결합 자연 | ✅ ~90 LOC 확장 (v2.6.1 위에) |
| Design | Entry에 timeStr 미저장 (millis만) | heap fragmentation 우려 | ✅ Round 1 fix에서 char[16] 고정 크기로 저장으로 재조정 |
| Fix Round 1 | 시분초 표시 필요 | 상대 시간이 사용성 낮음 | ✅ Entry.timeStr[16] + NTP getter |
| Fix Round 1 | 실제 HTTP 결과 반영 필요 | "무조건 전송" 표시 문제 | ✅ WebRequestHandler event 추적 + onFireResult |
| Fix Round 1 | 색상 반전 (재실=초록) | UX 감정 매핑 (정상=초록) | ✅ CSS 규칙 전면 통일 |
| Fix Round 2 | uploadFS 규칙 완화 | 서버 substring 매칭과 불일치로 경고 dialog | ✅ 클라이언트도 substring |
| Fix Round 2 | 이력 5건 표시 | 8건 대비 시인성 | ✅ 클라이언트 slice, 링버퍼 8건 유지 (외부 API 완전성) |

### 2.2 Session-detected Gaps (in-session resolved)

Plan/Design 초기에는 예측하지 않았으나 실기 검증에서 발견하고 즉시 반영한 7건:

1. **G1** — 시스템 로그(Logger)에 재부재 이벤트 기록 필요
2. **G2** — 전송 성공/실패 아이콘 O/X로 명확화
3. **G3** — httpCode 실제 반영 (기존은 항상 -1 pending)
4. **G4** — 시분초 절대 시각 표시
5. **G5** — 색상 통일 (재실=초록, 부재=빨강)
6. **G6** — uploadFS 클라이언트 규칙과 서버 규칙 불일치
7. **G7** — 이력 화면 표시 개수 조정 (5건)

---

## 3. Success Criteria Final Status

| # | 기준 | 상태 |
|:-:|---|:-:|
| SC-1 | attendance.enabled=true → 홈 카드 | ✅ |
| SC-2 | enabled=false → 카드 미표시 | ✅ |
| SC-3 | 상태 3s 갱신 | ✅ WebSocket |
| SC-4 | 이력 5s 갱신 | ✅ setInterval |
| SC-5 | 상태 모니터 이름·순서·PC LED dot | ✅ 첨부 이미지 준수 |
| SC-6 | GPIO2 실시간 ≤2s | ✅ broadcast 강화 |
| SC-7 | 부팅 후 15s attendance URL | ✅ syncOnBoot |
| SC-8 | /api/status attendance 필드 | ✅ |
| SC-9 | /api/attendance/history 8건 | ✅ |
| SC-10 | NetManager diff=0 | ✅ |
| SC-11 | IC 파서 회귀 없음 | ✅ |
| SC-12 | 필드 dogfood | ✅ "검증 완료했어" |

**Success Rate**: **12/12 fully met**

---

## 4. Value Delivered

### 4.1 재부재 감시자
- 홈 화면 최상단에서 상태·이력 한눈에 확인
- 재실=🟢 / 부재=🔴 대형 뱃지, O/X 전송 결과, 시분초 시각
- 최근 5건 스크롤 없이 표시

### 4.2 외부 자동화 시스템
- `/api/status` 응답의 `attendance.current`로 즉시 조회
- `/api/attendance/history` 로 최근 8건 이력 조회
- IntegrateController 등 기존 파서 회귀 없음 (unknown 필드 무시)

### 4.3 필드 설치 인력
- 홈 화면 시인성 개선 (상태 모니터 순서·PC LED dot·색상 통일)
- 재부재 카드 자동 표시 (설정 조건부)
- spiffs 업로드 시 파일명 경고 dialog 제거

### 4.4 기술 부채
- WebRequestHandler event 추적 인프라 확립 (다른 이벤트 결과 반영에도 재사용 가능)
- AttendanceHandler Logger 브릿지 패턴 확립
- 색상 규칙 문서화 (재실=ON=성공=연결=초록, 부재=OFF=실패=연결안됨=빨강)

---

## 5. Lessons Learned

### 5.1 초기 Design의 예측 한계
- v2.6.2 Design 단계에서 Entry.timeStr을 heap fragmentation 우려로 배제했으나, 실기 검증에서 시분초 표시가 사용성상 필수임을 확인 → `char[16]` 고정 크기로 재도입
- **교훈**: 사용성 필드는 Design 초기에 배제하지 말고 fixed-size 저장 대안까지 고려

### 5.2 서버-클라이언트 규칙 이중화 위험
- OTAHandler는 substring 매칭인데 uploadFS는 endsWith 매칭 → 서버가 정상 라우팅해도 클라이언트가 사용자에게 경고 → 불안감 유발
- **교훈**: 클라이언트/서버 검증 로직은 규칙을 통일하거나 문서로 명시

### 5.3 fire-and-forget의 UX 함정
- v2.6.1 시점 AttendanceHandler는 fire만 하고 결과 몰라 UI가 "무조건 전송" 표시 → 사용자 신뢰도 낮음
- 이벤트별 result callback 인프라를 v2.6.2 fix에서 추가하여 근본 해결
- **교훈**: 비동기 fire의 결과를 서버측에서 소유하고 UI로 노출하는 통로 필요

---

## 6. Carry Items (v2.6.3+ 후보)

| Item | 즉시성 | 트리거 |
|---|:-:|---|
| IntegrateController에 attendance 컬럼 추가 | 낮음 | 통합 감시 필요 시 |
| 재부재 이력 검색·필터·엑스포트 | 낮음 | 이력 활용 요구 시 |
| GPIO1/GPIO3 감지·실시간 갱신 (GpioMonitor 승격) | 낮음 | 다른 접점 입력 요구 |
| 부팅 sync 개별 채널별 옵션화 | 낮음 | 이중 발화 문제 발생 시 |

---

## 7. 마감 절차

- [x] Analysis 문서
- [x] Report 문서 (본 문서)
- [ ] `/pdca archive RemoteDeck_PC_v2.6.2 --summary`
- [ ] commit + push
