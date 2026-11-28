---
template: plan
version: 1.0
feature: ic-pc-power
date: 2026-11-28
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
origin: 사용자 요청 (2026-11-28) — 비상시 PC 강제 켜기
status: archived
---

# IntegrateController "PC 전원" 켜기 버튼 Planning Document

> **Summary**: 표시 컬럼이 "RCM 허브"일 때 RCM 허브 컬럼 옆에 "PC 전원" 컬럼을 두고, 행(방)마다 "켜기" 버튼으로 장비에 릴레이1 펄스(`POST /api/relay {"cmd":"pulse","relay":1}`)를 보낸다. 허브·서버 웹이 안 될 때 관리자가 직접 켜는 비상 수단.
>
> **Date**: 2026-11-28

---

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | RCM 허브나 서버 웹에 문제가 있거나 LED 인식이 안 되는 방은 원격으로 PC를 켤 방법이 장비 웹 UI에 하나씩 들어가는 것뿐이다. |
| **Solution** | IntegrateController 그리드에 "PC 전원" 버튼 컬럼(RCM 허브 표시 모드에서만). 누르면 확인 창(방 이름·IP·현재 PC LED, LED 켜짐이면 "켜진 PC는 종료됨" 경고) 뒤 릴레이1 짧은 펄스. |
| **Function/UX Effect** | 관리자가 한 화면에서 여러 방을 비상으로 켤 수 있음. 펌웨어 변경 없음(v2.2부터 있는 API, RCM 허브 사용 중에도 펄스 허용). |
| **Core Value** | 허브·서버와 무관한 마지막 수단. 잘못 눌러 켜진 PC를 끄지 않도록 매번 확인. |

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | 허브·서버 웹·LED가 안 될 때 PC를 켤 비상 수단 |
| **WHO** | 현장 관리자(IntegrateController 사용자) |
| **RISK** | 켜진 PC에 짧은 펄스 = Windows 종료 → 매번 확인 창 + LED 켜짐 경고(LED 고장 가능성 때문에 막지는 않음, 사용자 결정). 버튼 연타 → 장비별 진행 중 잠금. 더블클릭이 편집 창을 엶 → 버튼 컬럼 제외 |
| **SUCCESS** | RCM 허브 모드에서만 컬럼 보임, 확인 후 펄스 200 OK, 오프라인 행은 누를 수 없음, `dotnet build` 0/0, 실기 펄스 확인 |
| **SCOPE** | `Services/RemoteDeckClient.cs`(PulseRelayAsync), `UI/MainForm.cs`(버튼 컬럼 코드 생성·클릭·확인 창). Designer 미수정 |

---

## 1. Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | "PC 전원" 버튼 컬럼을 RCM 허브 컬럼 바로 오른쪽에 코드로 생성(Designer 미수정, 인덱스는 `.Index`로만) | High |
| FR-02 | 표시 컬럼 = RCM 허브일 때만 보임, 재부재면 숨김 | High |
| FR-03 | 온라인 행 버튼 "켜기", 오프라인·폴링 전 행 "-"(눌러도 동작 없음, 상태줄 안내) | High |
| FR-04 | 누르면 확인 창: 방 이름·IP·현재 PC LED(ON/OFF). LED ON이면 "켜져 있는 PC는 이 펄스로 종료됨" 경고. 기본 버튼 "아니요" | High |
| FR-05 | 확인 시 `POST /api/relay {"cmd":"pulse","relay":1,"duration":500}` (Basic 인증). 결과를 상태줄에 표시, 실패는 메시지 창(401, 409, timeout 등) | High |
| FR-06 | 같은 장비 펄스 진행 중 재요청 무시 | Medium |
| FR-07 | 버튼 셀 더블클릭이 편집 창을 열지 않음 | Medium |

## 2. Out of Scope

- 끄기(길게 누름) 버튼 — 요청 범위 밖
- 허브 `power.on` 경유 — 비상 수단이라 장비에 직접 보냄
- 여러 방 한꺼번에 켜기

## 3. Success Criteria

- [ ] FR-01 ~ FR-07
- [ ] `dotnet build` 경고 0 / 오류 0
- [ ] 실기(사무실 장비) 펄스 → 장비 로그에 릴레이1 펄스
- [ ] gap ≥ 90%

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-11-28 | Initial (사용자 결정: 매번 확인 창, PDCA 문서) | KDI |
