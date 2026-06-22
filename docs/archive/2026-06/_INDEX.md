# Archive Index — 2026-06

PDCA 문서 아카이브 — 2026년 6월 완료 사이클.

## 완료 사이클

| Feature | 기간 | Match Rate | Iterations | Archived | 비고 |
|---------|------|:---:|:---:|----------|------|
| [RemoteDeck_Touch_v2.1](./RemoteDeck_Touch_v2.1/) | 2026-06-22 (1일) | **68%** | 0 | 2026-06-22 | LAN 스택 통일 완료. WebUI/PNG는 v2.2로 분리 |

## RemoteDeck_Touch_v2.1 요약

**핵심 성과**:
- arduino-libraries/Ethernet → ETH.h + ETH_PHY_W5500 (PC v2.3.0 패턴)
- TFT_eSPI 2.4.61 → 2.5.43 (Arduino-ESP32 3.x 호환)
- ArduinoHttpClient → ESP32 내장 HTTPClient (downloadFile/sendHttpMessage 재작성)
- setup() 순서 재배치로 W5500/TFT_eSPI SPI 충돌 해결

**Plan 외 추가 개선 (Positive findings)**:
- Long-click 35회 누적 → 1회 long-press 진입 (v1 dead-code 버그 동반 수정)
- DeviceManager Sleep 시간 저장/복원 (v1 regression fix)
- 이미지 디코드 메모리 안전장치 (크기/heap 검증, OLD 선행 free, 진단 로깅)
- downloadFile Content-Length 200KB 상한
- FULL (4MB) + OTA (1.72MB) 펌웨어 git 추적 + flash.bat 인프라

**v2.2 인계 사항**:
- AsyncTCP task slot 충돌 (WebUI 비활성)
- LV_USE_PNG=0 (PNG 디코더 미활성)
- OTA Handler / 로그 뷰어 / deviceconfig 웹 편집
- 시간 표시 UI

**Branch 전략**: `v2.1-lan` → `main` merge (commit `c44d348`)
**펌웨어**: `RemoteDeck_Touch/firmware/RemoteDeck_Touch_V2.1.0_{FULL|OTA}_20260622.bin`

## 문서 구성

- `RemoteDeck_Touch_v2.1.plan.md` — Plan (요구사항, 스코프, 리스크)
- `RemoteDeck_Touch_v2.1.design.md` — Design (Option C Pragmatic, C1~C10 commit 분할)
- `RemoteDeck_Touch_v2.1.analysis.md` — Gap Analysis (Match Rate 68%)
- `RemoteDeck_Touch_v2.1.report.md` — 통합 완료 보고서
