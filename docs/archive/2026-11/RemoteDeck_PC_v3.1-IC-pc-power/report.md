---
template: report
version: 1.1
feature: ic-pc-power
date: 2026-11-28
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
branch: ic-pc-power
matchRate: 97
status: completed
---

# IntegrateController "PC 전원" 비상 켜기 버튼 완료 보고

> **Status**: Complete
>
> **Project**: RemoteDeckSystem
> **Component**: IntegrateController (소비 API: RemoteDeck_PC `POST /api/relay` 펄스 v2.2+, RCM 허브 컬럼 v3.0+. 펌웨어 변경 없음)
> **Author**: KDI
> **Completion Date**: 2026-11-28

---

## Executive Summary

### 1.1 Project Overview

| Item | Content |
|------|---------|
| Feature | ic-pc-power — 표시 컬럼 "RCM 허브"일 때 행마다 "켜기" 버튼, 누르면 장비에 릴레이1 펄스 |
| Start / End | 2026-11-28 / 2026-11-28 |
| 출발점 | 사용자: "컬럼 표시: RCM 허브일 때 RCM허브 컬럼 옆에 'PC 전원' 컬럼 추가해서 '켜기' 실별로 버튼 나오게 해서 누르면 relay 1 펄스 명령 보내고 싶어. 비상시 강제로 켜는 목적" |
| 결과물 | `Services/RemoteDeckClient.cs`(+47), `UI/MainForm.cs`(+120), `UI/MainForm.Designer.cs`(헤더 1줄), 배포 `publish/IntegrateController.exe` |

### 1.2 Results Summary

```
┌─────────────────────────────────────────────┐
│  Completion Rate: 100% (성공 기준 4/4)        │
├─────────────────────────────────────────────┤
│  ✅ Complete:   FR 7/7                       │
│  ⏳ Partial:    0                            │
│  ❌ Cancelled:  0                            │
└─────────────────────────────────────────────┘
```

### 1.3 Value Delivered

| Perspective | Content |
|-------------|---------|
| **Problem** | RCM 허브·서버 웹·PC LED 중 하나라도 문제가 있으면 원격으로 PC를 켜려면 장비 웹 UI에 하나씩 들어가야 했다. |
| **Solution** | IntegrateController 그리드에 "PC 전원" 버튼 컬럼(RCM 허브 모드). 누르면 확인 창(방·IP·PC LED, LED 켜짐이면 "켜진 PC는 종료됨" 경고, 기본 "아니요") 뒤 `POST /api/relay` 릴레이1 0.5초 펄스. |
| **Function/UX Effect** | 한 화면에서 방마다 비상 켜기. 셀보다 작은 입체 버튼으로 버튼임이 분명하고, "PC" 컬럼은 "PC LED"로 이름을 바꿔 버튼 컬럼과 구별. |
| **Core Value** | 허브·서버와 무관한 마지막 수단. 펌웨어 변경 없음. 잘못 누름은 매번 확인으로 막음. |

## 1.4 Success Criteria Final Status

| 기준 | 상태 | 근거 |
|------|:----:|------|
| FR-01~07 | ✅ | analysis §1 |
| `dotnet build` 0/0 | ✅ | Debug 빌드, publish 성공 |
| 실기 펄스 | ✅ | 사용자 실기 "켜지고 꺼지고 하는 동작됨" |
| matchRate ≥ 90% | ✅ | 97% |

## 2. Key Decisions & Outcomes

| 결정 | 근거 | 결과 |
|------|------|------|
| 장비에 직접 펄스(허브 `power.on` 아님) | 비상 수단은 허브·서버가 안 될 때 써야 함 | 허브 사용 중에도 펌웨어가 펄스 허용(409는 latch만) |
| 매번 확인 창, LED 켜짐은 경고만(막지 않음) | LED 고장 방에서도 켤 수 있어야 함(사용자 결정) | 기본 "아니요"로 실수 방지 |
| 컬럼은 코드 생성 | WinForms Designer 3중 방어선 | Designer는 기존 컬럼 헤더 1줄만 수정 |
| 버튼 모양 변경(실기 피드백) | "셀과 크기가 비슷해 버튼인지 모름" | 입체 버튼, 고정 폭 80px, 셀 안쪽 여백 |
| "PC" → "PC LED"(사용자 요청) | 새 "PC 전원" 버튼 컬럼과 혼동 | 모든 모드에 적용 |

## 3. Carry Items

| 항목 | 비고 |
|------|------|
| 끄기(길게 누름) 버튼 | 요청 범위 밖. 필요하면 같은 구조로 추가 가능 |

## 4. 관련 문서

- [plan.md](plan.md) · [design.md](design.md) · [analysis.md](analysis.md)
- 관련: [RemoteDeck_PC_v3.0-IC](../RemoteDeck_PC_v3.0-IC/) (표시 컬럼 전환), [rcm-api_v1.4-led-ignore](../rcm-api_v1.4-led-ignore/) (LED 고장 방)
