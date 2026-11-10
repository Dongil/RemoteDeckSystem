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

    // v3.0 — 성공 응답을 보낸 뒤 메인 loop에서 재부팅(및 SPIFFS 설정 복원).
    // 응답 콜백 안에서 restart하면 응답이 flush되지 않아 클라이언트가 timeout까지 대기했다.
    void loop();

private:
    ProgressCallback _onProgress = nullptr;
    FilenameCallback _onFilename = nullptr;
    AuthCheck _authCheck = nullptr;
    bool _rejected = false;    // 인증 실패: 이후 조각 폐기, 응답 401, 재부팅 안 함
    size_t _totalSize = 0;
    volatile bool _rebootPending = false;   // 응답 콜백(async_tcp 태스크)에서 설정, loop()에서 처리
    volatile bool _restorePending = false;
    uint32_t _rebootAt = 0;

    // v2.5.1 — SPIFFS OTA 시 설정 파일 보존 (backup → OTA → remount → restore)
    int    _pendingCmd = U_FLASH;
    String _savedDeviceConfig;
    String _savedSchedule;

    void backupSpiffsConfigs();
    void restoreSpiffsConfigs();
};
