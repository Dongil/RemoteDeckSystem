#include "MQTTHandler.h"

extern ConfigManager configManager; // ConfigManager 인스턴스는 외부에서 선언된 것으로 가정

MQTTHandler::MQTTHandler(PubSubClient& client, DeviceConfig& devConfig, ServerConfig& servConfig)
: mqtt_client(client), deviceConfig(devConfig), serverConfig(servConfig) {}

void MQTTHandler::setup() {
    connectToWiFi();
    mqtt_client.setServer(serverConfig.mqttConfig.url.c_str(), serverConfig.mqttConfig.port);
    connectToMQTT();
}

void MQTTHandler::loop() {
    ensureWiFiConnected();  // Wi-Fi 연결을 확인하고 재연결
  
    if (WiFi.status() != WL_CONNECTED) {        
        return; // WiFi가 연결되어 있지 않으면 MQTT 연결 시도하지 않음
    }

    //mqtt 재연결 횟수 이내이면 재연결 시도
    if (mqttRetryCount < maxRetries) {
      if (!mqtt_client.connected()) {
        reconnect();
      }
      mqtt_client.loop();
    }
}

void MQTTHandler::retryConnect(){
    //재연결 시도 초기화
    wifiRetryCount = 0;
    mqttRetryCount = 0;

    //재연결
    reconnect();
}

void MQTTHandler::publish(const char* topic, const char* payload) {
    mqtt_client.publish(topic, payload);
}

void MQTTHandler::xenoMqttPublish(const char* msg){
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
    doc["mac"] = deviceConfig.networkConfig.wifiMAC.c_str();
    doc["ip"] = WiFi.localIP();
    
    // JSON 객체를 문자열로 변환합니다
    char payload[256];
    serializeJson(doc, payload);

    // MQTT 메시지 발송
    mqtt_client.publish(serverConfig.mqttConfig.pubTopic.c_str(), payload);

    // 디버깅용으로 시리얼 출력
    Serial.print("Publishing to server : ");
    Serial.print(serverConfig.mqttConfig.pubTopic.c_str());
    Serial.print(", Message: ");
    Serial.println(payload);
}

void MQTTHandler::connectToWiFi() {
    if (wifiRetryCount >= maxRetries) {
        Serial.println("Max WiFi retry limit reached, skipping WiFi connection.");
        return;
    }

    WiFi.begin(deviceConfig.networkConfig.wifiSSID.c_str(), deviceConfig.networkConfig.wifiPasswd.c_str());
    Serial.print("Connecting to Wi-Fi");

    while (WiFi.status() != WL_CONNECTED && wifiRetryCount < maxRetries) {
        delay(500);
        Serial.print(".");
        wifiRetryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("Connected to Wi-Fi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        wifiRetryCount = 0;  // 성공적으로 연결되면 시도 횟수 초기화

        deviceConfig.networkConfig.wifiMAC = WiFi.macAddress().c_str();     //Wifi MAC 을 가져와서 장치설정에 저장
        configManager.saveDeviceConfig(deviceConfig);  
        Serial.print("MAC Address: ");
        Serial.println(deviceConfig.networkConfig.wifiMAC.c_str());
    } else {
        Serial.println();
        Serial.println("Failed to connect to Wi-Fi after 5 attempts.");
    }
}

void MQTTHandler::connectToMQTT() {
    if (mqttRetryCount >= maxRetries) {
        Serial.println("Max MQTT retry limit reached, skipping MQTT connection.");
        return;
    }

    // WiFi가 연결되어 있는지 확인
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi is not connected, skipping MQTT connection.");
        return; // WiFi가 연결되어 있지 않으면 MQTT 연결 시도하지 않음
    }

    while (!mqtt_client.connected() && mqttRetryCount < maxRetries) {
        Serial.print("Attempting MQTT connection...");
        
        if (mqtt_client.connect(deviceConfig.networkConfig.wifiMAC.c_str(), deviceConfig.networkConfig.wifiSSID.c_str(), deviceConfig.networkConfig.wifiPasswd.c_str())) {
            Serial.println("connected");

            // MQTT 토픽 구독   "room/[device_id]" - [device_id]자리에 장치 id 넣어서 구독
            String subTopic =  replaceID(serverConfig.mqttConfig.subTopic, deviceConfig.deviceID).c_str();
            
            mqtt_client.subscribe(subTopic.c_str());
            
            Serial.print("Subscribe Topic: ");
            Serial.println(subTopic);

            // 시스템 상태 발행       
            xenoMqttPublish("ONLINE");
            mqttRetryCount = 0;  // 성공적으로 연결되면 시도 횟수 초기화
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            mqttRetryCount++;
            delay(5000);
        }
    }

    if (!mqtt_client.connected()) {
      Serial.println("Failed to connect to MQTT after 5 attempts.");
    }
}

void MQTTHandler::reconnect() {
    connectToWiFi();  // Wi-Fi 연결을 확인하고 재연결
    
    // WiFi가 연결되어 있는 경우에만 MQTT 재연결
    if (isWiFiConnected()) {
        connectToMQTT();  
    }
}

void MQTTHandler::ensureWiFiConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        //wifi 재연결 횟수 이내이면 재연결 시도
        if (wifiRetryCount < maxRetries) {
            connectToWiFi();
        }
    }
}

bool MQTTHandler::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool MQTTHandler::isMQTTConnected() {
    return mqtt_client.connected();
}

void MQTTHandler::disConnectToMQTT() {
    // mqtt 연결 끊기
    if(mqtt_client.connected()){
        mqtt_client.disconnect();
    }

    // WiFi가 연결되어 있는지 확인
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
    }
    
    mqttRetryCount = 0;  // 성공적으로 연결되면 시도 횟수 초기화
}

// 외부에서 콜백 함수를 설정하는 메서드
void MQTTHandler::setCallback(MQTT_CALLBACK_SIGNATURE) {
    mqtt_client.setCallback(callback);
}

std::string MQTTHandler::replaceID(const std::string& template_str, const std::string& device_id) {
    std::string result = template_str;
    size_t start_pos = result.find("[device_id]");  // [device_id]의 위치 찾기

    if (start_pos != std::string::npos) {
        // [device_id]를 찾았으면 해당 부분을 device_id로 대체
        result.replace(start_pos, std::string("[device_id]").length(), device_id);
    }
    
    return result;
}