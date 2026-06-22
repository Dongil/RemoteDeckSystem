#include "ConfigManager.h"

const char* DEVICE_CONFIG_FILE = "/deviceconfig.json";     // DeviceConfig 파일 경로 (기기 환경 설정)
const char* SERVER_CONFIG_FILE = "/serverconfig.json";     // ServerConfig 파일 경로 (연동 서버 환경 설정)
const char* IMAGES_CONFIG_FILE = "/imagesconfig.json";      // ImagesConfig 파일 경로 (UI 환경 설정)

bool ConfigManager::loadDeviceConfig(DeviceConfig& config, const char* filePath) {
    // SPIFFS에서 파일을 엽니다.
    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        Serial.print(filePath);
        Serial.println(" : Failed to open device config file for reading");        
        return false;
    }

    // 파일에서 JSON 데이터를 읽어옵니다.
    String json = file.readString();
    file.close();

    //Serial.println(json);
    
    // JSON 데이터를 역직렬화합니다.
    return JsonUtils::deserializeDeviceConfig(config, json);
}

bool ConfigManager::loadDeviceConfig(DeviceConfig& config) {
    // SPIFFS에서 파일을 엽니다.
    File file = SPIFFS.open(DEVICE_CONFIG_FILE, "r");
    if (!file) {
        Serial.print(DEVICE_CONFIG_FILE);
        Serial.println(" : Failed to open device config file for reading");        
        return false;
    }

    // 파일에서 JSON 데이터를 읽어옵니다.
    String json = file.readString();
    file.close();

    //Serial.println(json);
    
    // JSON 데이터를 역직렬화합니다.
    return JsonUtils::deserializeDeviceConfig(config, json);
}

bool ConfigManager::saveDeviceConfig(const DeviceConfig& config, const char* filePath) {
    // JSON 데이터를 직렬화합니다.
    String json;
    if (!JsonUtils::serializeDeviceConfig(config, json)) {
        Serial.println("Failed to serialize device config");
        return false;
    }

    // SPIFFS에 파일을 열어 JSON 데이터를 씁니다.
    File file = SPIFFS.open(filePath, "w");
    if (!file) {
        Serial.print(filePath);
        Serial.println(" : Failed to open device config file for reading"); 
        return false;
    }

    //Serial.println(json);

    file.print(json);
    file.close();
    return true;
}

bool ConfigManager::saveDeviceConfig(const DeviceConfig& config) {
    // JSON 데이터를 직렬화합니다.
    String json;
    if (!JsonUtils::serializeDeviceConfig(config, json)) {
        Serial.println("Failed to serialize device config");
        return false;
    }

    // SPIFFS에 파일을 열어 JSON 데이터를 씁니다.
    File file = SPIFFS.open(DEVICE_CONFIG_FILE, "w");
    if (!file) {
        Serial.print(DEVICE_CONFIG_FILE);
        Serial.println(" : Failed to open device config file for reading"); 
        return false;
    }

    //Serial.println(json);

    file.print(json);
    file.close();
    return true;
}

bool ConfigManager::loadServerConfig(ServerConfig& config, const char* filePath) {
    // SPIFFS에서 파일을 엽니다.
    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        Serial.print(filePath);
        Serial.println(" : Failed to open server config file for reading");
        return false;
    }

    // 파일에서 JSON 데이터를 읽어옵니다.
    String json = file.readString();
    file.close();

    //Serial.println(json);

    // JSON 데이터를 역직렬화합니다.
    return JsonUtils::deserializeServerConfig(config, json);
}

bool ConfigManager::loadServerConfig(ServerConfig& config) {
    // SPIFFS에서 파일을 엽니다.
    File file = SPIFFS.open(SERVER_CONFIG_FILE, "r");
    if (!file) {
        Serial.print(SERVER_CONFIG_FILE);
        Serial.println(" : Failed to open server config file for reading");
        return false;
    }

    // 파일에서 JSON 데이터를 읽어옵니다.
    String json = file.readString();
    file.close();

    //Serial.println(json);

    // JSON 데이터를 역직렬화합니다.
    return JsonUtils::deserializeServerConfig(config, json);
}

bool ConfigManager::saveServerConfig(const ServerConfig& config, const char* filePath) {
    // JSON 데이터를 직렬화합니다.
    String json;
    if (!JsonUtils::serializeServerConfig(config, json)) {
        Serial.println(" : Failed to serialize server config");
        return false;
    }

    // SPIFFS에 파일을 열어 JSON 데이터를 씁니다.
    File file = SPIFFS.open(SERVER_CONFIG_FILE, "w");
    if (!file) {
        Serial.print(SERVER_CONFIG_FILE);
        Serial.println("Failed to open server config file for writing");
        return false;
    }

    //Serial.println(json);

    file.print(json);
    file.close();
    return true;
}

bool ConfigManager::saveServerConfig(const ServerConfig& config) {
    // JSON 데이터를 직렬화합니다.
    String json;
    if (!JsonUtils::serializeServerConfig(config, json)) {
        
        Serial.println(" : Failed to serialize server config");
        return false;
    }

    // SPIFFS에 파일을 열어 JSON 데이터를 씁니다.
    File file = SPIFFS.open(SERVER_CONFIG_FILE, "w");
    if (!file) {
        Serial.print(SERVER_CONFIG_FILE);
        Serial.println("Failed to open server config file for writing");
        return false;
    }

    //Serial.println(json);

    file.print(json);
    file.close();
    return true;
}

bool ConfigManager::loadImagesConfig(ImagesConfig& config, const char* filePath) {
    // SPIFFS에서 파일을 엽니다.
    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        Serial.print(filePath);
        Serial.println(" : Failed to open images config file for reading");
        return false;
    }

    // 파일에서 JSON 데이터를 읽어옵니다.
    String json = file.readString();
    file.close();

    //Serial.println(json);

    // JSON 데이터를 역직렬화합니다.
    return JsonUtils::deserializeImagesConfig(config, json);
}

bool ConfigManager::loadImagesConfig(ImagesConfig& config) {
    // SPIFFS에서 파일을 엽니다.
    File file = SPIFFS.open(IMAGES_CONFIG_FILE, "r");
    if (!file) {
        Serial.print(IMAGES_CONFIG_FILE);
        Serial.println(" : Failed to open images config file for reading");
        return false;
    }

    // 파일에서 JSON 데이터를 읽어옵니다.
    String json = file.readString();
    file.close();

    //Serial.println(json);

    // JSON 데이터를 역직렬화합니다.
    return JsonUtils::deserializeImagesConfig(config, json);
}

bool ConfigManager::saveImagesConfig(const ImagesConfig& config, const char* filePath) {
    // JSON 데이터를 직렬화합니다.
    String json;
    if (!JsonUtils::serializeImagesConfig(config, json)) {
        Serial.println(" : Failed to serialize images config");
        return false;
    }

    // SPIFFS에 파일을 열어 JSON 데이터를 씁니다.
    File file = SPIFFS.open(filePath, "w");
    if (!file) {
        Serial.print(filePath);
        Serial.println("Failed to open images config file for writing");
        return false;
    }

    //Serial.println(json);

    file.print(json);
    file.close();
    return true;
}

bool ConfigManager::saveImagesConfig(const ImagesConfig& config) {
    // JSON 데이터를 직렬화합니다.
    String json;
    if (!JsonUtils::serializeImagesConfig(config, json)) {
        Serial.println(" : Failed to serialize images config");
        return false;
    }

    // SPIFFS에 파일을 열어 JSON 데이터를 씁니다.
    File file = SPIFFS.open(IMAGES_CONFIG_FILE, "w");
    if (!file) {
        Serial.print(IMAGES_CONFIG_FILE);
        Serial.println("Failed to open images config file for writing");
        return false;
    }

    //Serial.println(json);

    file.print(json);
    file.close();
    return true;
}