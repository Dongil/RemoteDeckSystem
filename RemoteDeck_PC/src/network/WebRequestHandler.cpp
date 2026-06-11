#include "WebRequestHandler.h"
#include <HTTPClient.h>

void WebRequestHandler::begin(const WebRequestConfig* config, const DeviceConfig* deviceConfig) {
    _config = config;
    _deviceConfig = deviceConfig;

    if (_queue == nullptr) {
        _queue = xQueueCreate(QUEUE_LEN, sizeof(RequestItem));
    }
    if (_task == nullptr) {
        xTaskCreate(workerTaskTrampoline, "webReqWorker", 8192, this, 1, &_task);
    }
}

void WebRequestHandler::fire(const char* event, int value) {
    if (!_config || !_config->enabled) return;
    if (_queue == nullptr) return;

    String url = getURL(event);
    if (url.isEmpty()) return;

    url = replacePlaceholders(url, event, value);

    RequestItem item{};
    strlcpy(item.url, url.c_str(), sizeof(item.url));
    item.timeoutMs = _config->timeoutMs;

    if (xQueueSend(_queue, &item, 0) != pdTRUE) {
        Serial.printf("WebRequest DROP (queue full): %s\n", item.url);
        if (_onLog) {
            char detail[280];
            snprintf(detail, sizeof(detail), "DROP %s", item.url);
            _onLog("WEBREQ", detail);
        }
    }
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

void WebRequestHandler::workerTaskTrampoline(void* arg) {
    static_cast<WebRequestHandler*>(arg)->workerLoop();
}

void WebRequestHandler::workerLoop() {
    HTTPClient http;
    http.setReuse(true);

    RequestItem item;
    for (;;) {
        if (xQueueReceive(_queue, &item, portMAX_DELAY) != pdTRUE) continue;

        http.setTimeout(item.timeoutMs);
        http.setConnectTimeout(item.timeoutMs);

        if (!http.begin(item.url)) {
            Serial.printf("WebRequest BEGIN FAIL: %s\n", item.url);
            if (_onLog) {
                char detail[280];
                snprintf(detail, sizeof(detail), "BEGIN FAIL %s", item.url);
                _onLog("WEBREQ", detail);
            }
            continue;
        }

        int code = http.GET();
        if (code > 0) {
            Serial.printf("WebRequest [%d]: %s\n", code, item.url);
        } else {
            Serial.printf("WebRequest FAIL [%d]: %s\n", code, item.url);
        }
        if (_onLog) {
            char detail[300];
            snprintf(detail, sizeof(detail), "[%d] %s", code, item.url);
            _onLog("WEBREQ", detail);
        }
        http.end();
    }
}
