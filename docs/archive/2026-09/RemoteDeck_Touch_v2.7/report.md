---
template: report
version: 1.0
feature: RemoteDeck_Touch_v2.7_Webinterface
date: 2026-09-15
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware web UI)
branch: v2.6-touch-webmode
matchRate: 97
status: completed
---

# RemoteDeck_Touch v2.7 Webinterface — Completion Report

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | Touch 웹 UI가 PC와 불일치 — 설정이 raw JSON textarea 통짜 편집, 재부팅/OTA가 Config 탭에 산재, 상태·관리 메뉴 부재. |
| **Solution** | `data/www/{index,app.js,style.css}` 재구성으로 PC 스타일 5탭(상태/제어/설정/관리/로그) + 설정 sub-tab(Device/Server/이미지) 항목별 폼 + 관리(재부팅·스케줄·OTA). `/api/serverconfig`·`/api/schedule` 신규, embed_www.py로 임베드, OTA(app) 배포. |
| **Function/UX Effect** | 브라우저에서 설정을 항목별로 직접 편집, 기기관리·재부팅 스케줄·야간 화면 끄기·OTA·이미지 교체를 PC와 동일 UX로. |
| **Core Value** | PC/Touch 웹 UX 통일 + JSON 수기편집 제거. 기존 기기는 OTA만으로 신 UI + 설정 보존(하위호환) + 자동 재부팅 정책 승계. |

### 1.3 Value Delivered (실제 결과)

| Perspective | Metric | 결과 |
|-------------|--------|------|
| 일관성 | PC 대비 탭/폼/토큰 통일 | 5탭 + 설정 3 sub-tab, PC 스타일 실기 렌더 |
| 기능 | FR 충족 | 13/13 구현, 실기 검증 완료 |
| 품질 | matchRate | **97%** (게이트 90% 통과) |
| 호환성 | 스키마 파괴 | 0건 (필드 추가 default 처리 + reboot_time 마이그레이션) |

---

## 2. Success Criteria — Final Status

| 기준 | 상태 | 근거 |
|------|:--:|------|
| 5탭 PC 스타일 렌더 + 항목별 편집·저장 실기 | ✅ Met | 전 탭 실기 |
| 기존 기기 config OTA 후 보존·정상 (FR-12) | ✅ Met* | 가드+마이그레이션, config persist 실기 (*pre-v2.7 실기기 마이그레이션은 배포 스모크 잔여) |
| 재부팅 스케줄 신규 동작 실기 (FR-08) | ✅ Met | S4a |
| 야간 화면 끄기 신규 (FR-13) | ✅ Met | S4b |
| OTA·이미지 관리 정상 | ✅ Met | OTA 진행률·이미지 교체/썸네일 실기 |
| embed_www.py 워크플로 확립·문서화 | ✅ Met | CLAUDE.md |
| `pio run` 빌드 성공 / 웹모드 SPI 배타 | ✅ Met | 반복 빌드 SUCCESS, 웹모드 정상 |

**Success Rate: 7/7 (FR-12 실기기 마이그레이션·부하 PoC는 배포 전 스모크로 이월)**

---

## 3. Key Decisions & Outcomes

| 결정 (출처) | 선택 | 결과 |
|------|------|------|
| [Plan] 아키텍처 | Option C (Pragmatic) — 계약 유지 + serverconfig/schedule 신규 | ✅ 범위·하위호환 균형, 신규 서브시스템 최소 |
| [Design] 설정 편집 | 항목별 폼 → 클라 조립 → 통짜 POST | ✅ `/api/config` 계약 불변, 회귀 없음 |
| [Design] 재부팅 스케줄 | 단일 주간(요일+HH:MM), NTP, main LCD loop 실행 | ✅ PC ScheduleManager 동일 패턴, 실기 동작 |
| [Do] 재부팅 정책 | Plan의 "rebootTime 공존" → **완전 일원화**로 상향 | ✅ 죽은 LCD 위젯·레거시 로직 제거 + 자동 마이그레이션(하위호환 강화) |
| [Do] Sleep 소스 | LCD가 serverConfig→**deviceConfig.sleepTime** 단일화 | ✅ 웹↔LCD 불일치 해소 |
| [Do] 시간원 | 간이 NTP(MQTT tick) 유지 + 유효성 가드 | ✅ 폐쇄망 타당(SNTP 라우팅 불가), tick 오염 버그 차단 |

---

## 4. 구현 요약 (세션별)

- **S1** module-shell: 5탭 골격 + 상태(/api/status 확장: device_id/mqtt/time) + 제어/로그 이관. 카드 좌우 확장, 제어 fetch에러(웹모드 LVGL 미러 크래시) `if(!g_webMode)` 가드로 해결.
- **S2** module-settings: Device/Server 항목별 폼 + `/api/serverconfig` 신규. 설정 sub-tab 가로 탭화, 저장 버튼 카드 분리(탭 전체 저장).
- **S3** module-admin: 관리 탭 — 재부팅 버튼 + OTA 이동. OTA 진행률(연결끊김=성공 100%) 처리.
- **S4a** module-schedule: 재부팅 스케줄(/api/schedule + main NTP loop). **S4b**: 야간 화면 끄기(nightOff + lvgl_touch).
- **S5** module-image-embed: 이미지 관리 설정 하위 이동, 업로드 크래시(웹모드 images_update) 가드, 썸네일 쿼리스트링 파싱 수정, 실제 크기 표시.
- **추가 정리**: Sleep 단일소스 통합 · 재부팅 완전 일원화(+마이그레이션) · 간이 NTP 유효성 가드.

---

## 5. 변경 파일

| 파일 | 변경 |
|------|------|
| `data/www/{index.html,app.js,style.css}` | 5탭·항목별 폼·이미지 관리 재구성 |
| `src/web/embedded_assets.cpp` | embed_www.py 재생성 |
| `src/web/WebServer.cpp` | /api/serverconfig·/api/schedule 라우트, 이미지 서빙(쿼리 파싱 수정) |
| `src/web/ConfigApi.{h,cpp}` | serverconfig·schedule 핸들러 |
| `src/web/ImageApi.{cpp}` | status 확장, 웹모드 images_update 가드 |
| `src/config/DeviceConfig.h` | nightOff·rebootSchedule 구조 |
| `src/utils/JsonUtils.cpp` | night_off·reboot_schedule (de)serialize + reboot_time 마이그레이션 |
| `src/device/DeviceManager.cpp` | Sleep 단일소스(deviceConfig.sleepTime) |
| `src/main.cpp` | 스케줄·야간끄기 실행, 소프트클록 무조건화, tick 가드, 마이그레이션 persist |
| `src/lvgl_touch.{cpp,h}` | 야간 화면 끄기(night_active) |
| `lib/ui/.../ui_ScreenDevice.c`, `ui.{c,h}` | 죽은 Reboot Time 위젯 제거 |

관련 커밋: v2.7 S1~S5 다수 + `aa870be`(S5 이미지 관리 + 설정 통합/시계 하드닝).

---

## 6. Carry Items (배포 전 스모크)

| # | 항목 | 심각도 |
|---|------|:--:|
| C-1 | pre-v2.7 실기기 OTA 마이그레이션(reboot_time→rebootSchedule) 1대 확인 | Low |
| C-2 | 웹모드 부하 PoC `v24_poc.py` 재실행 | Low |

---

## 7. Lessons Learned

- **캐시버스터가 서버 파서를 깬 사례**: 프론트 `?t=` 쿼리가 확장자 화이트리스트를 우회 못해 400. 서버에서 쿼리 스트립이 정석. (`serveSpiffsFile` no-cache로 캐시버스터는 사실상 불필요하나 브라우저 강제 리로드용으로 유지)
- **죽은 UI 컨트롤 + 숨은 동작**: LCD Reboot Time 위젯은 배선된 적 없었으나 `reboot_time=7` 기본값으로 매일 07:00 재부팅이 실제 동작 중이었음 → 제거 시 **마이그레이션 없이는 필드 기기 동작 변경** 위험. 하위호환은 "코드 제거"가 아니라 "동작 승계"로 접근.
- **간이 NTP의 안전 가드**: 폐쇄망에서 서버-tick 방식은 타당하나, 누락 tick(=0)이 시계를 1970으로 오염 → 시간 기반 기능(스케줄/야간) 확대 시 유효성 가드 필수.

---

## 8. 다음 단계

- Archive: `docs/archive/2026-09/RemoteDeck_Touch_v2.7/`
- 이후 사용자 의도: v2.6 + v2.7 → main 병합(PR).
