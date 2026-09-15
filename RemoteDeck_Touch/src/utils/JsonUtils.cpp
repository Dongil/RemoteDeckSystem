#include "JsonUtils.h"

// v2.7: 직전 deserializeDeviceConfig 호출에서 레거시 reboot_time → rebootSchedule 이관이
//   발생했는지 여부. setup()/다운로드 config 로드 후 1회 파일 persist 신호로 사용.
bool g_deviceConfigMigrated = false;

// DeviceConfig 직렬화
bool JsonUtils::serializeDeviceConfig(const DeviceConfig& config, String& output) {
    // JSON 문서를 저장할 StaticJsonDocument를 선언합니다. (v2.7: night_off/reboot_schedule 여유)
    StaticJsonDocument<2048> doc;

    // DeviceConfig 데이터를 JSON 형태로 변환합니다.
    doc["device_id"] = config.deviceID.c_str();

    // NetworkConfig 구조체를 저장합니다.
    JsonObject network = doc.createNestedObject("network_config");
    network["using_ethernet"] = config.networkConfig.usingEthernet;
    network["wifi_ssid"] = config.networkConfig.wifiSSID.c_str();
    network["wifi_passwd"] = config.networkConfig.wifiPasswd.c_str();
    network["wifi_mac"] = config.networkConfig.wifiMAC.c_str();
    network["using_static"] = config.networkConfig.usingStatic;
    network["static_ip"] = config.networkConfig.staticIP.c_str();
    network["static_gateway"] = config.networkConfig.staticGateway.c_str();
    network["static_subnet"] = config.networkConfig.staticSubnet.c_str();
    network["static_primaryDNS"] = config.networkConfig.staticPrimaryDNS.c_str();
    network["static_secondaryDNS"] = config.networkConfig.staticSecondaryDNS.c_str();
    network["static_mac"] = config.networkConfig.staticMAC.c_str();
    
    doc["server_url"] = config.serverURL.c_str();
    doc["reboot_time"] = config.rebootTime;
    doc["sleep_time"] = config.sleepTime;
    doc["web_config_mode"] = config.webConfigMode;   // v2.6

    // v2.7: 야간 화면 끄기
    JsonObject night = doc.createNestedObject("night_off");
    night["enabled"]      = config.nightOff.enabled;
    night["start_hour"]   = config.nightOff.startHour;
    night["start_minute"] = config.nightOff.startMinute;
    night["end_hour"]     = config.nightOff.endHour;
    night["end_minute"]   = config.nightOff.endMinute;

    // v2.7: 재부팅 스케줄
    JsonObject rs = doc.createNestedObject("reboot_schedule");
    rs["enabled"] = config.rebootSchedule.enabled;
    JsonArray rdays = rs.createNestedArray("days");
    for (int i = 0; i < 7; i++) if (config.rebootSchedule.days[i]) rdays.add(i);
    rs["hour"]    = config.rebootSchedule.hour;
    rs["minute"]  = config.rebootSchedule.minute;

    // VersionInfo 구조체를 저장합니다.
    JsonObject version = doc.createNestedObject("version_info");
    version["firmware_date"] = config.versionInfo.firmwareDate.c_str();
    version["server_config_version"] = config.versionInfo.serverConfigVersion.c_str();
    version["image_config_version"] = config.versionInfo.imageConfigVersion.c_str();

    // JSON 문서를 문자열로 직렬화합니다.
    serializeJson(doc, output);
    return true;
}

// DeviceConfig 역직렬화
bool JsonUtils::deserializeDeviceConfig(DeviceConfig& config, const String& json) {
    // JSON 문서를 저장할 StaticJsonDocument를 선언합니다. (v2.7: night_off/reboot_schedule 여유)
    StaticJsonDocument<2048> doc;

    // JSON 문자열을 파싱합니다.
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.f_str());
        return false;
    }

    g_deviceConfigMigrated = false;   // v2.7: 이번 로드의 마이그레이션 신호 초기화

    // JSON 데이터를 DeviceConfig 구조체로 변환합니다.
    config.deviceID = doc["device_id"].as<std::string>();

    // NetworkConfig 구조체를 로드합니다.
    JsonObject network = doc["network_config"];
    config.networkConfig.usingEthernet = network["using_ethernet"];
    config.networkConfig.wifiSSID = network["wifi_ssid"].as<std::string>();
    config.networkConfig.wifiPasswd = network["wifi_passwd"].as<std::string>();
    config.networkConfig.wifiMAC = network["wifi_mac"].as<std::string>();
    config.networkConfig.usingStatic = network["using_static"];
    config.networkConfig.staticIP = network["static_ip"].as<std::string>();
    config.networkConfig.staticGateway = network["static_gateway"].as<std::string>();
    config.networkConfig.staticSubnet = network["static_subnet"].as<std::string>();
    config.networkConfig.staticPrimaryDNS = network["static_primaryDNS"].as<std::string>();
    config.networkConfig.staticSecondaryDNS = network["static_secondaryDNS"].as<std::string>();
    config.networkConfig.staticMAC = network["static_mac"].as<std::string>();

    config.serverURL = doc["server_url"].as<std::string>();
    config.rebootTime = doc["reboot_time"];
    config.sleepTime = doc["sleep_time"];
    config.webConfigMode = doc["web_config_mode"] | false;   // v2.6: 필드 부재 시 false (하위호환)

    // v2.7: 야간 화면 끄기 (필드 부재 시 default — 하위호환)
    config.nightOff.enabled     = doc["night_off"]["enabled"] | false;
    config.nightOff.startHour   = doc["night_off"]["start_hour"] | 22;
    config.nightOff.startMinute = doc["night_off"]["start_minute"] | 0;
    config.nightOff.endHour     = doc["night_off"]["end_hour"] | 6;
    config.nightOff.endMinute   = doc["night_off"]["end_minute"] | 0;

    // v2.7: 재부팅 스케줄 (부재 시 default — 하위호환)
    config.rebootSchedule.enabled = doc["reboot_schedule"]["enabled"] | false;
    for (int i = 0; i < 7; i++) config.rebootSchedule.days[i] = false;
    JsonArray rdays = doc["reboot_schedule"]["days"];
    if (!rdays.isNull()) {
        for (JsonVariant v : rdays) { int d = v.as<int>(); if (d >= 0 && d < 7) config.rebootSchedule.days[d] = true; }
    }
    config.rebootSchedule.hour   = doc["reboot_schedule"]["hour"] | 4;
    config.rebootSchedule.minute = doc["reboot_schedule"]["minute"] | 0;

    // v2.7 마이그레이션: pre-v2.7 config 는 reboot_schedule 키가 없음.
    //   레거시 reboot_time(기본 7 = 매일 07:00 재부팅)을 rebootSchedule 로 이관 →
    //   재부팅을 웹 스케줄로 일원화(레거시 로직 제거)해도 기존 필드 기기의 자동 재부팅 유지.
    //   * reboot_schedule 키가 이미 있으면(v2.7+) 사용자가 껐을 수 있어 절대 덮어쓰지 않음.
    if (!doc.containsKey("reboot_schedule") && config.rebootTime > 0) {
        config.rebootSchedule.enabled = true;
        for (int i = 0; i < 7; i++) config.rebootSchedule.days[i] = true;
        config.rebootSchedule.hour   = config.rebootTime;
        config.rebootSchedule.minute = 0;
        g_deviceConfigMigrated = true;
        Serial.printf("[v2.7] reboot_time=%d → rebootSchedule 매일 %02d:00 이관\n",
                      config.rebootTime, config.rebootTime);
    }

    // VersionInfo 구조체를 로드합니다.
    JsonObject version = doc["version_info"];
    config.versionInfo.firmwareDate = version["firmware_date"].as<std::string>();
    config.versionInfo.serverConfigVersion = version["server_config_version"].as<std::string>();
    config.versionInfo.imageConfigVersion = version["image_config_version"].as<std::string>();

    return true;
}

// ServerConfig 직렬화
bool JsonUtils::serializeServerConfig(const ServerConfig& config, String& output) {
    StaticJsonDocument<1024> doc;
    doc["version"] = config.version.c_str();
    doc["config_url"] = config.configUrl.c_str();
    doc["image_url"] = config.imageUrl.c_str();
    doc["status_url"] = config.statusUrl.c_str();
    
    doc["mqtt_url"] = config.mqttConfig.url.c_str();
    doc["mqtt_user"] = config.mqttConfig.user.c_str();
    doc["mqtt_passwd"] = config.mqttConfig.passwd.c_str();
    doc["mqtt_port"] = config.mqttConfig.port;
    doc["mqtt_keepalive"] = config.mqttConfig.keepalive;
    doc["mqtt_pub"] = config.mqttConfig.pubTopic.c_str();
    doc["mqtt_sub"] = config.mqttConfig.subTopic.c_str();
    doc["mqtt_ping"] = config.mqttConfig.pingTopic.c_str();
    
    doc["using_httprequest"] = config.usingHttpRequest;
    doc["sleep_time"] = config.sleepTime;

    serializeJson(doc, output);
    
    return true;
}

// ServerConfig 역직렬화
bool JsonUtils::deserializeServerConfig(ServerConfig& config, const String& json) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.print("Failed to parse ServerConfig JSON: ");
        Serial.println(error.f_str());
        return false;
    }

    config.version = doc["version"].as<std::string>();
    config.configUrl = doc["config_url"].as<std::string>();
    config.imageUrl = doc["image_url"].as<std::string>();
    config.statusUrl = doc["status_url"].as<std::string>();

    config.mqttConfig.url = doc["mqtt_url"].as<std::string>();
    config.mqttConfig.user = doc["mqtt_user"].as<std::string>();
    config.mqttConfig.passwd = doc["mqtt_passwd"].as<std::string>();
    config.mqttConfig.port = doc["mqtt_port"];
    config.mqttConfig.keepalive = doc["mqtt_keepalive"];
    config.mqttConfig.pubTopic = doc["mqtt_pub"].as<std::string>();
    config.mqttConfig.subTopic = doc["mqtt_sub"].as<std::string>();
    config.mqttConfig.pingTopic = doc["mqtt_ping"].as<std::string>();

    config.usingHttpRequest = doc["using_httprequest"];
    config.sleepTime = doc["sleep_time"];

    return true;
}

// ImagesConfig 직렬화
bool JsonUtils::serializeImagesConfig(const ImagesConfig& config, String& output) {
     StaticJsonDocument<4096> doc;
    doc["version"] = config.version.c_str();

    // Buzzer 설정을 담는 JsonObject 생성
    JsonObject buzzerObj = doc.createNestedObject("buzzer");

    // Buzzer On 배열 생성
    JsonArray onArray = buzzerObj.createNestedArray("on");
    for (const auto& setting : config.buzzerOn) {
        JsonObject obj = onArray.createNestedObject();
        obj["freq"] = setting.freq;
        obj["duration"] = setting.duration;
        obj["duty"] = setting.duty;
    }

    // Buzzer Off 배열 생성
    JsonArray offArray = buzzerObj.createNestedArray("off");
    for (const auto& setting : config.buzzerOff) {
        JsonObject obj = offArray.createNestedObject();
        obj["freq"] = setting.freq;
        obj["duration"] = setting.duration;
        obj["duty"] = setting.duty;
    }

    // Images 배열 생성
    JsonArray imagesArray = doc.createNestedArray("images");
    for (const auto& image : config.images) {
        JsonObject obj = imagesArray.createNestedObject();
        obj["url"] = image.url.c_str();
        obj["x"] = image.x;
        obj["y"] = image.y;
        obj["z"] = image.z;
        obj["state"] = image.state.c_str();
    }

    // JSON 직렬화
    serializeJson(doc, output);
    return true;
}

// ImagesConfig 역직렬화
bool JsonUtils::deserializeImagesConfig(ImagesConfig& config, const String& json) {
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.print("Failed to parse ImagesConfig JSON: ");
        Serial.println(error.f_str());
        return false;
    }

    config.version = doc["version"].as<std::string>();

    JsonArray onArray = doc["buzzer"]["on"].as<JsonArray>();
    config.buzzerOn.clear();
    for (JsonVariant v : onArray) {
        BuzzerSetting setting;
        setting.freq = v["freq"];
        setting.duration = v["duration"];
        setting.duty = v["duty"];
        config.buzzerOn.push_back(setting);
    }

    JsonArray offArray = doc["buzzer"]["off"].as<JsonArray>();
    config.buzzerOff.clear();
    for (JsonVariant v : offArray) {
        BuzzerSetting setting;
        setting.freq = v["freq"];
        setting.duration = v["duration"];
        setting.duty = v["duty"];
        config.buzzerOff.push_back(setting);
    }

    config.images.clear();
    JsonArray imagesArray = doc["images"].as<JsonArray>();
    for (JsonVariant v : imagesArray) {
        ImageSetting image;
        image.url = v["url"].as<std::string>();
        image.x = v["x"];
        image.y = v["y"];
        image.z = v["z"];
        image.state = v["state"].as<std::string>();
        config.images.push_back(image);
    }

    return true;
}
