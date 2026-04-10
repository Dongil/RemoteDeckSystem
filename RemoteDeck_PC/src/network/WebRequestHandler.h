#pragma once
#include <Arduino.h>
#include <functional>
#include "config/DeviceConfig.h"

class WebRequestHandler {
public:
    void begin(const WebRequestConfig* config, const DeviceConfig* deviceConfig);
    void fire(const char* event, int value);

    using StringGetter = std::function<String()>;
    void setIPGetter(StringGetter cb) { _getIP = cb; }
    void setMACGetter(StringGetter cb) { _getMAC = cb; }

private:
    const WebRequestConfig* _config = nullptr;
    const DeviceConfig* _deviceConfig = nullptr;
    StringGetter _getIP = nullptr;
    StringGetter _getMAC = nullptr;

    volatile bool _busy = false;

    String getURL(const char* event) const;
    String replacePlaceholders(const String& url, const char* event, int value) const;

    struct RequestParams {
        char url[256];
        uint16_t timeoutMs;
        volatile bool* busyFlag;
    };
    static void httpGetTask(void* param);
};
