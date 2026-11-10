# Archive Index — 2026-11

| Feature | Status | Component | Match Rate | Archived |
|---------|--------|-----------|:----------:|----------|
| [RemoteDeck_PC_v3.0](RemoteDeck_PC_v3.0/) | Completed (RCM 허브 deck 연동) | RemoteDeck_PC (firmware + Web UI) | 98% | 2026-11-10 |

## Summaries

### RemoteDeck_PC_v3.0
RemoteDeck_PC를 **RCM 통합 API 허브의 `deck`**으로 연동(PDCA feature `rcm-hub-v3.0`). rcm-api 참조 라이브러리(`lib/RcmHub`, v1.0.2)를 반입하고 `HubBridge`로 감싼 Option C(Pragmatic) 구조. 허브 명령 `pc.powerOn/forceOff`는 PC LED를 확인한 뒤에만 누름. **충돌 정리(허브 사용 시에만, 부팅 시 값 `hubActive` 기준)**: 스케줄 릴레이1 건너뜀, 릴레이1 latch 409 거부(펄스 허용), MQTT 끔. 보안: `device_key` `****` 가림, `/api/ota` Basic 인증. Web UI: 설정 > 기타 「RCM-API 연동」 카드, 홈 「RCM 허브」 카드(상태 배지·통계 8칸·송수신 내역 40건, `GET /api/hub`). 실기 중 발견·수정: **OTA 응답 미전송**(v2.5.1부터 응답 콜백 안에서 restart → 응답 후 `loop()`에서 재부팅). Check 갭 G-01~03(UI 409 안내, 가드 부팅 시 고정, OTA UI 결과 판정) Act 수정. 송수신 내역용 `RcmHub::setTap()`은 rcm-api PR #1로 업스트림. 허브 서버 미완성이라 **rcm-api 참조 허브(가상, 방 RDTEST01/02)**로 시험: API T-01~12 중 11 PASS(T-10 RS485 보류), conformance basic·guard·relay·reboot·power·led 전부 PASS(power·led는 PC LED 연결 장비 192.168.10.193). matchRate 98%, 성공 기준 14/15(수동 4항목 일부 이월). Carry: 정식 허브 수동 시험(허브 단독 재기동·랜선·4001·4009), T-10, hub off 회귀표, IC hub 컬럼, UDP 인증+IPSetupTool, Touch 허브 연동. commit `d6b66c5`·`e25d44b`·`9c31c28`·`ef76782`, branch `v3.0-rcm-hub`, PR #3. 관련 [[project_spiffs_ota_preserve]] · [[project_rdpc_reboot_flush]] · [[feedback_hardware_first]].
