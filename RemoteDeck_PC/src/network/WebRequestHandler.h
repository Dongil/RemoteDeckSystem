#pragma once
#include <Arduino.h>
#include <functional>
#include "config/DeviceConfig.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
}

class WebRequestHandler {
public:
    void begin(const WebRequestConfig* config, const DeviceConfig* deviceConfig);
    void fire(const char* event, int value);

    using StringGetter = std::function<String()>;
    void setIPGetter(StringGetter cb) { _getIP = cb; }
    void setMACGetter(StringGetter cb) { _getMAC = cb; }

    using LogCallback = std::function<void(const char* event, const char* detail)>;
    void setLogger(LogCallback cb) { _onLog = cb; }

private:
    const WebRequestConfig* _config = nullptr;
    const DeviceConfig* _deviceConfig = nullptr;
    StringGetter _getIP = nullptr;
    StringGetter _getMAC = nullptr;
    LogCallback _onLog = nullptr;

    QueueHandle_t _queue = nullptr;
    TaskHandle_t _task = nullptr;

    struct RequestItem {
        char url[256];
        uint16_t timeoutMs;
    };
    static constexpr size_t QUEUE_LEN = 8;

    String getURL(const char* event) const;
    String replacePlaceholders(const String& url, const char* event, int value) const;

    static void workerTaskTrampoline(void* arg);
    void workerLoop();
};
