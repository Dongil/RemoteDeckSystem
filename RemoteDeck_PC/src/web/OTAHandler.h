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

    // Design Ref: rcm-hub-v3.0 §4.4 — 업로드 인증. nullptr이면 인증 없음(기존 동작).
    using AuthCheck = std::function<bool(AsyncWebServerRequest* req)>;
    void setAuthCheck(AuthCheck cb) { _authCheck = cb; }

private:
    ProgressCallback _onProgress = nullptr;
    FilenameCallback _onFilename = nullptr;
    AuthCheck _authCheck = nullptr;
    bool _rejected = false;    // 인증 실패: 이후 조각 폐기, 응답 401, 재부팅 안 함
    size_t _totalSize = 0;

    // v2.5.1 — SPIFFS OTA 시 설정 파일 보존 (backup → OTA → remount → restore)
    int    _pendingCmd = U_FLASH;
    String _savedDeviceConfig;
    String _savedSchedule;

    void backupSpiffsConfigs();
    void restoreSpiffsConfigs();
};
