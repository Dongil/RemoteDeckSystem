#pragma once
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <functional>

class OTAHandler {
public:
    void setup(AsyncWebServer* server, const char* path = "/api/ota");

    using ProgressCallback = std::function<void(uint8_t percent)>;
    void setOnProgress(ProgressCallback cb) { _onProgress = cb; }

    using FilenameCallback = std::function<void(const String& filename)>;
    void setOnFilename(FilenameCallback cb) { _onFilename = cb; }

private:
    ProgressCallback _onProgress = nullptr;
    FilenameCallback _onFilename = nullptr;
    size_t _totalSize = 0;
};
