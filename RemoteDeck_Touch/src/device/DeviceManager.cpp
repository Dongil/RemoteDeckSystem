#include "DeviceManager.h"

extern DeviceManager* deviceManager;   // 장치 연결 관리자

// Wi-Fi 정보가 변경되었을 때 호출할 대리 함수 선언 (외부에서 정의해야 함)
void wifiInfo_Changed();
void ethernetInfo_Changed();
void fetchServerInfo();
void fetchImageFiles();

DeviceManager::DeviceManager(DeviceConfig& device, ServerConfig& server, ImagesConfig& images) 
    : deviceConfig(device), serverConfig(server), imagesConfig(images) {}

void DeviceManager::updateSSIDList() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    int num_ssid = WiFi.scanNetworks();
    if (num_ssid == 0) {
        Serial.println("No networks found");
        lv_dropdown_set_options(ui_dropSSID, "No networks found");
        return;
    }
    
    int idx = -1;
    String oldSSID = deviceConfig.networkConfig.wifiSSID.c_str();
    String ssid_options;
    for (int i = 0; i < num_ssid; i++) {
        ssid_options += WiFi.SSID(i);
        
        if(oldSSID == WiFi.SSID(i)){
            idx = i;
        }

        if (i < num_ssid - 1) {
            ssid_options += "\n";
        }
    }

    lv_dropdown_set_options(ui_dropSSID, ssid_options.c_str());
    
    if(idx > -1) {
        lv_dropdown_set_selected(ui_dropSSID, idx);        
        lv_textarea_set_text(ui_txtaPassword, deviceConfig.networkConfig.wifiPasswd.c_str());
    }
    Serial.println("SSID list updated");    
}

void DeviceManager::connectToWiFi() {
    char selected_ssid[64];
    lv_dropdown_get_selected_str(ui_dropSSID, selected_ssid, sizeof(selected_ssid));

    const char* entered_password = lv_textarea_get_text(ui_txtaPassword);
    Serial.print("Connecting to SSID: ");
    Serial.println(selected_ssid);
    Serial.print("Password: ");
    Serial.println(entered_password);

    WiFi.begin(selected_ssid, entered_password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(1000);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        lv_scr_load(ui_ScreenMain);
        //startWifiCheckTimer();  // 연결 성공 후 타이머 재개
        
        Serial.println("WiFi info changed!");
       
        deviceConfig.networkConfig.wifiSSID = selected_ssid;
        deviceConfig.networkConfig.wifiPasswd = entered_password;

        ConfigManager::saveDeviceConfig(deviceConfig);    

        // Wi-Fi 정보 변경 시 대리 함수 호출
        wifiInfo_Changed();
    } else {
        // 연결 실패 시 메시지 박스 표시
        showConnectionFailedMsgBox();
    }
}

void DeviceManager::startWifiCheckTimer() {
    wifi_check_timer = lv_timer_create([](lv_timer_t* timer) {
        static_cast<DeviceManager*>(timer->user_data)->wifiCheckCallback(timer);
    }, 2000, this);
}

void DeviceManager::stopWifiCheckTimer() {
    lv_timer_pause(wifi_check_timer);
}

void DeviceManager::showDeviceSet() {
    lv_scr_load(ui_ScreenDevice);
    updateDeviceInfo();    
    //updateSSIDList();  
    updateEthernetInfo();
}

void DeviceManager::wifiCheckCallback(lv_timer_t* timer) {
    if (WiFi.status() != WL_CONNECTED) {
        lv_scr_load(ui_ScreenWifi);
        updateSSIDList();
        stopWifiCheckTimer();
    }
}

void DeviceManager::updateEthernetInfo() {    
    if(!deviceConfig.networkConfig.usingStatic){
        lv_obj_add_state(ui_chkDhcp, LV_STATE_CHECKED);   // dhcp o
    }

    lv_textarea_set_text(ui_txtaIp, deviceConfig.networkConfig.staticIP.c_str());
    lv_textarea_set_text(ui_txtaSubnet, deviceConfig.networkConfig.staticSubnet.c_str());
    lv_textarea_set_text(ui_txtaGateway, deviceConfig.networkConfig.staticGateway.c_str());
    lv_textarea_set_text(ui_txtaPriDns, deviceConfig.networkConfig.staticPrimaryDNS.c_str());
    lv_textarea_set_text(ui_txtaSeDns, deviceConfig.networkConfig.staticSecondaryDNS.c_str());
}

void DeviceManager::connectToEthernet() {
    deviceConfig.networkConfig.usingStatic = !lv_obj_has_state(ui_chkDhcp, LV_STATE_CHECKED);
    deviceConfig.networkConfig.staticIP = lv_textarea_get_text(ui_txtaIp);
    deviceConfig.networkConfig.staticSubnet = lv_textarea_get_text(ui_txtaSubnet);
    deviceConfig.networkConfig.staticGateway = lv_textarea_get_text(ui_txtaGateway);
    deviceConfig.networkConfig.staticPrimaryDNS = lv_textarea_get_text(ui_txtaPriDns);
    deviceConfig.networkConfig.staticSecondaryDNS = lv_textarea_get_text(ui_txtaSeDns);
    
    ConfigManager::saveDeviceConfig(deviceConfig);    

    Serial.println("Ethernet info changed!");

    // ethernet 정보 변경 시 대리 함수 호출
    ethernetInfo_Changed();
}

void DeviceManager::updateDeviceInfo() {    
    lv_textarea_set_text(ui_txtaDeviceID, deviceConfig.deviceID.c_str());
    lv_textarea_set_text(ui_txtaServerurl, deviceConfig.serverURL.c_str());
}

void DeviceManager::saveToDevice() {
    deviceConfig.deviceID = lv_textarea_get_text(ui_txtaDeviceID);
    deviceConfig.serverURL = lv_textarea_get_text(ui_txtaServerurl);
        
    ConfigManager::saveDeviceConfig(deviceConfig);    

    Serial.println("Device info changed!");
}

// 메시지 박스의 이벤트 처리 콜백 함수
void DeviceManager::msgBoxEventHandler(lv_event_t* e) {
    lv_obj_t * msgbox = lv_event_get_target(e);
    const char * btn_text = lv_msgbox_get_active_btn_text(msgbox);

    if(btn_text) {
        if(strcmp(btn_text, "OK") == 0) {
            LV_LOG_USER("OK 버튼 클릭됨");
            // OK 버튼 클릭 시 수행할 동작
            // Wi-Fi 목록을 다시 스캔
            static_cast<DeviceManager*>(lv_event_get_user_data(e))->updateSSIDList();
        }
        
        // 메시지 박스 닫기
        lv_obj_del(msgbox);
    }
}

void DeviceManager::showConnectionFailedMsgBox() {
    // 메시지 박스 스타일 설정
    static lv_style_t style_msgbox;
    lv_style_init(&style_msgbox);
    lv_style_set_radius(&style_msgbox, 15);                  // 둥근 모서리
    lv_style_set_bg_opa(&style_msgbox, LV_OPA_COVER);        // 배경 불투명
    lv_style_set_bg_color(&style_msgbox, lv_color_hex(0xFFFFFF));  // 흰색 배경
    lv_style_set_text_color(&style_msgbox, lv_color_black());   // 텍스트
    lv_style_set_shadow_width(&style_msgbox, 15);            // 그림자 크기
    lv_style_set_shadow_color(&style_msgbox, lv_color_hex(0x000000));  // 검정 그림자
    lv_style_set_shadow_opa(&style_msgbox, LV_OPA_30);       // 그림자 투명도

    // 버튼 스타일 설정 (Windows 11 스타일에 맞게)
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 10);                     // 버튼 둥글기
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x2095F6));  // 파란색 버튼
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);           // 불투명
    lv_style_set_border_color(&style_btn, lv_color_hex(0x000000));  // 버튼 테두리 색상
    lv_style_set_border_width(&style_btn, 1);                // 테두리 두께
    lv_style_set_text_color(&style_btn, lv_color_black());   // 버튼 텍스트 흰색  
    lv_style_set_text_align(&style_btn, LV_TEXT_ALIGN_CENTER);
    
    // 메시지 박스 생성
    static const char * btns[] = {NULL};
    lv_obj_t * mbox = lv_msgbox_create(NULL, "WIFI Connect Fail", "The SSID you entered, Password, cannot be accessed. Please check and try again.", btns, true);
    //lv_obj_add_event_cb(mbox,  WiFiManager::msgBoxEventHandler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_style(mbox, &style_msgbox, 0);                // 메시지 박스에 스타일 적용
    lv_obj_center(mbox);                                     // 화면 중앙에 위치

    // 메시지 박스의 버튼에 스타일 적용
    lv_obj_t * btnm = lv_msgbox_get_btns(mbox);
    lv_obj_add_style(btnm, &style_btn, 0);                   // 버튼에 스타일 적용
    lv_obj_set_style_text_align(btnm, LV_TEXT_ALIGN_CENTER, 0); // 버튼 텍스트 중앙 정렬    
    lv_obj_set_style_pad_all(btnm, 10, 0);                   // 패딩 추가로 중앙에 위치하도록 설정
}

//////////////  Device, Wifi, Ethernet 설정 UI 이벤트 처리

//////////// ui_ScreenDevice에 포함된 위젯 이벤트 핸들러
void deviceInput_Focused(lv_event_t * e)
{
    lv_obj_t * target = lv_event_get_target(e);
    
    lv_obj_clear_flag(ui_Keyboard3, LV_OBJ_FLAG_HIDDEN);  // 숨겨져있던 키보드 보이기 

    // 키보드가 해당 텍스트 영역과 연결되도록 설정
    if(target == ui_txtaDeviceID){
        lv_keyboard_set_textarea(ui_Keyboard3, ui_txtaDeviceID);     // ip
    } else if(target == ui_txtaServerurl){
        lv_keyboard_set_textarea(ui_Keyboard3, ui_txtaServerurl);   // subnet
    }
}

void deviceInput_Defocused(lv_event_t * e)
{
	lv_obj_add_flag(ui_Keyboard3, LV_OBJ_FLAG_HIDDEN);  //포커스 잃으면 키보드 숨김
}

void btnSaveID_Click(lv_event_t * e)
{
	deviceManager->saveToDevice();
}

void btnLoadMqtt_Click(lv_event_t * e)
{
	fetchServerInfo();
}

void btnLoadImages_Click(lv_event_t * e)
{
	fetchImageFiles();
}

void btnReboot_Click(lv_event_t * e)
{
	// 약간의 딜레이 후 재부팅
    delay(1000);  // 1초 대기 후 재부팅

    lv_scr_load(ui_ScreenLogo);
    lv_timer_handler(); 

    delay(500);

    ESP.restart();  // 또는 ESP.reset(); 을 사용
}
////////////////////////////////////////////

////////////// ui_ScreenWifi에 포함된 위젯 이벤트 핸들러
void txtaPassword_Focused(lv_event_t * e)   // 비밀번호 입력창 포커스됨
{
  lv_keyboard_set_textarea(ui_Keyboard1, ui_txtaPassword);  // 키보드가 해당 텍스트 영역과 연결되도록 설정
  lv_obj_clear_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);  // 숨겨져있던 키보드 보이기 
}

void txtaPassword_Defocused(lv_event_t * e)      // 비밀번호 입력창 디포커스됨
{
  lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);  //포커스 잃으면 키보드 숨김
}

void chkLookPass_Checked(lv_event_t * e)
{
  bool checked = lv_obj_has_state(ui_chkLookPass, LV_STATE_CHECKED);
  lv_textarea_set_password_mode(ui_txtaPassword, !checked);  // 체크되면 비밀번호를 평문으로 표시, 체크 해제 시 *로 표시
}

void btnSerch_Click(lv_event_t * e)
{
  deviceManager->updateSSIDList();
}

void btnWifiConn_Click(lv_event_t * e)
{
  deviceManager->connectToWiFi();
}

extern void ibtnEthernet_Click(lv_event_t * e)
{
	lv_scr_load(ui_ScreenEthernet);  
}

extern void ibtnDevice_Click(lv_event_t * e)
{
	lv_scr_load(ui_ScreenDevice);
}
//////////////////////////////////////////

///////////////// ui_ScreenEthernet 이벤트 핸들러
void chkDhcp_Checked(lv_event_t * e)
{
    lv_obj_scroll_to(ui_Panel1, LV_HOR_RES, (LV_VER_RES * 0) / 10, LV_ANIM_ON);
}

void chkDhcp_Unchecked(lv_event_t * e)
{
    lv_obj_scroll_to(ui_Panel1, LV_HOR_RES, (LV_VER_RES * 0) / 10, LV_ANIM_ON);
}

void ethernetInput_Focused(lv_event_t * e)
{
  lv_obj_t * target = lv_event_get_target(e);
  
  lv_obj_clear_flag(ui_Keyboard2, LV_OBJ_FLAG_HIDDEN);  // 숨겨져있던 키보드 보이기 

  // 키보드가 해당 텍스트 영역과 연결되도록 설정
  if(target == ui_txtaIp){
    lv_keyboard_set_textarea(ui_Keyboard2, ui_txtaIp);     // ip
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_scroll_to(ui_Panel1, LV_HOR_RES, (LV_VER_RES * 0) / 10, LV_ANIM_ON);
  } else if(target == ui_txtaSubnet){
    lv_keyboard_set_textarea(ui_Keyboard2, ui_txtaSubnet);   // subnet
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_scroll_to(ui_Panel1, LV_HOR_RES, (LV_VER_RES * 1) / 10, LV_ANIM_ON);
  } else if(target == ui_txtaGateway){
    lv_keyboard_set_textarea(ui_Keyboard2, ui_txtaGateway);  // gateway
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_scroll_to(ui_Panel1, LV_HOR_RES, (LV_VER_RES * 3) / 10, LV_ANIM_ON);
  } else if(target == ui_txtaPriDns){
    lv_keyboard_set_textarea(ui_Keyboard2, ui_txtaPriDns);   // prime DNS
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 20);
    lv_obj_scroll_to(ui_Panel1, LV_HOR_RES, (LV_VER_RES * 4) / 10, LV_ANIM_ON);
  } else if(target == ui_txtaSeDns){
    lv_keyboard_set_textarea(ui_Keyboard2, ui_txtaSeDns);    // second DNS
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 40);
    lv_obj_scroll_to(ui_Panel1, LV_HOR_RES, (LV_VER_RES * 5) / 10, LV_ANIM_ON);
  }	
}

void ethernetInput_Defocused(lv_event_t * e)
{
	lv_obj_add_flag(ui_Keyboard2, LV_OBJ_FLAG_HIDDEN);  //포커스 잃으면 키보드 숨김
}

void btnEthernetConn_Click(lv_event_t * e)
{   
    deviceManager->connectToEthernet();
}

void ibtnWifi_Click(lv_event_t * e)
{
    lv_obj_t * target = lv_event_get_target(e);
  
    // 키보드가 해당 텍스트 영역과 연결되도록 설정
    if(target == ui_ibtnWifi){
        lv_scr_load(ui_ScreenDevice);
    } else {
        lv_scr_load(ui_ScreenEthernet);
    }
}
////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////
//  wifi이용 http file download 부분  - ethernet으로 바꿔서 참고용
/*
void DeviceManager::fetchServerConfig() {
    //서버에서 serverConfig 가져와서 로컬 저장
    String url = "/iot_device/serverconfig.json";

    if(fetchFileByHttp(SERVER_TEMP_FILE, url.c_str())){
        Serial.println("Download serverConfig file!");
        delay(100);

        if(ConfigManager::loadServerConfig(serverConfig, SERVER_TEMP_FILE)){
            delay(100);

            if(ConfigManager::saveServerConfig(serverConfig)){
                Serial.println("Restore serverConfig file!");
                FileUtils::remove(SERVER_TEMP_FILE);                
            }
        }
    } else {
        Serial.println("Fail to download serverConfig file!");
    }    
}

void DeviceManager::fetchImageConfig() {    
    //서버에서 imagesConfig 가져와서 로컬 저장
    String url = deviceConfig.serverURL.c_str();
    // image url    "/iot_device/[device_id]/" - [device_id]자리에 장치 id 넣어서 완성
    url += replaceID(serverConfig.imageUrl, deviceConfig.deviceID).c_str();
    url += "imagesconfig.json";

    if(ServerUtils::fetchConfig(IMAGES_TEMP_FILE, url.c_str())) {
        Serial.println("Download imagesConfig file!");

        if(ConfigManager::loadImagesConfig(imagesConfig, IMAGES_TEMP_FILE)) {
            delay(100);

            if(ConfigManager::saveImagesConfig(imagesConfig)) {
                Serial.println("Restore imagesConfig file!");
                ServerUtils::deleteTemp(IMAGES_TEMP_FILE);
                
                url = deviceConfig.serverURL.c_str();
                // image url    "/iot_device/[device_id]/" - [device_id]자리에 장치 id 넣어서 완성
                url += replaceID(serverConfig.imageUrl, deviceConfig.deviceID).c_str();
                String imageUrl, tempFile, moveFile;

                for (size_t i = 0; i < imagesConfig.images.size(); i++) {
                    imageUrl = url.c_str();
                    imageUrl += imagesConfig.images[i].url.c_str();

                    tempFile = "/temp/";
                    tempFile += imagesConfig.images[i].url.c_str();

                    moveFile = "/images/";
                    moveFile += imagesConfig.images[i].url.c_str();

                    Serial.print("Download image file url : ");
                    Serial.println(imageUrl.c_str());
                    Serial.print("Download image path : ");
                    Serial.println(tempFile.c_str());

                    int retryCount = 0;
                    int maxRetries = 3;
                    bool success = false;

                    while (retryCount < maxRetries && !success) {
                        if (ServerUtils::fetchImageFile(tempFile.c_str(), imageUrl.c_str())) {
                            Serial.println("BMP file downloaded successfully");
                            success = true;
                        } else {
                            Serial.printf("Failed to download BMP file, retrying... (%d/%d)\n", retryCount + 1, maxRetries);
                            retryCount++;
                            delay(2000);  // 2초 대기 후 재시도
                        }
                    }

                    if (success) {
                        ServerUtils::moveTemp(moveFile.c_str(), tempFile.c_str());
                    
                        Serial.println("download BMP file & move to images folder");
                    } else {
                        Serial.println("Failed to download BMP file after multiple retries");
                    }

                    delay(500);                    
                }

                ServerUtils::listFiles("/temp");
                ServerUtils::listFiles("/images");
            }
        }
    } else {
        Serial.println("Fail to download imageConfig file!");
    }    
}

bool DeviceManager::fetchFileByHttp(const char* targetPath, const char* _url)
{
    String domain;
    int port;

    // 서버 url에서 도메인, 포트 추출
    FileUtils::urlToDomain(deviceConfig.serverURL.c_str(), domain, port);


    Serial.println(domain.c_str());
    Serial.println(port);

    //httpClient 빈객체이면 생성시킴
    if (httpClient != nullptr) {
        httpClient = new HttpClient(ethClient, domain, port);
        Serial.println("HTTP Client initialize.");
    }

    // HTTP GET 요청 보내기
    httpClient->get(_url);

    // 응답 코드 확인
    int statusCode = httpClient->responseStatusCode();
    if (statusCode != 200) {
        Serial.print("Failed to download file, status code: ");
        Serial.println(statusCode);
        return false;
    }

    // SPIFFS 파일 시스템에 파일 저장
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount file system");
        return false;
    }

    // 하부 경로가 포함된 targetPath 경로의 디렉토리 생성
    FileUtils::createDir(targetPath);

    File file = SPIFFS.open(targetPath, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return false;
    }

    // HTTP 데이터를 받아서 파일에 쓰기
    while (httpClient->available()) {
        uint8_t buffer[128];  // 128 바이트씩 읽기
        int bytesRead = httpClient->read(buffer, sizeof(buffer));

        if (bytesRead > 0) {
            file.write(buffer, bytesRead);  // 읽은 바이트를 파일에 저장
        }
    }

    file.close();  // 파일 저장 완료 후 닫기
   
    // SPIFFS 내 전체 경로 및 파일 목록 출력
    Serial.println("SPIFFS contents:");
    FileUtils::list("/temp");  // 해당 디렉토리 모든 파일 출력

    return false;    
}
*/
