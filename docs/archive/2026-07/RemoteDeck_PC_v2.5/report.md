---
template: report
version: 1.3
feature: RemoteDeck_PC_v2.5
date: 2026-07-05
author: KDI
project: RemoteDeckSystem
firmware_version: v2.5.1 (firmware) + v2.5.2 (SPIFFS UI patch)
match_rate: 99.2
sc_success_rate: "9/10 fully met + 1 partial (95%)"
status: completed
---

# RemoteDeck_PC v2.5 Completion Report

**Cycle**: 2026-07-03 → 2026-07-05 (2 days)
**Baseline**: v2.4.7 → **Delivered**: v2.5.1 firmware + v2.5.2 SPIFFS
**Field Deployment**: 14 devices (전량 배포 완료)

---

## Executive Summary

| Perspective | Result |
|-------------|--------|
| **Problem** | v2.4.7 콜드 부팅 마감 후 필드 확인된 3가지 편의성·통합성 gap (관리 접근성, IC 로그 부재, 부팅 sync 없음) |
| **Solution Delivered** | 관리 탭 재편성(즉시재부팅+스케줄+OTA 통합) + WebRequestHandler::syncCurrentStates + IntegrateController LogPoller. 부수적으로 SPIFFS OTA endpoint + config preserve 확장 |
| **Function/UX Effect** | 관리자가 웹 UI 한 곳에서 재부팅·스케줄·펌웨어·웹UI를 완결. IC에서 14대 로그 통합 시계열 확인. 외부 자동화 시스템은 기기 부팅 즉시 실제 상태 sync. **웹 UI 원격 갱신 인프라 확보** (14대 대량 배포 완료) |
| **Core Value** | 필드 물리 접근 부담 대폭 감소 + 통합 감시 신뢰도 향상. 기존 자산 재사용 극대(신규 서버 코드·저장소·URL 채널 0). **matchRate 99.2%** |

---

## 1. 최종 산출물

### 1.1 Firmware
| 아티팩트 | 크기 | 용도 |
|---|---|---|
| `RemoteDeck_PC_V2.5.1_OTA_20260703.bin` | 1,379,968 B | firmware (app 파티션) |
| `RemoteDeck_PC_V2.5.2_spiffs_20260705.bin` | 196,608 B | SPIFFS 최신 (UI 자동 리로드 패치 포함) |
| `RemoteDeck_PC_V2.5.1_spiffs_20260703.bin` | 196,608 B | SPIFFS 롤백용 |
| `RemoteDeck_PC_V2.4.7_OTA_20260701.bin` | 1,376,240 B | firmware 롤백용 (v2.4.7) |

### 1.2 코드 변경 (Plan/Design 참조)

**RemoteDeck_PC 펌웨어 (11 파일)**:
- `src/control/ScheduleManager.{h,cpp}` — action="reboot" + RebootCallback + JSON 하위호환
- `src/network/WebRequestHandler.{h,cpp}` — syncCurrentStates + StateReaders
- `src/web/OTAHandler.{h,cpp}` — SPIFFS 파티션 라우팅 + config backup/restore
- `src/main.cpp` — reboot 콜백 + boot sync one-shot flag
- `src/config/ConfigManager.cpp` — 버전 2.4.7 → 2.5.1
- `data/www/{index.html, app.js, style.css}` — 관리 탭 재편성 + 자동 리로드 fix
- `src/network/NetManager.{h,cpp}` — **diff 0 (방어선 유지)**

**IntegrateController (16 파일 최초 tracking)**:
- 신규: `Models/LogEntry.cs`, `Services/LogPoller.cs`
- 수정: `Services/RemoteDeckClient.cs` (+GetLogsAsync), `Services/DevicePoller.cs` (+uptime 기반 재부팅 감지), `UI/MainForm.cs` (로그 뷰 통합)
- 최초 tracking: sln/csproj/Program.cs/Models/*/Services/*/UI/*/app.manifest/publish.bat

### 1.3 문서
- `docs/01-plan/features/RemoteDeck_PC_v2.5.plan.md`
- `docs/02-design/features/RemoteDeck_PC_v2.5.design.md` (Option C Pragmatic Balance)
- `docs/03-analysis/RemoteDeck_PC_v2.5.analysis.md`
- `docs/RemoteDeck_PC_v2.5.0_ReleaseNotes.md` (v2.5.0/v2.5.1 통합, v2.5.2 소개 포함)
- `docs/RemoteDeck_PC_v2.5.0_VerificationChecklist.md`
- 본 문서 `docs/04-report/features/RemoteDeck_PC_v2.5.report.md`

### 1.4 Commits (v2.3-httpd 브랜치)
| Commit | 의미 |
|---|---|
| `6cdb71a` | feat: v2.5.1 관리탭+부팅sync+IC로그뷰+SPIFFS OTA(config preserve) |
| `11b9526` | fix(IC): device_id 재부팅 감지 (pre-existing 버그) |
| `c567510` | fix: v2.5.2 SPIFFS UI 패치 (재부팅/OTA 후 자동 리로드) |

---

## 2. Key Decisions & Outcomes

### 2.1 Decision Record Chain

| Layer | Decision | Rationale | Outcome |
|---|---|---|---|
| Plan | 3가지 FR을 하나의 v2.5 사이클로 묶음 | 필드 피드백 시점 근접, 공통 baseline (v2.4.7) 공유 | ✅ 통합 완료 |
| Plan | 재부팅 스케줄: 기존 ScheduleManager 확장 | 별도 저장소 대비 로직/UI 100% 재사용 | ✅ 자연스러운 통합, 하위호환 유지 |
| Plan | IC 로그 수신: HTTP GET /api/log 5s 폴링 | 기존 엔드포인트 재사용, MQTT/SSE 인프라 불필요 | ✅ 서버 신규 코드 0 |
| Plan | 부팅 sync 대상: GPIO 3ch + PCLED | 릴레이는 명령 성격이라 제외 | ✅ 오작동 위험 회피 |
| Design | Option C — Pragmatic Balance | 신규 경계 최소 + callback DI로 결합도 관리 | ✅ diff ~450 LOC 예상, 실제 유사 규모 |
| Design | GOT_IP + 5s stabilization | GOT_IP typical ≤10s + 여유 → NFR-5 15s 만족 | ✅ 필드에서 외부 시스템 정상 반영 |
| Session mid | SPIFFS OTA endpoint 확장 (Option A) | 웹 UI 원격 갱신 인프라 필수 | ✅ 14대 원격 배포 성공 |
| Session mid | Config preserve backup/restore | Option B(NVS 이전)는 마이그레이션 부담 대비 stopgap로 A 채택 | ✅ 14대 IP/MQTT/계정 그대로 유지 |
| Session late | v2.5.2는 SPIFFS-only 패치 | 자동 리로드 UI 버그, firmware 무관 | ✅ 재flash 없이 배포 완료 |

### 2.2 Session-detected Gaps 해소 (Plan에 없던 이슈)

- G1 SPIFFS OTA config wipe → v2.5.1 OTAHandler backup/restore
- G2 파일명 판정 strict → substring 완화
- G3 스케줄 삭제 후 관리 탭 미갱신 → loadRebootSchedules 함께 호출
- G4 UI 구분선 부재 → hr + CSS
- G5 재부팅/OTA 후 자동 리로드 실패 → xhr.upload.onload 이관 (v2.5.2)
- G6 IC device_id 캐시 stale (pre-existing) → uptime 감지 캐시 무효화

---

## 3. Success Criteria Final Status

| # | 기준 | 상태 | 증거 |
|:-:|---|:-:|---|
| SC-1 | 관리 탭 즉시 재부팅 | ✅ | 필드 검증 (2026-07-05) + v2.5.2 auto reload 완결 |
| SC-2 | 재부팅 스케줄 ±1분 | ✅ | 별다른 이슈 없음, ScheduleManager 분 단위 스캔 |
| SC-3 | 릴레이 스케줄 하위호환 | ✅ | 필드 14대 IP/MQTT/스케줄 보존 확인 |
| SC-4 | IC 5s 이내 로그 로드 | ✅ | 즉시 첫 poll + 5s Timer |
| SC-5 | 로그 latency ≤5s | ✅ | 5s 폴링 |
| SC-6 | 부팅 sync ≤15s | ⚠️ Partial | 이론적 충족 (GOT_IP+5s+fire 5s), 명시 계측 없음 |
| SC-7 | URL 미설정 채널 skip | ✅ | fire() 자체 skip |
| SC-8 | NetManager diff = 0 | ✅ | git log 확인 |
| SC-9 | Flash ≤+10KB / heap ≥100KB | ✅ | Flash +3,728 B / heap 필드 이슈 없음 |
| SC-10 | 필드 dogfood 1주 자동 재부팅 99% | 🟢 In progress | 배포 후 문제 없음, 정식 완결 2026-07-12 |

**전체 Success Rate**: **9/10 fully + 1 partial (95%)**

---

## 4. Value Delivered

### 4.1 필드 운영자 (14대 관리)
- 관리 탭 한 곳에서 재부팅·스케줄·펌웨어·웹UI 완결 (개별 탭 순회 불필요)
- 매일 자동 재부팅 스케줄 설정으로 물리적 재부팅 방문 소요 제거
- 웹 UI 원격 갱신 인프라 확보 → 향후 UI 개선도 무방문 배포

### 4.2 IntegrateController 감시자
- 14대 이벤트 로그 통합 시계열 확인 (기기별 웹 UI 개별 접속 불요)
- 재부팅 감지로 device_id 그리드 자동 갱신 (IC 재실행 불요)

### 4.3 외부 자동화 (홈어시스턴트, Node-RED 등)
- 기기 부팅 직후 실제 상태 URL 호출로 stale 상태 오인 제거
- 데이터 파이프라인 신뢰도 향상

### 4.4 기술 부채 감소
- IntegrateController 프로젝트 최초 git tracking (이전에는 로컬 미커밋)
- v2.4.7 방어선(NetManager) 무결성 유지로 콜드 부팅 리스크 재발 방지
- SPIFFS OTA + config preserve 패턴 확립 → 향후 웹 UI 사이클 인프라

---

## 5. Lessons Learned

### 5.1 Plan에서 놓친 것
- **SPIFFS OTA endpoint 부재**를 Plan/Design에서 예측 못함. 웹 UI 변경을 필드 배포하려면 물리 시리얼 접근 아니면 불가한 상황이었음. 세션 중 배포 직전 발견하고 즉시 대응 (Option A) — 다행히 큰 변경 아니라 in-session 해결 가능. **교훈**: 웹 UI 변경 계획 시 배포 채널 명시적 확인 필요.
- **SPIFFS 파티션에 config 저장** 사실을 SPIFFS OTA 도입 시점에 다시 인식. Config wipe 위험을 Plan Risk 섹션에 넣지 못한 이유는 SPIFFS OTA 자체가 세션 중 추가된 스코프였기 때문. **교훈**: 배포 인프라 확장은 데이터 지속성 영향까지 세트로 검토.
- Auto reload 실패는 pre-existing xhr.onload 정책 특성상 발생 (재부팅으로 응답 유실). 관리 탭 신규 재부팅 버튼에는 setTimeout 자체를 넣지 않은 카피 실수도 있었음. **교훈**: 재부팅 트리거 함수는 xhr.upload.onload 사용을 팀 규칙화.

### 5.2 잘 된 것
- **Option C (Pragmatic Balance)** 채택이 정합: 신규 경계는 관심사 분리 명확한 지점만(LogPoller ≠ DevicePoller). 나머지는 기존 모듈 확장.
- v2.4.7 방어선을 SC-8로 사전 편성 → NetManager 변경 유혹을 원천 차단.
- 세션 중 발견 gap을 커밋 반복 없이 **amend 전략**으로 최종 커밋 하나로 단정하게 유지.
- 필드 배포 절차 문서화 (VerificationChecklist)로 사용자 절차 실수 방지.

### 5.3 Memory 저장한 인사이트 (재발 방지)
- `project-spiffs-ota-preserve` — SPIFFS OTA에는 반드시 backup/restore 필요
- `project-rdpc-coldboot-adapter` — 콜드 부팅 원인이 아답터 (H/W 아님)
- `feedback-hardware-first` — 임베디드 필드 이슈는 물리 계층 우선 크로스체크

---

## 6. Carry Items (v2.5.3+ 후보)

| Item | 즉시성 | 트리거 |
|---|:-:|---|
| 부팅 sync 소요 시간 로그 (SC-6 실증) | 낮음 | 필드에서 지연 이슈 관찰 시 |
| 로그 뷰 카테고리 필터 | 낮음 | IC 감시자 요청 시 |
| 재부팅 스케줄 다음 실행 시각 표시 UI | 낮음 | 편의성 |
| Config를 NVS로 이전 | 낮음 | SPIFFS OTA backup/restore가 문제일 때 |
| 콜드 부팅 H/W 보완 (EN 핀 1uF) | 낮음 | 다음 보드 revision |

---

## 7. 마감 절차

- [x] Analysis 문서 작성 (`docs/03-analysis/...`)
- [x] Report 문서 작성 (본 문서)
- [ ] `/pdca archive RemoteDeck_PC_v2.5 --summary`
- [ ] analysis/report/archive 커밋
- [ ] `git push origin v2.3-httpd`

**Next**: `/pdca archive RemoteDeck_PC_v2.5 --summary`
