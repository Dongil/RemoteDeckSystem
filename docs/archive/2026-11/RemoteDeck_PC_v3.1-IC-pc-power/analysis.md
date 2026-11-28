---
template: analysis
version: 1.0
feature: ic-pc-power
date: 2026-11-28
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
status: archived
matchRate: 97
---

# IntegrateController "PC 전원" 켜기 버튼 Gap Analysis

| Key | Value |
|-----|-------|
| **WHY** | 허브·서버 웹·LED가 안 될 때 PC를 켤 비상 수단 |
| **WHO** | 현장 관리자 |
| **RISK** | 켜진 PC 종료 → 매번 확인 + LED 경고 / 연타 → 잠금 / 더블클릭 편집 창 → 제외 |
| **SUCCESS** | 허브 모드에서만 보임, 확인 후 펄스, 빌드 0/0, 실기 확인 |
| **SCOPE** | RemoteDeckClient.cs, MainForm.cs (+ Designer 헤더 1줄) |

## 1. Requirements

| ID | 결과 | 근거 |
|----|------|------|
| FR-01 버튼 컬럼 코드 생성, RCM 허브 오른쪽 | ✅ | `MainForm.AddPcPowerColumn` (`IndexOf(ColAttendance)+1`, 인덱스는 `.Index`로만) |
| FR-02 RCM 허브 모드에서만 보임 | ✅ | `SetFeatureMode` `_colPcPower.Visible`, 사용자 화면 확인 |
| FR-03 온라인 "켜기", 오프라인 "-" | ✅ | `SetPcPowerCell` (두 셀 세팅 경로 모두) |
| FR-04 확인 창(이름·IP·LED, LED ON 경고, 기본 "아니요") | ✅ | `PcPowerClickAsync`, 사용자 화면 확인 |
| FR-05 `POST /api/relay` 펄스, 결과 표시 | ✅ | `RemoteDeckClient.PulseRelayAsync`, 목 장비 시험(200/401/409/timeout), 실기 |
| FR-06 진행 중 재요청 무시 | ✅ | `_pulsing` |
| FR-07 버튼 더블클릭이 편집 창 안 엶 | ✅ | `CellDoubleClick` 컬럼 검사 |

## 2. Success Criteria

| 기준 | 결과 |
|------|------|
| FR-01~07 | ✅ |
| `dotnet build` 0/0 | ✅ 경고 0 / 오류 0 |
| 실기 펄스 | ✅ 사용자 실기: "켜지고 꺼지고 하는 동작됨" |
| gap ≥ 90% | ✅ 97% |

## 3. 설계와 다른 점 (실기 피드백)

| 항목 | 내용 |
|------|------|
| 버튼 모양 | 설계는 `FlatStyle.Popup` + `FillWeight`. 실기에서 "셀 크기와 비슷해 버튼인지 모름" → `FlatStyle.Standard`(입체) + 고정 폭 80px + 셀 안쪽 여백(12,2,12,2)으로 셀보다 작은 버튼 |
| PC 컬럼 이름 | 사용자 요청으로 모든 모드에서 "PC" → "PC LED"("PC 전원" 버튼과 구별). `MainForm.Designer.cs` 헤더 텍스트 1줄만 수정(컬럼 구성은 그대로라 Designer 재생성에도 유지) |

## 4. 시험

- 목 장비(Node, 포트 4개) + 빌드된 어셈블리를 PowerShell에서 호출: 200 → Ok, 401 → "인증 실패 (401)", 409 → "HTTP 409: relay1 latch blocked…", 무응답 → "timeout". 요청은 `POST /api/relay`, `{"cmd":"pulse","relay":1,"duration":500}`, Basic 인증.
- 사용자 실기·화면 확인(켜기·끄기 동작, 컬럼 표시, 확인 창, 버튼 모양).

**Match Rate: 97%** (설계 변경 2건은 사용자 피드백 반영)
