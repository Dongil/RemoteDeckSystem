---
template: design
version: 1.0
feature: ic-pc-power
date: 2026-11-28
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
status: archived
---

# IntegrateController "PC 전원" 켜기 버튼 Design Document

| Key | Value |
|-----|-------|
| **WHY** | 허브·서버 웹·LED가 안 될 때 PC를 켤 비상 수단 |
| **WHO** | 현장 관리자 |
| **RISK** | 켜진 PC 종료 → 매번 확인 + LED 경고 / 연타 → 잠금 / 더블클릭 편집 창 → 제외 |
| **SUCCESS** | 허브 모드에서만 보임, 확인 후 펄스, 빌드 0/0, 실기 확인 |
| **SCOPE** | RemoteDeckClient.cs, MainForm.cs |

## 1. Overview

선택: Option C(기존 패턴 재사용). 새 파일 없이 IP 컬럼·표시 컬럼 선택과 같은 방식으로 `MainForm`에서 컬럼을 코드 생성한다(WinForms Designer 3중 방어선 유지). 장비 API는 기존 `POST /api/relay` 펄스(RemoteDeck_PC v2.2+, v3.0 허브 사용 중에도 펄스 허용)라 펌웨어 변경 없음.

## 2. API (`RemoteDeckClient.PulseRelayAsync`)

```
POST {BaseUrl}/api/relay   Authorization: Basic
{"cmd":"pulse","relay":1,"duration":500}
200 → Ok   401 → "인증 실패 (401)"   409 → 응답 본문 error   그 밖 → "HTTP n"   timeout → "timeout"
```

## 3. UI (`MainForm`)

- `_colPcPower = new DataGridViewButtonColumn { Name="ColPcPower", HeaderText="PC 전원", UseColumnTextForButtonValue=false, FillWeight=10, Visible=false, FlatStyle=Popup }`, `ColAttendance` 다음 위치에 `Insert` (IP 컬럼 삽입 뒤에 호출).
- `SetFeatureMode`: `_colPcPower.Visible = mode == Hub`.
- `SetPcPowerCell(row, status)`: 온라인이면 "켜기", 아니면 "-". `ApplyStatusToRow`·`RefreshAllUnboundCells` 두 곳에서 호출.
- `grid.CellContentClick`: `e.ColumnIndex == _colPcPower.Index` → `PcPowerClickAsync(device)`.
- `grid.CellDoubleClick`: 버튼 컬럼이면 편집 창을 열지 않음.
- `PcPowerClickAsync`:
  1. `_pulsing`(HashSet) 진행 중이면 무시
  2. 상태 없음·오프라인 → 상태줄 "오프라인: 켜기 불가"
  3. 확인 창(Warning, 기본 "아니요"): 방 이름(IP), "PC 전원 버튼을 짧게 누릅니다(릴레이1 0.5초).", "현재 PC LED: ON/OFF". LED ON이면 "⚠ PC가 켜져 있으면 이 펄스로 Windows가 종료됩니다. LED 고장으로 실제로 꺼져 있을 때만 누르세요." 허브 연결 장비면 "RCM 허브를 거치지 않고 장비에 직접 보냅니다."
  4. `PulseRelayAsync` → 상태줄 "켜기 펄스 전송: 이름 (RTT n ms)" / 실패 메시지 창

## 8. Test Plan

| ID | 시험 |
|----|------|
| T-01 | 재부재 모드: 컬럼 숨김 / RCM 허브 모드: RCM 허브 오른쪽에 보임, 전환 반복 |
| T-02 | 오프라인 행 "-", 눌러도 요청 없음 |
| T-03 | 확인 창 "아니요" → 요청 없음 / "예" → 200, 장비 로그 릴레이1 펄스 |
| T-04 | LED ON 장비: 경고 문구 표시 |
| T-05 | 버튼 더블클릭 → 편집 창 안 열림 |
| T-06 | 잘못된 비밀번호 → 401 메시지 |
| T-07 | `dotnet build` 0/0 |

## 11. Implementation Guide

1. `RemoteDeckClient.PulseRelayAsync`
2. `MainForm`: `AddPcPowerColumn`, `SetPcPowerCell`, `SetFeatureMode` 가시성, 클릭·더블클릭 처리, `PcPowerClickAsync`

### 11.3 Session Guide

한 세션(모듈 1개).
