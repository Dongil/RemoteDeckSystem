#include <Arduino.h>
#include <ui.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HttpClient.h>
#include <time.h>

#include "lvgl_touch.h"
#include "config/ConfigManager.h"
#include "device/DeviceManager.h"
//#include "webserver/WebServerHandler.h"
#include "mqtt/MQTTHandler.h"
#include "mqtt/ethernet_mqtt.h"
#include "images/images.h"
#include "utils/TypeUtils.h"

#define FORMAT_SPIFFS_IF_FAILED true

DeviceConfig deviceConfig;
ServerConfig serverConfig;
ImagesConfig imagesConfig;

WiFiClient wifi_client;   // Wifi, Mqtt 객체 생성
PubSubClient mqtt_client(wifi_client);
MQTTHandler mqttHandler(mqtt_client, deviceConfig, serverConfig);

EthernetClient ethClient; // Ethernet 객체 생성
PubSubClient mqttEthernet_Client(ethClient); // Ethernet용 mqtt

// xml에서 읽어와서 ip바꿈
//HttpClient http(ethClient, "192.168.10.198", 80);  // domain : smartbtn.xenoglobal.co.kr,  port : 1019
String httpUrl = "";
uint16_t httpPort = 0;

//WebServer web_server(80);
//WebServerHandler webServerHandler(deviceConfig, serverConfig, imagesConfig);

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

void setup()
{
    Serial.begin(115200);

    //delay(5000);
    
    lvgl_touch_init(240, 320);  // Initialize LVGL + Touch

    ui_init();  // Initialize UI
    
    lv_timer_handler(); //초기화면 리플레쉬

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
      Serial.println("SPIFFS Mount Failed");
      return;
    }

    // Load configurations
    ConfigManager::loadDeviceConfig(deviceConfig);
    ConfigManager::loadServerConfig(serverConfig);
    ConfigManager::loadImagesConfig(imagesConfig);

    delay(500);
    
    // 저장된 설정에서 ip, port를 파싱해옴
    if(TypeUtils::parseAddress(deviceConfig.serverURL.c_str(), httpUrl, httpPort)) {
        Serial.println("Http Ip : ");
        Serial.println(httpUrl);
        Serial.println("Http Port : ");
        Serial.println(httpPort);        
    }
    else {
        Serial.println("Server URL parsing Error!");
    }

    // 먼저 ethernet mqtt 연결 시도 
    mqttEthernet_init(); // Initialize Ethernet + MQTT
    mqttEthernet_setCallback(mqtt_ReceivedCallback); // Set MQTT callback

    delay(500);

    if (Ethernet.localIP() != INADDR_NONE &&    // INADDR_NONE은 0.0.0.0을 의미
        mqttEthernet_connected() ) {  
        lv_scr_load(ui_ScreenMain);   // ethernet 정상 연결시
        ethernet_conn = true;
        httpRequestUsing = serverConfig.usingHttpRequest;   //Web Server로 상태 명령 보낼때
    }
    else{
        deviceManager = new DeviceManager(deviceConfig, serverConfig, imagesConfig);
        deviceManager->showDeviceSet();
    }

    images_update();    //다운로드 이미지 불러와서 표시

    screen_saver_init(serverConfig.sleepTime);  //스크린세이브 설정

    screen_main = true; //메인 화면으로 왔는지

    Serial.println("setup done");
}

//HttpClient http(ethClient, httpUrl, httpPort);  // 포트는 변수로 안됨 
HttpClient http(ethClient, httpUrl, 80);  // domain : smartbtn.xenoglobal.co.kr,  port : 1019

void loop()
{
    delay(10);  
    lvgl_loop();    //lvgl 화면 갱신, 화면보호기 체크

    // Mqtt 사용시 
    if(ethernet_conn){
        mqttEthernet_loop();          // MQTT keep alive
    }    
    
    if(wifi_conn){
        mqttHandler.loop();
        //web_server.handleClient();
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

// Design Ref: §11.2 M3 — downloadFile() 재작성
// Plan SC: FR-06 (단일 file handle, 청크 단위 write, Content-Length 검증)
// 변경 포인트:
//  - 2바이트마다 close/reopen 제거 → 단일 핸들 유지
//  - 버퍼 1024B, idle 타임아웃 5초, 전체 타임아웃 60초
//  - Content-Length 불일치 또는 read=0 idle 초과 시 partial 파일 자동 삭제
//  - 성공/실패 모두 명확한 Serial 로그
bool downloadFile(const char* urlPath, const char* spiffsPath) {
    http.setTimeout(10000);
    http.get(urlPath);

    int statusCode = http.responseStatusCode();
    if (statusCode != 200) {
        Serial.printf("Download HTTP %d: %s\n", statusCode, urlPath);
        http.stop();
        return false;
    }

    int contentLength = http.contentLength();
    Serial.printf("Download start: %s (Content-Length=%d)\n", urlPath, contentLength);

    File file = SPIFFS.open(spiffsPath, FILE_WRITE);
    if (!file) {
        Serial.printf("SPIFFS open failed: %s\n", spiffsPath);
        http.stop();
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
    while (ethClient.connected() || http.available()) {
        // 전체 타임아웃
        if (millis() - startMs > TOTAL_TIMEOUT_MS) {
            Serial.println("Download total timeout");
            ok = false;
            break;
        }

        int avail = http.available();
        if (avail <= 0) {
            if (millis() - lastDataMillis > IDLE_TIMEOUT_MS) {
                Serial.println("Download idle timeout");
                ok = false;
                break;
            }
            delay(10);
            continue;
        }

        int bytesRead = http.read(buffer, BUF_SIZE);
        if (bytesRead <= 0) {
            // available > 0 인데 read=0 인 경우는 짧은 stall — 다음 루프에서 재시도
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

        // Content-Length 도달 시 조기 종료
        if (contentLength > 0 && totalBytesRead >= contentLength) break;
    }

    file.flush();
    file.close();

    // Content-Length 검증
    if (ok && contentLength > 0 && totalBytesRead != contentLength) {
        Serial.printf("Size mismatch: got=%d expected=%d\n", totalBytesRead, contentLength);
        ok = false;
    }

    if (!ok) {
        // partial 파일 정리
        SPIFFS.remove(spiffsPath);
        Serial.printf("Download failed, removed partial: %s\n", spiffsPath);
        http.stop();
        return false;
    }

    Serial.printf("Download ok: %s (%d bytes)\n", spiffsPath, totalBytesRead);
    return true;
}

void downloadFileWithResume(const char* urlPath, const char* spiffsPath) {

    // SPIFFS에서 파일 열기 (이미 존재하는 경우 이어받기)
    File file = SPIFFS.open(spiffsPath, FILE_APPEND);
    if (!file) {
        file = SPIFFS.open(spiffsPath, FILE_WRITE);  // 파일이 없으면 새로 생성
        if (!file) {
            Serial.println("Failed to open file for writing");
            return;
        }
    }

    // 이미 다운로드된 파일 크기를 확인
    size_t fileSize = file.size();
    Serial.print("Resuming download, already downloaded size: ");
    Serial.println(fileSize);

    // HTTP Range 요청 설정 (이미 다운로드된 부분 이후로 이어받기)
    http.connectionKeepAlive();
    http.beginRequest();
    http.get(urlPath);
    http.sendHeader("Range", String("bytes=") + fileSize + "-");
    http.endRequest();

    // 응답 코드 확인
    int statusCode = http.responseStatusCode();
    if (statusCode != 200 && statusCode != 206) {  // 206은 Partial Content (이어받기 성공)
        Serial.print("Failed to download file, status code: ");
        Serial.println(statusCode);
        file.close();
        return;
    }

    // Content-Length 확인 (전체 파일 길이가 아니라 남은 부분 길이)
    int contentLength = http.contentLength();
    if (contentLength == -1) {
        Serial.println("Failed to get content length for resumed download.");
        file.close();
        return;
    }

    Serial.print("Content-Length for resumed part: ");
    Serial.println(contentLength);

    // 데이터를 받아서 이어서 파일에 쓰기
    uint8_t buffer[512];  // 버퍼 크기
    int totalBytesRead = 0;
    int writeCounter = 0;

    while (http.available()) {
        int bytesRead = http.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            if (writeCounter % 2 == 0) {
                file.close();  // 파일을 닫고
                file = SPIFFS.open(spiffsPath, FILE_APPEND);  // 다시 열기
                if (!file) {
                    Serial.println("Failed to reopen file for appending");
                    return;
                }
            }

            file.write(buffer, bytesRead);
            totalBytesRead += bytesRead;
            writeCounter++;
            
            Serial.print("Bytes read: ");
            Serial.println(bytesRead);
        }

        if (!ethClient.connected()) {
            Serial.println("Connection lost during download.");
            break;
        }
    }

    file.close();  // 파일 닫기
    Serial.print("File saved to: ");
    Serial.println(spiffsPath);
    Serial.print("Total bytes downloaded this session: ");
    Serial.println(totalBytesRead);
}

void downloadFileWithRetries(const char* urlPath, const char* spiffsPath, int maxRetries) {
    int retryCount = 0;
    bool downloadComplete = false;

    while (retryCount < maxRetries && !downloadComplete) {
        downloadFileWithResume(urlPath, spiffsPath);
        if (ethClient.connected()) {
            downloadComplete = true;           
        } else {
            Serial.print("Retrying download... Attempt: ");
            Serial.println(retryCount + 1);
            retryCount++;
            delay(5000);  // 재시도 간격을 두기 위해 5초 대기
        }
    }

    if (!downloadComplete) {
        Serial.println("Failed to complete the download after retries.");
    }
}

void sendHttpMessage(const char* msg) {
    // serverConfig.statusUrl : status_url = "/attend_status.php?node_id=[device_id]&status=[status]" [device_id]자리에 장치 id [status] 자리에 msg 넣어서 완성
    std::string httpRequestPath = TypeUtils::makeHttpPath(serverConfig.statusUrl, deviceConfig.deviceID, msg);
    Serial.print("Sending GET request to: ");
    Serial.print(httpUrl);
    Serial.println(httpRequestPath.c_str());

    // HTTP GET 요청 보내기
    http.get(httpRequestPath.c_str());

    // 응답 코드 확인
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();
    
    if (statusCode != 200) {
        Serial.print("Fail to Request server , status code: ");
        Serial.println(statusCode);
        return;
    }

    Serial.print("Response: ");
    Serial.println(response);
    http.stop();

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
void ibtnLogo_LongClick(lv_event_t * e)
{
    if(!screen_main){
        return;
    }

    clickCount++;
    clickCount_t = millis();
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