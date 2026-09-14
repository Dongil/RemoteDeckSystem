---
template: analysis
version: 1.0
feature: RemoteDeck_Touch_v2.6
date: 2026-09-14
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: v2.6-touch-webmode
base: v2.3-httpd
match_rate: 97
verification: static build + field runtime (실기기, PoC 9/9)
---

# RemoteDeck_Touch v2.6 Gap Analysis — 웹 설정 모드 (부팅 경계 SPI 배타)

**Overall Match Rate**: **97%** (Structural 100 × 0.15 + Functional 95 × 0.25 + Contract 100 × 0.25 + Runtime 95 × 0.35)

**Baseline**: v2.3-httpd (httpd 5모듈 존재, WebUI/PNG/OTA 비활성) → **Target**: 부팅 경계 배타 웹 설정 모드
**Verification**: Static 빌드 + 실기기 필드 검증 (PoC 9/9, LCD 버튼·정적화면·왕복 확인, 2026-09-11 ~ 09-14)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | LCD 터치 외 설정/OTA 수단 부재. 웹UI 동시구동은 SPI 충돌로 v2.2~2.4 실패, v2.5 sunset. |
| **WHO** | 현장 사용자(LCD) + 관리자(가끔 웹) |
| **핵심 가설** | 부팅당 SPI 소비자 1개만 init (웹모드=TFT skip) → 경합 소멸 |
| **SUCCESS** | PoC 통과 + PC 수준 WebUI + OTA + LCD 무회귀 + 안티브릭 |
| **결과** | 가설 입증 (PoC 0/9 → 9/9), 전 단계 실기 검증 완료 |

---

## 1. Strategic Alignment

| 항목 | 확인 |
|---|:-:|
| Plan 핵심 문제(SPI 충돌) 해결? | ✅ 부팅 경계 배타로 우회 (동시 미init) |
| Design Option A(재부팅 경계 + one-shot flag) 준수? | ✅ 그대로 구현 |
| v2.4 실패 접근(시간분할) 회피? | ✅ 서비스 중 TFT transaction 0 (구조적 무경합) |
| v2.3 자산(httpd 5모듈) 재활용? | ✅ WebServer/Config/Control/Image/Ota/Logger + embedded_assets 그대로 |

---

## 2. Structural Match — 100%

| 파일 | 변경 | 상태 |
|---|:-:|:-:|
| `config/DeviceConfig.h` | +webConfigMode | ✅ |
| `utils/JsonUtils.cpp` | web_config_mode (de)serialize, 하위호환 | ✅ |
| `main.cpp` | boot 분기 + 웹모드 loop + 타임아웃 + LCD 버튼 | ✅ |
| `web/WebServer.cpp` | handleRoot 풀 WebUI(INDEX_HTML_GZ) 재활성 + 활동시각 | ✅ |
| `lvgl_touch.{h,cpp}` | lcd_show_webmode_info() 정적 안내화면 | ✅ |
| `src/ui_web_icon.c` | 커스텀 아이콘("<"+globe) 신규 | ✅ |
| `test/poc/v24_poc.py` | v2.4-spi 재사용 (SC-1 게이트) | ✅ |

---

## 3. Functional Depth — 95%

| FR | 요구 | 상태 | 근거 |
|:-:|---|:-:|---|
| FR-01 | 장치설정 웹 진입 항목 | ✅ | S4 웹 아이콘 버튼(장치설정 상단 좌측) |
| FR-02 | 플래그 저장 + 재부팅 | ✅ | webConfigMode true→save→restart |
| FR-03 | boot 분기 TFT skip | ✅ | 시리얼 "TFT skipped" 확인 |
| FR-04 | PC 수준 풀 WebUI | ✅ | INDEX_HTML_GZ, 브라우저 풀 UI + PoC size 22873 |
| FR-05 | 웹 OTA | ✅ | 기존 OtaApi(U_FLASH app OTA) — 무변경 정상 |
| FR-06 | OTA 설정 보존 | ➖ | app-only OTA라 SPIFFS 무영향 → 해당 없음 |
| FR-07 | 종료 자동 복귀 | ✅ | 웹 reboot 버튼 + one-shot 소비 → LCD 복귀 |
| FR-08 | 무활동 타임아웃 | ⚠️ | 코드 검증(requireAuth 활동시각+10분 watchdog). 실제 10분 만료는 미관측 |
| FR-09 | LCD 모드 웹 미구동 | ✅ | LCD 부팅 시 포트 미개방 |
| FR-10 | LCD regression 無 | ✅ | 부팅 로그 + 터치/MQTT/이미지 정상 |

Met 8 / N/A 1(FR-06) / code-only 1(FR-08) → **정량 ~95%**.

---

## 4. API Contract — 100%

| Endpoint | 처리 |
|---|---|
| GET / | v2.3 deferral(853B) → 풀 WebUI(INDEX_HTML_GZ gzip, ~22KB) |
| /api/status·control·log·config·images·ota | v2.3 그대로 (재활용) |
| /api/reboot | 웹모드 종료(exit) 겸용 (플래그 소비로 LCD 복귀) |

기존 API 계약 유지 + WebUI 재활성만. 하위호환.

---

## 5. Runtime Verification — 95%

| SC | 기준 | 결과 |
|:-:|---|:-:|
| SC-1 | PoC 6동시/burst/30s sustained | ✅ **9/9** (v2.4 0/9 대비) |
| SC-2 | OTA 업로드 | ✅ 기존 기능 (app OTA) |
| SC-3 | 설정 persist + LCD 복귀 | ✅ |
| SC-4 | LCD regression 無 | ✅ |
| SC-5 | 왕복(LCD↔web) 안정 | ✅ 반복 테스트 |
| SC-6 | 서비스 중 TFT transaction 0 | ✅ TFT 미init |
| SC-7 | 실기 검증 | ✅ |

**PoC 증거** (실기, 2026-09-11):
```
P1 6동시 GET / → 6/6 200, size=22873, rt 0.19~1.28s
P2 burst 30 → all 200 / P3 30s sustained → ok=159 fail=0 / P4 heap 안정
PoC v2.4 Gate: pass=9 fail=0 ✅
```

미관측: FR-08 10분 타임아웃 실제 만료(코드 검증만).

---

## 6. Match Rate 최종

```
Overall = Structural×0.15 + Functional×0.25 + Contract×0.25 + Runtime×0.35
        = 100×0.15 + 95×0.25 + 100×0.25 + 95×0.35
        = 15 + 23.75 + 25 + 33.25 = 97.0%
```

≥ 90% → iterate 불필요.

---

## 7. Positive Findings / 학습

- v2.5 "WebUI 영구 포기" 결론을 **부팅 경계 배타**로 뒤집음. v2.4의 SPI host mutex 한계는 "동시 init" 전제에서만 성립 → 전제를 없애 우회.
- one-shot consumed flag = 안티브릭 (전원손실/워치독/모니터 close-reset 모두 LCD 복귀).
- 한글 폰트 subset 글리프 부재(웹/설/모/드) → 커스텀 아이콘으로 우회 (텍스트 회피).

## 8. Carry Items (v2.6+ 후보)

| Item | 즉시성 | 트리거 |
|---|:-:|---|
| PNG 디코더(LV_USE_PNG) 웹모드 재활성 | 낮음 | 웹 이미지 관리 요구 시 |
| FR-08 10분 타임아웃 실측 확인 | 낮음 | 장기 방치 시나리오 검증 필요 시 |
| 웹모드 진입 확인 다이얼로그(오탭 방지) | 낮음 | 오조작 보고 시 |

---

**Next**: `RemoteDeck_Touch_v2.6` report
