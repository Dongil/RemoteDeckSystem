---
template: report
version: 1.0
feature: RemoteDeck_Touch_v2.6
date: 2026-09-14
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: v2.6-touch-webmode
base: v2.3-httpd
match_rate: 100
sc_success_rate: "7/7 met (SC-1~7) + FR-08 실측"
status: completed
---

# RemoteDeck_Touch v2.6 Completion Report — 웹 설정 모드 (부팅 경계 SPI 배타)

**Cycle**: 2026-09-11 ~ 2026-09-14 (S1~S5)
**Baseline**: v2.3-httpd (WebUI SPI 충돌로 비활성, v2.5 sunset) → **Delivered**: 부팅 경계 배타 웹 설정 모드
**Field Verification**: 실기기 PoC 9/9 + LCD 버튼/정적화면/왕복/무회귀 확인

---

## Executive Summary

| Perspective | Result |
|-------------|--------|
| **Problem** | RemoteDeck_Touch는 설정/OTA 수단이 LCD 터치뿐. 웹UI 동시구동은 W5500+TFT VSPI 공유 충돌로 v2.2~v2.4 전부 실패, v2.5에서 "영구 포기(sunset)"로 마감됨. |
| **Solution Delivered** | 웹과 LCD를 **동시에 안 돌리고 부팅 경계로 배타 분리**. "웹 설정 모드"로 부팅하면 TFT/LVGL/터치를 아예 init 하지 않고 WebServer가 SPI를 단독 점유 → v2.4가 실패한 SPI host mutex 경합이 구조적으로 성립하지 않음. 장치설정 화면의 웹 아이콘 버튼으로 진입, 재부팅으로 복귀. |
| **Function/UX Effect** | 관리자는 브라우저로 RemoteDeck_PC 수준 풀 WebUI(설정 변경·로그·제어·OTA)를 사용. 웹모드 진입 시 LCD엔 "WEB CONFIG MODE / URL / Auth" 정적 안내. 종료(웹 재부팅/무활동 10분/전원)하면 LCD 터치 모드로 자동 복귀. 일상 사용은 기존 LCD 그대로(무회귀). |
| **Core Value** | v2.5에서 포기했던 WebUI를 **되살림**. 동시성 대신 모드 전환으로 SPI 한계 우회. matchRate 100%, PoC 0/9→9/9. |

---

## 1. 최종 산출물

### 1.1 코드 (브랜치 v2.6-touch-webmode)
- **신규**: `src/ui_web_icon.c` (커스텀 "<"+globe 아이콘, RGB565 32×15)
- **확장**: `config/DeviceConfig.h` + `utils/JsonUtils.cpp` (webConfigMode 플래그, 하위호환)
- **수정**: `main.cpp` (boot 분기 · 웹모드 loop · 무활동 타임아웃 · 정적화면 호출 · LCD 진입 버튼)
- **수정**: `web/WebServer.cpp` (handleRoot 풀 WebUI 재활성 · requireAuth 활동시각)
- **확장**: `lvgl_touch.{h,cpp}` (lcd_show_webmode_info 정적 안내화면)
- **재활용**: v2.3 httpd 5모듈(WebServer/Config/Control/Image/Ota/Logger + embedded_assets) + v24_poc.py

### 1.2 문서
- `plan.md` / `design.md` / `analysis.md` / 본 문서 (본 아카이브)

---

## 2. 단계별 진행 (커밋)

| 단계 | 커밋 | 내용 | 검증 |
|---|---|---|---|
| Plan | `147cfd8` | 부팅 경계 SPI 배타 계획 | — |
| Design | `0b9d8ff` | Option A + one-shot flag | — |
| S1 | `54c43bd`·`6bc74aa` | boot 분기 + 풀 WebUI 재활성 | **PoC 9/9** |
| S2 | `c1706ad` | 정적 안내화면 + 무활동 타임아웃 + exit | 실기 |
| S3 | — | OTA 설정보존 → app-only OTA라 불필요 | ➖ |
| S4 | `bc88334` | 장치설정 웹 진입 아이콘 버튼 | 실기 |
| S5 | `35c2a38`·`a06c461` | 시리얼 트리거 제거 + 진입 확인 다이얼로그(오탭 방지) + FR-08 실측 + 아카이브 | 실기 |

---

## 3. Key Decisions & Outcomes

| Layer | Decision | Rationale | Outcome |
|---|---|---|---|
| Design | 재부팅 경계 배타(Option A) | 라이브 teardown race 회피 | ✅ 서비스 중 TFT transaction 0 |
| Design | one-shot consumed flag | 안티브릭 (어떤 리셋도 LCD 복귀) | ✅ 전원손실·모니터 close-reset 안전 |
| S1 | handleRoot INDEX_HTML_GZ 재활성 | v2.3 deferral 사유(SPI) 제거됨 | ✅ 22KB 풀 UI, PoC 9/9 |
| S2 | 정적 안내화면(TFT 1회) | 정지 이미지 대신 상태 표시 | ✅ "WEB CONFIG MODE" |
| S2 | 종료=기존 /api/reboot | 플래그 소비로 자동 LCD 복귀 | ✅ 신규 엔드포인트 불필요 |
| S4 | 커스텀 아이콘(텍스트 아님) | 한글 폰트 subset 글리프 부재 | ✅ "<"+globe, nav 대칭 |

---

## 4. Success Criteria Final

SC-1~7 **7/7 met** (§analysis §5). 대표: PoC 9/9(v2.4 0/9), TFT transaction 0, LCD 무회귀.

---

## 5. Value Delivered

- **관리자**: 브라우저로 설정/OTA (LCD 소형 터치의 불편 해소 — 본 사이클의 출발 요구)
- **현장 사용자**: 일상은 LCD 그대로, 웹모드 중엔 안내화면
- **프로젝트**: v2.5 sunset 결론 반전 — SPI 물리 한계를 SW 모드 전환으로 우회한 사례 확보

---

## 6. Lessons Learned

- **"동시" 전제를 버리면 하드웨어 한계를 우회할 수 있다.** v2.4는 web+LCD 동시 구동을 per-request로 조율하려다 SPI mutex에 막혔다. v2.6은 부팅당 하나만 init → 경합 자체를 없앰.
- **안티브릭은 플래그 소비로.** 진입 즉시 flag=false 저장 → 미완 종료도 항상 LCD 복귀.
- **ESP32 시리얼 open/close가 리셋을 유발**한다(DTR/RTS). 테스트 중 모니터를 닫으면 기기가 리셋된다 — PoC는 이더넷이므로 모니터를 연 채 실행해야 함(디버깅에서 실제로 겪음).
- **SquareLine 폰트는 subset** — 새 한글 라벨은 글리프 부재로 깨진다. 텍스트 대신 아이콘(자체 생성 자산)으로 회피.

---

## 7. Carry Items 처리 (2026-09-14 사용자 결정)

| Item | 결과 |
|---|---|
| FR-08 10분 타임아웃 실측 | ✅ 완료 (웹접속 없이 10분 뒤 자동 LCD 재부팅 실기 확인) |
| 웹모드 진입 확인 다이얼로그 | ✅ 완료 (OK/Cancel msgbox 구현·실기 확인, commit a06c461) |
| PNG 디코더 재활성 | ⛔ 미채택 (BMP 유지, heap 부담 대비 실익 낮음) |

→ 잔여 carry item 없음.

---

## 8. 마감 절차

- [x] Analysis 문서 (matchRate 97%)
- [x] Report 문서 (본 문서)
- [x] 코드 commit (S1~S5)
- [x] 임시 시리얼 트리거 제거 + 클린 펌웨어 실기 업로드
- [x] Archive (docs/archive/2026-09/RemoteDeck_Touch_v2.6/)
- [x] archive commit + origin push
- [x] Carry items 처리 (FR-08 실측 ✅ / 확인 다이얼로그 ✅ / PNG 미채택 ⛔)
