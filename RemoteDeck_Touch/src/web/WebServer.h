#pragma once
// Design Ref: §2.1, §5.3 — esp_http_server (ESP-IDF native) 래퍼
//   - core 0 pinning (LVGL/MQTT main loop = core 1 자연 격리)
//   - v2.2-zero 콜백 시그니처 보존 (Option C — Pragmatic)
//   - Basic Auth middleware (admin:12345) 단일 진입점
// Plan SC: FR-01 (별도 task), FR-08 (Basic Auth), FR-09 (LCD regression 無)

#include <Arduino.h>
#include <esp_http_server.h>
#include <functional>

// v2.1 동일 — admin:12345 (LAN 내부 사용 전제, Design §7)
struct TouchAuth {
    const char* user = "admin";
    const char* pass = "12345";
};

class WebServer {
public:
    // httpd 시작 — 별도 task 생성 (core 0, priority 5)
    // Design Ref: §2.1 component diagram
    bool begin(uint16_t port, const TouchAuth* auth);
    void stop();

    // v2.2-zero 콜백 시그니처 (인터페이스 보존 — Option C)
    // 각 *Api 모듈이 attach() 안에서 등록한다.
    using StatusGetter         = std::function<String()>;
    using ImagesListGetter     = std::function<String()>;
    using ImagesConfigGetter   = std::function<String()>;
    using ImageDeleteHandler   = std::function<bool(const String& name)>;
    using ImageUploadStarter   = std::function<void(const String& filename, size_t total)>;
    using ImageUploadChunk     = std::function<bool(uint8_t* data, size_t len, bool isFinal)>;
    using LogCallback          = std::function<void(const char* event, const char* detail)>;

    void setStatusGetter(StatusGetter cb)             { _getStatus       = cb; }
    void setImagesListGetter(ImagesListGetter cb)     { _getImagesList   = cb; }
    void setImagesConfigGetter(ImagesConfigGetter cb) { _getImagesConfig = cb; }
    void setImageDeleteHandler(ImageDeleteHandler cb) { _deleteImage     = cb; }
    void setImageUploadStarter(ImageUploadStarter cb) { _uploadStart     = cb; }
    void setImageUploadChunk(ImageUploadChunk cb)     { _uploadChunk     = cb; }
    void setLogger(LogCallback cb)                    { _onLog           = cb; }

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

    // Basic Auth middleware — 모든 /api/* 진입 1곳
    // Design Ref: §10.4 — Single Source of Truth
    bool requireAuth(httpd_req_t* req);
    void send401(httpd_req_t* req);
    void sendJson(httpd_req_t* req, int statusCode, const char* json);

    // URI handler 등록 (module-poc: /api/status 만)
    // 향후 module-webui 단계에서 /api/images/*, /api/config, /api/log 등 추가
    void registerHandlers();

    // C trampoline: esp_http_server URI handler 는 C function pointer.
    // user_ctx 를 통해 WebServer* 로 디스패치.
    static esp_err_t trampolineStatus(httpd_req_t* req);
    static esp_err_t trampolineRoot(httpd_req_t* req);

    // 실제 핸들러
    esp_err_t handleStatus(httpd_req_t* req);
    esp_err_t handleRoot(httpd_req_t* req);

    void logEvent(const char* event, const char* detail);
};
