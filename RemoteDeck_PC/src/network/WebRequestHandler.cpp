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

// Design Ref: §5.2 — one-shot 부팅 sync. 기존 fire() 파이프라인 재사용.
// Plan SC-6/7: URL 미설정·reader unavailable 채널은 skip, 실패해도 부팅에 영향 없음.
void WebRequestHandler::syncCurrentStates(const StateReaders& readers) {
    auto tryFire = [&](const std::function<int()>& reader, const char* on, const char* off) {
        if (!reader) return;
        int v = reader();
        if (v < 0) return;  // unavailable
        const char* ev = (v == 1) ? on : off;
        fire(ev, v);  // fire()가 URL 빈 채널은 자체 skip
        if (_onLog) _onLog("BOOT_SYNC", ev);
    };
    tryFire(readers.gpio1, "gpio1_high", "gpio1_low");
    tryFire(readers.gpio2, "gpio2_high", "gpio2_low");
    tryFire(readers.gpio3, "gpio3_high", "gpio3_low");
    tryFire(readers.pcled, "pcled_on",   "pcled_off");
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
    // Design Ref: v2.6.1 §5.3 — Attendance URLs
    if (strcmp(event, "attendance_on") == 0)  return String(_config->attendance_on.c_str());
    if (strcmp(event, "attendance_off") == 0) return String(_config->attendance_off.c_str());
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
