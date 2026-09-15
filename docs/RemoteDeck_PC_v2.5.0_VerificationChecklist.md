---
feature: RemoteDeck_PC_v2.5
version: 2.5.0
firmware: RemoteDeck_PC_V2.5.0_OTA_20260703.bin (1,377,808 B)
date: 2026-07-03
type: verification-checklist
---

# RemoteDeck_PC v2.5.0 Field Verification Checklist

Plan `docs/01-plan/features/RemoteDeck_PC_v2.5.plan.md` §3.3 Success Criteria 필드 확인용.

> **UPDATE 2026-07-03**: v2.5.0의 SPIFFS OTA가 config 초기화 이슈로 필드 부적합. **v2.5.1을 사용**할 것.
> - `RemoteDeck_PC_V2.5.1_OTA_20260703.bin`
> - `RemoteDeck_PC_V2.5.1_20260703.spiffs.bin`
>
> v2.5.1의 SPIFFS OTA는 `/deviceconfig.json`, `/schedule.json`을 자동 백업/복원. 필드 대량 배포 가능.

## 사전 준비
- [ ] v2.5.1 firmware bin: `RemoteDeck_PC/firmware/RemoteDeck_PC_V2.5.1_OTA_20260703.bin`
- [ ] v2.5.1 spiffs bin: `RemoteDeck_PC/firmware/RemoteDeck_PC_V2.5.1_20260703.spiffs.bin`
- [ ] 대상 기기 1대 이상 v2.4.7 부팅 상태
- [ ] IntegrateController 최신 빌드 (net8.0-windows / win-x64)
- [ ] 웹 브라우저 접속 계정 (admin/*)

## OTA 배포 (2단계, v2.5.1)

### 1단계: 펌웨어 업로드 (app 파티션)
1. 대상 기기 웹 UI 접속 → 기존 `펌웨어` 탭에서 `RemoteDeck_PC_V2.5.1_OTA_20260703.bin` 업로드
2. 자동 재부팅 후 확인:
   - 상단 헤더 버전 스탬프 = `2.5.1`
   - **웹 UI는 아직 v2.4.7 그대로** (탭이 `펌웨어`로 표시됨) — 정상
   - `Serial` 로그에 `OTA: target = APP partition (firmware) [RemoteDeck_PC_V2.5.1_OTA_20260703.bin]`

### 2단계: 웹 UI 업로드 (SPIFFS 파티션, 설정 자동 보존)
3. 웹 UI 접속 후 `펌웨어` 탭 진입 (아직 관리 탭 없음, SPIFFS가 v2.4.7이라 정상)
   - `펌웨어 업데이트` 파일 선택 → `RemoteDeck_PC_V2.5.1_20260703.spiffs.bin` 업로드
4. 시리얼 로그 확인 (v2.5.1 OTAHandler):
   ```
   OTA: target = SPIFFS partition (web assets) [RemoteDeck_PC_V2.5.1_...spiffs.bin]
   OTA: backed up /deviceconfig.json (nnn bytes)
   OTA: backed up /schedule.json (nnn bytes)
   OTA: Update complete (196608 bytes)
   OTA: restored /deviceconfig.json (nnn bytes)
   OTA: restored /schedule.json (nnn bytes)
   OTA: Update successful, rebooting...
   ```
5. 자동 재부팅 후 확인:
   - 상단 탭 = `홈 | 제어 | 스케줄 | 설정 | 관리 | 로그`
   - `관리` 탭: `기기 관리` / `펌웨어 업데이트` / `웹 UI 업데이트` 3개 카드
   - **기존 설정 유지**: 네트워크·MQTT·계정·WebRequest URL·기존 릴레이 스케줄 모두 이전 값 그대로

> **중요**: 이후 배포는 관리 탭에서 순서 무관 firmware.bin / *.spiffs.bin 두 파일 모두 업로드 가능. SPIFFS OTA는 항상 설정 파일 자동 백업/복원됨.

---

## Success Criteria 검증

### SC-1: 즉시 재부팅
- [ ] 관리 탭 진입 → `즉시 재부팅` 섹션에 `재부팅` 버튼 표시
- [ ] 버튼 클릭 → 확인 dialog "기기를 지금 재부팅합니다..."
- [ ] 확인 클릭 → 5초 이내 기기 재부팅 시작
- [ ] 부팅 완료 후 재접속 정상

### SC-2: 재부팅 스케줄 등록·실행
- [ ] 관리 탭 → 재부팅 스케줄 폼에 시간(예: 09:00) + 요일(예: 화) 입력
- [ ] `추가` 클릭 → 재부팅 스케줄 목록에 `🔁 재부팅 [화] 09:00 활성` 표시
- [ ] 대상 시각 도래 → 기기 자동 재부팅 (**±1분 이내**)
- [ ] Serial/로그 탭에 `Schedule: reboot triggered` 확인

### SC-3: 릴레이 스케줄 하위호환
- [ ] v2.4.7 시절 등록한 릴레이 스케줄이 있는 기기 대상
- [ ] v2.5.0 OTA 후 `스케줄` 탭 진입 → 기존 릴레이 스케줄 목록 손실 없음
- [ ] 이후 릴레이 스케줄이 정상 실행됨

### SC-4: IntegrateController 로그 뷰 로드
- [ ] IntegrateController 실행 → 기기 리스트에서 대상 기기 클릭
- [ ] 우측 하단 로그 뷰 패널 표시 (시간 / 이벤트 / 상세 3컬럼)
- [ ] 5초 이내 최근 이벤트 목록 로드

### SC-5: 로그 실시간 반영 (≤5s)
- [ ] IntegrateController 로그 뷰 열어둔 상태 유지
- [ ] RD_PC 웹 UI에서 `릴레이1 펄스` 클릭 → RELAY 로그 발생
- [ ] IntegrateController 로그 뷰에 5초 이내 `RELAY Relay1 ON` 표시

### SC-6: 부팅 sync (GPIO/PCLED URL 호출)
- [ ] 설정 → Web Request 탭에서 `gpio1_high` / `gpio1_low` URL 등록 (예: `http://192.168.1.100:8080/gpio1?[value]`)
- [ ] GPIO1 물리적으로 HIGH 상태 확인
- [ ] 기기 재부팅
- [ ] 부팅 완료 후 15초 이내 등록된 URL(gpio1_high)이 1회 호출됨 확인 (외부 수신측 로그 or Wireshark)
- [ ] 로그 탭에 `BOOT_SYNC gpio1_high` 이벤트 확인

### SC-7: URL 미설정 채널 skip
- [ ] Web Request 탭에서 gpio2_high/low URL 비어 있는 상태
- [ ] 기기 재부팅
- [ ] gpio2 관련 HTTP 호출 없음 (외부 수신측 로그 확인)
- [ ] 로그에 `BOOT_SYNC gpio2_*` 는 남지만 실제 HTTP 호출은 없음 (WEBREQ 로그 없음)

### SC-8: v2.4.7 방어선 무결성
- [ ] `git diff v2.4.7..HEAD -- RemoteDeck_PC/src/network/NetManager.h RemoteDeck_PC/src/network/NetManager.cpp` = **empty**
- [ ] 콜드 부팅 시험: 안정 아답터에서 정상 부팅 (v2.4.7과 동일)

### SC-9: Flash/Heap
- [ ] 빌드 로그: Flash 사용량 v2.4.7 대비 ≤ +10KB
  - 이미 검증됨: v2.4.7 1,376,240 B → v2.5.0 1,377,448 B = **+1,208 B** ✅
- [ ] 부팅 후 Serial `heap_free` ≥ 100KB (`GET /api/status` `heap_free` 필드)

### SC-10: 필드 dogfood (1주)
- [ ] 매일 03:00 자동 재부팅 성공률 ≥ 99% (7일 중 7회)
- [ ] IntegrateController 로그 뷰 안정성 (memory leak, freeze 없음)
- [ ] WebRequest 부팅 sync 실패율 ≤ 5% (외부 수신 로그 or 재부팅 20회 중 최소 19회 성공)

---

## 회귀 검증 (Regression)
- [ ] `POST /api/reboot`: 웹 UI 재부팅 버튼 v2.4.7 flush 정책 유지 동작
- [ ] `GET /api/log`: 응답 shape `{"logs":[...]}` 유지
- [ ] `GET /api/schedule`: 응답에 reboot action 포함되며 기존 파서 호환
- [ ] Home/제어/스케줄/설정/로그 탭 기존 기능 모두 정상

---

## 이상 발생 시
- 문제 재현 로그를 `logs/2026-07-03/` 에 저장
- `/pdca analyze RemoteDeck_PC_v2.5` 로 gap analysis 진입
- 심각한 회귀는 v2.4.7 롤백 (`RemoteDeck_PC_V2.4.7_OTA_20260701.bin` 재플래시)
