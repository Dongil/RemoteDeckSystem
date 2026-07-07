# Archive Index — 2026-07

| Feature | Status | Firmware | Match Rate | Archived |
|---------|--------|----------|:----------:|----------|
| [RemoteDeck_PC_LAN_Recovery](RemoteDeck_PC_LAN_Recovery/) | Resolved (아답터 교체) | v2.4.7 | 100% | 2026-07-03 |
| idle-legacy (15 features) | Cleanup | — | — | 2026-07-03 |
| [RemoteDeck_PC_v2.5](RemoteDeck_PC_v2.5/) | Completed (필드 14대 배포) | v2.5.1 fw + v2.5.2 SPIFFS | 99.2% | 2026-07-05 |
| [RemoteDeck_PC_v2.6](RemoteDeck_PC_v2.6/) | Completed (GPIO2 접점 감지) | v2.6.0 fw | 100% | 2026-07-06 |

## Summaries

### RemoteDeck_PC_LAN_Recovery
콜드 부팅 시 LAN 미연결 문제. 8회 펌웨어 iteration (v2.4.0 → v2.4.7) 후 필드 재검증으로 **실제 원인은 아답터 불안정 전류 입력**임을 확정. 문제 보드에 **정상 아답터 교체하는 것만으로 해결**. v2.4.7 펌웨어는 미검증 개입(brownout disable, MAC stagger, NVS 추적) 모두 제거하고 최소 방어선(pre-init delay 500ms, W5500 SW reset, ETH.begin retry 3회, GOT_IP watchdog 20s)만 유지한 상태로 정식 마감. 보드 보완(EN 핀 1uF 콘덴서 등 전원 안정화)은 향후 개정판으로 이관 — 즉시 조치 불필요.

### RemoteDeck_PC_v2.6
GPIO2 상태 변화 자동 fire 활성화. `pinMode(INPUT)→INPUT_PULLUP` + SwitchMonitor 클래스(PCMonitor 미러) 신규. 상태 전이 시 기존 `gpio2_high`/`gpio2_low` WebRequest 이벤트 발화. 신규 이벤트/URL/UI/스키마 0 — 기존 자산 100% 재사용. 조명 스위치→광커플러→GPIO2 접점(GND) 배선을 통한 재부재 판정 신호가 첫 사용 사례. NetManager 방어선 diff=0. matchRate 100%, SC 10/10 met. 코드 diff ~76 LOC (신규 SwitchMonitor.{h,cpp} + main.cpp wiring + 버전 스탬프). SPIFFS 무변경 (v2.5.2 그대로 사용).

### RemoteDeck_PC_v2.5
v2.4.7 콜드 부팅 마감 후 필드 피드백 3건(관리 접근성/IC 로그 부재/부팅 sync 없음)을 통합 반영. 관리 탭 재편성(즉시재부팅+스케줄+OTA), WebRequestHandler::syncCurrentStates(GPIO/PCLED 부팅 동기화), IntegrateController LogPoller(5s 폴링 + 300ms stagger). 세션 중 SPIFFS OTA endpoint 추가 (파일명 substring 라우팅 + config backup/restore) — 웹 UI 원격 갱신 인프라 확보로 14대 대량 배포 완료. v2.4.7 NetManager 방어선 diff=0 유지. matchRate 99.2%, SC 9/10 met + 1 partial(SC-6 실증 계측 없음). Firmware v2.5.1 + SPIFFS v2.5.2(자동 리로드 UI 패치).

### idle-legacy (15 features)
`.bkit/state/pdca-status.json`의 `activeFeatures`에 잔존하던 15개 항목 일괄 정리 (2026-07-03):
- **RemoteDeck_Touch**: v2.5(WebUI 제거, MQTT-only)에서 이미 정식 완료. 활성 목록의 잔여 marker만 archived 처리.
- **14 legacy markers** (config, RemoteDeck_PC, control, network, serial, web, www, Models, Services, IPSetupTool, Ethernet2, RemoteDeckTest, REST-api, firmware): 초기 세션 파일 편집 시 디렉토리 이름 단위로 자동 등록된 marker. 실제 PDCA 사이클 아님. `matchRate=null`, `iter=0` 상태 유지된 채 4~6월부터 유휴. `activeFeatures = []`, `primaryFeature = null` 로 리셋.
