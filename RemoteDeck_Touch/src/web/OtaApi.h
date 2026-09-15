#pragma once
// Design Ref: §4.1 #14 — /api/ota
// Plan SC: FR-05 (OTA 핸들러, 1.72MB OTA bin 전용)
//
// 흐름:
//   begin(total)              : Update.begin(total, U_FLASH) — OTA partition 사용 (1.72MB)
//   chunk(data, len, isFinal) : Update.write 청크 단위 + isFinal 시 Update.end(true)
//   loop()                    : end 성공 후 1초 grace 뒤 ESP.restart()

#include <Arduino.h>

class WebServer;

class OtaApi {
public:
    static constexpr size_t OTA_MAX_BYTES = 1900 * 1024;  // ~1.85MB (OTA partition 한계 마진)

    void attach(WebServer* ws);

    bool onUploadStart(size_t total);                              // false → reject (size 초과 등)
    bool onUploadChunk(uint8_t* data, size_t len, bool isFinal);  // false → fail

    // main loop 에서 호출 — end 성공 후 1초 후 reboot
    void loop();

    // 진행률 조회 (status JSON 에 포함)
    size_t expected() const { return _expected; }
    size_t written()  const { return _written;  }
    bool   inProgress() const { return _inProgress; }
    bool   pendingReboot() const { return _rebootRequested; }
    String lastError() const { return _error; }

private:
    size_t _expected = 0;
    size_t _written  = 0;
    bool   _inProgress = false;
    bool   _rebootRequested = false;
    uint32_t _rebootArmedAt = 0;
    String _error;
};
