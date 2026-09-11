---
template: analysis
version: 1.3
feature: RemoteDeck_PC_v2.6.2-IC
date: 2026-09-11
author: KDI
project: RemoteDeckSystem
component: IntegrateController (client)
consumes: RemoteDeck_PC v2.6.2 /api/status.attendance
match_rate: 100
verification: static build + field runtime (실기기 테스트 통과)
---

# IntegrateController 재부재 통합 Gap Analysis (v2.6.2 Carry Item #1)

**Overall Match Rate**: **100%**

**Origin**: `RemoteDeck_PC_v2.6.2` report §6 Carry Item #1 — "IntegrateController에 attendance 컬럼 추가"
**Baseline**: IC 그리드 (재부재 컬럼 없음) → **Target**: /api/status.attendance 를 그리드·로그뷰에 노출
**Verification**: dotnet build 0/0 + 실기기 테스트 통과 ("이상없었어", 2026-09-11)

---

## Context Anchor

| Key | Value |
|-----|-------|
| **WHY** | v2.6.2 펌웨어가 `/api/status`에 attendance 미니 블록을 노출 → 통합 감시 화면(IC)에서 다수 기기의 재부재 상태를 한눈에 확인할 수단 필요 |
| **WHO** | 통합 관제자 (14대 필드 기기를 IC로 모니터링) |
| **RISK** | 구버전(v2.6.1 이하) 기기 파서 회귀 · Designer 재생성 시 컬럼 유실 · 그리드 컬럼 순서 붕괴 |
| **SUCCESS** | 재부재 컬럼 표시(재실/부재/미설정) · 하위호환 · 로그뷰 날짜 컬럼 · 빌드 0/0 · 실기 검증 |
| **SCOPE** | Models(필드) → Service(파싱) → Formatter(렌더) → Designer/MainForm(컬럼) |

---

## 1. Strategic Alignment

| 항목 | 확인 |
|---|:-:|
| v2.6.2 Recommendation #1 (IC attendance 컬럼) 반영? | ✅ 그리드 재부재 컬럼 신규 |
| v2.6.2 API Contract 준수 (`/api/status.attendance {enabled,source,current}`)? | ✅ 3필드 그대로 파싱 |
| 하위호환 (SC-11: IC 파서 회귀 없음) 유지? | ✅ 블록 부재 시 unknown/미설정 기본 |
| WinForms Designer 3중 방어선 (기존 패턴) 준수? | ✅ .Index 기반 셀 접근, IP 컬럼 런타임 삽입 패턴 재사용 |

---

## 2. Structural Match — 100%

| 파일 | 변경 | 상태 |
|---|:-:|:-:|
| `Models/DeviceStatus.cs` | +6 (AttendanceEnabled/Source/Current) | ✅ |
| `Models/LogEntry.cs` | +5/-1 (ReceivedAt) | ✅ |
| `Services/RemoteDeckClient.cs` | +8 (attendance 블록 파싱) | ✅ |
| `UI/StatusFormatter.cs` | +15 (FormatAttendance) | ✅ |
| `UI/MainForm.Designer.cs` | +13/-2 (ColAttendance) | ✅ |
| `UI/MainForm.cs` | +14/-4 (셀 세팅 2곳 + 로그뷰 날짜 컬럼) | ✅ |

**총계**: 6 files, 54 insertions / 7 deletions (commit `0660ef0`).
**후속 정리**: comment 버전 태그 v2.6.3 → v2.6.2 통일 + `.gitignore` publish 경로 오타 수정 (commit `9df2767`).

---

## 3. Functional Depth — 100%

| # | 요구 | 반영 위치 |
|:-:|---|---|
| SC-1 | `/api/status.attendance` 파싱 | `RemoteDeckClient.ParseStatus` — `attendance` object 존재 시 enabled/source/current 세팅 |
| SC-2 | 그리드 재부재 컬럼 (재실/부재/미설정) | `StatusFormatter.FormatAttendance` + `ColAttendance` |
| SC-3 | 하위호환 (구버전 기기 회귀 없음) | 블록 부재 → `AttendanceCurrent="unknown"` → "미설정" |
| SC-4 | 로그뷰 날짜 컬럼 | `LogEntry.ReceivedAt` (client 수신 시각) + `_colLogDate` yyyy-MM-dd |
| SC-5 | 그리드 컬럼 순서 유지 | IP 런타임 삽입(`ColDeviceId.Index+1`)이 ColAttendance를 한 칸 뒤로 밀어 최종 순서 정합 |
| SC-6 | 빌드 무경고 | dotnet build 0 warnings / 0 errors |
| SC-7 | 실기기 검증 | "이상없었어" (2026-09-11) |

**렌더 규칙** (v2.6.2 색상/의미 규칙과 정합):
- `enabled=false` → **미설정**
- `current=present` → **재실**
- `current=absent` → **부재**
- `unknown`/기타 → **미설정**

**최종 그리드 순서**: No. | 연결 | 기기 이름 | 기기 ID | IP | **재부재** | PC | GPIO | FW | Uptime | Last Seen
**최종 로그 순서**: **날짜** | 시간 | 이벤트 | 상세

---

## 4. API Contract — 100%

| 필드 | v2.6.2 firmware | IC 처리 |
|---|---|---|
| `attendance.enabled` | bool | `AttendanceEnabled` |
| `attendance.source` | "pcled" \| "gpio2" | `AttendanceSource` |
| `attendance.current` | "present" \| "absent" \| "unknown" | `AttendanceCurrent` |
| (블록 부재, v2.6.1 이하) | — | enabled=false/current=unknown 기본 → "미설정" |

**응답 shape 하위호환 100%**. unknown 필드 무시, 블록 부재 안전 처리.

---

## 5. Success Criteria 최종 상태

SC-1 ~ SC-7 **7/7 fully met**. iterate 불필요.

---

## 6. Runtime Evidence

- `dotnet build IntegrateController.sln` → **경고 0개 / 오류 0개** (dotnet 8.0.319, 세션 내 2회 재현)
- 실기기 테스트: 사용자 확인 **"재부재 실기기 테스트 이상없었어"** (2026-09-11)
- 그리드에 재부재 컬럼(재실/부재/미설정) 정상 표시, 로그뷰 날짜 컬럼 정상

---

## 7. Match Rate 최종

```
Overall = Structural × 0.2 + Functional × 0.4 + Contract × 0.4
        = 100 × 0.2 + 100 × 0.4 + 100 × 0.4
        = 100.0%
```

iterate 불필요.

---

## 8. Recommendations (다음 후보)

- 재부재 이력 검색·필터·엑스포트 (v2.6.2 Carry Item #2, 즉시성 낮음)
- 그리드 재부재 컬럼 색상 강조 (재실=초록/부재=빨강 셀 배경) — 현재는 텍스트만
- `/api/attendance/history` 를 IC 로그뷰와 연동 (현재는 device Logger 기반)

---

**Next**: `RemoteDeck_PC_v2.6.2-IC` report
