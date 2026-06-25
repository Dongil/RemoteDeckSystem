#pragma once
// Design Ref: §4.1 #7,8,9,10 — /api/imagesconfig, /api/config, /api/reboot
// Plan SC: FR-03 (3탭 — Config)

#include <Arduino.h>

class WebServer;

class ConfigApi {
public:
    // WebServer setter 등록 (현재 module-webui 단계는 colback 인터페이스 추가 없이
    // WebServer 가 ConfigApi* 직접 보관 — 단순화)
    void attach(WebServer* ws);

    // /api/config GET — /deviceconfig.json raw 반환
    String readDeviceConfigJson() const;

    // /api/config POST — JSON 검증 + 저장. 성공 시 true. (apply 은 reboot 권장)
    bool   writeDeviceConfigJson(const String& body, String& errOut);

    // /api/imagesconfig GET — /imagesconfig.json raw 반환
    String readImagesConfigJson() const;

    // /api/reboot — 메인 루프에서 ESP.restart() 처리 위한 flag
    void   requestReboot()       { _rebootRequested = true; }
    bool   pendingReboot() const { return _rebootRequested; }
    void   clearReboot()         { _rebootRequested = false; }

    // 메인 루프에서 호출 — pending 시 1초 대기 후 재부팅 (응답 반환 보장)
    void   loop();

private:
    bool _rebootRequested = false;
    uint32_t _rebootArmedAt = 0;
};
