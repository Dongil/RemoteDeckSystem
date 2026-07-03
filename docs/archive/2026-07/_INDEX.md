# Archive Index — 2026-07

| Feature | Status | Firmware | Match Rate | Archived |
|---------|--------|----------|:----------:|----------|
| [RemoteDeck_PC_LAN_Recovery](RemoteDeck_PC_LAN_Recovery/) | Resolved (아답터 교체) | v2.4.7 | 100% | 2026-07-03 |
| idle-legacy (15 features) | Cleanup | — | — | 2026-07-03 |

## Summaries

### RemoteDeck_PC_LAN_Recovery
콜드 부팅 시 LAN 미연결 문제. 8회 펌웨어 iteration (v2.4.0 → v2.4.7) 후 필드 재검증으로 **실제 원인은 아답터 불안정 전류 입력**임을 확정. 문제 보드에 **정상 아답터 교체하는 것만으로 해결**. v2.4.7 펌웨어는 미검증 개입(brownout disable, MAC stagger, NVS 추적) 모두 제거하고 최소 방어선(pre-init delay 500ms, W5500 SW reset, ETH.begin retry 3회, GOT_IP watchdog 20s)만 유지한 상태로 정식 마감. 보드 보완(EN 핀 1uF 콘덴서 등 전원 안정화)은 향후 개정판으로 이관 — 즉시 조치 불필요.

### idle-legacy (15 features)
`.bkit/state/pdca-status.json`의 `activeFeatures`에 잔존하던 15개 항목 일괄 정리 (2026-07-03):
- **RemoteDeck_Touch**: v2.5(WebUI 제거, MQTT-only)에서 이미 정식 완료. 활성 목록의 잔여 marker만 archived 처리.
- **14 legacy markers** (config, RemoteDeck_PC, control, network, serial, web, www, Models, Services, IPSetupTool, Ethernet2, RemoteDeckTest, REST-api, firmware): 초기 세션 파일 편집 시 디렉토리 이름 단위로 자동 등록된 marker. 실제 PDCA 사이클 아님. `matchRate=null`, `iter=0` 상태 유지된 채 4~6월부터 유휴. `activeFeatures = []`, `primaryFeature = null` 로 리셋.
