---
template: report
version: 1.0
feature: RemoteDeck_Touch_v2.2
date: 2026-06-23
author: KDI
project: RemoteDeckSystem
status: Completed (sync WebServer 가설 폐기 + v2.3 carry-over)
plan_doc: ../01-plan/features/RemoteDeck_Touch_v2.2.plan.md
design_doc: ../02-design/features/RemoteDeck_Touch_v2.2.design.md
analysis_doc: ../03-analysis/RemoteDeck_Touch_v2.2.analysis.md
match_rate: 49
branch: v2.2-zero (보존, origin push 됨)
operational_state: v2.1 운영 유지 (main 156d089)
---

# RemoteDeck_Touch v2.2 Completion Report

> **Summary**: zero-base 설계로 sync WebServer (Arduino 내장) 시도. PoC 단발 동작은 성공이었으나 Phase 2 풀세트(4탭 UI + Image/Config/Logger + Control) 도달 후 **W5500+MQTT+LVGL+다중 client 환경에서 연속/병렬 요청 처리 본질적 불안정** 발현. 가설 폐기 + v2.1 운영 유지 + v2.3 esp_http_server 재설계 결정.
>
> **Match Rate**: 49% (Plan SC 4 Met / 4 Partial / 2 Not Met)
> **Period**: 2026-06-23 (1일, 5개 commit on v2.2-zero)
> **Operational Outcome**: 단말은 v2.1 (156d089) 유지, v2.2 코드는 `v2.2-zero` 브랜치 보존 (origin push 완료)

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v2.1에서 분리된 WebUI 활성화 (W5500+MQTT 환경) + PNG 지원 필요. AsyncWebServer task slot 충돌 회피 위해 sync WebServer 가설 검증. |
| **Solution** | ESP32 Arduino 내장 sync WebServer + 협력적 yield (Option C) 채택. ImageApi/ConfigApi/Logger 모듈 신설 + 4탭 UI. v2.2-zero 별도 브랜치에서 시도. |
| **Function/UX Effect** | **달성 못함**. 단발 동작 검증되었으나 풀세트 시점에 라우팅 불안정(handler not found 반복), 이미지 디코드 fail 패턴, Control 탭 부팅 hang. 운영 인도 불가. |
| **Core Value** | **부정적 발견의 가치**: sync WebServer 가설 폐기 → v2.3 esp_http_server 재설계 방향 명확. `v2.2-zero` 코드 보존으로 학습 자산 유지. main 안정성 100% 보호 (별도 브랜치 전략 성공). |

### Value Delivered (4 perspectives)

| 관점 | 달성 |
|------|------|
| **Problem 해결** | ❌ WebUI 활성 실패 — 운영급 미달 |
| **Solution 적용** | ⚠️ Option C 구현 완료, 그러나 가설 자체 오류 입증 |
| **Function/UX** | ❌ 사용자 인도 불가, v2.1 운영 유지로 가치 ZERO |
| **Metrics** | Match Rate 49% / heap 78KB 최저 / 5분 운영 시 불안정 — 모두 목표 미달 |

### 학습 가치 (별도)

| 학습 항목 | 가치 |
|----------|------|
| sync WebServer 가설 폐기 검증 | v2.3 이후 시도 회피 |
| 별도 브랜치 (`v2.2-zero`) 전략 | main 100% 안전 + 학습 자산 보존 |
| 격리 진단 방식 (WebServer #if 비활성 빌드) | 향후 hang 디버깅 방법론 |
| Phase 1 PoC 한계 인식 | v2.3 PoC 는 풀세트 케이스 필수 |
| esptool 직접 호출 우회 (--connect-attempts 5) | pio 자동 reset 실패 시 대안 확보 |

---

## 1. PDCA Journey

| Phase | Document | Commits (v2.2-zero) | 결과 |
|-------|----------|---------------------|------|
| Plan | [v2.2.plan.md](../01-plan/features/RemoteDeck_Touch_v2.2.plan.md) | `0f4d687` (main) | Zero-base 설계, 5 Phase 구조, v2.3 백로그 명시 |
| Design | [v2.2.design.md](../02-design/features/RemoteDeck_Touch_v2.2.design.md) | `0f4d687` (main) | Option C - Pragmatic (sync WebServer + 협력적 yield) |
| Do Phase 1 (PoC) | (code) | `c514ecf` | sync + W5500 + MQTT 단발 동작 검증 — `/api/status` 200 OK |
| Do Phase 2 (API) | (code) | `dd4183f` | ImageApi + ConfigApi + Logger + 4탭 UI (Images/Config/Logs) |
| Do (Control 시도) | (code) | `122624b` → 부팅 hang | LCD 미러링 + IN/OUT 토글 → 부팅 후 행 걸림 → Polling 제거(`f4c0f84`)도 무효 → Control 롤백 |
| Do (격리 진단) | (code) | `6d660b3` | WebServer #if 비활성 빌드 → v1/v2.1 기능 100% 정상 → WebServer = hang 원인 확정 |
| Check | [v2.2.analysis.md](../03-analysis/RemoteDeck_Touch_v2.2.analysis.md) | `52734f1` (main) | Match Rate 49%, sync WebServer 가설 폐기 |
| Act | — | — | Skip (라이브러리 교체는 iterate 범위 외) |
| Report | (this doc) | TBD | — |
| Operational Rollback | (firmware) | — | 단말에 v2.1 (156d089) FULL+SPIFFS 재업로드 |

---

## 2. Key Decisions & Outcomes

### 2.1 Decision Record Chain

```
[Plan]   MQTT 유지     : Yes (사용자 결정) — OK 적용
[Plan]   WebUI 스코프   : 풀세트 (image+OTA+config+logs)
[Plan]   PNG 방식       : lv_png_init + LV_USE_FS_STDIO
[Plan]   Branch        : v2.2-zero (zero-base)
[Design] Architecture : Option C - sync WebServer + 협력적 yield  ← 가설 오류
[Design] HTTP Lib     : ESP32 Arduino 내장 WebServer
[Design] OTA Lib      : Update.h
[Design] Logger       : in-memory ring buffer 50건
[Design] PoC          : §12.1 단발 /api/status 검증
```

### 2.2 결정 vs 결과 (Decision Record Verification)

| Decision | Followed | Outcome |
|----------|:--:|---------|
| MQTT 유지 | ✅ | 운영 호환성 유지 — 정상 |
| 풀세트 스코프 | ⚠️ | Phase 2 까지 도달, PNG/OTA 미진행 |
| PNG lv_png_init | ❌ | 미수행 (Phase 3 미도달) |
| **Option C sync WebServer** | ❌ | **가설 오류 입증** — 단발 OK, 연속/병렬 불안정 |
| **PoC = 단발 /api/status** | ⚠️ | 가설 검증 한계 — 풀세트 케이스 미포함 |
| OTA Update.h | ❌ | 미수행 (Phase 4 미도달) |
| v2.2-zero 별도 브랜치 | ✅ | main 안전 보존 — 전략 효과 입증 |
| Basic Auth | ✅ | 정상 적용 |

### 2.3 Plan에 없던 적응 결정

| 시점 | 결정 | 이유 |
|------|------|------|
| Control 탭 hang 발견 | 격리 진단 빌드 (`#if WEB_SERVER_DISABLED_DEBUG`) | hang 원인을 WebServer vs 다른 모듈로 정확 격리 |
| 격리 결과 확정 | sync WebServer 자체가 원인 → 가설 폐기 | 자동 iterate 범위 외, 라이브러리 교체 필요 |
| v2.1 롤백 | main 156d089 펌웨어 + SPIFFS 재업로드 | 운영 단말 안전 복귀 |
| v2.2-zero 브랜치 origin push | 학습 자산 보존 | v2.3 esp_http_server 재설계 시 참고 |

---

## 3. Success Criteria Final Status

| ID | Criteria | Status | Evidence |
|----|----------|:--:|----------|
| FR-01 | W5500+MQTT+WebServer 3자 동시 동작 task slot 충돌 ZERO | ⚠️ Partial | task error 0건. 그러나 `handler not found` 반복 |
| FR-02 | Ethernet IP → 이미지 관리 UI | ⚠️ Partial | 단발 OK, 연속 불안정 |
| FR-03 | PNG 업로드 → LCD 5초 갱신 | ❌ Not Met | 미구현 |
| FR-04 | OTA 펌웨어 업데이트 | ❌ Not Met | 미구현 |
| FR-05 | deviceconfig/serverconfig 웹 편집 | ⚠️ Partial | GET/POST 동작, 안정성 미흡 |
| FR-06 | 로그 뷰어 50건 ring buffer | ✅ Met | curl 200 OK |
| FR-07 | Basic Auth (admin:12345) | ✅ Met | 401/200 검증 |
| FR-08 | v2.1 회귀 ZERO | ⚠️ Partial | WebServer OFF 시 정상, ON 시 이미지 디코드 fail |
| FR-09 | PIO/Arduino-ESP32 호환 | ✅ Met | pioarduino 53.03.10 유지 |
| FR-10 | PC 디자인 토큰 재사용 | ✅ Met | style.css 동일 |

**합계**: Met 4 / Partial 4 / Not Met 2 = **40% Met, 80% (Met + Partial)**

---

## 4. Memory & Performance Metrics (실측)

| 지표 | 측정값 | 목표 | 평가 |
|------|--------|------|------|
| heap free (idle WebServer ON) | 92~125 KB | ≥ 40KB | ✅ |
| heap free (5분 운영) | 78 KB (최저) | ≥ 40KB | ⚠️ — v2.1 (56KB) 대비 감소 |
| WebServer 응답 (단발) | 50-300ms | — | ✅ |
| 연속 요청 안정성 | 불안정 (handler not found 반복) | 0건 | ❌ |
| 이미지 디코드 (upload 후) | 1/3 ~ 2/3 성공 (서로 다른 이미지 fail) | 100% | ❌ |
| Flash 사용량 | 56.9-57.0% | — | ✅ (v2.1 65.3% 대비 -8KB) |
| 빌드 시간 | 120-200초 (clean) | — | ✅ |

---

## 5. Artifacts Delivered

### 5.1 Documents (main 브랜치)

| 문서 | 위치 |
|------|------|
| Plan | `docs/01-plan/features/RemoteDeck_Touch_v2.2.plan.md` |
| Design | `docs/02-design/features/RemoteDeck_Touch_v2.2.design.md` |
| Analysis | `docs/03-analysis/RemoteDeck_Touch_v2.2.analysis.md` |
| Report (this) | `docs/04-report/RemoteDeck_Touch_v2.2.report.md` |

### 5.2 Code (v2.2-zero branch, 학습 보존)

**main 변경 없음** (v2.1 156d089 안정 운영 유지)

**v2.2-zero 신규/수정 (운영 미적용)**:
- `src/web/WebServer.{h,cpp}` — sync TouchWebServer 래퍼
- `src/web/ImageApi.{h,cpp}` — sync upload 핸들러 (협력적 yield)
- `src/web/ConfigApi.{h,cpp}` — deviceconfig/serverconfig GET/POST
- `src/web/Logger.{h,cpp}` — in-memory ring buffer 50건
- `src/web/ControlApi.{h,cpp}` — LCD 미러링 + IN/OUT 토글 (Control 탭, rollback됨)
- `data/www/index.html`, `style.css`, `app.js` — 4탭 UI
- `platformio.ini` — mathieucarbou async 제거, sync 사용

### 5.3 Commit Sequence (v2.2-zero branch)

```
6d660b3 wip: yieldToCore() + Control rollback residue (보존용)
f4c0f84 fix: 자동 polling 모두 제거 (sync 부하 회피 시도, 무효)
122624b feat: Control 탭 추가 — LCD 미러링 + IN/OUT 토글 (hang 유발)
dd4183f feat: C4-C7 Phase 2 API 풀세트 + 4탭 UI
c514ecf feat: C1-C3 Phase 1 PoC — sync WebServer 동시 동작 검증
156d089 (v2.1 분기점, main 안정 운영 상태)
```

**main 변경**:
```
52734f1 docs(v2.2): Analysis (Check phase) - Match Rate 49%
0f4d687 docs(v2.2): Plan + Design
156d089 fix(v2.1): DHCP 타임아웃 30s → 15s + 1회 재시도 (운영)
```

### 5.4 Operational State

- **단말 펌웨어**: v2.1 (156d089) FULL + SPIFFS 재업로드 완료
- 단말 IP: 192.168.10.122, MQTT broker 192.168.10.230, room/node_1
- 안정 운영 중 (LCD/터치/IN-OUT/Long-press/Sleep/BMP/MQTT/시간 동기화)

---

## 6. Lessons Learned

### 6.1 잘 된 점

1. **별도 브랜치 (`v2.2-zero`) 전략** — main 안전 100% 보존. 실패한 가설을 main에 반영하지 않음으로써 운영 단말은 zero downtime
2. **격리 진단 방식** — WebServer 비활성 빌드로 hang 원인을 명확히 격리 (`#if WEB_SERVER_DISABLED_DEBUG`)
3. **단계별 commit** — PoC → Phase 2 → Control → 격리 진단까지 각 commit 으로 정확한 차단 지점 식별 가능
4. **사용자 보고 반영 속도** — 행 걸림 보고 → 폴링 제거 → 부족 → 격리 진단 → 가설 폐기 결정까지 한 세션에 완료

### 6.2 잘못된 가설

1. **"sync WebServer 가 단발 검증되면 풀세트도 OK"** — PoC §12.1 의 핵심 가설 오류. 풀세트 (4탭 UI 동시 로드 + upload + polling) 부하는 단발과 본질적으로 다름
2. **"협력적 yield (lv_timer_handler())로 LVGL 지연 회피 가능"** — handleClient() 가 main loop 점유하는 동안 다른 client 처리 못 함은 별개 문제
3. **"Control 탭 추가는 단순 endpoint 추가"** — 실제로는 부하 임계점 넘김 → 부팅 hang

### 6.3 v2.3 인계 사항 (Carry-over)

| 우선순위 | 작업 | 의존성 |
|:---:|------|--------|
| 1 | **WebServer 라이브러리 교체** — esp_http_server (ESP-IDF native, 별도 task, core pinning) | 단독 |
| 2 | **PoC 강화** — 단발 /api/status 가 아닌, 풀세트 시뮬레이션 (4탭 동시 로드 + upload + polling 5분) | esp_http_server 도입 |
| 3 | PNG 디코더 활성화 (lv_png_init + LV_USE_FS_STDIO) | 단독 |
| 4 | OTA Handler (Update.h) | esp_http_server 안정화 후 |
| 5 | Control 탭 (LCD 미러링 + IN/OUT) | esp_http_server 안정화 후 |
| 6 | 시간 표시 UI (사용자 v2.1 보고 item 4) | 독립 |

### 6.4 일반화 가능한 교훈

- **임베디드에서 "단발 PoC ≠ 풀세트 안정성"** — 모든 PoC 는 실제 운영 부하의 최소 패턴을 시뮬레이션 해야 함
- **별도 브랜치 + main 안전 분리** 는 실패 시도의 가치를 보존 (학습 자산화)
- **격리 진단 빌드(`#if DEBUG`)** 가 hang 디버깅에 가장 강력

---

## 7. Recommendation for Next Steps

1. **v2.2 종료** — 이 Report 로 사이클 닫고 archive
2. **v2.3 plan 시작** — esp_http_server 기반 재설계 (다음 세션 권장)
3. **단말 운영** — v2.1 (156d089) 유지, 일반 운영 가능
4. **v2.2-zero 브랜치** — 그대로 보존 (origin push 완료). v2.3 작업 시 참고 자료

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-23 | 초안 — Match Rate 49%, sync WebServer 가설 폐기, v2.3 esp_http_server 인계 | KDI |
