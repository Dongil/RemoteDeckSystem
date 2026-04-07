#pragma once
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <functional>

class OTAHandler {
public:
    void setup(AsyncWebServer* server, const char* path = "/api/ota");

    using ProgressCallback = std::function<void(uint8_t percent)>;
    void setOnProgress(ProgressCallback cb) { _onProgress = cb; }

private:
    ProgressCallback _onProgress = nullptr;
    size_t _totalSize = 0;
};
