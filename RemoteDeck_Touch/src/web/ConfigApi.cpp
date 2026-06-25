// Design Ref: §4.1 #7,8,9,10 — Config / ImagesConfig / Reboot
// Plan SC: FR-03

#include "ConfigApi.h"
#include "WebServer.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

static const char* DEVICE_CONFIG_PATH = "/deviceconfig.json";
static const char* IMAGES_CONFIG_PATH = "/imagesconfig.json";

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
