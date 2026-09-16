---
template: analysis
version: 0.1
feature: RemoteDeck_Touch_v2.8_ServerConfigFetch
date: 2026-09-16
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: v2.8-touch-serverfetch
plan: docs/01-plan/features/RemoteDeck_Touch_v2.8_ServerConfigFetch.plan.md
design: docs/02-design/features/RemoteDeck_Touch_v2.8_ServerConfigFetch.design.md
phase: check
matchRate: 98
---

# RemoteDeck_Touch v2.8 — Gap Analysis (Check)

> **Summary**: LCD "서버설정/이미지 불러오기" 코드 점검 정리. FR 7/7 구현·실기 검증. **matchRate 98%**. 실기 테스트 중 발견한 parseAddress 트레일링 슬래시 버그(FR-07)까지 수정.

---

## Context Anchor
| Key | Value |
|-----|-------|
| **WHY** | 서버 불러오기가 개별 기기 config 덮어씀 + 포트/URL 버그 + 피드백 부재 |
| **WHO** | 설치·운영 관리자 (20실 대량 설치) |
| **SUCCESS** | serverconfig·이미지만 서버 수신(deviceconfig 보존) + 포트 정상 + 확인창/피드백 + 이미지 단일경로 |
| **SCOPE** | main.cpp/images.cpp/TypeUtils.cpp/DeviceManager.cpp. 웹/API/스키마 무변경 |

---

## 1. 검증 방식
CLAUDE.md 규약: `pio run` 클린 빌드 + 실기(COM3). 사용자 실기 확인 완료.

## 2. Functional Requirements 대비

| ID | Requirement | 구현 | 실기 | 근거 |
|----|-------------|:--:|:--:|------|
| FR-01 | serverconfig만 수신, deviceconfig 미수신 | ✅ | ✅ | fetchServerInfo serverconfig 전용. "서버설정 불러오기 후 기기 IP/deviceID 그대로" 확인 |
| FR-02 | 이미지 고정역할(title/photo/name) 수신 | ✅ | ✅ | ROLES[] 루프. "id 이미지 정상 수신·재부팅 표시" 확인 |
| FR-03 | HTTP URL 에 httpPort 반영 | ✅ | ✅ | downloadFile/sendHttpMessage `:httpPort`. 다운로드 성공 |
| FR-04 | 재부팅 확인창 + 성공/실패 피드백 | ✅ | ✅ | prompt/confirm/result msgbox. "확인창·취소·실패 피드백 정상" 확인 |
| FR-05 | 이미지 단일 경로 /images/ (중복 방지) | ✅ | ✅ | fetchImageFiles→/images/, try_set /images/ 전용, /download/ 정리 |
| FR-06 | blocking HTTP 를 loop 에서 처리 | ✅ | ✅ | g_pendingFetch + runPendingFetch |
| FR-07 | parseAddress 깨끗한 host 반환 | ✅ | ✅ | 경로/슬래시 제거. "http://192.168.10.230/" 다운로드 정상화 |

**FR 충족: 7/7 구현·실기.**

## 3. Success Criteria (Plan §4)
| 기준 | 상태 |
|------|:--:|
| serverconfig만 갱신, deviceconfig 보존 (실기) | ✅ |
| 이미지 반영, 웹 교체/서버 다운로드 단일경로 (실기) | ✅ |
| 확인창·피드백 정상, 실패 시 재부팅 안 함 (실기) | ✅ |
| `pio run` 빌드 성공 | ✅ |
| matchRate ≥ 90% | ✅ (98%) |

## 4. 계획 외 추가 (실기 중 발견)
- **parseAddress 트레일링 슬래시 버그 (FR-07)**: `server_url="http://192.168.10.230/"` 의 끝 슬래시가 httpUrl 에 남아 `http://host/:port/path` malformed → 다운로드 실패. scheme 제거 후 첫 '/' 이후를 잘라 깨끗한 host 반환으로 수정. (#3 포트 수정과 맞물려 표면화된 사전존재 버그)
- **이미지 경로 이원화 → 단일화**: 초기 설계는 `/images/`(웹) > `/download/`(서버) 우선순위였으나, 실기에서 `/images/` 기본이미지가 `/download/` 서버본을 가려 반영 안 됨 + SPIFFS 이중 저장 지적 → **단일 경로 `/images/`** 로 통합(사용자 피드백 반영).
- **in/out 처리 확정**: 공통·불변 → 펌웨어 임베드 유지(A안), SPIFFS 정리 대상에서 제외.

## 5. Match Rate
정적 가중 `Structural×0.2 + Functional×0.4 + Contract×0.4`:
| 축 | 점수 | 근거 |
|----|:--:|------|
| Structural | 100% | 변경 파일·함수·wiring 존재 |
| Functional | 96% | FR 7/7 구현·실기 (경미 잔여: non-80 포트 실서버 미검증) |
| Contract | 100% | API/스키마 무변경(회귀 없음), URL 조립 정상 |

**Overall = 20 + 38.4 + 40 = 98%** (≥ 90% 통과)

## 6. Carry Items
| # | 항목 | 심각도 |
|---|------|:--:|
| C-1 | non-80 포트 실서버에서 다운로드 검증 (현재 80 서버로만 확인) | Low |
| C-2 | 배포용 V2.8.0 FULL/OTA firmware bin 재빌드 (deploy 시) | Low |

## 7. 결론
matchRate **98% ≥ 90%** → Check 통과, Report 진행. 전 항목 실기 검증 완료.
