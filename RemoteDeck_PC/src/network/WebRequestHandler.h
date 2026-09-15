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

    // Design Ref: §5.2 — 부팅 후 1회 채널 상태 sync (best-effort)
    struct StateReaders {
        std::function<int()> gpio1;  // 0=LOW, 1=HIGH, -1=unavailable
        std::function<int()> gpio2;
        std::function<int()> gpio3;
        std::function<int()> pcled;  // 0=OFF, 1=ON, -1=unavailable
    };
    void syncCurrentStates(const StateReaders& readers);

    using StringGetter = std::function<String()>;
    void setIPGetter(StringGetter cb) { _getIP = cb; }
    void setMACGetter(StringGetter cb) { _getMAC = cb; }

    using LogCallback = std::function<void(const char* event, const char* detail)>;
    void setLogger(LogCallback cb) { _onLog = cb; }

    // v2.6.2 fix-1: 이벤트별 HTTP 결과 콜백 (event 이름 + httpCode)
    using ResultCallback = std::function<void(const char* event, int httpCode)>;
    void setResultCallback(ResultCallback cb) { _onResult = cb; }

private:
    const WebRequestConfig* _config = nullptr;
    const DeviceConfig* _deviceConfig = nullptr;
    StringGetter _getIP = nullptr;
    StringGetter _getMAC = nullptr;
    LogCallback _onLog = nullptr;
    ResultCallback _onResult = nullptr;

    QueueHandle_t _queue = nullptr;
    TaskHandle_t _task = nullptr;

    struct RequestItem {
        char url[256];
        char event[24];      // v2.6.2 fix-1: 결과 콜백 라우팅용
        uint16_t timeoutMs;
    };
    static constexpr size_t QUEUE_LEN = 8;

    String getURL(const char* event) const;
    String replacePlaceholders(const String& url, const char* event, int value) const;

    static void workerTaskTrampoline(void* arg);
    void workerLoop();
};
