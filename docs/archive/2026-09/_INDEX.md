# Archive Index — 2026-09

| Feature | Status | Component | Match Rate | Archived |
|---------|--------|-----------|:----------:|----------|
| [RemoteDeck_PC_v2.6.2-IC](RemoteDeck_PC_v2.6.2-IC/) | Completed (IC 재부재 컬럼) | IntegrateController (client) | 100% | 2026-09-11 |
| [RemoteDeck_Touch_v2.6](RemoteDeck_Touch_v2.6/) | Completed (WebUI 부팅경계 부활) | RemoteDeck_Touch (firmware) | 97% | 2026-09-14 |

## Summaries

### RemoteDeck_Touch_v2.6
v2.2~v2.4에서 W5500+TFT VSPI 공유 충돌로 실패하고 v2.5에서 "영구 포기(sunset)"됐던 Touch WebUI를 **부팅 경계 SPI 배타**로 부활. 웹과 LCD를 동시에 init 하지 않고, "웹 설정 모드"로 부팅하면 TFT/LVGL/터치를 스킵하고 WebServer가 SPI를 단독 점유 → v2.4가 막혔던 SPI host mutex 경합이 구조적으로 성립하지 않음. `DeviceConfig.webConfigMode`(one-shot 소비 플래그, 안티브릭)로 진입/복귀. handleRoot를 v2.3 deferral 안내(853B)에서 풀 WebUI(INDEX_HTML_GZ ~22KB)로 재활성 → v2.4에서 0/9였던 부하 PoC(6동시/burst/30s sustained)가 **9/9 통과**. 장치설정 화면 상단 좌측에 커스텀 웹 아이콘("<"+globe, 우측 nav 대칭) 버튼으로 진입, 웹모드 LCD엔 정적 안내화면, 종료는 웹 재부팅/무활동 10분/전원. OTA는 기존 app-only(U_FLASH)라 SPIFFS 설정 보존 자동. matchRate 97%, SC 7/7. 브랜치 `v2.6-touch-webmode`. 실기 검증 완료. 관련 [[project_touch_webui_bootmode]].

### RemoteDeck_PC_v2.6.2-IC
`RemoteDeck_PC_v2.6.2` report §6 Carry Item #1("IntegrateController에 attendance 컬럼 추가")의 후속 완료. IC 그리드에 재부재 컬럼(기기ID/IP 뒤·PC 앞) 신규 — `RemoteDeckClient.ParseStatus`가 `/api/status.attendance {enabled,source,current}` 미니 블록을 하위호환 파싱, `StatusFormatter.FormatAttendance`로 재실/부재/미설정 렌더. 로그뷰에 날짜 컬럼(client 수신 시각 yyyy-MM-dd) 추가. 구버전(v2.6.1 이하) 기기는 블록 부재 시 "미설정" 안전 표시 → 필드 14대 혼재 환경 회귀 없음. 후속으로 comment 버전 태그를 v2.6.2 라인으로 통일하고 `.gitignore` publish 경로 오타(`IPSetupTool/publish/` → `IPSetupTool/**/publish/`) 수정. dotnet build 0/0, 실기기 테스트 통과("이상없었어", 2026-09-11). matchRate 100%, SC 7/7 met. commit `0660ef0`(구현) + `9df2767`(정리).
