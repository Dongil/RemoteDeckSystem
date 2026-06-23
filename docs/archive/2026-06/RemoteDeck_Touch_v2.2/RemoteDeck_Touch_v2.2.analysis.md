---
template: analysis
version: 1.0
feature: RemoteDeck_Touch_v2.2
date: 2026-06-23
author: KDI
project: RemoteDeckSystem
status: Draft
plan_doc: ../01-plan/features/RemoteDeck_Touch_v2.2.plan.md
design_doc: ../02-design/features/RemoteDeck_Touch_v2.2.design.md
base_commit_main: 156d089 (v2.1 안정)
v22_zero_branch_head: 6d660b3 (v2.2-zero, origin push 됨, 보존)
match_rate: 49
verdict: Sync WebServer 가설 폐기 → v2.3 esp_http_server 재설계 필요
---

# RemoteDeck_Touch v2.2 Gap Analysis

> **Match Rate**: **49%** (Plan SC 4/10 Met, 4 Partial, 2 Not Met)
> **Verdict**: sync WebServer 가설 폐기. Touch (LVGL+W5500+MQTT) 환경에서 연속 요청 처리 불안정 확정.
> **결과**: v2.1 안정 상태 (main 156d089) 운영 유지, v2.2 코드는 `v2.2-zero` 브랜치 보존, v2.3 esp_http_server 재설계.

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | W5500+MQTT 환경 WebUI + PNG 콘텐츠 지원으로 단말 자급운영 + 원격 관리 완성 |
| **WHO** | Touch 단말 운영자 (Ethernet 기본), 콘텐츠 담당자, 원격 펌웨어 관리자 |
| **RISK** | sync WebServer LVGL 지연 (실제 발현 + 라우팅 불안정 추가 발견) |
| **SUCCESS** | Ethernet WebUI 100% / PNG ≤5초 / OTA 성공 / v2.1 회귀 ZERO / heap ≥40KB |
| **SCOPE** | Phase 1 PoC → 2 API → 3 PNG → 4 OTA → 5 회귀 (Phase 1-2 부분 완료, 3-5 미진행) |

---

## 1. Strategic Alignment Check

### 1.1 WHY 달성도

| 목표 | 결과 |
|------|------|
| W5500+MQTT 환경 WebUI 정상 동작 | ❌ **미달** — task 충돌은 없으나 연속/병렬 요청 시 라우팅 불안정 + 이미지 디코드 fail 패턴 |
| PNG 콘텐츠 지원 | ❌ **미달** — Phase 3 미진행 |
| 단말 자급운영 + 원격 관리 | ❌ **미달** — 운영자에게 인도 가능한 안정성 미충족 |

**Critical 평가**: v2.1 의 "WiFi 환경에서만 WebUI" 한계 해소가 목표였으나, **v2.2 sync WebServer 가 Ethernet 환경에서도 안정 동작 못 함**. 결과적으로 v2.1 대비 추가 가치 ZERO + 운영 위험만 추가.

### 1.2 Design Decision 준수 여부

| Decision (Design §7.2) | Followed | Outcome |
|------------------------|:--:|---------|
| Web Server: sync (Option C) | ✅ | 가설 자체가 오류 — 단발 동작은 OK, 연속 불안정 |
| MQTT 유지 | ✅ | 정상 (v2.1 그대로) |
| LV_USE_PNG + LV_USE_FS_STDIO | ❌ | 미수행 (Phase 3) |
| OTA Update.h | ❌ | 미수행 (Phase 4) |
| Auth Basic | ✅ | 정상 (Basic Auth 401 검증) |
| Logger ring buffer 50건 | ✅ | 구현 + curl 검증 OK |
| PIO platform | ✅ | pioarduino 53.03.10 유지 |
| `v2.2-zero` 별도 브랜치 | ✅ | 회귀 없이 main 보존 — 전략 효과 발휘 |

**준수 5/8** — PNG/OTA 미수행 + sync WebServer 가설 오류.

---

## 2. Plan Success Criteria 평가 (Plan §3.1)

| ID | Criteria | 결과 | Evidence |
|----|----------|:--:|----------|
| FR-01 | W5500+MQTT+WebServer 3자 동시 동작 (task slot 충돌 ZERO) | ⚠️ **Partial** | task error 0건. 그러나 연속 요청 시 `[E][WebServer.cpp:794] request handler not found` 반복 |
| FR-02 | Ethernet IP → 이미지 관리 UI 표시 | ⚠️ **Partial** | 정적 파일/단발 API OK. 연속 사용 시 응답 누락 |
| FR-03 | PNG 업로드 → LCD 5초 갱신 | ❌ **Not Met** | LV_USE_PNG=0 유지, 미구현 |
| FR-04 | OTA 펌웨어 업데이트 | ❌ **Not Met** | Phase 4 미진행 |
| FR-05 | deviceconfig/serverconfig 웹 편집 | ⚠️ **Partial** | GET/POST 구현 + curl 200 OK. 연속/동시 사용 시 불안정 |
| FR-06 | 로그 뷰어 (ring buffer 50건) | ✅ **Met** | Logger 구현 + /api/log 200 OK |
| FR-07 | Basic Auth | ✅ **Met** | 401 without creds, 200 with admin:12345 |
| FR-08 | v2.1 회귀 ZERO | ⚠️ **Partial** | WebServer OFF 시 100% 정상. ON 시 이미지 디코드 fail 패턴 — 운영급 미달 |
| FR-09 | PIO/Arduino-ESP32 최신 + lib 호환 | ✅ **Met** | pioarduino 53.03.10 유지, 호환 검증 |
| FR-10 | PC 디자인 토큰 재사용 | ✅ **Met** | style.css 동일 |

**합계**: Met 4 / Partial 4 / Not Met 2 = 40% Met, 80% (Met + Partial)

---

## 3. Match Rate (Static + Board Boot Verification)

### 3.1 점수 산출

| 축 | 점수 | 근거 |
|----|------|------|
| **Structural** | **75%** | WebServer/ImageApi/ConfigApi/Logger/ControlApi + www/ 5탭 모두 작성. PNG/OTA 모듈 미작성 |
| **Functional** | **40%** | 단발 동작 OK, 연속/지속 사용 불안정. PNG/OTA 미구현 |
| **Contract** | **45%** | Basic Auth + status/list/log 정상. upload/config 응답 누락 빈발, PNG/OTA 미구현 |

**Overall** (static-only):
```
Overall = 75 × 0.2 + 40 × 0.4 + 45 × 0.4
        = 15      + 16        + 18
        = 49%  (반올림)
```

### 3.2 Runtime Verification (보드 실측)

| Layer | Status | Result |
|-------|--------|--------|
| L1 — API curl | ✅ 부분 | /api/status, /api/log, /api/config GET 정상. upload/POST 응답 빈번 누락 |
| L2 — UI | ⚠️ 부분 | 브라우저 단발 동작 OK. 연속/탭 전환 시 hang 또는 무응답 |
| L3 — E2E | ❌ | 일관된 시나리오 완주 못 함 |
| L4 — Stability | ❌ | 5분 운영 중 이미지 디코드 실패 패턴 + handler not found 반복 |

---

## 4. Decision Record Verification

| Decision | Followed | Outcome |
|----------|:--:|---------|
| [Plan] Scope = 핵심 (WebUI + PNG) | ⚠️ Partial — WebUI Phase 1-2 도달, PNG 미진행 | WebUI 자체가 불안정으로 PNG 단계 진입 못 함 |
| [Plan] Branch `v2.2-zero` | ✅ | main 안전 보존, 전략 효과 |
| [Plan] PIO 업그레이드 가능 | ✅ | 현 platform 유지가 합리적이었음 |
| [Design] Option C - sync + 협력적 yield | ❌ **가설 오류** | Touch (LVGL + W5500 + MQTT + 다중 client) 환경에서 sync 부적합 |
| [Design] Phase 1 PoC escape hatch | ⚠️ 부분 활용 | PoC 단계는 성공으로 진행했으나 Phase 2 풀세트에서 본질적 불안정 발현 — PoC 가설 검증이 단발 케이스만 다뤄서 미흡 |

**핵심 학습**: Design §12.1 PoC 가 "단발 /api/status 응답"만 검증해서 가설 오류를 못 잡음. 풀세트(동시 다수 client + upload + config + log + UI 페이지 로드) 시점에 본질이 드러남.

---

## 5. Gap List

### 5.1 Critical (운영 차단)

| # | Gap | 원인 | Action |
|---|-----|------|--------|
| C1 | sync WebServer 가 ESP32 Touch 환경에서 연속/병렬 요청 처리 불안정 | 단일 thread, main loop 점유, 다중 client 처리 능력 부족 | **v2.3 사이클**: esp_http_server (별도 task + core pinning) 로 재설계 |
| C2 | PNG 디코더 미구현 | Phase 3 미도달 | v2.3 사이클로 분리 |
| C3 | OTA Handler 미구현 | Phase 4 미도달 | v2.3 사이클로 분리 |
| C4 | Control 탭 추가 시 부팅 hang | sync WebServer + 다중 endpoint 부하 임계점 | esp_http_server 도입 후 재시도 |

### 5.2 Important

| # | Gap | Action |
|---|-----|--------|
| I1 | "request handler not found" 반복 발생 | 라우팅 등록 패턴 점검 (Phase 2의 onNotFound 중복 등록 의심) |
| I2 | 이미지 디코드 fail 패턴 (서로 다른 이미지 번갈아) | try_set 의 OLD free 후 재할당 race 추정 — esp_http_server 환경에서 재검증 |
| I3 | heap free 78KB 까지 하락 | 임시 객체 누적, JSON 직렬화 최적화 검토 |

### 5.3 Positive findings (Plan 외 추가 학습)

| # | Item | 효과 |
|---|------|------|
| P1 | `v2.2-zero` 별도 브랜치 전략 | main 안정 100% 보존, 회귀 없이 학습 보유 |
| P2 | Phase 1 PoC 단계로 가설 사전 검증 | 다음 사이클에서는 PoC 가 풀세트 케이스도 포함해야 함을 학습 |
| P3 | 격리 진단 방식 (WebServer 비활성 빌드) | hang 원인 명확 격리 — 디버깅 방법론 확립 |
| P4 | esptool 직접 호출 + connect-attempts 5 | pio 실패 시 대안 업로드 경로 확보 |

---

## 6. Recommendation

### 6.1 Match Rate 49% < 90% — iterate 적용 가능 여부

`pdca-iterator` 의 자동 fix loop 는 **단발 코드 버그**에 적합하나, v2.2 의 본질 문제는 **sync WebServer 라이브러리 자체의 부적합**. 자동 fix 로 해결 불가.

### 6.2 결정 후보

| 옵션 | 설명 |
|------|------|
| **A. 그대로 진행 (v2.1 운영 유지) + v2.3 새 사이클 시작** | **권장** — v2.2 = "sync WebServer 가설 실패 + 학습 보존" 으로 종료. esp_http_server 기반 v2.3 plan 시작 |
| B. iterate 시도 | 의미 없음 (라이브러리 교체는 자동 fix 범위 외) |
| C. 즉시 esp_http_server 시도 | 컨텍스트 한계 — 다음 세션에 v2.3 plan 부터 |

### 6.3 v2.3 사이클 필수 조건 (학습 반영)

1. **PoC 가 풀세트 케이스를 시뮬레이션 해야 함** — 단발 /api/status 응답이 아닌, 4탭 UI 로드 + 동시 다수 요청 + upload 1회를 PoC 단계에서 실행
2. **WebServer 라이브러리는 별도 task 보장** — esp_http_server, or AsyncWebServer_ESP32_W5500 (khoih-prog), or 커스텀 task 분리
3. **L4 Stability 를 Design Phase 단계에서 정의** — 5분 / 30분 / 1시간 운영 후 heap 추이 측정

---

## 7. Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-23 | 초안 — Match Rate 49%, sync WebServer 가설 폐기, v2.3 esp_http_server 권장 | KDI |
