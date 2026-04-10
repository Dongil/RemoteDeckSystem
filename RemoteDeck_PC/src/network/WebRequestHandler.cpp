#include "WebRequestHandler.h"
#include <HTTPClient.h>

void WebRequestHandler::begin(const WebRequestConfig* config, const DeviceConfig* deviceConfig) {
    _config = config;
    _deviceConfig = deviceConfig;
}

void WebRequestHandler::fire(const char* event, int value) {
    if (!_config || !_config->enabled) return;
    if (_busy) return;

    String url = getURL(event);
    if (url.isEmpty()) return;

    url = replacePlaceholders(url, event, value);

    auto* params = new RequestParams();
    strlcpy(params->url, url.c_str(), sizeof(params->url));
    params->timeoutMs = _config->timeoutMs;
    params->busyFlag = &_busy;
    _busy = true;

    xTaskCreate(httpGetTask, "webReq", 8192, params, 1, NULL);
}

String WebRequestHandler::getURL(const char* event) const {
    if (strcmp(event, "relay1_on") == 0)  return String(_config->relay1_on.c_str());
    if (strcmp(event, "relay1_off") == 0) return String(_config->relay1_off.c_str());
    if (strcmp(event, "relay2_on") == 0)  return String(_config->relay2_on.c_str());
    if (strcmp(event, "relay2_off") == 0) return String(_config->relay2_off.c_str());
    if (strcmp(event, "pcled_on") == 0)   return String(_config->pcled_on.c_str());
    if (strcmp(event, "pcled_off") == 0)  return String(_config->pcled_off.c_str());
    if (strcmp(event, "gpio1_high") == 0) return String(_config->gpio1_high.c_str());
    if (strcmp(event, "gpio1_low") == 0)  return String(_config->gpio1_low.c_str());
    if (strcmp(event, "gpio2_high") == 0) return String(_config->gpio2_high.c_str());
    if (strcmp(event, "gpio2_low") == 0)  return String(_config->gpio2_low.c_str());
    if (strcmp(event, "gpio3_high") == 0) return String(_config->gpio3_high.c_str());
    if (strcmp(event, "gpio3_low") == 0)  return String(_config->gpio3_low.c_str());
    return "";
}

String WebRequestHandler::replacePlaceholders(const String& url, const char* event, int value) const {
    String result = url;
    result.replace("[device_id]", _deviceConfig->deviceId.c_str());
    result.replace("[device_name]", _deviceConfig->deviceName.c_str());
    if (_getIP) result.replace("[ip]", _getIP());
    if (_getMAC) result.replace("[mac]", _getMAC());
    result.replace("[event]", event);
    result.replace("[value]", String(value));
    return result;
}

void WebRequestHandler::httpGetTask(void* param) {
    auto* p = (RequestParams*)param;
    HTTPClient http;
    http.setTimeout(p->timeoutMs);
    http.begin(p->url);
    int code = http.GET();
    if (code > 0) {
        Serial.printf("WebRequest [%d]: %s\n", code, p->url);
    } else {
        Serial.printf("WebRequest FAIL [%d]: %s\n", code, p->url);
    }
    http.end();
    *(p->busyFlag) = false;
    delete p;
    vTaskDelete(NULL);
}
