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
// Design Ref: §5.3 Component List — Web Layer (v2 Image Manager)
#include "web/WebServer.h"
#include "web/ImageApi.h"

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

// v2 Web UI - Plan SC: FR-01, FR-02, FR-03
WebServer webServer;
ImageApi  imageApi;
TouchAuth touchAuth;  // 기본 admin/12345 (TODO: deviceconfig 에서 로드)

DeviceManager* deviceManager;   // 장치 연결 관리자

const char* DEVICE_DOWNLOAD_PATH = "/download/deviceconfig.json";  // DeviceConfig 파일 다운로드 경로 (장치 IP 설정)
const char* SERVER_DOWNLOAD_PATH = "/download/serverconfig.json";  // ServerConfig 파일 다운로드 경로 (연동 서버 환경 설정)
const char* IMAGES_DOWNLOAD_PATH = "/download/imagesconfig.json";  // ImagesConfig 파일 다운로드 경로 (UI 환경 설정)

time_t currentTime = 0; // 서버에서 받은 시간을 저장
unsigned long lastSyncMillis = 0;   // 마지막 동기화 이후의 millis() 값을 저장하여 경과 시간을 계산
int rebootTime = 0; // 재부팅 시간

bool room = false;
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

    // 3) LCD(TFT_eSPI) + LVGL 초기화 — ETH 이후
    lvgl_touch_init(240, 320);
    ui_init();

    // v2.1 fix: SquareLine 자동생성 ui_event_ibtnLogo 는 LV_EVENT_CLICKED 만 처리.
    // LV_EVENT_LONG_PRESSED 를 main.cpp 의 ibtnLogo_LongClick 으로 직접 라우팅.
    lv_obj_add_event_cb(ui_ibtnLogo, ibtnLogo_LongClick, LV_EVENT_LONG_PRESSED, NULL);

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

    // v2.3-httpd module-poc: esp_http_server (ESP-IDF native) 활성화
    // Design Ref: §2.1 — core 0 별도 task, LVGL/MQTT (core 1) 와 물리 격리
    // Plan SC: FR-01, FR-02 (PoC gate), FR-08 (Basic Auth)
    // ImageApi.attach() 는 module-webui 단계에서 — module-poc 는 /api/status 만.
    {
        IPAddress ip = ETH.localIP();
        imageApi.setNetworkInfo("ethernet", ip.toString());
        imageApi.setFirmwareInfo("2.3.0-poc", "2026-06-23");
    }
    webServer.setStatusGetter([]() {
        // Plan SC: FR-02 — PoC 시나리오에서 1초 응답
        return imageApi.buildStatusJson();
    });
    if (webServer.begin(80, &touchAuth)) {
        Serial.println("WebServer (esp_http_server) started on port 80");
    } else {
        Serial.println("WebServer start FAILED — PoC gate fail, check serial");
    }

    screen_saver_init(serverConfig.sleepTime);  //스크린세이브 설정

    screen_main = true; //메인 화면으로 왔는지

    Serial.println("setup done");
}

// v2.1 C4: HTTPClient (ESP32 내장) - lwIP 위에서 ETH/WiFi 모두 동작
HTTPClient http;

void loop()
{
    delay(10);
    lvgl_loop();    //lvgl 화면 갱신, 화면보호기 체크

    // Design Ref: §12.2 — main loop 에서 핫리로드 처리 (web task ↔ LVGL race 회피)
    imageApi.loop();

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
    } else if (strcmp(status, "OUT") == 0) {
        // 여기서 'OUT' 상태일 때의 처리 로직 추가
        lv_imgbtn_set_src(ui_ibtnRoom, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_out_png, NULL);  
        room = false;
        Serial.println("Room OUT");
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