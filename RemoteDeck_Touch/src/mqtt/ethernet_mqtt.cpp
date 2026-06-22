#include "ethernet_mqtt.h"
#include "esp_mac.h"   // esp_read_mac, ESP_MAC_ETH (Arduino-ESP32 3.x에서는 자동 include 안 됨)

extern ConfigManager configManager; // ConfigManager 인스턴스는 외부에서 선언된 것으로 가정
extern DeviceConfig deviceConfig;   // 장치 설정 
extern ServerConfig serverConfig;   // 서버 설정

extern EthernetClient ethClient;
extern PubSubClient mqttEthernet_Client;

extern void mqttConnect_Broken();
extern void mqttConnect_ReConnect();

int retryCount = 0;
const int maxRetries = 5;   // Ethernet MQTT 재시도 최대 횟수
bool conn_broken = false;   // mqtt 연결 끊김

void mqttEthernet_init()
{
    // W5500 initialization
    SPI.begin(SCK_GPIO, MISO_GPIO, MOSI_GPIO, W5500_CS_GPIO);
    Ethernet.init(W5500_CS_GPIO); // W5500 CS 핀 설정

    delay(100);
    
    // 링크 상태 확인
    if (Ethernet.linkStatus() == LinkON) {
        Serial.println("Ethernet cable is connected.");
    } else {
        Serial.println("Ethernet cable is not connected.");
        return;
    }    

    // dhcp 사용으로 이더넷 연결
    byte mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);  // MAC 주소 읽기

    char strMAC[18];  // "XX:XX:XX:XX:XX:XX" 형태의 문자열을 저장할 배열 (17 + NULL 문자 1개)
    mqttEthernet_macToStr(mac, strMAC);  // MAC 주소 변환 함수 호출

    unsigned ethRetry_t = millis();

    // 고정 IP 또는 DHCP 선택
    if (deviceConfig.networkConfig.usingStatic) {
        Serial.println("Static IP configuration is used.");

        // 고정 IP 사용
        IPAddress staticIP = stringToIPAddress(deviceConfig.networkConfig.staticIP);
        IPAddress staticGateway = stringToIPAddress(deviceConfig.networkConfig.staticGateway);
        IPAddress staticSubnet = stringToIPAddress(deviceConfig.networkConfig.staticSubnet);
        IPAddress primaryDNS = stringToIPAddress(deviceConfig.networkConfig.staticPrimaryDNS);
        //IPAddress secondaryDNS = stringToIPAddress(deviceConfig.networkConfig.staticSecondaryDNS);

        Ethernet.begin(mac, staticIP, primaryDNS, staticGateway, staticSubnet);
           
        while ( millis() - ethRetry_t < 5000){
            Serial.print(".");
            delay(100);
        }
    } else {
        Serial.println("Dhcp IP configuration is used.");

        // DHCP 사용
        while (Ethernet.begin(mac) == 0 && millis() - ethRetry_t < 5000){
            Serial.print(".");
            delay(100);
        }
    }
    
    // IP 주소 확인
    if (Ethernet.localIP() != INADDR_NONE) {  // INADDR_NONE은 0.0.0.0을 의미
        deviceConfig.networkConfig.staticMAC = strMAC;   // Ethernet MAC 을 가져와서 장치설정에 저장
        configManager.saveDeviceConfig(deviceConfig);
        Serial.print("Ethernet connected, MAC address: ");
        Serial.print(deviceConfig.networkConfig.staticMAC.c_str());
        Serial.print(", IP address: ");
        Serial.println(Ethernet.localIP());
    } else {
        Serial.println("Failed to configure Ethernet.");
    }

    delay(100);


    Serial.print("Try to Connect Mqtt server, Url : ");
    Serial.print(serverConfig.mqttConfig.url.c_str());
    Serial.print(", Port : ");
    Serial.println(serverConfig.mqttConfig.port);

    // MQTT server setup
    mqttEthernet_Client.setServer(serverConfig.mqttConfig.url.c_str(), serverConfig.mqttConfig.port);
    mqttEthernet_connect();
}

void mqttEthernet_connect() {    
    if (retryCount >= maxRetries) {
        Serial.println("Max Ethernet MQTT retry limit reached, skipping MQTT connection.");
        return;
    }
    
    // 링크 상태 확인
    //if (Ethernet.linkStatus() != LinkON) {
    //    Serial.println("Ethernet cable is not connected.");
    //    return;
    //}
    
    // IP 주소 확인
    if (Ethernet.localIP() == INADDR_NONE) {  // INADDR_NONE은 0.0.0.0을 의미
        Serial.println("Mqtt Not Try Connect. Failed to configure Ethernet");
        return;
    }

    while (!mqttEthernet_Client.connected() && retryCount < maxRetries) {
        Serial.print("Attempting MQTT connection...");
        
        // 클라이언트 ID 생성
        String clientId = "SmartRoomClient-";
        clientId += String(random(0xffff), HEX);
        
        if (mqttEthernet_Client.connect(clientId.c_str())) {
            Serial.println("connected");
                        
            // MQTT 토픽 구독   "room/[device_id]" - [device_id]자리에 장치 id 넣어서 구독
            String subTopic =  mqttEthernet_replaceID(serverConfig.mqttConfig.subTopic, deviceConfig.deviceID).c_str();
            
            mqttEthernet_Client.subscribe(subTopic.c_str());
            
            Serial.print("Subscribe Topic: ");
            Serial.println(subTopic);

            delay(100);
            
            // 시스템 상태 발행       
            if(!serverConfig.usingHttpRequest) {
                mqttEthernet_publish("ONLINE");
            }

            retryCount = 0;  // 성공적으로 연결되면 시도 횟수 초기화
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttEthernet_Client.state());
            Serial.println(" try again in 5 seconds");
            retryCount++;
            delay(5000);
        }

        delay(1000);
    }

    if (!mqttEthernet_Client.connected()) {
        Serial.println("Failed to connect to MQTT after 5 attempts.");
    }
}

void mqttEthernet_reconnect() {    
    
    if (retryCount == maxRetries) {
        retryCount = 0;        
    }
    
    // 링크 상태 확인
    //if (Ethernet.linkStatus() != LinkON) {
    //    Serial.println("Ethernet cable is not connected.");
    //    return;
    //}
    
    // IP 주소 확인
    if (Ethernet.localIP() == INADDR_NONE) {  // INADDR_NONE은 0.0.0.0을 의미
        Serial.println("Mqtt Not Try Connect. Failed to configure Ethernet");
        return;
    }

    while (!mqttEthernet_Client.connected() && retryCount < maxRetries) {
        Serial.print("Attempting MQTT connection...");
        
        // 클라이언트 ID 생성
        String clientId = "SmartRoomClient-";
        clientId += String(random(0xffff), HEX);
        
        if (mqttEthernet_Client.connect(clientId.c_str())) {
            Serial.println("connected");
                        
            // MQTT 토픽 구독   "room/[device_id]" - [device_id]자리에 장치 id 넣어서 구독
            String subTopic =  mqttEthernet_replaceID(serverConfig.mqttConfig.subTopic, deviceConfig.deviceID).c_str();
            
            mqttEthernet_Client.subscribe(subTopic.c_str());
            
            Serial.print("Subscribe Topic: ");
            Serial.println(subTopic);

            delay(100);
            
            // 시스템 상태 발행       
            if(!serverConfig.usingHttpRequest) {
                mqttEthernet_publish("ONLINE");
            }

            if(conn_broken){
                mqttConnect_ReConnect();    
                Serial.println("Goto Main Screen");
            }

            conn_broken = false;
            retryCount = 0;  // 성공적으로 연결되면 시도 횟수 초기화            
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttEthernet_Client.state());
            Serial.println(" try again in 5 seconds");
            retryCount++;
            delay(4000);
        }

        delay(1000);
    }

    if (!mqttEthernet_Client.connected() &&
        !conn_broken ) {
        conn_broken = true;
        Serial.println("Goto Device Config Screen");
        mqttConnect_Broken();
    }
}

void mqttEthernet_loop()
{
    // MQTT 서버와의 연결 유지
    if (!mqttEthernet_Client.connected()) {
        mqttEthernet_reconnect();
    }

    mqttEthernet_Client.loop();
}

void mqttEthernet_setCallback(MQTT_CALLBACK_SIGNATURE)
{
    mqttEthernet_Client.setCallback(callback);
}

void mqttEthernet_publish(const char* msg){
    /*
    payload 형식 :	{"device_id":"node_2","status":"IN","ip":"192.168.0.1","mac":"08b61f3a1e84"}
    "status" : 상태전송
	"IN" - 재실 상태
	"OUT" - 부재 상태
	"ONLINE" - 단말기 연결됨 
    */

    // JSON 객체를 만듭니다
    StaticJsonDocument<256> doc;
    doc["device_id"] = deviceConfig.deviceID.c_str();
    doc["status"] = msg;
    doc["mac"] = deviceConfig.networkConfig.staticMAC.c_str();
    doc["ip"] = Ethernet.localIP();
    
    // JSON 객체를 문자열로 변환합니다
    char payload[256];
    serializeJson(doc, payload);

    // MQTT 메시지 발송
    mqttEthernet_Client.publish(serverConfig.mqttConfig.pubTopic.c_str(), payload);

    // 디버깅용으로 시리얼 출력
    Serial.print("Publishing to server : ");
    Serial.print(serverConfig.mqttConfig.pubTopic.c_str());
    Serial.print(", Message: ");
    Serial.println(payload);
}

void mqttEthernet_disconnect(){
    mqttEthernet_Client.disconnect();
}

bool mqttEthernet_connected(){
    return mqttEthernet_Client.connected();
}

void mqttEthernet_retryClear() {
    retryCount = 0;
}

void mqttEthernet_macToStr(const byte* mac, char* strMAC) {
    sprintf(strMAC, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

std::string mqttEthernet_replaceID(const std::string& template_str, const std::string& device_id) {
    std::string result = template_str;
    size_t start_pos = result.find("[device_id]");  // [device_id]의 위치 찾기

    if (start_pos != std::string::npos) {
        // [device_id]를 찾았으면 해당 부분을 device_id로 대체
        result.replace(start_pos, std::string("[device_id]").length(), device_id);
    }
    
    return result;
}

// 문자열을 IPAddress로 변환하는 함수
IPAddress stringToIPAddress(const std::string& str)
{
    int parts[4] = {0};  // IP 주소의 각 부분을 저장할 배열
    sscanf(str.c_str(), "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]);  // 문자열을 파싱

    return IPAddress(parts[0], parts[1], parts[2], parts[3]);
}