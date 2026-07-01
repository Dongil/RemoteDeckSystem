# Archive Index — 2026-07

| Feature | Status | Firmware | Match Rate | Archived |
|---------|--------|----------|:----------:|----------|
| [RemoteDeck_PC_LAN_Recovery](RemoteDeck_PC_LAN_Recovery/) | Partial (H/W 이관) | v2.4.7 | 5/6 (83% SC) | 2026-07-01 |

## Summaries

### RemoteDeck_PC_LAN_Recovery
콜드 부팅 시 LAN 미연결 문제. 8회 펌웨어 iteration (v2.4.0 → v2.4.7) 끝에 원인을 **보드 H/W (setup() 미진입)** 로 확정. v2.4.6 STATUS1 LED blink 진단이 결정타. v2.4.7에서 미검증 개입(brownout disable, MAC stagger, NVS 추적) 모두 제거하고 최소 방어선(pre-init delay 500ms, SW reset, retry 3회, GOT_IP watchdog 20s)만 유지. 실증 해결은 H/W 사이클(EN 핀 1uF 콘덴서 등)로 이관.
