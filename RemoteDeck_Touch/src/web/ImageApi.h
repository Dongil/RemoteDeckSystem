#pragma once
// Design Ref: §2.2 Data Flow — Upload + Hot Reload, §12.2 동시성
// Plan SC: FR-02 (SPIFFS 저장), FR-03 (재부팅 없는 갱신), FR-11 (실패 시 fail-soft)

#include <Arduino.h>
#include <SPIFFS.h>

class WebServer;

class ImageApi {
public:
    // WebServer 와 결합 - 콜백 등록
    void attach(WebServer* ws);

    // main loop 에서 호출 - 업로드 완료 flag 감지 시 images_update() 트리거
    void loop();

    // status JSON 생성기 (WebServer.setStatusGetter 에 연결)
    String buildStatusJson() const;

    // 이미지 목록 JSON 생성기 (WebServer.setImagesListGetter 에 연결)
    String buildImagesListJson() const;

    // imagesconfig.json 반환 (WebServer.setImagesConfigGetter 에 연결)
    String readImagesConfigJson() const;

    // 활성 네트워크 인터페이스 표시 ("ethernet" | "wifi" | "none")
    void setNetworkInfo(const char* iface, const String& ip) {
        _iface = iface ? iface : "none";
        _ip = ip;
    }

    // 펌웨어 버전 표시
    void setFirmwareInfo(const char* version, const char* date) {
        _fwVersion = version ? version : "1.0.0-touch";
        _fwDate = date ? date : "2026-06-22";
    }

    // v2.7: 상태 탭 PC 스타일 상세정보용 — 장치 ID(정적) + 런타임(MQTT/시각, loop 갱신)
    void setDeviceId(const String& id) { _deviceId = id; }
    void setRuntimeInfo(bool mqttConnected, const String& timeStr) {
        _mqttConnected = mqttConnected;
        _timeStr = timeStr;
    }

private:
    static constexpr size_t IMAGE_MAX_BYTES = 200 * 1024;

    File _uploadFile;
    String _uploadName;       // 검증된 basename
    size_t _uploadExpected = 0;
    size_t _uploadWritten = 0;
    bool _uploadOpen = false;
    bool _pendingReload = false;
    bool _uploadOk = true;

    const char* _iface = "none";
    String _ip;
    const char* _fwVersion = "1.0.0-touch";
    const char* _fwDate = "2026-06-22";
    String _deviceId;                 // v2.7
    bool   _mqttConnected = false;    // v2.7
    String _timeStr;                  // v2.7 (HH:MM:SS)

    // 콜백 핸들러
    void onUploadStart(const String& filename, size_t total);
    bool onUploadChunk(uint8_t* data, size_t len, bool isFinal);
    bool onImageDelete(const String& name);

    // 파일명에서 basename 추출 + 화이트리스트 확장자 검증
    bool sanitizeFilename(const String& in, String& out) const;
};
