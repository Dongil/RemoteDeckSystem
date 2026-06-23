#pragma once
// Design Ref: §4 API + §11.2 C4 — sync WebServer 기반 이미지 관리
// Plan SC: FR-02 (이미지 업로드 + 즉시 갱신)
//
// AuthMiddleware 는 별도 파일 안 만들고 TouchWebServer 내부 helper 사용.

#include <Arduino.h>
#include <SPIFFS.h>

class TouchWebServer;
class Logger;

class ImageApi {
public:
    void attach(TouchWebServer* ws, Logger* logger);
    void loop();  // main loop — pending reload 처리

    String buildListJson() const;
    String readImagesConfigJson() const;

private:
    TouchWebServer* _ws = nullptr;
    Logger* _logger = nullptr;

    static constexpr size_t IMAGE_MAX_BYTES = 200 * 1024;

    File _uploadFile;
    String _uploadName;
    size_t _uploadWritten = 0;
    bool _uploadOk = false;
    bool _uploadOpen = false;
    bool _pendingReload = false;

    void handleList();
    void handleConfigJson();
    void handleGetImage();
    void handleDeleteImage();
    void handleUploadResponse();    // upload 완료 후 응답
    void handleUploadChunk();       // upload 청크 처리

    bool sanitizeFilename(const String& in, String& out) const;
    void log(const char* event, const char* detail);
};
