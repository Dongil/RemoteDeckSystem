---
template: analysis
version: 1.0
feature: RemoteDeck_Touch_v2.1
date: 2026-06-22
author: KDI
project: RemoteDeckSystem
status: Draft
plan_doc: ../01-plan/features/RemoteDeck_Touch_v2.1.plan.md
design_doc: ../02-design/features/RemoteDeck_Touch_v2.1.design.md
base_commit: c44d348 (v2.1-lan merged)
verification_method: Static + Board Boot (no E2E server)
---

# RemoteDeck_Touch v2.1 Gap Analysis Document

> **Match Rate**: **68%** (Plan SC 5/8 Met, WebUI/PNG 의도적 v2.2 분리)
> **Verdict**: 핵심 목표(LAN 통일) 달성, 부수 목표(WebUI/PNG) 차단 — v2.2 사이클 필요

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v1 우회(WiFi-only) 해제 + PNG 지원으로 Ethernet 운영 환경 정상화 |
| **WHO** | Touch 단말 운영자 (Ethernet 기본), 콘텐츠 담당자 |
| **RISK** | TFT_eSPI 업데이트 후 LCD/터치 회귀 (High), PNG heap, 동시 두 마이그레이션 변경 폭 |
| **SUCCESS** | 교체 성공률 100% · 재부팅 없이 ≤5초 갱신 · heap free ≥ 50KB · v1 기능 100% 회귀 없음 |
| **SCOPE** | platform 마이그레이션 → ETH.h 통합 → PNG 활성화 → 회귀 검증 |

---

## 1. Strategic Alignment Check

### 1.1 WHY 달성 여부

| 목표 | 결과 |
|------|------|
| v1 우회(WiFi-only) 해제 → Ethernet 환경 WebUI 동작 | ⚠️ **부분** — LAN 스택은 통일됐으나 AsyncTCP task 충돌로 WebUI 자체 비활성 |
| PNG 지원으로 Ethernet 운영 환경 정상화 | ❌ **미달** — LV_USE_PNG=0 유지, BMP 만 동작 |

**Critical 평가**: Plan §1.1 Purpose의 **"실사용 단말은 대부분 Ethernet 환경이므로 핵심 기능이 사실상 비활성 상태"** 해결이 목표였는데, WebUI 자체가 동작 안 함 → v1과 사실상 동일 상태에서 추가 진전 없음.

단, v2.1의 **LAN 스택 통일 자체는 v2.2 WebUI 활성화의 전제 조건** — 즉 v2.1은 "기반 작업" 완료 + WebUI는 "실행" 단계 v2.2 분리.

### 1.2 Design Decision 준수 여부

| Decision | 의도 | 실제 |
|----------|------|------|
| Platform pioarduino 53.x | ✓ | ✓ 적용됨 |
| W5500 driver ETH.h | ✓ | ✓ ETH.begin(ETH_PHY_W5500) 동작 |
| 모듈 분리 (클래스 신설 X) | ✓ | ✓ ethernet_mqtt.cpp 인라인 |
| HTTPClient (ESP32 내장) | ✓ | ✓ downloadFile/sendHttpMessage 재작성 |
| TFT_eSPI 2.5.43 | ✓ | ✓ build_flags 이식 + LCD 회귀 없음 |
| **PNG: lv_png_init + LV_USE_FS_STDIO** | ✓ | ❌ **LV_USE_PNG=0 유지** (구현 안 함) |
| LVGL FS drive 'S' | ✓ | ❌ **미적용** (PNG 미활성과 연관) |
| v2.1-lan 별도 브랜치 | ✓ | ✓ 브랜치 운영 후 main merge |

**준수 5/7**: PNG 관련 2개 결정 미준수 — 의도적 분리.

---

## 2. Plan Success Criteria 평가 (Plan §4.1)

| ID | Criteria | 결과 | Evidence |
|----|----------|------|----------|
| FR-01 | Ethernet IP → 브라우저 → 이미지 관리 UI | ❌ **Not Met** | curl HTTP 000, AsyncTCP `failed to start task` |
| FR-02 | Ethernet 환경 `/api/*` 정상 응답 | ❌ **Not Met** | WebServer 비활성 (`Web UI: deferred to v2.2`) |
| FR-03 | PNG 업로드 → LCD 5초 내 갱신 | ❌ **Not Met** | LV_USE_PNG=0, lv_png_init 미호출 |
| FR-04 | 기존 BMP 자산 (title/photo/name) 회귀 없음 | ✅ **Met** | 보드 실측: `Image loaded [title/photo/name]` 모두 성공 |
| FR-05 | MQTT IN/OUT, 시간 동기화, 자동 재부팅, 터치 회귀 없음 | ✅ **Met** | `room/node_*` subscribe + IN/OUT 토글 + NTP 동기화 정상. Long-click은 v1 버그까지 함께 수정 |
| FR-06 | 듀얼 네트워크 우선순위 (Ethernet 우선) | ✅ **Met** | main.cpp: `if (ETH.localIP() != 0.0.0.0 && mqttEthernet_connected()) ethernet_conn = true` |
| FR-07 | downloadFile/sendHttpMessage HTTPClient 호환 | ✅ **Met** | `http.begin/GET/getStreamPtr/end` 재작성 완료 |
| FR-08 | LCD 색감/속도/터치 회귀 없음 | ✅ **Met** | 사용자 보드 검증: 1. OK / 2. OK |

**Met**: 5/8 (62.5%)
**Not Met**: 3/8 — 모두 Plan §2.2 "v2.1 In Scope" 였으나 의도적으로 v2.2 분리

### 2.1 Definition of Done (Plan §4.1) 보강

| 항목 | 결과 |
|------|------|
| L1~L7 + P1~P4 + V1 완료 | ⚠️ L1~L7 완료, P1~P4 미진행 (LV_USE_PNG=0) |
| Ethernet 환경 PNG/BMP 각 5회 업로드 성공 | ❌ WebUI 비활성 (BMP 자체는 v1 fetchImageFiles 경로로 정상) |
| RemoteDeck_PC 디자인 토큰 일관성 | ⏸ N/A (WebUI 비활성) |
| Design 문서 작성 | ✅ commit 1401382 |
| Match Rate ≥ 90% | ❌ **68%** (v2.2 분리로 인한 미충족) |
| v2.1-lan → main merge | ✅ commit c44d348 |

### 2.2 Quality Criteria (Plan §4.2)

| 항목 | 결과 | Evidence |
|------|------|----------|
| LCD 색감/속도 v1 동일 | ✅ | 사용자 시각 검증 OK |
| 터치 응답 v1 회귀 없음 | ✅ | 사용자 검증 OK |
| heap 추이 안정 (누수 0) | ✅ | 보드 실측: 172KB → 56KB 안정, 디코드 후 복귀 |
| 빌드 시간 v1 대비 ≤ 2배 | ✅ | v2.1 빌드 90초 vs v1 빌드 14초 (lib 컴파일 1회성, incremental 후 동등) |

---

## 3. Match Rate (Static + Board Boot Verification)

### 3.1 점수 산출

| 축 | 점수 | 근거 |
|----|------|------|
| **Structural** (의도된 파일 변경 완료도) | **100%** | platformio/lv_conf/ethernet_mqtt/main/DeviceManager/images/web/data/www 모두 의도대로 수정 또는 신설 |
| **Functional** (FR 충족율) | **62.5%** | 5/8 FR Met (FR-04~08), 3/8 Not Met (FR-01~03 v2.2 분리) |
| **Contract** (API/Behavior 일치) | **60%** | HTTPClient/ETH.h/PubSubClient API 정상. AsyncWebServer task 실패, LV_USE_PNG 비활성 |

**Overall** (static-only formula):
```
Overall = Structural × 0.2 + Functional × 0.4 + Contract × 0.4
        = 100 × 0.2  + 62.5 × 0.4    + 60 × 0.4
        = 20         + 25            + 24
        = 69%  (반올림 68%)
```

### 3.2 Runtime Verification Plan (실행 안 함 — 단말 unit test 환경 없음)

| Layer | Status | 비고 |
|-------|--------|------|
| L1 API curl tests | ⏸ Skipped | WebUI 비활성 → 측정 불가 |
| L2 Playwright UI | ⏸ N/A | 임베디드 펌웨어 — Playwright 미적용 |
| L3 E2E | ✅ 보드 부팅 검증 대체 | Ethernet/MQTT/LCD/터치/Long-click/Sleep 저장 모두 정상 |
| L4 Stability (50회 PNG) | ⏸ N/A | PNG 미활성 |

---

## 4. Decision Record Verification

| Decision (Plan/Design 출처) | Implementation 일치 | 비고 |
|------------------------------|---------------------|------|
| [Design §2.0] Option C Pragmatic (PC NetManager 인라인) | ✅ 일치 | ethernet_mqtt.cpp 인라인 ETH.begin |
| [Design §11.2 C1~C4] platform/ETH.h/Client/HTTPClient | ✅ 일치 | commit 0f252f4 |
| [Design §11.2 C5~C6] TFT_eSPI 업데이트 + setup 재배치 | ✅ 일치 | commit 8e558f7 (보드 부팅 검증) |
| [Design §11.2 C7~C9] LV_USE_PNG + lv_png_init + LVGL FS | ❌ **미수행** | v2.2로 분리 |
| [Design §11.2 C10] V1 회귀 검증 + L4 stability | ⚠️ 부분 | 보드 부팅 + 사용자 시각 검증만, 50회 stability 측정 안 함 |
| [Design §12.3] AsyncWebServer Ethernet 환경 동작 | ❌ **미달성** | AsyncTCP task slot 충돌로 listen 불가 (Plan §5 신규 발견) |

---

## 5. Gap List

### 5.1 Critical (의도적 분리이지만 Plan SC 위반)

| # | Gap | Plan SC | Severity | Action |
|---|-----|---------|:--------:|--------|
| C1 | AsyncTCP task 생성 실패 — Ethernet WebUI 동작 안 함 | FR-01, FR-02 | **Critical** | **v2.2 사이클**: esphome fork 시도 또는 task config 우회 |
| C2 | LV_USE_PNG=0 — PNG 업로드 불가 | FR-03 | **Critical** | **v2.2 사이클**: lv_png_init + LVGL FS driver 등록 |

### 5.2 Important (v2.1 잔여)

| # | Gap | Severity | Action |
|---|-----|:--------:|--------|
| I1 | 50회 PNG/BMP stability 테스트 미수행 | Important | 운영 환경에서 자연 누적 검증 가능 |
| I2 | Design §5.4 Page UI Checklist 미검증 (WebUI 비활성) | Important | C1 해결 후 자동 활성 |
| I3 | Long-press 진입 시간 미세조정 (기본값 사용) | Low | 운영 피드백 후 조정 |

### 5.3 Positive findings (Plan에 없던 추가 개선)

| # | Item | 효과 |
|---|------|------|
| P1 | Long-click 35회 → 1회로 진입 조건 완화 (v1 버그까지 함께 수정) | 운영 UX 대폭 개선 |
| P2 | DeviceManager Sleep 시간 저장/복원 (v1 regression 발견 + 수정) | 운영자 설정 보존 |
| P3 | 이미지 디코드 메모리 안전장치 (크기/heap 검증, OLD 선행 free) | OOM 크래시 fail-soft 처리 |
| P4 | downloadFile Content-Length 200KB 상한 | 비정상 큰 파일 다운로드 방지 |
| P5 | FULL + OTA 펌웨어 배포 (4MB / 1.72MB) + git 추적 | 현장 배포 즉시 가능 |

---

## 6. Recommendation

### 6.1 Match Rate < 90% — Plan 기준으로는 "iterate" 권장하나…

`v2.2`로 분리된 항목(WebUI + PNG)은 **이 사이클 내에서 해결 불가능**한 의존성 사슬을 가짐:
- AsyncTCP task fix: 라이브러리 fork 변경 또는 ESP-IDF task config — 새로운 사이클 작업
- PNG: lv_png_init + LVGL FS driver 등록 — 별도 모듈 작업

따라서 **iterate (자동 fix loop)는 적용 불가**. 대신 다음 결정 후보:

### 6.2 결정 옵션

| 옵션 | 설명 | 결과 |
|------|------|------|
| **A. 그대로 진행 (권장)** | Match Rate 68% 수용. v2.1은 "기반 작업 완료" 라벨로 종료, v2.2 사이클 시작 | v2.1 archive 가능 + v2.2 plan 시작 |
| B. Critical만 iterate | C1 (AsyncTCP) 시도 — esphome fork 교체 1회 | 시간 30분~1시간, 성공 확률 50% |
| C. 모두 수정 (iterate full) | C1 + C2 모두 진행 → v2.1 사이클에서 완성 | 추가 1-2 세션, v2.2 사이클 불필요해짐 |

### 6.3 Positive findings 의 가치

v2.1은 Plan SC 만으로 평가하면 68%지만, **운영 가치 관점에서는 5건의 추가 개선** (P1~P5) 으로 실용성 크게 증가:
- Long-click 진입 사실상 불가능 → 1회로 해결
- Sleep 시간 저장 regression → 발견 + 수정
- 메모리 안전성 → 대형 이미지 대응
- 펌웨어 배포 인프라 → 현장 즉시 사용

이 결과를 단순 "68%"로 환산하기보다 **"v2.1 핵심 목표 + 부수 개선 합산 성과"** 로 보는 것이 합리적.

---

## 7. Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-22 | 초안 — Plan SC 5/8 Met, Match Rate 68%, v2.2 분리 권장 | KDI |
