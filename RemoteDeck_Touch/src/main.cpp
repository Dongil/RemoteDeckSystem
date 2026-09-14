#include <Arduino.h>
#include <ui.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <HTTPClient.h>  // ESP32 내장 (대문자) — v2.1 ArduinoHttpClient 대체
#include <time.h>

#include "lvgl_touch.h"
#include "config/ConfigManager.h"
#include "device/DeviceManager.h"
#include "mqtt/MQTTHandler.h"
#include "mqtt/ethernet_mqtt.h"
#include "images/images.h"
#include "utils/TypeUtils.h"
// Design Ref: §5.3 Component List — Web Layer (v2.3 module-webui)
#include "web/WebServer.h"
#include "web/ImageApi.h"
#include "web/ConfigApi.h"
#include "web/Logger.h"
#include "web/OtaApi.h"
#include "web/ControlApi.h"

#define FORMAT_SPIFFS_IF_FAILED true

DeviceConfig deviceConfig;
ServerConfig serverConfig;
ImagesConfig imagesConfig;

WiFiClient wifi_client;   // Wifi, Mqtt 객체 생성
PubSubClient mqtt_client(wifi_client);
MQTTHandler mqttHandler(mqtt_client, deviceConfig, serverConfig);

// v2.1: WiFiClient는 lwIP socket이라 ETH/WiFi 공용 (Design §2.3)
WiFiClient ethClient;
PubSubClient mqttEthernet_Client(ethClient);

// xml에서 읽어와서 ip바꿈
//HttpClient http(ethClient, "192.168.10.198", 80);  // domain : smartbtn.xenoglobal.co.kr,  port : 1019
String httpUrl = "";
uint16_t httpPort = 0;

// v2 Web UI - Plan SC: FR-01, FR-02, FR-03, FR-04
WebServer webServer;
ImageApi  imageApi;
ConfigApi configApi;
Logger    webLogger;
OtaApi    otaApi;
ControlApi controlApi;
TouchAuth touchAuth;  // 기본 admin/12345 (TODO: deviceconfig 에서 로드)

DeviceManager* deviceManager;   // 장치 연결 관리자

const char* DEVICE_DOWNLOAD_PATH = "/download/deviceconfig.json";  // DeviceConfig 파일 다운로드 경로 (장치 IP 설정)
const char* SERVER_DOWNLOAD_PATH = "/download/serverconfig.json";  // ServerConfig 파일 다운로드 경로 (연동 서버 환경 설정)
const char* IMAGES_DOWNLOAD_PATH = "/download/imagesconfig.json";  // ImagesConfig 파일 다운로드 경로 (UI 환경 설정)

time_t currentTime = 0; // 서버에서 받은 시간을 저장
unsigned long lastSyncMillis = 0;   // 마지막 동기화 이후의 millis() 값을 저장하여 경과 시간을 계산
int rebootTime = 0; // 재부팅 시간

bool room = false;
bool g_webMode = false;      // v2.6: 이번 부팅이 웹 설정 모드인지 (Design §2.1)
uint32_t g_webModeStartMs = 0;                       // v2.6 S2: 웹모드 진입 시각
extern volatile uint32_t g_webLastActivityMs;        // v2.6 S2: WebServer.cpp — 마지막 요청 시각
extern "C" const lv_img_dsc_t ui_img_web;            // v2.6 S4: 웹 globe 아이콘 (src/ui_web_icon.c)
static const uint32_t WEB_IDLE_TIMEOUT_MS = 600000;  // v2.6 S2: 무활동 10분 → LCD 복귀
unsigned main_t=0;
bool ethernet_conn = false;
bool wifi_conn = false;
bool httpRequestUsing = false;
bool sendToOnline = false;
bool screen_main = false;   // 메인 화면에 있는지
uint8_t clickCount = 0;     // 타이틀 클릭 누적
unsigned clickCount_t = 0;  // 최종 클릭 시각

void mqtt_ReceivedCallback(char* topic, byte* payload, unsigned int length); //mqtt 수신 콜백 선언
bool downloadFile(const char* urlPath, const char* spiffsPath); // download file by http
void sendHttpMessage(const char* msg);  //http request로 메세지 전송 함수
void message_process(String msg);   //mqtt, webrequest에서 받아온 메세지 처리 함수
void gotoDeviceManager();   //장치 설정으로 이동
void ibtnLogo_LongClick(lv_event_t * e);  // v2.1: LV_EVENT_LONG_PRESSED 핸들러 (setup() 에서 직접 등록)

// v2.6 S4: 장치 설정 "웹 설정 모드" 버튼 콜백 — deviceconfig 갱신 후 웹모드로 재부팅.
//   임시 시리얼 webmode 트리거를 대체하는 정식 진입 UX (사용자 요구: 장치설정에서 진입).
static void webmodeBtn_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    Serial.println("[v2.6] 웹 설정 모드 버튼 — deviceconfig 갱신 후 재부팅");
    deviceConfig.webConfigMode = true;
    ConfigManager::saveDeviceConfig(deviceConfig);
    delay(200);
    ESP.restart();
}

void setup()
{
    Serial.begin(115200);

    // v2.1 C6: setup() 순서 재배치 — ETH(W5500) 를 LCD(TFT_eSPI) 보다 먼저 초기화
    // 두 디바이스가 SPI 버스 공유 (SCK=18, MOSI=23, MISO=19) — W5500 reset/init이 TFT_eSPI 점유 후엔 실패.
    // SPIFFS + Config 먼저 로드 → ETH 초기화 → LCD 초기화 순으로 처리.

    // 1) SPIFFS + 설정 로드
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    ConfigManager::loadDeviceConfig(deviceConfig);
    ConfigManager::loadServerConfig(serverConfig);
    ConfigManager::loadImagesConfig(imagesConfig);
    delay(100);

    // 저장된 설정에서 ip, port를 파싱
    if (TypeUtils::parseAddress(deviceConfig.serverURL.c_str(), httpUrl, httpPort)) {
        Serial.printf("Http Ip : %s\nHttp Port : %u\n", httpUrl.c_str(), httpPort);
    } else {
        Serial.println("Server URL parsing Error!");
    }

    // 2) ETH(W5500) 먼저 초기화 — SPI 버스 점유 (TFT_eSPI 이전)
    mqttEthernet_init();
    mqttEthernet_setCallback(mqtt_ReceivedCallback);

    // v2.6: 부팅 경계 SPI 배타 모드 분기 (Design §2.1)
    // webConfigMode 면 TFT(TFT_eSPI) 를 아예 init 하지 않고 WebServer 가 SPI 를 단독 점유한다.
    // → LCD transaction 이 존재하지 않으므로 v2.4 가 실패한 SPI host mutex 경합이 성립하지 않음.
    g_webMode = deviceConfig.webConfigMode;
    if (g_webMode) {
        // one-shot consume: 플래그를 즉시 소비(false 저장) → 전원손실/워치독 등 어떤 비정상
        // 종료에도 다음 부팅은 LCD 모드로 복귀 (안티브릭, Design §1.2 · §6)
        deviceConfig.webConfigMode = false;
        ConfigManager::saveDeviceConfig(deviceConfig);

        // ── WEB CONFIG MODE — LCD/LVGL/터치 미초기화, WebServer 가 SPI 단독 점유 ──
        IPAddress ip = ETH.localIP();
        ethernet_conn = (ip != IPAddress(0, 0, 0, 0));
        // v2.6 S2: 정적 안내화면 1회 렌더 (TFT 만, LVGL 없음). 이후 tft 무접근 → 서비스 중 경합 없음.
        lcd_show_webmode_info(ip.toString().c_str());
        imageApi.setNetworkInfo("ethernet", ip.toString());
        imageApi.setFirmwareInfo("2.6.0-webmode", "2026-09-11");
        imageApi.attach(&webServer);
        configApi.attach(&webServer);
        webLogger.attach(&webServer);
        otaApi.attach(&webServer);
        controlApi.begin();
        controlApi.setMqttPublisher([](const char* status) {
            if (ethernet_conn) mqttEthernet_publish(status);
        });
        controlApi.attach(&webServer);

        if (webServer.begin(80, &touchAuth)) {
            webLogger.log("BOOT", "WebServer started (v2.6 web config mode, TFT skipped)");
            Serial.printf("[v2.6] WEB CONFIG MODE — TFT skipped, http://%s:80\n", ip.toString().c_str());
        } else {
            Serial.println("[v2.6] WebServer start FAILED");
        }
        g_webModeStartMs = millis();   // v2.6 S2: 무활동 타임아웃 기준
        Serial.println("setup done (web mode)");
        return;   // ── 웹 모드 setup 종료: 아래 TFT 경로 진입 안 함 ──
    }

    // ── LCD MODE (기본) — 기존 v2.x 동작, WebServer 미구동 (FR-09) ──
    // 3) LCD(TFT_eSPI) + LVGL 초기화 — ETH 이후
    lvgl_touch_init(240, 320);
    ui_init();

    // v2.1 fix: SquareLine 자동생성 ui_event_ibtnLogo 는 LV_EVENT_CLICKED 만 처리.
    // LV_EVENT_LONG_PRESSED 를 main.cpp 의 ibtnLogo_LongClick 으로 직접 라우팅.
    lv_obj_add_event_cb(ui_ibtnLogo, ibtnLogo_LongClick, LV_EVENT_LONG_PRESSED, NULL);

    // v2.6 S4: 장치 설정 화면 "웹 설정 모드" 진입 아이콘 버튼 (동적 생성 — SquareLine regen 안전).
    //   기존 네트워크 nav 아이콘 스타일에 맞춰 상단(제목 아래) 좌측에 웹 globe 아이콘 배치.
    //   한글 폰트 subset 에 웹/설/모/드 글리프 부재 → 텍스트 대신 아이콘 사용 (사용자 요구).
    {
        lv_obj_t* wb = lv_imgbtn_create(ui_ScreenDevice);
        lv_imgbtn_set_src(wb, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_web, NULL);
        lv_obj_set_size(wb, 32, 15);                     // 네트워크 nav 아이콘과 동일 크기
        lv_obj_align(wb, LV_ALIGN_CENTER, -90, -127);    // 우측 nav(ibtnWifi2 x=+90) 미러 → 제목 같은 줄·좌우대칭
        lv_obj_add_flag(wb, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(wb, webmodeBtn_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_timer_handler();

    delay(500);

    if (ETH.localIP() != IPAddress(0, 0, 0, 0) &&
        mqttEthernet_connected() ) {
        lv_scr_load(ui_ScreenMain);
        ethernet_conn = true;
        httpRequestUsing = serverConfig.usingHttpRequest;
    }
    else {
        deviceManager = new DeviceManager(deviceConfig, serverConfig, imagesConfig);
        deviceManager->showDeviceSet();
    }

    images_update();    //다운로드 이미지 불러와서 표시

    screen_saver_init(serverConfig.sleepTime);  //스크린세이브 설정

    screen_main = true; //메인 화면으로 왔는지

    Serial.println("setup done (lcd mode)");
}

// v2.1 C4: HTTPClient (ESP32 내장) - lwIP 위에서 ETH/WiFi 모두 동작
HTTPClient http;

void loop()
{
    delay(10);

    // v2.6: 웹 설정 모드 — 웹 모듈만 서비스. lvgl_loop/터치 미호출 → 서비스 중 TFT transaction 0 (SPI 경합 없음)
    if (g_webMode) {
        imageApi.loop();
        configApi.loop();   // /api/reboot(웹 "재부팅" 버튼) → ESP.restart() = LCD 모드 복귀(exit)
        otaApi.loop();
        if (ethernet_conn) mqttEthernet_loop();
        // v2.6 S2: 무활동 타임아웃 — 마지막 요청(없으면 진입 시각) 기준 10분 경과 시 LCD 복귀
        uint32_t ref = (g_webLastActivityMs != 0) ? g_webLastActivityMs : g_webModeStartMs;
        if ((uint32_t)(millis() - ref) > WEB_IDLE_TIMEOUT_MS) {
            Serial.println("[v2.6] 웹 설정 모드 무활동 타임아웃 — LCD 모드로 재부팅");
            delay(100);
            ESP.restart();
        }
        return;
    }

    lvgl_loop();    //lvgl 화면 갱신, 화면보호기 체크

    // Mqtt 사용시
    if(ethernet_conn){
        mqttEthernet_loop();          // MQTT keep alive
    }

    if(wifi_conn){
        mqttHandler.loop();
    }

    if(httpRequestUsing) {
        //http request 사용시 ONLINE 상태를 최초 한번 보내준다.
        if(ethernet_conn){
            if(!sendToOnline) {
                sendHttpMessage("ONLINE");
                sendToOnline = true;
            }
        }    
    }

    if(millis() - main_t > 2000)
    {
        if(!screen_main)
            return;
        
        // 누른횟수 체크
        if(clickCount > 35) {
            gotoDeviceManager();
            Serial.print("gotoDeviceManager :");
            clickCount = 0;
            screen_main = false;
        }
        else{
            if(millis() - clickCount_t > 2000) {
                clickCount = 0;
            }
        }   
        
        main_t = millis();

        if(rebootTime > 0)
        {
            unsigned long elapsedSeconds = (main_t - lastSyncMillis) / 1000;
            currentTime += elapsedSeconds;  // 마지막 동기화 이후의 경과 초 더함
            lastSyncMillis += elapsedSeconds * 1000;  // 최종 동기화 시각 저장

            struct tm* timeInfo = gmtime(&currentTime);
            
            // char timeString[30];
            // strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeInfo);

            // Serial.print("현재 시간 : ");
            // Serial.println(timeString);

            // 재부팅 조건 체크
            // 1. 기기가 06시 이전에 켜져 있다가 06:00:00 ~ 06:00:04 사이에 재부팅
            if (timeInfo->tm_hour == rebootTime && timeInfo->tm_min == 00 && timeInfo->tm_sec > 0 && timeInfo->tm_sec < 4) {
                Serial.println("재부팅합니다.");
                delay(3000);
                ESP.restart();
            }           
        }
    }

    /*
    if(millis() - main_t > 1000)
    {
        Serial.print("+");
        main_t = millis();
    }
    */
}

// devicemanager에서 wifi 정보 변경되면 호출됨
void wifiInfo_Changed() {
    // wifi mqtt 연결 성공시
    mqttHandler.setup();  //Mqtt 서버와 연결
    mqttHandler.setCallback(mqtt_ReceivedCallback); // MQTT 메시지 수신 콜백 설정
    
    if(mqttHandler.isWiFiConnected()){
        //webServerHandler.setupRoutes(web_server);    // Set up web server routes
        //web_server.begin(); // Start the server
        wifi_conn = true; 
    }      
}

// devicemanager에서 ethernet 정보 변경되면 호출됨
void ethernetInfo_Changed() {
    // 약간의 딜레이 후 재부팅
    delay(1000);  // 1초 대기 후 재부팅

    lv_scr_load(ui_ScreenLogo);
    lv_timer_handler(); 

    delay(500);

    ESP.restart();  // 또는 ESP.reset(); 을 사용    
}

// devicemanager에서 서버 정보 내려받기
void fetchServerInfo() {
    //서버에서 serverConfig json 가져와서 로컬 저장
    String url = "/iot_device/serverconfig.json";

    if(downloadFile(url.c_str(), SERVER_DOWNLOAD_PATH)) {
        Serial.print("Download serverconfig json : ");
        Serial.println(url.c_str());

        if(ConfigManager::loadServerConfig(serverConfig, SERVER_DOWNLOAD_PATH)) {
            ConfigManager::saveServerConfig(serverConfig);
            Serial.println("Save serverconfig Info");
            FileUtils::remove(SERVER_DOWNLOAD_PATH);
        }
    }

    // 서버에서 deviceConfig json 가져와서 로컬 저장
    url = TypeUtils::replaceID(serverConfig.imageUrl, deviceConfig.deviceID).c_str();
    String urlPath = url + "deviceconfig.json";

    if(downloadFile(urlPath.c_str(), DEVICE_DOWNLOAD_PATH)) {
        Serial.println("Download deviceconfig json");

        if(ConfigManager::loadDeviceConfig(deviceConfig, DEVICE_DOWNLOAD_PATH)) {
            ConfigManager::saveDeviceConfig(deviceConfig);
            Serial.println("Save deivceconfig Info");
            FileUtils::remove(DEVICE_DOWNLOAD_PATH);
        }
    } 

    // 약간의 딜레이 후 재부팅
    delay(1000);  // 1초 대기 후 재부팅

    lv_scr_load(ui_ScreenLogo);
    lv_timer_handler(); 

    delay(500);

    ESP.restart();  // 또는 ESP.reset(); 을 사용  
}

// devicemanager에서 UI image 파일 내려받기
void fetchImageFiles() {
    // JSON 파일 다운로드 및 저장
    // image url    "/iot_device/[device_id]/" - [device_id]자리에 장치 id 넣어서 완성
    String url = TypeUtils::replaceID(serverConfig.imageUrl, deviceConfig.deviceID).c_str();
    String urlPath = url + "imagesconfig.json";

    if(downloadFile(urlPath.c_str(), IMAGES_DOWNLOAD_PATH)) {
        Serial.println("Download imagesconfig json");

        if(ConfigManager::loadImagesConfig(imagesConfig, IMAGES_DOWNLOAD_PATH)) {
            ConfigManager::saveImagesConfig(imagesConfig);
            Serial.println("Save imagesconfig Info");
            FileUtils::remove(IMAGES_DOWNLOAD_PATH);
        }
    } 

    // BMP 파일 다운로드 및 저장
    urlPath = url;
    urlPath += "title.bmp";
    
    if(downloadFile(urlPath.c_str(), "/download/title.bmp")) {
        Serial.println("Download title.bmp Images Resource");
    }

    urlPath = url;
    urlPath += "photo.bmp";
    
    if(downloadFile(urlPath.c_str(), "/download/photo.bmp")) {
        Serial.println("Download photo.bmp Images Resource");
    }

    urlPath = url;
    urlPath += "name.bmp";

    if(downloadFile(urlPath.c_str(), "/download/name.bmp")) {
        Serial.println("Download name.bmp Images Resource");
    }

    FileUtils::list("/images");
    FileUtils::list("/download");
    Serial.printf("Free heap before malloc: %d bytes\n", ESP.getFreeHeap());

     // 약간의 딜레이 후 재부팅
    delay(1000);  // 1초 대기 후 재부팅

    lv_scr_load(ui_ScreenLogo);
    lv_timer_handler(); 

    delay(500);

    ESP.restart();  // 또는 ESP.reset(); 을 사용   
}

// MQTT 메시지 수신 콜백 함수
void mqtt_ReceivedCallback(char* topic, byte* payload, unsigned int length) {
    String message;

    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    Serial.println(message);

    message_process(message);   //수신 메세지 처리
}

// MQtt & WebResquest 에서 받아온 메세지 처리 부분
void message_process(String msg) {
    // JSON 파싱
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, msg);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
    }

    // JSON 데이터에서 필드 값 추출
    const char* status = doc["status"];
    const char* data = doc["data"];
    uint64_t tick = doc["tick"];

    // 필드 값 출력 확인
    Serial.print("status: ");
    Serial.println(status);
    Serial.print("data: ");
    Serial.println(data);
    Serial.print("tick: ");
    Serial.println(tick);

    // status 에 따라서 추가 처리 로직 작성
    if (strcmp(status, "IN") == 0) {
        // 여기서 'IN' 상태일 때의 처리 로직 추가
        lv_imgbtn_set_src(ui_ibtnRoom, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_in_png, NULL);
        room = true;
        Serial.println("Room IN");
        // v2.3 module-control: web Long polling client 갱신
        controlApi.notifyState(true, false);
    } else if (strcmp(status, "OUT") == 0) {
        // 여기서 'OUT' 상태일 때의 처리 로직 추가
        lv_imgbtn_set_src(ui_ibtnRoom, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_out_png, NULL);
        room = false;
        Serial.println("Room OUT");
        controlApi.notifyState(false, true);
    }

    // tick 값 추출 (서버의 Unix 타임스탬프, UTC 기준)
    // UTC+9 (서울)로 변환하기 위해 9시간을 더함
    tick += 9UL * 3600;

    // 변환된 tick 값을 time_t로 변환 후, gmtime()으로 구조체 생성
    currentTime = tick;
    lastSyncMillis = millis();

    rebootTime = deviceConfig.rebootTime;
    // Serial.print("재부팅 시각: ");
    // Serial.println(rebootTime);

    struct tm *timeInfo = gmtime(&currentTime);
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", timeInfo);

    Serial.print("시간 동기화: ");
    Serial.println(timeString);
}

void gotoDeviceManager()
{
    // 이더넷 연결 되어 있으면 mqtt 연결 끊고
    if(ethernet_conn){
        //mqttEthernet_disconnect();
        //ethernet_conn = false;

        // 장치 설정 화면 관리
        deviceManager = new DeviceManager(deviceConfig, serverConfig, imagesConfig);
        deviceManager->showDeviceSet();
    }
    
    // wifi 연결 되어 있으면 wifi, mqtt 연결 끊기
    if(wifi_conn){        
        //mqttHandler.disConnectToMQTT();
        //wifi_conn = false;

        // 장치 설정 화면 관리
        deviceManager = new DeviceManager(deviceConfig, serverConfig, imagesConfig);
        deviceManager->showDeviceSet();
    }
}

// v2.1 C4: HTTPClient (ESP32 내장) 기반 — Design §11.2
// Plan SC: FR-06, FR-07 (HTTPClient 호환)
bool downloadFile(const char* urlPath, const char* spiffsPath) {
    String fullUrl = String("http://") + httpUrl + urlPath;
    http.setTimeout(10000);
    http.setConnectTimeout(10000);

    if (!http.begin(fullUrl)) {
        Serial.printf("Download begin failed: %s\n", fullUrl.c_str());
        return false;
    }

    int statusCode = http.GET();
    if (statusCode != HTTP_CODE_OK) {
        Serial.printf("Download HTTP %d: %s\n", statusCode, fullUrl.c_str());
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    Serial.printf("Download start: %s (Content-Length=%d)\n", fullUrl.c_str(), contentLength);

    // v2.1 fix: 이미지 파일은 SPIFFS + 디코딩 메모리 한계 고려해 200KB 상한 적용
    // (BMP 24bit 320x240 raw = 230KB / 디코드 후 RGB565 153KB. PNG 240x320 RGBA decode = 307KB)
    const int DOWNLOAD_MAX_BYTES = 200 * 1024;
    if (contentLength > DOWNLOAD_MAX_BYTES) {
        Serial.printf("Download reject: too large %d > %d bytes\n", contentLength, DOWNLOAD_MAX_BYTES);
        http.end();
        return false;
    }

    File file = SPIFFS.open(spiffsPath, FILE_WRITE);
    if (!file) {
        Serial.printf("SPIFFS open failed: %s\n", spiffsPath);
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        file.close();
        http.end();
        return false;
    }

    const size_t BUF_SIZE = 1024;
    uint8_t buffer[BUF_SIZE];
    int totalBytesRead = 0;
    unsigned long lastDataMillis = millis();
    const unsigned long IDLE_TIMEOUT_MS  = 5000;
    const unsigned long TOTAL_TIMEOUT_MS = 60000;
    unsigned long startMs = millis();

    bool ok = true;
    while (http.connected() && (contentLength <= 0 || totalBytesRead < contentLength)) {
        if (millis() - startMs > TOTAL_TIMEOUT_MS) {
            Serial.println("Download total timeout");
            ok = false;
            break;
        }

        size_t avail = stream->available();
        if (avail == 0) {
            if (millis() - lastDataMillis > IDLE_TIMEOUT_MS) {
                Serial.println("Download idle timeout");
                ok = false;
                break;
            }
            delay(10);
            continue;
        }

        int bytesRead = stream->readBytes(buffer, std::min(avail, BUF_SIZE));
        if (bytesRead <= 0) {
            if (millis() - lastDataMillis > IDLE_TIMEOUT_MS) { ok = false; break; }
            delay(5);
            continue;
        }

        size_t written = file.write(buffer, bytesRead);
        if (written != (size_t)bytesRead) {
            Serial.printf("SPIFFS write short: %u/%d\n", (unsigned)written, bytesRead);
            ok = false;
            break;
        }
        totalBytesRead += bytesRead;
        lastDataMillis = millis();
    }

    file.flush();
    file.close();
    http.end();

    if (ok && contentLength > 0 && totalBytesRead != contentLength) {
        Serial.printf("Size mismatch: got=%d expected=%d\n", totalBytesRead, contentLength);
        ok = false;
    }

    if (!ok) {
        SPIFFS.remove(spiffsPath);
        Serial.printf("Download failed, removed partial: %s\n", spiffsPath);
        return false;
    }

    Serial.printf("Download ok: %s (%d bytes)\n", spiffsPath, totalBytesRead);
    return true;
}

void sendHttpMessage(const char* msg) {
    // v2.1 C4: HTTPClient 기반
    std::string httpRequestPath = TypeUtils::makeHttpPath(serverConfig.statusUrl, deviceConfig.deviceID, msg);
    String fullUrl = String("http://") + httpUrl + httpRequestPath.c_str();
    Serial.print("Sending GET request to: ");
    Serial.println(fullUrl);

    if (!http.begin(fullUrl)) {
        Serial.println("sendHttpMessage begin failed");
        return;
    }
    http.setTimeout(10000);
    int statusCode = http.GET();
    if (statusCode != HTTP_CODE_OK) {
        Serial.printf("Fail to Request server , status code: %d\n", statusCode);
        http.end();
        return;
    }
    String response = http.getString();
    http.end();

    Serial.print("Response: ");
    Serial.println(response);

    message_process(response);
}

void mqttConnect_Broken() {
    deviceManager = new DeviceManager(deviceConfig, serverConfig, imagesConfig);
    deviceManager->showDeviceSet();    
}

void mqttConnect_ReConnect() {
    lv_scr_load(ui_ScreenMain);   // mqtt 새로 연결될 경우
}

/////////////////// 상단 로고 꾹 누르고 있을때 UI 이벤트 처리  ////////////////////////////
// v2.1: 진입 조건 완화 — long-press 1회로 DeviceManager 진입
// (이전: 35회 long-press 누적 in 2초 → 사실상 진입 불가능)
void ibtnLogo_LongClick(lv_event_t * e)
{
    if (!screen_main) return;

    Serial.println("ibtnLogo long-press → DeviceManager");
    screen_main = false;
    clickCount = 0;
    gotoDeviceManager();
}

/////////////////// 재부재 버턴 UI 이벤트 처리  ////////////////////////////
extern void ibtnRoom_Click(lv_event_t * e)   // 재부재 버턴 이벤트 핸들러
{    
    if(room)
    { 
        if(httpRequestUsing) {
            //http request 로 명령 보낼때             
            
            //ethernet 연결시
            if(ethernet_conn){
                sendHttpMessage("OUT");
            }
        }
        else {
            //Mqtt 로 명령 보낼때

            //ethernet 사용시
            if(ethernet_conn){
                mqttEthernet_publish("OUT");
            }
            
            //wifi 사용시
            if(wifi_conn){
                mqttHandler.xenoMqttPublish("OUT");
            }
        }

        Serial.println("Out Command Send");
    }
    else
    {
        if(httpRequestUsing) {
            //http request 로 명령 보낼때   
            
            //ethernet 연결시
            if(ethernet_conn){
                sendHttpMessage("IN");
            }
        }
        else {
            //Mqtt로 명령 보낼때
        
            //ethernet 연결시
            if(ethernet_conn){
                mqttEthernet_publish("IN");
            }
            
            //wifi 사용시
            if(wifi_conn){
                mqttHandler.xenoMqttPublish("IN");
            }
            
        }

        Serial.println("In Command Send");
    }  
}


/*
2032_25_SPI_CU 프로젝트 : 2025.02.20 - mqtt 연결시 받아온 tick을 시간으로 변환해서 지정시간에 재부팅하는 기능 추가
*/