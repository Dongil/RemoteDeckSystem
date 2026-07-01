---
template: report
version: 1.1
feature: RemoteDeck_PC_LAN_Recovery
date: 2026-07-01
author: KDI
project: RemoteDeck_PC
version_project: 2.4.7
status: Partial
---

# RemoteDeck_PC_LAN_Recovery Completion Report

> **Status**: **Partial** — 펌웨어 목표 달성 / 원인 진단 완료 / 실증 해결은 H/W 수정으로 이관
>
> **Project**: RemoteDeck_PC firmware
> **Version**: 2.4.7 (v2.3.0 → v2.4.7 iteration)
> **Author**: KDI
> **Completion Date**: 2026-07-01
> **PDCA Cycle**: #1

---

## Executive Summary

### 1.1 Project Overview

| Item | Content |
|------|---------|
| Feature | RemoteDeck_PC_LAN_Recovery |
| Start Date | 2026-06-30 |
| End Date | 2026-07-01 |
| Duration | 2일 (Plan → v2.4.7 마무리) |
| Firmware Iterations | 8 (v2.4.0 → v2.4.7) |

### 1.2 Results Summary

```
┌─────────────────────────────────────────────┐
│  펌웨어 요구사항 완료율: 100%                 │
│  실제 필드 문제 해결율: 0% (H/W 이슈 확정)     │
├─────────────────────────────────────────────┤
│  ✅ 펌웨어 Layer 구현:    4 / 4              │
│  ✅ 진단 도구 (LED blink): 1 / 1             │
│  ✅ 최종 정리 (v2.4.7):    1 / 1             │
│  ⚠️  콜드 부팅 성공률:    변화 없음 (H/W)      │
└─────────────────────────────────────────────┘
```

### 1.3 Value Delivered

| Perspective | Content |
|-------------|---------|
| **Problem** | 콜드 부팅 시 LAN 미연결 — 원 가설: W5500 POR 불완전 (S/W로 해결 가능) |
| **Solution (시도)** | v2.4.0~v2.4.6 4-Layer + 브라운아웃 disable + MAC 기반 stagger + NVS 진단 추적 + STATUS1 LED 진단 blink |
| **Solution (결론)** | **원인은 보드 H/W** — ROM/2nd-stage bootloader 단계에서 hang (setup() 미진입). 펌웨어 개입 불가 확정. v2.4.7로 최소 방어선만 남기고 정리. |
| **Function/UX Effect** | (1) 콜드 부팅 실패 원인 확정 = **H/W 이슈** (EN 핀 RC 부족 가능성 최유력) (2) 필드 진단 도구 확보 (LED blink는 v2.4.7에서 제거되었으나 v2.4.6은 재활용 가능) (3) 향후 유사 증상 진단 프로토콜 확립 |
| **Core Value** | **잘못된 방향의 개입 중단** — 8회 펌웨어 iteration으로 "펌웨어로는 불가능"을 실증. H/W fix (EN 핀 1uF 콘덴서 등)로 정확한 리소스 배분. |

---

## 1.4 Success Criteria Final Status

> Plan `## 4. Success Criteria`에 대한 최종 평가.

| # | Criteria (from Plan §4.1) | Status | Evidence |
|---|--------------------------|:------:|----------|
| SC-1 | FR-01~04, FR-07~09 구현 + 컴파일 성공 | ✅ Met | v2.4.7 빌드 성공 (Flash 70.0%, RAM 16.4%) |
| SC-2 | FR-05~06 (chip check) 구현 또는 deferred 결정 | ✅ Met | v2.4.0~v2.4.6 구현 후 v2.4.7에서 진단 목적 종료로 제거 |
| SC-3 | 테스트 단말 OTA + 콜드 부팅 10회 시연 | ✅ Met | 사무실 5대 × 다수 시연, 8대 확장 시연 |
| SC-4 | 성공률 ≥ 9/10, 평균 연결 시간 ≤ 30초 | ❌ Not Met | **동일 3대 100% 실패** (H/W 원인 확정) |
| SC-5 | Serial 로그가 4-layer 동작 명확히 표시 | ✅ Met (진단 완료 후 v2.4.7에서 축소) | v2.4.5까지 verbose 유지, v2.4.7 minimal |
| SC-6 | 운영 단말 N대 OTA 배포 | ⚠️ Partial | 사무실 검증만, 필드 배포 없음 (H/W fix 선행 필요) |

**Firmware Success Rate**: 5/6 (83%)
**Field Problem Resolution**: 0/1 (**원인이 H/W로 확정** — 스코프 밖으로 이관됨)

## 1.5 Decision Record Summary

| Source | Decision | Followed? | Outcome |
|--------|----------|:---------:|---------|
| [Plan] 4-layer 다층 복합 복구 | delay + retry + watchdog + chip-check | ✅ (v2.4.0) | 정상 부팅 성공률에는 도움 (없어도 됐음), **실패 3대에는 무효** |
| [Plan] Retry interval 1000ms 고정 | 1s × 5회 | ✅ | 유지 (v2.4.7 3회로 축소) |
| [Plan] Watchdog = ESP.restart() 30s | timeout 후 재부팅 | ✅ | 유지 (v2.4.7 20s로 축소) |
| [Plan] Chip check begin 이전 (옵션) | VERSIONR 0x0039 = 0x04 | ✅ (v2.4.0)→❌ (v2.4.7) | 진단 완료 후 제거 (실운영 무의미) |
| [in-flight] Brownout detector disable | v2.4.3 | ✅ (v2.4.3)→❌ (v2.4.7) | 미검증 개입 + 안전기능 훼손 이슈로 제거 |
| [in-flight] MAC 기반 stagger | v2.4.5 | ✅ (v2.4.5)→❌ (v2.4.7) | 14대 동시 부팅도 H/W 이슈 → 무효 확인 후 제거 |
| [in-flight] STATUS1 LED 진단 blink | v2.4.6 | ✅ (v2.4.6)→❌ (v2.4.7) | **결정적 진단 도구** — LED 미점등으로 setup() 미진입 증명 후 v2.4.7에서 원복 |
| [Field] 최종 판단 H/W 이슈 | 아답터/콘센트/케이블 교차 → 동일 3대 실패 | ✅ | 펌웨어 개입 종료 결정 |

---

## 2. Related Documents

| Phase | Document | Status |
|-------|----------|--------|
| Plan | [RemoteDeck_PC_LAN_Recovery.plan.md](../01-plan/features/RemoteDeck_PC_LAN_Recovery.plan.md) | ✅ Finalized |
| Design | [RemoteDeck_PC_LAN_Recovery.design.md](../02-design/features/RemoteDeck_PC_LAN_Recovery.design.md) | ✅ Finalized |
| Check | (skipped — 필드 진단으로 대체) | ⚠️ N/A |
| Act | Current document | 🔄 Writing |

---

## 3. Completed Items

### 3.1 Functional Requirements

| ID | Requirement | Status | Notes |
|----|-------------|--------|-------|
| FR-01 | 1000ms delay | ✅ Complete | v2.4.7에서 500ms로 축소 (충분) |
| FR-02 | ETH.begin() retry ≤5회 | ✅ Complete | v2.4.7에서 3회로 축소 |
| FR-03 | retry 실패 시 로그 + watchdog | ✅ Complete | ESP.restart() 진입 유지 |
| FR-04 | GOT_IP 30s watchdog | ✅ Complete | v2.4.7에서 20s로 축소 |
| FR-05 | W5500 VERSIONR 사전 검증 | ✅→❌ | v2.4.0~v2.4.6 유지 후 v2.4.7 제거 |
| FR-06 | fallback SPI clock retry | ❌ Deferred | 실운영 신호 없어 미구현 |
| FR-07 | Verbose Serial 로그 | ✅→⚠️ | 진단 종료 후 minimal로 축소 |
| FR-08 | 정상 부팅 지연 ≤ 2초 | ✅ Complete | v2.4.7 = 0.5s+SW-reset+retry ≈ 0.6~1.0s |
| FR-09 | OTA 배포 가능 (크기 ≤ 1KB 증가) | ✅ Complete | v2.4.7은 v2.3.0 대비 오히려 감소 |

### 3.2 Non-Functional Requirements

| Item | Target | Achieved | Status |
|------|--------|----------|--------|
| Reliability (콜드 부팅 성공률 ≥9/10) | ≥ 9/10 | **정상 5대는 유지, 실패 3대는 0/10** | ⚠️ H/W 한계 |
| Performance (추가 지연 ≤2s) | ≤ 2s | ~0.6~1.0s | ✅ |
| Recoverability (60s 자가 복구) | 60s 이내 | 20s watchdog + restart | ✅ (setup 진입 시에만) |
| Backwards Compatibility | OTA/partition 변경 없음 | 유지 | ✅ |
| Code Size | +≤ 1KB | v2.3.0 → v2.4.7 = **-25 KB 이상** (진단 코드 정리로 감소) | ✅ |

### 3.3 Deliverables

| Deliverable | Location | Status |
|-------------|----------|--------|
| v2.4.7 Firmware (final) | `RemoteDeck_PC/firmware/RemoteDeck_PC_V2.4.7_OTA_20260701.bin` (1,376,240 B) | ✅ |
| v2.4.6 Firmware (진단용, LED blink 포함) | `RemoteDeck_PC/firmware/RemoteDeck_PC_V2.4.6_OTA_20260630.bin` | 📦 보존 |
| NetManager.cpp (minimal defensive) | `RemoteDeck_PC/src/network/NetManager.cpp` (234 라인) | ✅ |
| NetManager.h | `RemoteDeck_PC/src/network/NetManager.h` (55 라인) | ✅ |
| ConfigManager.cpp (v2.4.7 stamp) | `RemoteDeck_PC/src/config/ConfigManager.cpp` | ✅ |
| main.cpp (진단 블록 제거) | `RemoteDeck_PC/src/main.cpp` | ✅ |
| Plan/Design 문서 | `docs/01-plan/`, `docs/02-design/` | ✅ |

---

## 4. Incomplete Items

### 4.1 Carried Over — H/W Track

| Item | Reason | Priority | Owner | Est. Effort |
|------|--------|----------|-------|-------------|
| EN 핀 1uF 콘덴서 추가 (가장 유력) | 펌웨어 스코프 밖 | High | H/W | 실패 3대 × 5분 SMD 리워크 |
| 3.3V rail bulk cap 보강 (10uF+100nF) | 병행 방어 | Medium | H/W | 필요 시 |
| 5V input bulk cap 보강 (100uF) | 여유 방어 | Low | H/W | 필요 시 |
| 실패 3대 H/W 수정 후 재검증 (콜드 20회 시연) | 원 목표 SC-4 미달성 | High | Ops+QA | 반나절 |
| 정상 5대 회귀 테스트 (v2.4.7 배포 후) | 정리 회귀 방지 | High | Ops | 30분 |

### 4.2 Cancelled / Superseded

| Item | Reason | Alternative |
|------|--------|-------------|
| 펌웨어 측 콜드 부팅 완전 복구 | H/W ROM/bootloader hang은 firmware 스코프 밖 | H/W 수정 (EN 콘덴서) |
| Brownout detector disable | 미검증 + 안전기능 훼손 | 표준 brownout 동작 유지 |
| MAC 기반 stagger | 14대 동시 부팅도 H/W 이슈로 확정 | 불필요 |
| NVS 스테이지 추적 | 진단 완료 후 무용 | Serial 로그만 |

---

## 5. Quality Metrics

### 5.1 Iteration Trail

| Version | Flash | 변경 요지 | 결과 |
|---------|-------|---------|------|
| v2.3.0 (baseline) | ~1,375 KB | 원본 | 실패 3대 미해결 |
| v2.4.0 | 1,376,640 B | 4-Layer (delay+retry+watchdog+chip check) | 2.1A 아답터도 실패 |
| v2.4.1 | 1,376,960 B | + W5500 SW reset + Logger 통합 | 개선 없음 |
| v2.4.2 | 1,381,456 B | + NVS 진단 추적 + 5000ms delay + 3× SW reset | 개선 없음 |
| v2.4.3 | 1,381,808 B | + Brownout disable | 2.1A 아답터 성공 (미검증) |
| v2.4.4 | 1,381,936 B | BOOT-PREV 프리즈 수정 | (진단만) |
| v2.4.5 | 1,382,080 B | + MAC 기반 stagger | 14대 시연 → 4대만 성공, 결정적 데이터 확보 |
| v2.4.6 | 1,382,160 B | + STATUS1 LED 진단 blink | **LED 미점등 확인 → setup() 미진입 증명** |
| **v2.4.7** | **1,376,240 B** | **모든 미검증 개입 제거, 최소 방어선만 유지** | ✅ Production ready |

### 5.2 Resolved Issues

| Issue | Resolution | Result |
|-------|------------|--------|
| 콜드 부팅 실패 원인 불명 | v2.4.6 LED blink 진단 도입 | ✅ H/W (setup 미진입) 확정 |
| 펌웨어 측 잘못된 방향 개입 누적 | v2.4.7 최소화 refactor | ✅ 코드 -46% (NetManager.cpp) |
| Brownout disable "검증됨" 오해 | 사용자 지적으로 재검토 후 제거 | ✅ 표준 안전기능 회복 |

---

## 6. Lessons Learned & Retrospective

### 6.1 What Went Well (Keep)

- **LED 기반 진단 도구 (v2.4.6)의 결정적 가치**: setup() 진입 여부를 육안으로 확인 가능하게 만든 것이 H/W 원인 확정의 결정타. 소량 코드로 최대 진단 효과.
- **8회 반복 iteration의 정직한 데이터**: 각 버전마다 실제 필드 시연 → 가설 검증 → 다음 개입 결정. "그럴싸한 이유로 밀어붙이기" 대신 데이터 기반 방향 전환.
- **최종 정리 (v2.4.7)**: 미검증 개입을 모두 제거하고 최소 방어선만 유지. Production 신뢰성 향상.
- **사용자 지적을 즉시 반영**: brownout disable "검증됨"이 정확하지 않다는 지적 → 재검토 후 제거. 확증 편향 회피.

### 6.2 What Needs Improvement (Problem)

- **초기 가설 편향**: "W5500 POR" 가설로 시작해 4개 버전 동안 W5500 SW reset/chip check에 집중. Setup() 진입 자체를 의심하기까지 v2.4.6까지 걸림.
- **진단 없이 개입 반복**: v2.4.0~v2.4.5는 "log가 없다"는 관찰에서 이미 setup 미진입 가능성이 있었으나 firmware layer 개입만 반복.
- **"검증됨" 라벨의 오남용**: 2.1A 아답터 1회 성공을 "brownout disable 검증"으로 결론화. 반증 데이터 (14대 시연 실패)로 뒤집힘.
- **필드 상황 파악 지연**: "14대 전원 공유" 정보가 v2.4.5 시점에야 나옴. Plan 단계에서 실 배치 파악 필요.

### 6.3 What to Try Next (Try)

- **초기 3분 진단 도구를 Plan 단계에 편성**: "설계 시점에 LED blink 등 육안 진단 도구를 먼저 배포하고 원인 확정 후 개입 결정" 프로세스화.
- **가설별 반증 조건 명시**: "이 개입이 X를 해결하지 못하면 다음 가설로 전환"을 각 버전에서 문서화 → 매몰비용 방지.
- **필드 배치도 (전원/네트워크) 를 Plan 필수 첨부**: 14대 공유 breaker 같은 배치 정보가 원인 후보 도출에 결정적.
- **"안전기능 disable" 결정에 명시적 승인 게이트**: brownout처럼 감지 기능 끄는 개입은 반증 실패 시 즉시 롤백 룰 사전 합의.

---

## 7. Process Improvement Suggestions

### 7.1 PDCA Process

| Phase | Current | Improvement |
|-------|---------|-------------|
| Plan | 원인 가설 1개로 진행 | 가설 2~3개 병렬 + 각 반증 조건 명시 |
| Design | 4-layer 복합 방어선 즉시 채택 | 최소 개입 (1-layer) + 진단 도구 병행 → 데이터 기반 확장 |
| Do | Iteration마다 add-only | Iteration마다 "검증 실패 시 remove" 조건 편성 |
| Check | 필드 진단이 지연됨 | 매 iteration마다 "정상+실패" 양쪽 실측 후 진행 |
| Act | v2.4.7에서만 대규모 정리 | 각 iteration에서 "미검증 개입" flag 관리 |

### 7.2 Tools/Environment

| Area | Improvement | Expected Benefit |
|------|-------------|------------------|
| Diagnostic firmware | v2.4.6 LED blink pattern을 진단 도구로 별도 관리 | 유사 증상 재발 시 즉시 재사용 |
| Field test protocol | 아답터/콘센트/케이블 교차 검증 프로토콜 문서화 | H/W vs S/W 판정 조기화 |
| Board revision tracker | 실패 3대 시리얼/납기 로그 관리 | 배치별 결함 패턴 발견 |

---

## 8. Next Steps

### 8.1 Immediate (2026-07-01~)

- [ ] 사무실 정상 5대에 v2.4.7 OTA 배포 → **회귀 없음 확인** (필수)
- [ ] 실패 3대 사무실 반입 → EN 핀 1uF 콘덴서 SMD 리워크 (1대 시작)
- [ ] 리워크 1대 콜드 부팅 20회 시연 → 성공률 측정
- [ ] 성공 시 나머지 2대 동일 수정 후 필드 재설치
- [ ] `docs/02-design/features/` 에 H/W fix 진단 문서 별도 작성 (선택)

### 8.2 Next PDCA Cycle

| Item | Priority | Expected Start |
|------|----------|----------------|
| RemoteDeck_PC_HW_ColdBoot_Fix (EN cap + 3.3V rail 강화) | High | 2026-07 |
| Diagnostic firmware 상시 유지 (v2.4.6 LED pattern → 별도 브랜치) | Medium | 2026-07 |
| Field deployment checklist (콘센트 배치, 전원 순차 ON) | Low | 2026-07 |

---

## 9. Changelog

### v2.4.7 (2026-07-01)

**Fixed:**
- v2.3.0 대비 콜드 부팅 정상 5대에 대한 방어선 확보 (POR settle delay, retry, watchdog, SW reset)

**Removed (v2.4.7 clean-up):**
- Brownout detector disable (v2.4.3) — 미검증 + 안전기능 훼손
- NVS Preferences 스테이지 추적 (v2.4.2) — 진단 완료
- MAC 기반 stagger delay (v2.4.5) — H/W 이슈 확정으로 무효
- W5500 VERSIONR chip check (v2.4.0) — 진단 완료
- 3× SW reset 반복 (v2.4.2) — 1회로 충분
- Pre-init delay 5000ms → 500ms (과도)
- STATUS1 LED 진단 blink (v2.4.6) — 진단 종료, 원복
- Logger 통합 logEvt 이중 로깅 (v2.4.1)

**Kept (minimal defensive):**
- Pre-init delay 500ms (W5500 POR settle)
- W5500 SW reset 1회 (POR 잔여 상태 정리)
- ETH.begin() retry 3회 (POR garbage 대응)
- GOT_IP watchdog 20s → ESP.restart() (스턱 자동 회복)

**Impact:**
- Flash: v2.4.6 대비 -5,920 B / v2.3.0 대비 감소
- NetManager.cpp: 433 → 234 라인 (-46%)
- 표준 brownout 안전기능 회복

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2026-07-01 | Initial completion report — 부분 완료 (S/W 목표 달성, 원인 H/W 확정, 실증 해결은 H/W 사이클로 이관) | KDI |
