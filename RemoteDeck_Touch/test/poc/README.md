# RemoteDeck_Touch v2.3 PoC — module-poc Gate

Design Ref: §8.3 — Phase 1 PoC 엄격 시나리오
Plan SC: FR-02 — PoC 풀세트 통과 (1개라도 fail → v2.4 분기)

## 목적

v2.2 실패 (sync WebServer 가설 폐기) 재발 방지. 라이브러리 교체 직후
**연속 / 병렬 / MQTT 동시 / idle** 4가지 시나리오로 esp_http_server 검증.

## 사전 준비

1. v2.3-httpd 브랜치 펌웨어를 단말에 flash
2. 단말이 LAN 에 연결됐는지 확인 (LCD 또는 라우터 DHCP 표)
3. 브라우저 `http://<device_ip>/api/status` 가 admin:12345 인증 후 JSON 반환하는지 1회 확인

## 실행

터미널 A — MQTT publisher (P3 용 백그라운드):
```bash
pip install paho-mqtt
python3 mqtt_pub.py <broker_ip>
```

터미널 B — PoC 시나리오:
```bash
chmod +x run_poc.sh
./run_poc.sh <device_ip>
```

P3 직전에 터미널 A 시작, P3 종료 후 Ctrl+C.

## Pass 기준

| Step | 통과 조건 |
|------|---------|
| P1 | 10회 모두 200, heap_free ≥ 80KB |
| P2 | 30초 동안 병렬 GET 5개 실패 0건 |
| P3 | MQTT 1Hz publish 동시에 GET 1분 실패 0건 |
| P4 | idle 1분 후 heap 변화 < 5KB |

수동 추가 관찰:
- 부팅 hang 無 (LCD 정상 그려짐)
- LVGL frame drop 시각 無
- Long-click 1회 → DeviceManager 진입 정상 (v2.1 regression 보호)
- Sleep 시간 저장 / 복원 정상

## Fail 시 액션

1개라도 fail → Plan §5 Risk 트리거.
- v2.4 분기 결정 (esp_http_server 도 본질적 fail 인지 판정)
- 가능한 다음 단계: RTOS queue 직렬화, 별도 chip (W5500 → ESP32-EVB 등), Mongoose
- module-webui / module-ota / module-control 작업 시작 금지

## 통과 시 다음 단계

```
/pdca do remotedeck_touch_v2.3 --scope module-webui
```
