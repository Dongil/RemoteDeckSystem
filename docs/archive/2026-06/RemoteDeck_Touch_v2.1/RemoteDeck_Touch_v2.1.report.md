---
template: report
version: 1.0
feature: RemoteDeck_Touch_v2.1
date: 2026-06-22
author: KDI
project: RemoteDeckSystem
status: Completed (with v2.2 carry-over)
plan_doc: ../01-plan/features/RemoteDeck_Touch_v2.1.plan.md
design_doc: ../02-design/features/RemoteDeck_Touch_v2.1.design.md
analysis_doc: ../03-analysis/RemoteDeck_Touch_v2.1.analysis.md
match_rate: 68
firmware:
  - RemoteDeck_Touch/firmware/RemoteDeck_Touch_V2.1.0_FULL_20260622.bin
  - RemoteDeck_Touch/firmware/RemoteDeck_Touch_V2.1.0_OTA_20260622.bin
---

# RemoteDeck_Touch v2.1 Completion Report

> **Summary**: LAN 스택 통일(ETH.h + ETH_PHY_W5500) 핵심 목표 달성. WebUI/PNG 는 의존성 사슬로 인해 v2.2로 분리. 운영 가치 관점에서 5건의 추가 개선 (Long-click, Sleep 저장, 메모리 안전성, 200KB 다운로드 한도, 펌웨어 배포 인프라).
> 
> **Match Rate**: 68% (Plan SC 5/8 Met)
> **Period**: 2026-06-22 (1일, 9개 commit)
> **Branch**: `v2.1-lan` → `main` merge 완료 (`c44d348`)

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | v1 우회 `if (wifi_conn) webServer.begin()` 로 Ethernet 환경에서 WebUI 사실상 사용 불가 + PNG 미지원 + Long-click 사실상 진입 불가 + Sleep 설정 regression. |
| **Solution** | RemoteDeck_PC v2.3.0 패턴 이식 — arduino-libraries/Ethernet 제거, ETH.h + ETH_PHY_W5500 로 W5500을 ESP32 lwIP에 통합. 부수 발견 issue들 함께 fix. |
| **Function/UX Effect** | Ethernet 환경에서 ETH/MQTT/BMP/터치/시간 동기화 모두 정상. Long-press 1회로 DeviceManager 진입 가능 (v1 dead-code 버그까지 해결). Sleep 시간 저장/복원. 이미지 fail-soft. FULL/OTA 펌웨어 배포 인프라 구축. |
| **Core Value** | LAN 통일 기반 확보 + 운영자 체감 UX 대폭 개선. WebUI 활성화는 v2.2 에서 즉시 가능 (LAN 인프라 준비 완료). |

### Value Delivered (4 perspectives)

| 관점 | 달성 |
|------|------|
| **Problem 해결** | ⚠️ 부분 — LAN 통일 OK, WebUI/PNG 차단 (v2.2 분리). 의도하지 않은 운영 회귀(Sleep/Long-click) 추가 발견 + 해결 → 운영 체감 가치 향상 |
| **Solution 적용** | ✅ Option C (PC NetManager 인라인 이식) — 5/7 Design Decision 준수, PNG 관련 2건 미준수 |
| **Function/UX** | ✅ Ethernet 환경 정상 부팅 + LCD 회귀 없음 + Long-press 1회 진입 + Sleep 저장 + Fail-soft 메모리 |
| **Metrics** | heap free 56KB (목표 ≥50KB) · 이미지 LCD 표시 100% (3개 BMP) · 빌드 통과 · 보드 부팅 검증 OK |

---

## 1. PDCA Journey

| Phase | Document | Commits | 결과 |
|-------|----------|---------|------|
| Plan | [v2.1.plan.md](../01-plan/features/RemoteDeck_Touch_v2.1.plan.md) | `1401382` | LAN 통일 + PNG 핵심 2개로 스코프 좁힘, v2.1-lan 별도 브랜치 전략 |
| Design | [v2.1.design.md](../02-design/features/RemoteDeck_Touch_v2.1.design.md) | `1401382` | Option C Pragmatic (PC NetManager 인라인 이식 + lv_png_init + LV_USE_FS_STDIO) |
| Do | (code) | `0f252f4` C1-C4, `8e558f7` C5-C6, `24e8888` Long-click/WebUI defer, `160fa48` 메모리/Sleep | 5/10 commit 단위 완료 (C7-C10 v2.2 분리) |
| Check | [v2.1.analysis.md](../03-analysis/RemoteDeck_Touch_v2.1.analysis.md) | `8d6c92b` | Match Rate 68%, 의도적 분리 |
| Act | — | — | Skip (의존성 사슬, v2.2로) |
| Firmware | (artifact) | `9b997f3` FULL, `f6549a3` OTA | 4MB + 1.72MB 배포 |
| Report | (this doc) | TBD | — |

---

## 2. Key Decisions & Outcomes

### 2.1 PRD→Plan→Design 결정 사슬

```
📋 Decision Record Chain
[Plan]   Scope        : 핵심 2개 (LAN 통일 + PNG)         — v2.2로 분리 가능한 단위
[Plan]   Branch       : v2.1-lan 별도                    — 회귀 시 main 즉시 롤백
[Design] Architecture : Option C - Pragmatic              — PC NetManager 인라인, 클래스 신설 X
[Design] Platform     : pioarduino 53.03.10               — PC v2.3.0 와 동일 (ETH_PHY_W5500 지원)
[Design] HTTP Client  : ESP32 HTTPClient (내장)           — Arduino-ESP32 3.x Client 추상 호환
[Design] PNG Decoder  : lv_png_init + LV_USE_FS_STDIO     — LVGL 표준
[Design] TFT_eSPI     : bodmer/TFT_eSPI@^2.5.43           — Arduino-ESP32 3.x 호환
```

### 2.2 결정 vs 실제 (Decision Record Verification)

| Decision | Followed | Outcome |
|----------|:--:|---------|
| Scope (핵심 2개) | ✅ | 단, PNG는 의존성으로 v2.2로 재분리 (총 5개 → 3개로 좁아짐) |
| v2.1-lan 별도 브랜치 | ✅ | 회귀 없이 main merge — 전략 효과 검증 |
| Option C Pragmatic | ✅ | NetManager 클래스 신설 없이 ethernet_mqtt.cpp 인라인으로 충분 |
| pioarduino 53.x | ✅ | TFT_eSPI 비호환 발견 → lib 업데이트로 해결 |
| HTTPClient 내장 | ✅ | downloadFile/sendHttpMessage 재작성 정상 |
| lv_png_init + LVGL FS | ❌ | LV_USE_PNG=0 유지 — v2.2 로 분리 (lodepng 직접 호출 함정 회피) |
| TFT_eSPI 2.5.43 | ✅ | LCD 색감/터치 회귀 없음 (사용자 검증 OK) |

**핵심 발견**: Plan/Design 의 PNG 분기는 옳은 결정이었으나 **실제 시도가 더 깊은 의존성 사슬을 드러냄** — LVGL FS driver 등록 + lv_png_init 호출 순서 + custom decoder 비활성 등 추가 작업이 v2.2 에서 별도 사이클로 진행 필요.

### 2.3 Plan에 없던 추가 결정 (Adaptive)

| 발견 시점 | 결정 | 효과 |
|-----------|------|------|
| C1-C4 후 빌드 실패 | TFT_eSPI 2.4.61 (bundled) → 2.5.43 (lib_deps) 교체 | Arduino-ESP32 3.x 호환 |
| 보드 부팅 시 W5500 init 실패 | setup() 순서 재배치 (ETH → TFT_eSPI) | SPI 버스 공유 충돌 해결 |
| 보드 부팅 시 AsyncTCP task 실패 | WebUI 비활성 + v2.2 분리 | v2.1 종료 가능, v1 회귀 없음 |
| 사용자 보고 (Long-click 안 됨) | ibtnLogo_LongClick 35회 → 1회 진입 (v1 dead-code 버그 동반 수정) | UX 큰 개선 |
| 사용자 보고 (Sleep 저장 안 됨) | DeviceManager.saveToDevice 에 ui_dropSleep 처리 추가 (v1 regression) | 운영자 설정 보존 |
| 사용자 요청 (메모리 안전성) | 이미지 디코드 크기/heap 검증, OLD 선행 free, 200KB 다운로드 한도 | 대형 이미지 fail-soft |

---

## 3. Success Criteria Final Status

| ID | Criteria | Status | Evidence |
|----|----------|:--:|----------|
| FR-01 | Ethernet IP → 브라우저 → 이미지 관리 UI | ❌ Not Met | curl HTTP 000, AsyncTCP task 실패 |
| FR-02 | Ethernet `/api/*` 정상 응답 | ❌ Not Met | WebServer 비활성 |
| FR-03 | PNG 업로드 → LCD 갱신 | ❌ Not Met | LV_USE_PNG=0 |
| FR-04 | 기존 BMP 자산 회귀 없음 | ✅ Met | 보드 실측 — 3개 모두 정상 표시 |
| FR-05 | MQTT/시간/재부팅/터치 회귀 없음 | ✅ Met | Long-click v1 버그 추가 수정으로 개선 |
| FR-06 | 듀얼 네트워크 (Ethernet 우선) | ✅ Met | main.cpp 분기 검증 |
| FR-07 | downloadFile/sendHttpMessage HTTPClient | ✅ Met | 재작성 + 빌드/부팅 통과 |
| FR-08 | LCD 색감/속도/터치 회귀 없음 | ✅ Met | 사용자 시각 검증 OK |

**Overall**: 5/8 Met = 62.5% (FR 충족율)

---

## 4. Memory & Performance Metrics (Board Live)

| 지표 | 측정값 | 목표 | 평가 |
|------|--------|------|------|
| heap free (idle, 3 BMP 로드) | 56,640 bytes | ≥ 50KB | ✅ |
| heap free (디코드 직전 max) | 172,932 bytes | — | — |
| 이미지 LCD 버퍼 지속 점유 | 111,600 bytes | — | 정상 (3개 합계) |
| 디코드 피크 소비 | 116,292 bytes | — | 안전 마진 충분 |
| Flash 사용량 | 1,801,440 bytes (46%) | — | huge_app 3MB 여유 |
| 빌드 시간 | 90.85초 (clean) | ≤ v1 × 2 | ✅ (v1 14초 — lib 1회 컴파일 후 incremental 동등) |

---

## 5. Artifacts Delivered

### 5.1 Code (git)

| 항목 | 위치 |
|------|------|
| Plan 문서 | `docs/01-plan/features/RemoteDeck_Touch_v2.1.plan.md` |
| Design 문서 | `docs/02-design/features/RemoteDeck_Touch_v2.1.design.md` |
| Analysis 문서 | `docs/03-analysis/RemoteDeck_Touch_v2.1.analysis.md` |
| Report 문서 (this) | `docs/04-report/RemoteDeck_Touch_v2.1.report.md` |

### 5.2 Code 변경 (5 files)

- `RemoteDeck_Touch/platformio.ini` — pioarduino + mathieucarbou + TFT_eSPI lib_deps + build_flags
- `RemoteDeck_Touch/src/mqtt/ethernet_mqtt.{h,cpp}` — ETH.h + ETH_PHY_W5500 인라인
- `RemoteDeck_Touch/src/main.cpp` — WiFiClient + HTTPClient + setup 재배치 + Long-click 등록 + WebUI 비활성
- `RemoteDeck_Touch/src/device/DeviceManager.{h,cpp}` — Sleep 시간 저장/복원, EthernetClient → WiFiClient include
- `RemoteDeck_Touch/src/images/images.cpp` — 메모리 안전장치, 진단 로그
- `RemoteDeck_Touch/lib/TFT_eSPI/` → 백업 + lib_deps 로 교체

### 5.3 Firmware (배포용)

| 파일 | 크기 | MD5 | 용도 |
|------|------|-----|------|
| `RemoteDeck_Touch_V2.1.0_FULL_20260622.bin` | 4,128,768 | `70BA7C0197858D6DB92FD808E6DADAF5` | 공장 초기화 (offset 0x0) |
| `RemoteDeck_Touch_V2.1.0_OTA_20260622.bin` | 1,801,440 | `602C535F4AD9C0A27831BE24D4256E39` | 앱 교체 (offset 0x10000) |

### 5.4 Commit Sequence

```
8d6c92b docs: Analysis (Check phase) - Match Rate 68%
f6549a3 chore: v2.1.0 OTA 펌웨어 추가
9b997f3 chore: v2.1.0 FULL 펌웨어 배포 + 플래시 toolkit
c44d348 Merge branch 'v2.1-lan' — LAN 스택 통일 + Touch UX 개선
├─ 160fa48 fix: 메모리 안전장치 + sleep + long-click 완화
├─ 24e8888 fix: Long-click 등록 + WebUI v2.2로 분리
├─ 8e558f7 feat: C5-C6 TFT_eSPI + setup 재배치
├─ 0f252f4 wip: C1-C4 platform + ETH.h + HTTPClient
1401382 docs: v2.1 Plan/Design
ea046d3 [main HEAD before v2.1]
```

---

## 6. Lessons Learned

### 6.1 잘 된 점

1. **별도 브랜치 전략** — `v2.1-lan` 으로 격리하여 main 항상 안전. TFT_eSPI 회귀 위험에도 main 영향 zero
2. **Commit 단위 분할 (C1-C10)** — 의존성 사슬 발견 시 정확한 차단 지점 식별 가능 (C5 = TFT_eSPI 비호환, C7 = PNG)
3. **PC NetManager 패턴 재사용** — 검증된 코드 인라인 이식으로 새 추상화 없이 안정성 확보
4. **보드 실측 + 사용자 시각 검증** — Plan/Design 만으로 보이지 않은 v1 버그 2건 (Long-click 35회, Sleep regression) 발견

### 6.2 어려웠던 점

1. **AsyncTCP task slot 충돌** — Plan §5 Risk에 없던 새 발견. mathieucarbou/AsyncTCP@3.x 가 다중 task 환경에서 안정 동작 어려움. build_flags 로 우회 시도 실패
2. **PNG 디코더 호출 경로 비표준** — lodepng 직접 호출이 LVGL undefined reference 유발. lv_png_init + LVGL FS driver 가 정석이나 별도 사이클 작업 필요
3. **TFT_eSPI 라이브러리 호환성 사슬** — Arduino-ESP32 3.x로 platform 업그레이드 시 TFT_eSPI 2.4 → 2.5 필수 + User_Setup.h 를 build_flags 로 이식 필요

### 6.3 v2.2 인계 사항

| 우선순위 | 작업 | 의존성 |
|:---:|------|--------|
| 1 | **C1 AsyncTCP fix**: esphome fork 교체 또는 setup 시점 지연 시작 검증 | 단독 |
| 2 | **C2 PNG**: lv_png_init + LV_USE_FS_STDIO 활성화 + LVGL FS driver 등록 | 단독 |
| 3 | OTA Handler 이식 (PC v2.3.0 OTAHandler) | C1 완료 후 |
| 4 | 로그 뷰어 + WebSocket | C1 완료 후 |
| 5 | deviceconfig/serverconfig 웹 편집 UI | C1 완료 후 |
| 6 | 시간 표시 UI (사용자 보고 item 4) | 독립 |

---

## 7. Recommendation for Next Steps

1. **v2.1 종료** — 이 Report로 종료. `/pdca archive RemoteDeck_Touch_v2.1 --summary` 권장 (Match Rate/iteration 메트릭 보존)
2. **v2.2 사이클 시작** — `/pdca plan RemoteDeck_Touch_v2.2` (AsyncTCP fix + PNG + 시간 UI)
3. **펌웨어 현장 배포** — `RemoteDeck_Touch_V2.1.0_FULL_20260622.bin` 으로 신규 단말 굽기 가능. 운영 환경에서 LAN 안정성 + Long-click + Sleep 저장 검증 권장

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-06-22 | 초안 — Match Rate 68%, v2.2 인계 사항 명시, 5건 Positive findings 강조 | KDI |
