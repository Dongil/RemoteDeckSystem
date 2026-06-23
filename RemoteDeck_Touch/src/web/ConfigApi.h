#pragma once
// Design Ref: §4.2 /api/config + /api/serverconfig
// Plan SC: FR-05 (설정 편집 + 저장 + 재부팅 자동 적용)

#include <Arduino.h>

class TouchWebServer;
class Logger;

class ConfigApi {
public:
    void attach(TouchWebServer* ws, Logger* logger);

private:
    TouchWebServer* _ws = nullptr;
    Logger* _logger = nullptr;

    void handleGetDeviceConfig();
    void handlePostDeviceConfig();
    void handleGetServerConfig();
    void handlePostServerConfig();
    void handleReboot();

    void log(const char* event, const char* detail);
    void streamSpiffsJson(const char* path);
    bool acceptUploadAsJson(const char* targetPath, const char* logEvent);
};
