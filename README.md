# RemoteDeck System

ESP32 기반 원격 PC 전원관리 시스템. 릴레이를 통한 PC 전원 제어, IO 모니터링, 외부 시스템 연동을 지원합니다.

## 프로젝트 구조

```
RemoteDeckSystem/
├── RemoteDeck_PC/              ESP32 펌웨어 (PlatformIO)
│   ├── src/                    소스 코드
│   │   ├── config/             설정 (DeviceConfig, ConfigManager)
│   │   ├── control/            제어 (Relay, PCMonitor, Schedule, WOL)
│   │   ├── network/            통신 (MQTT, NetManager, WebRequest, UDPDiscovery)
│   │   ├── serial/             RS485
│   │   ├── web/                WebServer, WebSocket, OTA
│   │   ├── utils/              Logger
│   │   └── main.cpp            메인 (명령 처리, 상태 발신)
│   ├── data/                   SPIFFS (Web UI, deviceconfig.json)
│   ├── firmware/               빌드된 배포용 펌웨어 (.bin)
│   ├── build_firmware.bat      펌웨어 빌드 스크립트
│   ├── platformio.ini          PlatformIO 설정
│   └── partitions.csv          파티션 테이블
├── IPSetupTool/                초기 네트워크 설정 도구 (.NET 8 WinForms)
├── APITestUtility_v2/          API 테스트 유틸리티 (.NET 8 WinForms)
├── asset/                      하드웨어 자료 (회로도, 릴레이, F_Panel)
└── docs/                       문서
    ├── RemoteDeck_PC_Manual.md 사용자 매뉴얼 + API 문서
    ├── wiring-guide.md         배선 가이드
    ├── 01-plan/                PDCA Plan
    ├── 02-design/              PDCA Design
    ├── 03-analysis/            Gap Analysis
    ├── 04-report/              완료 보고서
    └── archive/                아카이브 (v2.0, v2.1)
```

## 주요 기능

| 기능 | 설명 |
|------|------|
| PC 전원 제어 | 릴레이 펄스로 전원 켜기/끄기/강제 종료 |
| PC 상태 모니터링 | 전원 LED 감지로 ON/OFF 확인 |
| 릴레이 2 | 보조 장비 (조명, 빔프로젝터 등) 제어 |
| GPIO 입력 | 3채널 범용 디지털 입력 |
| Wake-on-LAN | 네트워크 원격 부팅 |
| 스케줄 | 시간/요일 기반 자동 전원 제어 |
| Web Request | IO 변경 시 외부 서버로 HTTP GET 자동 호출 |

## 지원 인터페이스

| 인터페이스 | 프로토콜 | 용도 |
|-----------|---------|------|
| Web UI | HTTP :5050 | 브라우저 기반 설정/제어 |
| WebSocket | WS :5050/ws | 실시간 상태 모니터링 |
| MQTT | TCP :1883 | IoT 플랫폼 연동 |
| RS485 | Serial 9600bps | 유선 시리얼 통신 |
| UDP | UDP :5051 | 장치 탐색 (IPSetupTool) |
| Web Request | HTTP GET | IO 변경 시 외부 호출 |

## API (v2.2)

### Command (장치로 전송)

```json
{"cmd":"relay","relay":1,"state":"on"}     릴레이 ON
{"cmd":"relay","relay":1,"state":"off"}    릴레이 OFF
{"cmd":"pulse","relay":1}                  릴레이 펄스 (500ms)
{"cmd":"pulse","relay":1,"duration":5000}  긴 펄스 (5초, 강제 종료)
{"cmd":"status"}                           전체 상태 요청
{"cmd":"wol","mac":"AA:BB:CC:DD:EE:FF"}   Wake-on-LAN
{"cmd":"reboot"}                           재부팅
```

### Status Event (장치에서 수신)

```json
{"id":"node_1","event":"online","ip":"192.168.1.200","name":"새기기","fw":"2.2.0"}
{"id":"node_1","event":"relay","relay1":1,"relay2":0}
{"id":"node_1","event":"pcled","pc_on":true}
{"id":"node_1","event":"full","relay1":1,"relay2":0,"pc_on":true,"gpio1":0,"gpio2":1,"gpio3":0,...}
```

### HTTP REST API

```bash
# 상태 조회
curl -u admin:12345 http://192.168.1.200:5050/api/status

# 릴레이1 ON
curl -u admin:12345 -X POST -H "Content-Type: application/json" \
  -d '{"relay":1,"state":"on"}' http://192.168.1.200:5050/api/relay

# 릴레이1 펄스 (PC 전원 토글)
curl -u admin:12345 -X POST -H "Content-Type: application/json" \
  -d '{"cmd":"pulse","relay":1}' http://192.168.1.200:5050/api/relay
```

> 전체 API 문서: [docs/RemoteDeck_PC_Manual.md](docs/RemoteDeck_PC_Manual.md)

## 빌드

### 펌웨어 (ESP32)

[PlatformIO](https://platformio.org/) 필요.

```bash
cd RemoteDeck_PC

# 빌드
pio run

# 빌드 + 업로드 (USB 연결)
pio run --target upload

# SPIFFS 업로드 (Web UI, 설정 파일)
pio run --target uploadfs

# 배포용 펌웨어 빌드 (Windows)
build_firmware.bat
# → firmware/RemoteDeck_PC_V2.2.0_YYYYMMDD.bin
```

### IPSetupTool (Windows)

.NET 8 SDK 필요.

```bash
cd IPSetupTool/IPSetupTool
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o ../publish
# → publish/IPSetupTool.exe (관리자 권한 필요)
```

### APITestUtility_v2 (Windows)

```bash
cd APITestUtility_v2/RemoteDeckTest/RemoteDeckTest
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o ./publish
# → publish/RemoteDeckTest.exe
```

## 배포 파일

| 파일 | 용도 |
|------|------|
| `RemoteDeck_PC_V*.bin` | OTA 펌웨어 (웹 UI에서 업로드) |
| `IPSetupTool.exe` | 초기 네트워크 설정 (관리자 권한) |
| `RemoteDeckTest.exe` | API 테스트 (HTTP/MQTT/WS/RS485) |

## 하드웨어

| 항목 | 사양 |
|------|------|
| MCU | ESP32-D0WDR2-V3 (Dual Core 240MHz) |
| Ethernet | W5500 (SPI) |
| WiFi | ESP32 내장 (STA 모드) |
| 릴레이 | SONGLE SRD-05VDC-SL-C x2 (10A/250VAC) |
| RS485 | UART2 (9600bps) |
| GPIO 입력 | 3채널 (GPIO 12, 14, 15) |
| PC-LED 입력 | GPIO 4 (반전: LOW=PC ON) |

> 배선 가이드: [docs/wiring-guide.md](docs/wiring-guide.md)

## 기본 설정

| 항목 | 값 |
|------|-----|
| IP (Ethernet) | 192.168.1.200 |
| Web UI | http://{IP}:5050 |
| 인증 | admin / 12345 |
| 네트워크 모드 | Ethernet |

## 문서

| 문서 | 내용 |
|------|------|
| [사용자 매뉴얼](docs/RemoteDeck_PC_Manual.md) | 설치, 웹 UI, API, 도구 사용법 |
| [배선 가이드](docs/wiring-guide.md) | 릴레이-PC 연결, SSR/접촉기 배선 |

## 버전 이력

| 버전 | 날짜 | 주요 변경 |
|------|------|----------|
| v2.2.0 | 2026-04-10 | API 단순화 (평문 JSON), Web Request, APITestUtility_v2 |
| v2.1.0 | 2026-04-08 | 네트워크 모드 통합 (ETH/WiFi), 인증, MQTT 연결 테스트 |
| v2.0.0 | 2026-04-05 | 초기 릴리스 (PC 전원관리, 웹 UI, IPSetupTool) |

## 라이선스

사내 전용. 무단 배포 금지.
