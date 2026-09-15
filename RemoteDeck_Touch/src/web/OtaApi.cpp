// Design Ref: §4.1 #14 — /api/ota 구현
// Plan SC: FR-05

#include "OtaApi.h"
#include "WebServer.h"
#include <Update.h>

void OtaApi::attach(WebServer* ws) {
    if (!ws) return;
    ws->setOtaStarter([this](size_t total) { return onUploadStart(total); });
    ws->setOtaChunk([this](uint8_t* d, size_t l, bool f) { return onUploadChunk(d, l, f); });
}

bool OtaApi::onUploadStart(size_t total) {
    if (total == 0 || total > OTA_MAX_BYTES) {
        _error = String("size_reject:") + total;
        Serial.printf("OTA reject size: %u > %u\n", (unsigned)total, (unsigned)OTA_MAX_BYTES);
        return false;
    }
    if (!Update.begin(total, U_FLASH)) {
        _error = String("begin_fail:") + Update.errorString();
        Serial.printf("Update.begin failed: %s\n", Update.errorString());
        return false;
    }
    _expected = total;
    _written = 0;
    _inProgress = true;
    _error = "";
    Serial.printf("OTA start: %u bytes\n", (unsigned)total);
    return true;
}

bool OtaApi::onUploadChunk(uint8_t* data, size_t len, bool isFinal) {
    if (!_inProgress) {
        _error = "no_session";
        return false;
    }
    if (len > 0) {
        size_t w = Update.write(data, len);
        if (w != len) {
            _error = String("write_short:") + Update.errorString();
            Serial.printf("Update.write short %u/%u: %s\n",
                          (unsigned)w, (unsigned)len, Update.errorString());
            Update.abort();
            _inProgress = false;
            return false;
        }
        _written += len;
    }
    if (isFinal) {
        if (!Update.end(true)) {
            _error = String("end_fail:") + Update.errorString();
            Serial.printf("Update.end failed: %s\n", Update.errorString());
            _inProgress = false;
            return false;
        }
        Serial.printf("OTA complete: %u bytes — reboot armed\n", (unsigned)_written);
        _inProgress = false;
        _rebootRequested = true;  // main loop 에서 reboot 처리
    }
    return true;
}

void OtaApi::loop() {
    if (_rebootRequested && _rebootArmedAt == 0) {
        _rebootArmedAt = millis();
        Serial.println("OtaApi: reboot armed (1s grace)");
    }
    if (_rebootArmedAt > 0 && (millis() - _rebootArmedAt) > 1000) {
        Serial.println("OtaApi: rebooting to new firmware");
        delay(100);
        ESP.restart();
    }
}
