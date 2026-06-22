#pragma once
// Design Ref: §5.3 Component List — Web Layer (RemoteDeck_PC 패턴 슬림 이식)
// Plan SC: FR-01, FR-09, FR-12 (브라우저 접근, Basic Auth, 듀얼 네트워크)

#include <ESPAsyncWebServer.h>
#include <functional>

// Touch 전용 간이 인증 (PC의 AuthConfig 와 동일 인터페이스)
struct TouchAuth {
    const char* user = "admin";
    const char* pass = "12345";
};

class WebServer {
public:
    void begin(uint16_t port, const TouchAuth* auth);

    using StatusGetter   = std::function<String()>;
    using ImagesListGetter = std::function<String()>;
    using ImagesConfigGetter = std::function<String()>;
    using ImageDeleteHandler = std::function<bool(const String& name)>;
    using ImageUploadStarter  = std::function<void(const String& filename, size_t total)>;
    using ImageUploadChunk    = std::function<bool(uint8_t* data, size_t len, bool isFinal)>;
    using LogCallback = std::function<void(const char* event, const char* detail)>;

    void setStatusGetter(StatusGetter cb)             { _getStatus       = cb; }
    void setImagesListGetter(ImagesListGetter cb)     { _getImagesList   = cb; }
    void setImagesConfigGetter(ImagesConfigGetter cb) { _getImagesConfig = cb; }
    void setImageDeleteHandler(ImageDeleteHandler cb) { _deleteImage     = cb; }
    void setImageUploadStarter(ImageUploadStarter cb) { _uploadStart     = cb; }
    void setImageUploadChunk(ImageUploadChunk cb)     { _uploadChunk     = cb; }
    void setLogger(LogCallback cb)                    { _onLog           = cb; }

    AsyncWebServer* server() { return _server; }

private:
    AsyncWebServer* _server = nullptr;
    const TouchAuth* _auth  = nullptr;

    StatusGetter         _getStatus       = nullptr;
    ImagesListGetter     _getImagesList   = nullptr;
    ImagesConfigGetter   _getImagesConfig = nullptr;
    ImageDeleteHandler   _deleteImage     = nullptr;
    ImageUploadStarter   _uploadStart     = nullptr;
    ImageUploadChunk     _uploadChunk     = nullptr;
    LogCallback          _onLog           = nullptr;

    bool requireAuth(AsyncWebServerRequest* req);
    void setupStaticFiles();
    void setupAPI();
    void logEvent(const char* event, const char* detail);
};
