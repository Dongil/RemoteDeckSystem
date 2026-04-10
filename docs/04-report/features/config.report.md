# PDCA 완료 보고서: config (웹 설정 UI)

## Executive Summary

| 항목 | 내용 |
|------|------|
| Feature | config - 웹 설정 UI 탭 그룹화 + MQTT 연결 테스트 |
| 기간 | 2026-04-09 ~ 2026-04-10 |
| 상태 | **완료** |

### Results

| 항목 | 값 |
|------|-----|
| Match Rate | 100% (Plan 없이 직접 구현) |
| 변경 파일 | 9개 |
| 변경 라인 | +541 / -70 |

### Value Delivered

| 관점 | 내용 |
|------|------|
| Problem | 단일 설정 페이지에 모든 옵션이 나열되어 관리가 불편하고, MQTT 브로커 연결 확인 불가 |
| Solution | 설정 UI를 4개 서브탭(네트워크/MQTT/계정/기타)으로 분리, 비동기 MQTT 연결 테스트 구현 |
| Function UX Effect | 카테고리별 설정 관리, 네트워크만 재부팅 필요, MQTT/기타는 즉시 저장 |
| Core Value | ESP32 Ethernet DNS 제한을 브라우저 DNS로 우회, 사용자 경험 대폭 개선 |

---

## 1. 구현 내역

### 1.1 웹 UI (index.html + style.css)
- 설정 페이지를 4개 서브탭으로 분리: 네트워크 / MQTT / 계정 / 기타
- 서브탭 네비게이션 CSS 스타일링 (다크 테마 일관성)
- 반응형 디자인 유지

### 1.2 프론트엔드 로직 (app.js)
- **loadConfig()**: 서버에서 설정 로드 → 모든 필드에 자동 바인딩
- **saveNetwork()**: 네트워크 설정 저장 + 재부팅 (confirm 다이얼로그)
- **saveMQTT()**: MQTT 설정 저장 (재부팅 불필요)
  - hostname → IP 자동 변환 (Google DNS API 활용)
  - ESP32 Ethernet DNS 제한 workaround
- **testMQTT()**: 비동기 MQTT 연결 테스트
  - DNS 해석 → POST /api/mqtttest → polling GET /api/mqtttest
  - 10초 타임아웃, 성공/실패 시각 피드백
- **saveEtc()**: 릴레이/모니터링/WOL/NTP 설정 저장 (재부팅 불필요)
- **changeAccount()**: 관리자 계정 (ID + PW) 변경
- **toggleMQTTFields()**: MQTT 비활성화 시 필드 dim 처리

### 1.3 백엔드 API (WebServer.cpp)
- `POST /api/config`: 부분 머지 방식 설정 저장
- `POST /api/mqtttest`: 비동기 MQTT 테스트 시작
- `GET /api/mqtttest`: 테스트 결과 폴링
- `POST /api/auth`: 계정 변경 (현재 패스워드 검증)
- `POST /api/reboot`: 장치 재부팅

### 1.4 펌웨어 (main.cpp)
- **MQTTTestState** 구조체: FreeRTOS task 기반 비동기 연결 테스트
- **onUDPConfig()**: config POST 시 모든 필드 (relay/monitor/wol/ntp 포함) 머지
- MQTT 연결 성공 시 ONLINE 상태 발행 (콜백에서 처리)
- MQTT 무한 재시도 (10초 간격)

### 1.5 설정 스키마 (deviceconfig.json)
- 전체 설정 구조: device_id, device_name, network(ethernet/wifi), auth, mqtt, relay, monitor, wol, ntp, firmware

## 2. 아키텍처 패턴

```
[브라우저]  ──HTTP──>  [ESPAsyncWebServer]  ──>  [ConfigManager]  ──>  [SPIFFS]
    │                        │
    │ WebSocket              │ FreeRTOS Task
    └──실시간 상태──          └──MQTT Test──>  [PubSubClient]
```

- **부분 머지(Partial Merge)**: POST /api/config는 전송된 필드만 업데이트, 나머지 보존
- **비동기 MQTT 테스트**: main loop 블로킹 방지를 위한 FreeRTOS task + polling 패턴
- **DNS Workaround**: ESP32 Ethernet은 DNS 불가 → 브라우저에서 Google DNS API로 해석 후 IP 전달

## 3. 테스트 결과

| 테스트 항목 | 결과 |
|-------------|------|
| 네트워크 설정 저장 + 재부팅 | PASS |
| MQTT 설정 저장 (재부팅 없음) | PASS |
| MQTT hostname → IP 변환 | PASS |
| MQTT 비동기 연결 테스트 | PASS |
| 계정 변경 (패스워드 검증) | PASS |
| 기타 설정 저장 | PASS |
| Ethernet + WiFi 양쪽 MQTT 연결 | PASS |
| ONLINE 상태 MQTT 발행 | PASS |

## 4. 주요 커밋

- `1715bc6` feat: 웹 설정 UI 탭 그룹화 + MQTT 연결 테스트 + ONLINE 상태 수정
