# Archive Index — 2026-09

| Feature | Status | Component | Match Rate | Archived |
|---------|--------|-----------|:----------:|----------|
| [RemoteDeck_PC_v2.6.2-IC](RemoteDeck_PC_v2.6.2-IC/) | Completed (IC 재부재 컬럼) | IntegrateController (client) | 100% | 2026-09-11 |
| [RemoteDeck_Touch_v2.6](RemoteDeck_Touch_v2.6/) | Completed (WebUI 부팅경계 부활) | RemoteDeck_Touch (firmware) | 100% | 2026-09-14 |
| [RemoteDeck_Touch_v2.7](RemoteDeck_Touch_v2.7/) | Completed (WebUI PC스타일 리뉴얼) | RemoteDeck_Touch (firmware) | 97% | 2026-09-15 |

## Summaries

### RemoteDeck_Touch_v2.7
v2.6에서 되살린 Touch 웹 UI를 **RemoteDeck_PC 스타일 5탭(상태/제어/설정/관리/로그)**으로 리뉴얼. 설정을 raw JSON textarea → **항목별 폼**으로 전환하고 sub-tab(Device Config / Server Config / 이미지 관리)으로 정리, 관리 탭에 재부팅·재부팅 스케줄·OTA를 통합. 신규 API `GET/POST /api/serverconfig`·`/api/schedule`(계약 추가, 기존 `/api/config` 불변). Option C(Pragmatic). 신규 기능: **재부팅 스케줄**(단일 주간 요일+HH:MM, NTP, main LCD loop 실행)·**야간 화면 끄기**(nightOff, 시간대 기반, 스크린세이버와 별개, 터치 시 잠깐 wake). 하위호환: JsonUtils default 가드 + 스키마 무파괴. 실기 검증 중 품질 delta 확보 — 썸네일 쿼리스트링(`?t=`) 서버 파싱 수정(400→정상), 이미지 카드 실제 크기(100%) 표시, **Sleep 단일소스 통합**(LCD가 serverConfig→deviceConfig.sleepTime), **재부팅 완전 일원화**(죽은 LCD 위젯·레거시 reboot_time 로직 제거 + reboot_time→rebootSchedule 자동 마이그레이션으로 필드 기기 07:00 재부팅 승계), **간이 NTP(MQTT tick) 유효성 가드**(누락 tick의 1970 시계오염 차단) + currentTime 소프트클록 무조건화. RemoteDeck_PC NTP(SNTP)와 비교 검토: 폐쇄망 특성상 서버-tick 방식 타당. matchRate 97%, FR 13/13, 전 단계(S1~S5) 실기 검증. Carry: pre-v2.7 실기기 마이그레이션·부하 PoC 배포 스모크. 브랜치 `v2.6-touch-webmode`, commit `aa870be` 외. 관련 [[project_touch_webui_bootmode]] · [[feedback_client_version_follows_contract]].

### RemoteDeck_Touch_v2.6
v2.2~v2.4에서 W5500+TFT VSPI 공유 충돌로 실패하고 v2.5에서 "영구 포기(sunset)"됐던 Touch WebUI를 **부팅 경계 SPI 배타**로 부활. 웹과 LCD를 동시에 init 하지 않고, "웹 설정 모드"로 부팅하면 TFT/LVGL/터치를 스킵하고 WebServer가 SPI를 단독 점유 → v2.4가 막혔던 SPI host mutex 경합이 구조적으로 성립하지 않음. `DeviceConfig.webConfigMode`(one-shot 소비 플래그, 안티브릭)로 진입/복귀. handleRoot를 v2.3 deferral 안내(853B)에서 풀 WebUI(INDEX_HTML_GZ ~22KB)로 재활성 → v2.4에서 0/9였던 부하 PoC(6동시/burst/30s sustained)가 **9/9 통과**. 장치설정 화면 상단 좌측에 커스텀 웹 아이콘("<"+globe, 우측 nav 대칭) 버튼으로 진입, 웹모드 LCD엔 정적 안내화면, 종료는 웹 재부팅/무활동 10분/전원. OTA는 기존 app-only(U_FLASH)라 SPIFFS 설정 보존 자동. 진입 시 확인 다이얼로그(오탭 방지), 무활동 10분 자동 복귀 실측. matchRate 100%, SC 7/7 + FR-08 실측. 브랜치 `v2.6-touch-webmode`. 실기 검증 완료. 관련 [[project_touch_webui_bootmode]].

### RemoteDeck_PC_v2.6.2-IC
`RemoteDeck_PC_v2.6.2` report §6 Carry Item #1("IntegrateController에 attendance 컬럼 추가")의 후속 완료. IC 그리드에 재부재 컬럼(기기ID/IP 뒤·PC 앞) 신규 — `RemoteDeckClient.ParseStatus`가 `/api/status.attendance {enabled,source,current}` 미니 블록을 하위호환 파싱, `StatusFormatter.FormatAttendance`로 재실/부재/미설정 렌더. 로그뷰에 날짜 컬럼(client 수신 시각 yyyy-MM-dd) 추가. 구버전(v2.6.1 이하) 기기는 블록 부재 시 "미설정" 안전 표시 → 필드 14대 혼재 환경 회귀 없음. 후속으로 comment 버전 태그를 v2.6.2 라인으로 통일하고 `.gitignore` publish 경로 오타(`IPSetupTool/publish/` → `IPSetupTool/**/publish/`) 수정. dotnet build 0/0, 실기기 테스트 통과("이상없었어", 2026-09-11). matchRate 100%, SC 7/7 met. commit `0660ef0`(구현) + `9df2767`(정리).
