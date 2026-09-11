# Archive Index — 2026-09

| Feature | Status | Component | Match Rate | Archived |
|---------|--------|-----------|:----------:|----------|
| [RemoteDeck_PC_v2.6.2-IC](RemoteDeck_PC_v2.6.2-IC/) | Completed (IC 재부재 컬럼) | IntegrateController (client) | 100% | 2026-09-11 |

## Summaries

### RemoteDeck_PC_v2.6.2-IC
`RemoteDeck_PC_v2.6.2` report §6 Carry Item #1("IntegrateController에 attendance 컬럼 추가")의 후속 완료. IC 그리드에 재부재 컬럼(기기ID/IP 뒤·PC 앞) 신규 — `RemoteDeckClient.ParseStatus`가 `/api/status.attendance {enabled,source,current}` 미니 블록을 하위호환 파싱, `StatusFormatter.FormatAttendance`로 재실/부재/미설정 렌더. 로그뷰에 날짜 컬럼(client 수신 시각 yyyy-MM-dd) 추가. 구버전(v2.6.1 이하) 기기는 블록 부재 시 "미설정" 안전 표시 → 필드 14대 혼재 환경 회귀 없음. 후속으로 comment 버전 태그를 v2.6.2 라인으로 통일하고 `.gitignore` publish 경로 오타(`IPSetupTool/publish/` → `IPSetupTool/**/publish/`) 수정. dotnet build 0/0, 실기기 테스트 통과("이상없었어", 2026-09-11). matchRate 100%, SC 7/7 met. commit `0660ef0`(구현) + `9df2767`(정리).
