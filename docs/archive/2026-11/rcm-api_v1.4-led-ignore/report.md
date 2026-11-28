---
template: report
version: 1.1
feature: rcm-led-ignore
date: 2026-11-28
author: KDI
project: RemoteDeckSystem (+ rcm-api)
component: rcm-api v1.3.1 + v1.4.0, RemoteDeck_PC v3.1.1
branch: rcm-api esp32-no-origin (PR #26, 병합 0aac6fc) + led-ignore-v1.4 (PR #27, 병합 efec209), RemoteDeckSystem v3.1.1-hub-no-origin (PR #12, 병합 8868806) + release/v3.1.1-bin (PR #13, 병합 96e87d8)
matchRate: 96
status: completed
---

# 현장 적용 대응: SmartDeck Origin 수정(v1.3.1) + PC LED 없이 전원 운영(v1.4.0) 완료 보고

> **Status**: Complete
>
> **Project**: RemoteDeckSystem + rcm-api
> **Version**: rcm-api 1.4.0 (규격: 호환되는 추가, 태그 `v1.3.1` = `0aac6fc`, `v1.4.0` = `efec209`), RemoteDeck_PC 3.1.1
> **Author**: KDI
> **Completion Date**: 2026-11-28
> **PDCA Cycle**: #1 (현장 장애 2건, 같은 날 대응)

---

## Executive Summary

### 1.1 Project Overview

| Item | Content |
|------|---------|
| Feature | rcm-led-ignore — 현장 적용 중 발견된 두 문제: (1) SmartDeck이 운영 허브에 접속하지 못함, (2) PC LED 인식이 안 되는 방은 끈 뒤 다시 켤 수 없음 |
| Start / End | 2026-11-28 / 2026-11-28 |
| 출발점 | 사용자: "rcm-api 현장에 적용중인데 연결이 안되고 서버에도 기록이 없어." / "일부 장비가 PC LED 인식이 안되는 문제가 있어서 PC LED 무관하게 녹화 장비에서 종료되면 시스템 오프로 바뀌고 서버웹에서 오프로 바뀌고 다시 시스템 켤 수 있도록 … 현재 PC LED 상태를 보고하지 않는 실은 켜는 명령이 안나가." |
| 결과물 | rcm-api PR #26·#27, 태그 v1.3.1·v1.4.0, 공지 이슈 #28(#23 닫음). RemoteDeckSystem PR #12(v3.1.1), #13(배포 파일 FULL/OTA/spiffs) |

### 1.2 Results Summary

```
┌─────────────────────────────────────────────┐
│  Completion Rate: 100% (성공 기준 6/6)        │
├─────────────────────────────────────────────┤
│  ✅ Complete:   FR 11/11 (LED 무시) + Origin │
│  ⏳ Partial:    0                            │
│  ❌ Cancelled:  0                            │
└─────────────────────────────────────────────┘
```

### 1.3 Value Delivered

| Perspective | Content |
|-------------|---------|
| **Problem** | (1) SmartDeck 라이브러리가 기본 `Origin: file://`을 보내 `allowedOrigins`를 설정한 운영 허브가 연결 전에 401로 거부 → 덱은 "연결 중", 허브엔 기록 없음. (2) LED가 켜짐으로 붙은 방은 녹화장비를 꺼도 웹이 "켜짐"이고 `power.on`이 noop(덱도 누르기 거부) → 다시 켤 수 없음. 꺼짐으로 붙은 방은 켜기 실패 뒤 재시도 누름이 부팅 중 PC를 끌 수 있음. |
| **Solution** | (1) `RcmHub::begin()`에서 `setExtraHeaders("")`, conformance 검사 허브도 운영처럼 `allowedOrigins` 설정 → RemoteDeck_PC v3.1.1. 구 펌웨어는 서버 `allowedOrigins`에 `"file://"`(현장 즉시 적용). (2) 허브 설정 `power.ignoreLed`(전체/방 목록): 녹화장비 접속 여부만으로 on/off, 켜기는 `deck.relay.pulse`로 한 번 누르고 에이전트 접속으로 확인(재시도 없음), 덱 ping에 PC가 응답하면 누르지 않음, 끄기는 정상 종료 → 접속 끊김, 시간 초과면 길게 누름. |
| **Function/UX Effect** | 현장 덱이 운영 허브에 붙음. LED가 고장 난 방도 녹화장비에서 끄면 서버 웹이 "꺼짐"이 되고 다시 켤 수 있음. **SmartDeck 펌웨어 업데이트 없이** 허브 설정만으로 적용. LED 정상 방은 동작 그대로. |
| **Core Value** | 현장 배선 상태와 무관하게 방 운영. 장애 두 건을 같은 날 원인 확인 → 임시 조치 → 근본 수정 → 배포까지. |

---

## 1.4 Success Criteria Final Status

| 기준 | 상태 | 근거 |
|------|:----:|------|
| SmartDeck이 `allowedOrigins` 운영 허브에 접속 | ✅ | 현장: `"file://"` 추가 후 접속 확인. 근본 수정: conformance 시험 1건(`Origin` 보내는 장비는 원인 항목 FAIL), ESP32·RemoteDeck_PC 빌드 |
| LED 켜짐/꺼짐 고정 모두 끈 뒤 다시 켜기 | ✅ | `test/led-ignore.test.js` I-01·I-02 |
| ping 응답 중 누르지 않음 | ✅ | I-03 |
| 설정 안 한 방 기존 시험 무수정 통과 | ✅ | 기존 104건 시험 파일 수정 없음, I-07 |
| `npm run check && npm test` | ✅ | 112/112 (main 병합 후 재확인) |
| 사용자 확인 | ✅ | 사용자: "led 무시 api 변경 확인했어" (2026-11-28) |

---

## 2. 경과

| 순서 | 내용 |
|------|------|
| 1 | 덱 로그에 'd'(끊김)만, 허브 기록 없음 → 네트워크(같은 대역, MQTT 공인 브로커 연결 성공)는 정상으로 배제 |
| 2 | 로컬 재현: `Origin` 없음 → 101, `Origin: file://` → 401 `[conn] origin refused file://` |
| 3 | 현장 임시 조치: 서버 `allowedOrigins`에 `"file://"` → 접속 확인 |
| 4 | 근본 수정 v1.3.1 / RemoteDeck_PC v3.1.1 (PR #26, #12) |
| 5 | LED 인식 불가 방: 허브(`actualOf`가 에이전트 끊김 뒤 LED로 복귀, `power.on` LED 켜짐 noop)와 덱(`pc.powerOn`이 LED 켜짐이면 안 누름) 양쪽이 막는 것 확인 |
| 6 | 사용자 결정: 허브 설정(자동 판단 제외), v1.3.1 뒤 별도 v1.4.0, PDCA 문서 |
| 7 | 구현·시험 8건 → PR #27, 태그, 공지 #28, 배포 파일 PR #13 |

## 3. Key Decisions & Outcomes

| 결정 | 근거 | 결과 |
|------|------|------|
| 허브 설정으로 방 지정(자동 판단 아님) | 종료 중 멈춤(LED 켜짐이 맞음)과 LED 고장을 구별할 수 없음 | 오판 없음, 대상 방 고르는 법을 SERVER.md에 기록 |
| `deck.relay.pulse` 사용(덱 `pc.powerOn` 확장 아님) | 현장 덱 OTA 없이 적용 | 펌웨어 변경 0, v3.0 이상 동작 |
| 재시도 누름 없음 | 부팅 중 PC를 끌 수 있음 | 실패는 진단(`no_boot`/`no_agent`/`unknown`)으로 |
| ping 응답이면 누르지 않음 | LED 대신 "켜져 있음"을 알 수 있는 유일한 근거 | 방화벽 ping 차단 방은 보호 없음(문서화) |
| v1.3.1과 v1.4.0 분리 | 규격 변경 없는 수정과 기능 추가를 구분 | v1.4.0 브랜치를 v1.3.1 위에 쌓아 순서대로 병합 |

## 4. 교훈

- **시험 환경이 운영 설정과 달랐다**: 그동안 conformance·실기 시험은 `allowedOrigins` 없이 해서 `Origin` 문제가 드러나지 않았음 → 검사 허브가 운영처럼 `allowedOrigins`를 설정하도록 고침.
- **라이브러리 기본값 확인**: arduinoWebSockets는 생성자에서 `Origin: file://`을 넣음(문서화돼 있지 않음).
- **"LED 신뢰"는 안전장치이자 단일 장애점**: LED 하나가 고장 나면 방 전체가 막혔음. v1.3.0(에이전트 접속 = 켜짐)에 이어, LED 없이도 운영할 수 있는 경로를 둠.

## 5. Carry Items

| 항목 | 담당 |
|------|------|
| 서버 허브 v1.4.0 반영, LED 고장 방 `power.ignoreLed` 설정 | 서버 담당(공지 #28) |
| 현장 덱 v3.1.1 OTA 뒤 `allowedOrigins`에서 `"file://"` 제거 | 서버 담당·현장 |
| LED 무시 방 `os_updating`(업데이트 종료) 경로 전용 시험 | rcm-api |
| PC 방화벽 ICMP 허용(LED 무시 방의 켜진 PC 보호) | 현장 |

## 6. 관련 문서

- [plan.md](plan.md) · [design.md](design.md) · [analysis.md](analysis.md)
- rcm-api: CHANGELOG v1.4.0·v1.3.1, docs/API.md 5.1·6.2, handoff/SERVER.md 4.1
- 관련: [rcm-api_v1.3-multi-encoder](../rcm-api_v1.3-multi-encoder/) (에이전트 접속 = 켜짐, `led_mismatch`)
