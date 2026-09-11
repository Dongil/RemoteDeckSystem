---
template: plan
version: 0.1
feature: RemoteDeck_Touch_v2.6
date: 2026-09-11
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: v2.6-touch-webmode
base: v2.3-httpd (httpd 5모듈 코드 온전)
status: plan
---

# RemoteDeck_Touch v2.6 Plan — 웹 설정 모드 (부팅 경계 SPI 배타)

> **한 줄 요약**: LCD 터치와 웹서버를 **동시에 돌리지 않고**, "웹 인터페이스 사용" 선택 시 **재부팅하여 LCD/TFT를 아예 초기화하지 않은 채 웹서버만 SPI를 단독 점유**하는 모드로 진입한다. 설정·OTA를 웹에서 처리한 뒤 재부팅하면 평소의 LCD 터치 모드로 복귀한다.

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | RemoteDeck_Touch는 펌웨어/설정 수정 수단이 LCD 터치뿐이라 OTA·설정 변경이 매우 불편. v2.2~v2.4에서 웹UI 동시 구동은 SPI 충돌로 전부 실패. |
| **WHO** | 현장 사용자(평소 LCD 터치) + 관리자(가끔 웹으로 설정/OTA) |
| **RISK** | 라이브 전환 시 TFT/LVGL teardown·SPI 해제 race / 웹 모드에서 빠져나올 방법 부재 / LCD 모드 regression |
| **SUCCESS** | 웹 모드에서 RemoteDeck_PC 수준 설정+OTA 동작 · LCD 모드 무회귀 · SPI 충돌 0 · 모드 왕복 안정 |
| **SCOPE** | S1 boot 분기 + 모드 플래그 → S2 웹 모드(기존 httpd 5모듈 재활성) → S3 OTA/설정 보존 → S4 복귀/안전장치 → S5 PoC 검증 |

---

## 1. 문제 배경 (v2.2~v2.5 학습 요약)

W5500 이더넷과 TFT_eSPI 디스플레이가 **같은 SPI 버스**(SCK18/MOSI23/MISO19)를 공유한다 (`main.cpp` setup() 주석 명시). 지금까지의 웹UI 시도는 전부 **둘을 동시에 켠 상태**에서 SPI를 나눠 쓰려다 실패했다:

| 버전 | 접근 | 결과 |
|---|---|---|
| v2.2 | Arduino sync WebServer + yield | 단발 OK, 연속/병렬 불안정 (49%) |
| v2.3 | esp_http_server 5모듈 (task+core pinning) | 핵심 API는 되지만 SPI 충돌로 WebUI/PNG/OTA 비활성 (86.4%) |
| v2.4 | 시간 분할 (Web active 시 LCD freeze) | PoC 전량 실패 — SPI host mutex wait 무제한 (12.1%) |
| v2.5 | 웹UI 영구 폐기(sunset) | LCD/MQTT only 복귀 (97.1%) |

**핵심 교훈(v2.4 §9)**: freq 조정·gzip·socket 수·polling 조정 모두 *동시 사용* 전제에서는 본질 해결 안 됨. 남은 현실 옵션은 "SPI 물리 분리(HW)" 또는 "동시 사용 포기".

## 2. 이번 가설 — 부팅 경계 SPI 배타 (v2.4와 근본적으로 다름)

v2.4는 **둘 다 초기화한 채** per-request로 LCD를 freeze/resume 하려다 mutex 경합에 걸렸다. v2.6은 **부팅당 SPI 소비자를 하나만 초기화**한다:

- **LCD 모드 부팅**: SPIFFS+config → ETH → **TFT+LVGL+터치** 초기화 → MQTT. (웹서버 미구동 = v2.5 수준)
- **웹 모드 부팅**: SPIFFS+config → ETH → **TFT/LVGL/터치 초기화 자체를 스킵** → WebServer(httpd 5모듈) 구동. 웹서버가 SPI를 **단독 점유** → 경합 대상 없음.

즉 v2.4가 실패한 `SPI host mutex wait`는 **TFT transaction이 존재할 때만** 발생하는데, 웹 모드에서는 TFT를 아예 init하지 않으므로 TFT transaction이 0건 → 경합 자체가 성립하지 않는다. 이것이 이 계획의 성패를 가르는 단일 가설이다.

## 3. UX 흐름

```
[LCD 모드 · 평소]
  장치 설정 화면 → "웹 인터페이스 사용" 선택
        │  webConfigMode=true 저장(SPIFFS) + ESP.restart()
        ▼
[웹 모드 · 재부팅 후]
  TFT 미초기화 (또는 "웹 설정 모드 · http://<IP>:<port>" 정적 안내 1회만 그리고 정지)
  웹 브라우저로 접속 → 설정 변경 / 로그 / 제어 / OTA 펌웨어 업로드
        │  웹 UI "완료·재부팅" 버튼  또는  N분 무활동 타임아웃
        │  webConfigMode=false 저장 + ESP.restart()
        ▼
[LCD 모드 · 복귀]  변경된 설정/펌웨어 반영, 평소 터치 사용
```

## 4. Functional Requirements

| ID | 요구 | 우선순위 |
|----|------|:---:|
| FR-01 | 장치 설정(LCD) 화면에 "웹 인터페이스 사용" 진입 항목 추가 | High |
| FR-02 | 선택 시 `webConfigMode=true` 를 SPIFFS(deviceconfig.json)에 저장 후 재부팅 | High |
| FR-03 | 부팅 시 플래그 분기: web 모드 → **TFT/LVGL/터치 init 스킵**, ETH+WebServer 구동 | High |
| FR-04 | 웹 인터페이스 = RemoteDeck_PC 수준 (상태·설정 변경·로그·제어 탭) | High |
| FR-05 | 웹에서 OTA 펌웨어 업로드 + 적용 | High |
| FR-06 | OTA/설정 변경 시 SPIFFS 설정 보존 (RemoteDeck_PC v2.5.1 backup/restore 패턴 재사용) | High |
| FR-07 | 웹 모드 종료 → `webConfigMode=false` 자동 복귀 + 재부팅 → 다음 부팅 LCD 모드 | High |
| FR-08 | 안전장치: 웹 모드 무활동 타임아웃(예: 10분) 시 자동 LCD 모드 복귀 | Medium |
| FR-09 | LCD 모드에서는 WebServer 미구동 (리소스/Flash v2.5 수준 유지) | High |
| FR-10 | LCD 모드 regression 無 (터치/MQTT/이미지 기존 동작) | High |

## 5. Success Criteria & PoC Gate

> v2.4는 PoC(P1 단발 GET)부터 실패했다. 이번엔 **웹 모드에서 TFT 미초기화**이므로 동일 PoC가 통과해야 가설이 입증된다. `test/poc/v24_poc.py` 재사용.

| ID | 기준 | 측정 |
|----|------|------|
| SC-1 | 웹 모드에서 6 동시 GET / + 5 burst + 30초 sustained 통과 | v24_poc.py P1~P4 (LCD 미초기화 조건) |
| SC-2 | 웹 OTA 업로드 → 재부팅 후 새 펌웨어 버전 확인 | 수동 + 버전 조회 |
| SC-3 | 웹에서 변경한 설정이 LCD 모드 복귀 후 반영 | 왕복 시나리오 |
| SC-4 | LCD 모드 regression 無 (터치/MQTT/이미지) | L3 수동 |
| SC-5 | 모드 왕복(LCD→web→LCD) 3회 안정, heap 안정 | 반복 |
| SC-6 | 웹 모드 부팅 중 TFT SPI transaction 0건 (init 스킵 검증) | 시리얼 로그 |

**Gate**: SC-1(PoC) 미통과 시 → 가설 재검토. 통과 시 → module 순차 진행.

## 6. Design 방향 (design 단계에서 확정)

- **권장(Option A)**: 재부팅 경계 + SPIFFS 플래그. 부팅 분기에서 web 모드면 `lvgl_touch_init`/`ui_init` 호출 자체를 건너뛴다. 가장 단순·안전, 사용자 요구("재부팅하면 LCD")와 정확히 일치.
- 대안(Option B): 라이브 전환(무재부팅) — TFT/LVGL deinit + SPI 해제 후 웹 기동. teardown race 위험 커서 비권장.
- 웹 모드 LCD 표시: (a) 완전 off vs (b) 진입 직전 "웹 설정 모드 · IP" **정적 1회 렌더 후 정지**(이후 TFT 무접근 → 경합 없음). (b)가 UX 우수, design에서 결정.

## 7. 재활용 자산 (현재 브랜치 base = v2.3-httpd)

현재 브랜치에 **v2.3 httpd 5모듈이 온전히 존재** → 신규 작성 최소화:
`web/WebServer.{h,cpp}`, `web/ConfigApi`, `web/ControlApi`, `web/ImageApi`, `web/OtaApi`, `web/Logger`, `web/embedded_assets`, `test/poc/*`. v2.3에서 핵심 API+Control은 이미 동작 검증됨(86.4%) — 이번엔 SPI 배타 하에 WebUI/PNG/OTA까지 재활성이 목표.

## 8. 비범위 (Non-scope)

- 웹 + LCD **동시** 사용 (본 계획의 전제 자체가 배타)
- HW rewire / 보드 교체 (v2.4 옵션 C·D — 별도 트랙)
- RemoteDeck_PC 변경 (0건)

## 9. Risks & Mitigations

| Risk | 완화 |
|------|------|
| 웹 모드에서 못 빠져나옴 (브릭 우려) | FR-07 웹 "완료·재부팅" + FR-08 무활동 타임아웃 + 물리 리셋 시 기본 LCD 모드 부팅(플래그 기본=false) |
| OTA 중 SPIFFS 설정 유실 | FR-06 backup/restore (RemoteDeck_PC v2.5.1 검증 패턴 재사용) |
| web 모드 부팅 시 TFT가 어딘가에서 lazy-init | SC-6 로 transaction 0 검증, init 경로 전수 확인 |
| Flash 증가 (httpd 재도입) | LCD 모드 동작엔 영향 없음, 파티션 여유 확인 (v2.4 OTA partition 이슈 참고) |

## 10. Branch & 절차

- Branch: `v2.6-touch-webmode` (from `v2.3-httpd`) — 생성 완료
- 다음: design 문서(Option A/B 확정, boot 분기 상세, 모드 플래그 스키마) → PoC(SC-1) → module 순차 → analysis/report → archive
- 이력 정합성 참고: 현 mainline(v2.3-httpd)의 Touch 코드는 httpd 상태, v2.5-sunset(제거판)은 미병합·미푸시 — 본 시도는 httpd 자산을 되살리는 방향이므로 base로 v2.3-httpd가 적합.

## 11. Open Questions (design 확정 필요)

1. 웹 모드 LCD 표시: 완전 off vs 정적 안내 1회 (§6)
2. 웹 포트/경로: RemoteDeck_PC와 동일 `:5050` 통일 vs Touch 기존 유지
3. 무활동 타임아웃 값 (기본 10분?)
4. 인증: admin/12345 재사용 여부
5. 웹 모드 진입 트리거 UI 위치 (장치 설정 화면 내 정확한 위치)
