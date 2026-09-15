// Design Ref: §4.1 #7,8,9,10 — Config / ImagesConfig / Reboot
// Plan SC: FR-03

#include "ConfigApi.h"
#include "WebServer.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

static const char* DEVICE_CONFIG_PATH = "/deviceconfig.json";
static const char* IMAGES_CONFIG_PATH = "/imagesconfig.json";
static const char* SERVER_CONFIG_PATH = "/serverconfig.json";   // v2.7

void ConfigApi::attach(WebServer* ws) {
    if (!ws) return;
    ws->setImagesConfigGetter([this]() { return readImagesConfigJson(); });
    // device config getter/setter / reboot 은 WebServer 가 ConfigApi 직접 호출
    // (콜백 시그니처 추가는 WebServer.h 에서 setConfig* 로 확장)
    ws->setDeviceConfigGetter([this]() { return readDeviceConfigJson(); });
    ws->setDeviceConfigSetter([this](const String& body, String& err) {
        return writeDeviceConfigJson(body, err);
    });
    ws->setRebootHandler([this]() { requestReboot(); });
    // v2.7: server config getter/setter
    ws->setServerConfigGetter([this]() { return readServerConfigJson(); });
    ws->setServerConfigSetter([this](const String& body, String& err) {
        return writeServerConfigJson(body, err);
    });
    // v2.7: schedule getter/setter (deviceconfig 내 reboot_schedule)
    ws->setScheduleGetter([this]() { return readScheduleJson(); });
    ws->setScheduleSetter([this](const String& body, String& err) {
        return writeScheduleJson(body, err);
    });
}

String ConfigApi::readDeviceConfigJson() const {
    File f = SPIFFS.open(DEVICE_CONFIG_PATH, "r");
    if (!f) return "{}";
    String s; s.reserve(f.size() + 16);
    while (f.available()) s += (char)f.read();
    f.close();
    return s;
}

String ConfigApi::readImagesConfigJson() const {
    File f = SPIFFS.open(IMAGES_CONFIG_PATH, "r");
    if (!f) return "{}";
    String s; s.reserve(f.size() + 16);
    while (f.available()) s += (char)f.read();
    f.close();
    return s;
}

bool ConfigApi::writeDeviceConfigJson(const String& body, String& errOut) {
    // 1) JSON 검증
    StaticJsonDocument<2048> doc;
    DeserializationError de = deserializeJson(doc, body);
    if (de) {
        errOut = String("invalid_json: ") + de.f_str();
        return false;
    }
    // 2) 사이즈 sanity (deviceconfig 는 보통 1KB 이내)
    if (body.length() > 4096) {
        errOut = "too_large";
        return false;
    }
    // 3) atomic write — .tmp 작성 후 rename
    File f = SPIFFS.open("/deviceconfig.json.tmp", FILE_WRITE);
    if (!f) {
        errOut = "open_tmp_failed";
        return false;
    }
    size_t w = f.print(body);
    f.flush();
    f.close();
    if (w != body.length()) {
        SPIFFS.remove("/deviceconfig.json.tmp");
        errOut = "short_write";
        return false;
    }
    SPIFFS.remove(DEVICE_CONFIG_PATH);
    if (!SPIFFS.rename("/deviceconfig.json.tmp", DEVICE_CONFIG_PATH)) {
        errOut = "rename_failed";
        return false;
    }
    return true;
}

// v2.7: server config (device config 와 동일 패턴)
String ConfigApi::readServerConfigJson() const {
    File f = SPIFFS.open(SERVER_CONFIG_PATH, "r");
    if (!f) return "{}";
    String s; s.reserve(f.size() + 16);
    while (f.available()) s += (char)f.read();
    f.close();
    return s;
}

bool ConfigApi::writeServerConfigJson(const String& body, String& errOut) {
    StaticJsonDocument<2048> doc;
    DeserializationError de = deserializeJson(doc, body);
    if (de) { errOut = String("invalid_json: ") + de.f_str(); return false; }
    if (body.length() > 4096) { errOut = "too_large"; return false; }
    File f = SPIFFS.open("/serverconfig.json.tmp", FILE_WRITE);
    if (!f) { errOut = "open_tmp_failed"; return false; }
    size_t w = f.print(body);
    f.flush();
    f.close();
    if (w != body.length()) {
        SPIFFS.remove("/serverconfig.json.tmp");
        errOut = "short_write";
        return false;
    }
    SPIFFS.remove(SERVER_CONFIG_PATH);
    if (!SPIFFS.rename("/serverconfig.json.tmp", SERVER_CONFIG_PATH)) {
        errOut = "rename_failed";
        return false;
    }
    return true;
}

// v2.7: schedule = deviceconfig 내 reboot_schedule 서브트리 read / merge
String ConfigApi::readScheduleJson() const {
    DynamicJsonDocument doc(3072);
    File f = SPIFFS.open(DEVICE_CONFIG_PATH, "r");
    if (f) {
        deserializeJson(doc, f);
        f.close();
    }
    JsonVariant rs = doc["reboot_schedule"];
    String out;
    if (rs.isNull()) {
        out = "{\"enabled\":false,\"days\":[],\"hour\":4,\"minute\":0}";
    } else {
        serializeJson(rs, out);
    }
    return out;
}

bool ConfigApi::writeScheduleJson(const String& body, String& errOut) {
    // 1) body(schedule) 검증
    StaticJsonDocument<512> sched;
    if (deserializeJson(sched, body)) { errOut = "invalid_json"; return false; }
    // 2) deviceconfig 로드 후 reboot_schedule 병합
    DynamicJsonDocument doc(4096);
    File rf = SPIFFS.open(DEVICE_CONFIG_PATH, "r");
    if (!rf) { errOut = "device_config_open_failed"; return false; }
    DeserializationError de = deserializeJson(doc, rf);
    rf.close();
    if (de) { errOut = String("device_config_parse:") + de.f_str(); return false; }
    doc["reboot_schedule"] = sched.as<JsonObjectConst>();   // 서브트리 교체(deep copy)
    // 3) atomic write
    String out;
    serializeJson(doc, out);
    File f = SPIFFS.open("/deviceconfig.json.tmp", FILE_WRITE);
    if (!f) { errOut = "open_tmp_failed"; return false; }
    size_t w = f.print(out);
    f.flush(); f.close();
    if (w != out.length()) { SPIFFS.remove("/deviceconfig.json.tmp"); errOut = "short_write"; return false; }
    SPIFFS.remove(DEVICE_CONFIG_PATH);
    if (!SPIFFS.rename("/deviceconfig.json.tmp", DEVICE_CONFIG_PATH)) { errOut = "rename_failed"; return false; }
    return true;
}

void ConfigApi::loop() {
    if (_rebootRequested && _rebootArmedAt == 0) {
        _rebootArmedAt = millis();
        Serial.println("ConfigApi: reboot armed (1s grace)");
    }
    if (_rebootArmedAt > 0 && (millis() - _rebootArmedAt) > 1000) {
        Serial.println("ConfigApi: rebooting now");
        delay(100);
        ESP.restart();
    }
}
