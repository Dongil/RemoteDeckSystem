#include "ethernet_mqtt.h"

extern ConfigManager configManager;
extern DeviceConfig deviceConfig;
extern ServerConfig serverConfig;

// Design Ref: §2.3 — WiFiClient는 lwIP socket → ETH/WiFi 공용
extern WiFiClient ethClient;
extern PubSubClient mqttEthernet_Client;

extern void mqttConnect_Broken();
extern void mqttConnect_ReConnect();

int retryCount = 0;
const int maxRetries = 5;
bool conn_broken = false;

void mqttEthernet_init()
{
    // Design Ref: §2.3, §12.3 — ETH_PHY_W5500 인라인 이식 (PC v2.3.0 NetManager 패턴)
    // Plan SC: FR-01, FR-02 (lwIP 통합으로 AsyncWebServer Ethernet 호환)
    SPI.begin(SCK_GPIO, MISO_GPIO, MOSI_GPIO, W5500_CS_GPIO);
    Serial.println("Network: Initializing W5500 via ETH.h");

    if (!ETH.begin(ETH_PHY_W5500, 1, W5500_CS_GPIO, INT_GPIO, -1, SPI)) {
        Serial.println("ETH.begin() failed");
        return;
    }

    // 고정 IP 또는 DHCP — DHCP는 ETH 이벤트가 자동 처리
    if (deviceConfig.networkConfig.usingStatic) {
        Serial.println("Static IP configuration is used.");
        IPAddress staticIP      = stringToIPAddress(deviceConfig.networkConfig.staticIP);
        IPAddress staticGateway = stringToIPAddress(deviceConfig.networkConfig.staticGateway);
        IPAddress staticSubnet  = stringToIPAddress(deviceConfig.networkConfig.staticSubnet);
        IPAddress primaryDNS    = stringToIPAddress(deviceConfig.networkConfig.staticPrimaryDNS);
        ETH.config(staticIP, staticGateway, staticSubnet, primaryDNS);
    } else {
        Serial.println("Dhcp IP configuration is used.");
    }

    // 링크 + IP 획득 폴링 (최대 10초) - Design §12.3
    unsigned long start = millis();
    while ((ETH.localIP() == IPAddress(0, 0, 0, 0)) && (millis() - start < 10000)) {
        Serial.print(".");
        delay(200);
    }
    Serial.println();

    if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println("Failed to configure Ethernet.");
        return;
    }

    // MAC 주소 저장 - ETH.macAddress() 가 String 반환
    String macStr = ETH.macAddress();
    deviceConfig.networkConfig.staticMAC = macStr.c_str();
    configManager.saveDeviceConfig(deviceConfig);

    Serial.print("Ethernet connected, MAC address: ");
    Serial.print(macStr);
    Serial.print(", IP address: ");
    Serial.println(ETH.localIP());

    delay(100);

    Serial.print("Try to Connect Mqtt server, Url : ");
    Serial.print(serverConfig.mqttConfig.url.c_str());
    Serial.print(", Port : ");
    Serial.println(serverConfig.mqttConfig.port);

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
    if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
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
    if (ETH.localIP() == IPAddress(0, 0, 0, 0)) {
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
    doc["ip"] = ETH.localIP();
    
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