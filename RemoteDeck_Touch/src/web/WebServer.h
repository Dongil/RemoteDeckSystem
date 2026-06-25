#pragma once
// Design Ref: §2.1, §4, §5.3 — esp_http_server (ESP-IDF native) 래퍼
//   - core 0 pinning (LVGL/MQTT main loop = core 1)
//   - v2.2-zero 콜백 시그니처 보존 (Option C — Pragmatic)
//   - module-poc: /api/status + /
//   - module-webui: + /api/images/*, /api/imagesconfig, /api/config, /api/log, /api/reboot, static SPIFFS
// Plan SC: FR-01, FR-03, FR-04, FR-08

#include <Arduino.h>
#include <esp_http_server.h>
#include <functional>

struct TouchAuth {
    const char* user = "admin";
    const char* pass = "12345";
};

class WebServer {
public:
    bool begin(uint16_t port, const TouchAuth* auth);
    void stop();

    // v2.2-zero 콜백 시그니처 (보존)
    using StatusGetter         = std::function<String()>;
    using ImagesListGetter     = std::function<String()>;
    using ImagesConfigGetter   = std::function<String()>;
    using ImageDeleteHandler   = std::function<bool(const String& name)>;
    using ImageUploadStarter   = std::function<void(const String& filename, size_t total)>;
    using ImageUploadChunk     = std::function<bool(uint8_t* data, size_t len, bool isFinal)>;
    using LogCallback          = std::function<void(const char* event, const char* detail)>;
    // v2.3 module-webui 추가 시그니처
    using DeviceConfigGetter   = std::function<String()>;
    using DeviceConfigSetter   = std::function<bool(const String& body, String& errOut)>;
    using LogJsonGetter        = std::function<String()>;
    using RebootHandler        = std::function<void()>;
    // v2.3 module-ota 시그니처 — Image upload 와 동일 패턴 (start + chunk)
    using OtaStarter           = std::function<bool(size_t total)>;
    using OtaChunk             = std::function<bool(uint8_t* data, size_t len, bool isFinal)>;

    void setStatusGetter(StatusGetter cb)               { _getStatus       = cb; }
    void setImagesListGetter(ImagesListGetter cb)       { _getImagesList   = cb; }
    void setImagesConfigGetter(ImagesConfigGetter cb)   { _getImagesConfig = cb; }
    void setImageDeleteHandler(ImageDeleteHandler cb)   { _deleteImage     = cb; }
    void setImageUploadStarter(ImageUploadStarter cb)   { _uploadStart     = cb; }
    void setImageUploadChunk(ImageUploadChunk cb)       { _uploadChunk     = cb; }
    void setLogger(LogCallback cb)                      { _onLog           = cb; }
    void setDeviceConfigGetter(DeviceConfigGetter cb)   { _getDeviceConfig = cb; }
    void setDeviceConfigSetter(DeviceConfigSetter cb)   { _setDeviceConfig = cb; }
    void setLogJsonGetter(LogJsonGetter cb)             { _getLogJson      = cb; }
    void setRebootHandler(RebootHandler cb)             { _onReboot        = cb; }
    void setOtaStarter(OtaStarter cb)                   { _otaStart        = cb; }
    void setOtaChunk(OtaChunk cb)                       { _otaChunk        = cb; }

    httpd_handle_t handle() const { return _server; }

private:
    httpd_handle_t   _server = nullptr;
    const TouchAuth* _auth   = nullptr;

    StatusGetter         _getStatus       = nullptr;
    ImagesListGetter     _getImagesList   = nullptr;
    ImagesConfigGetter   _getImagesConfig = nullptr;
    ImageDeleteHandler   _deleteImage     = nullptr;
    ImageUploadStarter   _uploadStart     = nullptr;
    ImageUploadChunk     _uploadChunk     = nullptr;
    LogCallback          _onLog           = nullptr;
    DeviceConfigGetter   _getDeviceConfig = nullptr;
    DeviceConfigSetter   _setDeviceConfig = nullptr;
    LogJsonGetter        _getLogJson      = nullptr;
    RebootHandler        _onReboot        = nullptr;
    OtaStarter           _otaStart        = nullptr;
    OtaChunk             _otaChunk        = nullptr;

    // 공통 유틸
    bool requireAuth(httpd_req_t* req);
    void send401(httpd_req_t* req);
    void sendJson(httpd_req_t* req, int statusCode, const char* json);
    void sendJsonString(httpd_req_t* req, int statusCode, const String& json);
    esp_err_t serveSpiffsFile(httpd_req_t* req, const char* path, const char* mime);
    void logEvent(const char* event, const char* detail);

    // 파일명에서 basename 추출 + 화이트리스트 (.png/.bmp)
    bool sanitizeImageName(const String& in, String& out) const;

    // URI handler 등록
    void registerHandlers();

    // C trampolines (handler_t requires C func pointer)
    static esp_err_t trampolineRoot(httpd_req_t* req);
    static esp_err_t trampolineStyle(httpd_req_t* req);
    static esp_err_t trampolineAppJs(httpd_req_t* req);
    static esp_err_t trampolineStatus(httpd_req_t* req);
    static esp_err_t trampolineImagesList(httpd_req_t* req);
    static esp_err_t trampolineImagesUpload(httpd_req_t* req);
    static esp_err_t trampolineImagesGet(httpd_req_t* req);   // /api/images/<name>
    static esp_err_t trampolineImagesDel(httpd_req_t* req);
    static esp_err_t trampolineImagesConfig(httpd_req_t* req);
    static esp_err_t trampolineConfigGet(httpd_req_t* req);
    static esp_err_t trampolineConfigPost(httpd_req_t* req);
    static esp_err_t trampolineLog(httpd_req_t* req);
    static esp_err_t trampolineReboot(httpd_req_t* req);
    static esp_err_t trampolineOtaUpload(httpd_req_t* req);

    // 실제 핸들러
    esp_err_t handleRoot(httpd_req_t* req);
    esp_err_t handleStyle(httpd_req_t* req);
    esp_err_t handleAppJs(httpd_req_t* req);
    esp_err_t handleStatus(httpd_req_t* req);
    esp_err_t handleImagesList(httpd_req_t* req);
    esp_err_t handleImagesUpload(httpd_req_t* req);
    esp_err_t handleImagesGet(httpd_req_t* req);
    esp_err_t handleImagesDel(httpd_req_t* req);
    esp_err_t handleImagesConfig(httpd_req_t* req);
    esp_err_t handleConfigGet(httpd_req_t* req);
    esp_err_t handleConfigPost(httpd_req_t* req);
    esp_err_t handleLog(httpd_req_t* req);
    esp_err_t handleReboot(httpd_req_t* req);
    esp_err_t handleOtaUpload(httpd_req_t* req);

    // Multipart upload helper (state machine within single req scope)
    esp_err_t streamMultipartToCallback(httpd_req_t* req, const String& boundary);
};
