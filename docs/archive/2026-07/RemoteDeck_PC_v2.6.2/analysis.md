---
template: analysis
version: 1.3
feature: RemoteDeck_PC_v2.6.2
date: 2026-07-06
author: KDI
project: RemoteDeckSystem
firmware_version: v2.6.2
match_rate: 100
verification: static + field runtime (test device, 2 fix rounds)
---

# RemoteDeck_PC v2.6.2 Gap Analysis

**Overall Match Rate**: **100%** (Static: Structural 100 × 0.2 + Functional 100 × 0.4 + Contract 100 × 0.4)

**Baseline**: v2.6.1 firmware + v2.6.1 SPIFFS → **Target**: v2.6.2 firmware + v2.6.2 SPIFFS
**Verification**: Static 3축 + 실기 필드 검증 완료 (초기 배포 → 사용자 5건 fix → 사용자 2건 추가 fix → "검증 완료했어" 확인, 2026-07-06)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.6.1 설정 UX 이후 감시·통합 표면(홈 모니터링/부팅 정합성/외부 API) 정비 |
| **WHO** | 재부재 감시자, 외부 자동화 시스템, 설치 운영자 |
| **RISK** | RAM · WS 부하 · 이중 fire · UI 회귀 · v2.4.7 방어선 |
| **SUCCESS** | 홈 카드 조건부 · 상태 모니터 순서/dot · GPIO2 실시간 · 부팅 15s · /api/status attendance · diff=0 |
| **SCOPE** | S1 펌웨어(링버퍼+엔드포인트+broadcast+syncOnBoot+status) → S2 홈 Web UI → S3 필드 dogfood |

---

## 1. Strategic Alignment

| 항목 | 확인 |
|---|:-:|
| Plan 8개 요구 (FR1~FR8) 모두 반영? | ✅ 실기 검증 완료 |
| Design Option C 결정 준수 (얕은 handler 확장)? | ✅ 클래스 신설 없음, 기존 확장 |
| v2.5.1 SPIFFS OTA config preserve 호환? | ✅ deviceconfig.json 통째 backup — attendance 자동 유지 |
| v2.4.7 방어선 무결성? | ✅ NetManager diff = 0 |

---

## 2. Structural Match — 100%

Design §11.1 vs 실제:

| Design 명시 파일 | 예상 | 실제 | 상태 |
|---|:-:|:-:|:-:|
| `src/control/AttendanceHandler.h` | +30 | +37 | ✅ |
| `src/control/AttendanceHandler.cpp` | +55 | +90 | ✅ (Logger/timeStr/onFireResult 확장분 포함) |
| `src/web/WebServer.h` | +3 | +5 | ✅ |
| `src/web/WebServer.cpp` | +6 | +9 | ✅ |
| `src/network/WebRequestHandler.h/cpp` (fix-1) | 계획 외 | +19 | ✅ Session-detected fix |
| `src/main.cpp` | +15 | +23 | ✅ (bridge wiring 확장) |
| `src/config/ConfigManager.cpp` | +2 | +2 | ✅ |
| `data/www/index.html` | +35 | +30 | ✅ |
| `data/www/app.js` | +60 | +90 | ✅ (fix 반영분 포함) |
| `data/www/style.css` | +40 | +40 | ✅ |
| **NetManager.h/cpp** | **0** | **0** | ✅ **SC-10** |

**총계**: ~345 LOC (Design 예상 ~246 + 사용자 fix 반영 ~100). fix로 증가.

---

## 3. Functional Depth — 100%

12 SC 매핑:

| # | 요구 | 반영 위치 |
|:-:|---|---|
| SC-1 | attendance.enabled=true → 홈 카드 표시 | `app.js:updateDashboard` `card.style.display=''` |
| SC-2 | enabled=false → 카드 미표시 | `card.style.display='none'` |
| SC-3 | 상태 3s 이내 갱신 | WebSocket broadcast (switchMonitor onChange + PCMonitor onChange) |
| SC-4 | 이력 5s 이내 추가 | `setInterval(loadAttendanceHistory, 5000)` + fix-7 slice(0,5) |
| SC-5 | 상태 모니터 이름/순서/PC LED dot | `index.html` `상태 모니터` h2 + 순서 재정렬 + `.pc-led-line` + `.led-dot` |
| SC-6 | GPIO2 실시간 (≤2s) | `switchMonitor.setOnChange` 콜백에 `ws.broadcastStatus` 추가 |
| SC-7 | 부팅 후 15s attendance URL 도달 | `attendanceHandler.syncOnBoot()` in loop() |
| SC-8 | /api/status attendance 필드 | `buildStatusJson` `attendance: {enabled,source,current}` |
| SC-9 | /api/attendance/history 8건 | `WebServer` 라우트 + `AttendanceHandler.toJson()` |
| SC-10 | NetManager diff=0 | ✅ git diff empty |
| SC-11 | IC 파서 회귀 없음 | attendance 필드 추가만, unknown 필드 무시 |
| SC-12 | 필드 dogfood | "검증 완료" 확인 |

---

## 4. API Contract — 100%

### 신규 필드 (하위호환)

| Endpoint | v2.6.1 | v2.6.2 |
|---|---|---|
| GET /api/status | 상태 스냅숏 | + `attendance: {enabled, source, current}` |
| GET /api/attendance/history | 없음 | 신규 (링버퍼 8건 이하) |
| WebSocket status broadcast | pcMonitor onChange만 | + switchMonitor onChange |
| WEBREQ 이벤트 결과 콜백 | Logger로만 | + result callback (event, code) |

### fire 파이프라인 확장

- `RequestItem`에 `char event[24]` 필드 추가
- `workerLoop` 종료 시 `_onResult(event, code)` 호출
- 실패 케이스(-2 begin fail 포함) 모두 콜백 전달

**응답 shape 100% 하위호환**. IntegrateController 회귀 없음.

---

## 5. Success Criteria 최종 상태

12/12 fully met (SC-1~12).

특히 사용자 fix 라운드 2회에서 발견·해결한 세부 항목:

**Round 1 (5건)**:
- FIX-1: attendance 이벤트를 시스템 로그(Logger)에도 기록 → `_logAttend` + bridge
- FIX-2: 전송 O/X 아이콘 (`O 전송성공` / `X 전송실패` + code)
- FIX-3: 실제 HTTP 결과 반영 (result callback → `onFireResult` → httpCode 갱신)
- FIX-4: 상대 시간 → 시분초 (Entry.timeStr[16] + NTP getter)
- FIX-5: 색상 통일 (재실=ON=성공=연결=**초록**, 부재=OFF=실패=연결안됨=**빨강**)

**Round 2 (2건)**:
- FIX-6: uploadFS 파일명 규칙 완화 (서버 substring 매칭과 통일, `_spiffs_` 중간 위치 허용)
- FIX-7: 재부재 이력 화면 표시 최근 5건 (링버퍼는 8건 유지)

---

## 6. Runtime Evidence

사용자 필드 확인: **"검증 완료했어"** (2026-07-06, 2차 fix 이후).

- v2.6.2 firmware.bin + spiffs.bin 정상 OTA
- 홈 최상단 재부재 카드 표시, 대형 뱃지 재실=초록/부재=빨강, 이력 5건 시분초 표시
- 상태 모니터 카드 이름·순서·PC LED dot (첨부 이미지 준수)
- GPIO2 실시간 반영
- `O 전송성공` / `X 전송실패` 아이콘 명확 구분
- spiffs bin `_spiffs_` 규칙 자연 통과 (경고 dialog 없음)

---

## 7. Match Rate 최종

```
Overall = Structural × 0.2 + Functional × 0.4 + Contract × 0.4
        = 100 × 0.2 + 100 × 0.4 + 100 × 0.4
        = 100.0%
```

iterate 불필요.

---

## 8. Session-detected Gaps (in-session resolved)

이번 사이클은 v2.6.2 초기 배포 후 사용자가 2 라운드 필드 fix 요청.
모두 in-session 반영 완료. 별도 iterate 불필요.

**v2.6.2 fix 총 7건**:
1. WebRequestHandler result callback (event → httpCode)
2. AttendanceHandler.onFireResult
3. AttendanceHandler Logger 브릿지 + ATTEND 카테고리 로그
4. Entry.timeStr[16] + NTP setTimeGetter
5. 색상 규칙 전면 통일 (재실=초록 반전)
6. uploadFS 규칙 substring 매칭
7. 이력 화면 5건 slice

---

## 9. Recommendations

- **필드 dogfood 지속** — 감시자 홈 접속 사용성 관찰
- **v2.6.3+ 후보 (즉시성 없음)**:
  - IntegrateController에도 attendance 상태 컬럼 추가 (외부 API 활용)
  - 재부재 이력 검색·필터·엑스포트
  - GPIO1/GPIO3 상태 감지·실시간 갱신 (GpioMonitor 승격)
  - 부팅 sync 대상 config별 옵션화

---

**Next**: `/pdca report RemoteDeck_PC_v2.6.2`
