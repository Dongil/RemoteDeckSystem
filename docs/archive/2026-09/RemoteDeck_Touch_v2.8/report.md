---
template: report
version: 1.0
feature: RemoteDeck_Touch_v2.8_ServerConfigFetch
date: 2026-09-16
author: KDI
project: RemoteDeckSystem
component: RemoteDeck_Touch (firmware)
branch: v2.8-touch-serverfetch
matchRate: 98
status: completed
---

# RemoteDeck_Touch v2.8 — 완료 보고 (서버 설정/이미지 불러오기 정리)

## Executive Summary

| Perspective | Content |
|-------------|---------|
| **Problem** | LCD "서버설정/이미지 불러오기"가 deviceconfig(개별 IP/deviceID)까지 덮어씀, `imageUrl` 오용, httpPort 미반영(항상 80), 피드백 없이 무조건 재부팅, 이미지 이원경로(중복+서버본 미반영). 실기 중 `server_url` 트레일링 슬래시로 다운로드 실패도 발견. |
| **Solution** | 설계의도대로 serverconfig·id 이미지만 서버 일괄 / deviceconfig는 LCD·웹 전용. 이미지 단일 경로 `/images/` 통합, httpPort 반영, parseAddress host 정규화, 재부팅 확인창+결과 피드백. |
| **Function/UX Effect** | 대량 설치 시 공통 정보는 서버에서 즉시 수신(개별 IP 유실 없음), 불러오기 시 확인·성공/실패 피드백, 웹 교체/서버 다운로드가 같은 파일로 SPIFFS 절약. |
| **Core Value** | 안전한 대량 설치/복구 플로우 확립: FULL 플래시 → LCD 네트워크 입력 → 서버설정/이미지 불러오기 → 이후 OTA. |

### 1.3 Value Delivered
| Perspective | Metric | 결과 |
|-------------|--------|------|
| 안정성 | deviceconfig 보존 | ✅ 서버 불러오기 후 개별 IP/deviceID 유지 (실기) |
| 정확성 | URL 조립 | ✅ host:port 정규화, 다운로드 성공 (실기) |
| 효율 | 이미지 저장 | ✅ 단일 경로(중복 제거) |
| 품질 | matchRate | **98%** |

---

## 2. Success Criteria — Final Status
| 기준 | 상태 | 근거 |
|------|:--:|------|
| serverconfig만 갱신, deviceconfig 보존 | ✅ Met | 실기 (#1) |
| 이미지 반영, 단일 경로 | ✅ Met | 실기 (id 이미지 재부팅 표시) |
| 확인창·피드백, 실패 시 재부팅 안 함 | ✅ Met | 실기 (#2) |
| httpPort/parseAddress URL 정상 | ✅ Met | 실기 (다운로드 성공) |
| `pio run` 빌드 성공 | ✅ Met | 반복 빌드 SUCCESS |

**Success Rate: 5/5**

---

## 3. Key Decisions & Outcomes
| 결정 (출처) | 선택 | 결과 |
|------|------|------|
| [Plan] deviceconfig 서버 수신 | **제거**(LCD/웹 전용) | ✅ 개별 IP 유실·충돌 위험 제거 |
| [Design] 이미지 경로 | 초기 `/images/`>`/download/` → **단일 `/images/`** | ✅ 서버본 미반영·SPIFFS 중복 동시 해소(사용자 피드백) |
| [Design] in/out | 임베드 유지(A) | ✅ 공통·불변, 항상 표시, 정리 안전 |
| [Do] parseAddress | host 정규화 | ✅ 트레일링 슬래시 다운로드 실패 수정 |
| [Do] 불러오기 실행 | LVGL 콜백 밖(loop) + 확인/결과창 | ✅ UI 안정 + 피드백 |

---

## 4. 변경 파일
| 파일 | 변경 |
|------|------|
| `src/main.cpp` | fetchServerInfo(serverconfig 전용, bool) / fetchImageFiles(역할고정→/images/, bool) / 포트 반영 / 확인·결과창 + g_pendingFetch + runPendingFetch / loop 배선 |
| `src/utils/TypeUtils.cpp` | parseAddress host 정규화(경로·슬래시 제거) |
| `src/images/images.cpp` | try_set 단일 경로 /images/ |
| `src/device/DeviceManager.cpp` | 버튼 → promptFetch*(확인창) |

commit `3446930`.

---

## 5. 배포 플로우 (사용자 확정)
1. 오래된/불명 기기: **FULL 플래시**(huge_app 파티션 확인) → 초기화
2. **LCD에서 deviceID + 네트워크(serverURL) 입력** (개별 정보)
3. **서버설정 불러오기**(serverconfig 공통) → **이미지 불러오기**(id별 → /images/)
4. 이후 **OTA 업그레이드**(app만, SPIFFS 보존)

> 순서: serverURL·deviceID 를 불러오기 **전에** 입력해야 서버 접속/경로 치환이 됨.

---

## 6. Carry Items
| # | 항목 | 심각도 |
|---|------|:--:|
| C-1 | non-80 포트 실서버 다운로드 검증 | Low |
| C-2 | 배포용 V2.8.0 FULL/OTA firmware bin 재빌드 | Low |

---

## 7. Lessons Learned
- **트레일링 슬래시 + 포트 조립**: `server_url` 끝 슬래시가 host 에 남아 `:port` 삽입 시 malformed. 주소 파서는 항상 host 만 정규화해 반환해야 함. 기존 이중슬래시는 서버 관용으로 가려져 있었음.
- **이미지 이원경로의 함정**: 기본 이미지가 있는 `/images/`와 서버 `/download/`를 분리하면, 우선순위에 따라 한쪽이 영구히 가려짐 + 중복 저장. 소스가 여럿이어도 **표시 경로는 하나**로 통합하는 게 안전.
- **공통 vs 개별 자산 분리**: 개별(deviceconfig)은 로컬, 공통(serverconfig·id 이미지)은 서버 — 대량 설치의 핵심 경계.

---

## 8. 다음 단계
- Archive: `docs/archive/2026-09/RemoteDeck_Touch_v2.8/`
- PR: main ← v2.8-touch-serverfetch
- (배포 시) V2.8.0 firmware bin 재빌드
