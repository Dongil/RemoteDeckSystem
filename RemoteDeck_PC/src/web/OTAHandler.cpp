#include "OTAHandler.h"
#include <SPIFFS.h>

// v2.5.1 — SPIFFS OTA 시 설정 보존 대상 파일 목록.
// 다른 필드에서 유지해야 할 파일이 생기면 여기 목록만 늘리면 됨.
namespace {
    constexpr const char* kDeviceConfigPath = "/deviceconfig.json";
    constexpr const char* kSchedulePath     = "/schedule.json";
}

void OTAHandler::backupSpiffsConfigs() {
    _savedDeviceConfig = "";
    _savedSchedule = "";

    File f = SPIFFS.open(kDeviceConfigPath, "r");
    if (f) {
        _savedDeviceConfig = f.readString();
        f.close();
        Serial.printf("OTA: backed up %s (%u bytes)\n",
                      kDeviceConfigPath, (unsigned)_savedDeviceConfig.length());
    }
    f = SPIFFS.open(kSchedulePath, "r");
    if (f) {
        _savedSchedule = f.readString();
        f.close();
        Serial.printf("OTA: backed up %s (%u bytes)\n",
                      kSchedulePath, (unsigned)_savedSchedule.length());
    }
}

void OTAHandler::restoreSpiffsConfigs() {
    // Update.end(true)로 새 SPIFFS 파티션 커밋 완료. 이전 드라이버 캐시를 버리고 재마운트.
    SPIFFS.end();
    if (!SPIFFS.begin(true)) {
        Serial.println("OTA: SPIFFS remount FAILED after U_SPIFFS update");
        return;
    }
    if (_savedDeviceConfig.length() > 0) {
        File f = SPIFFS.open(kDeviceConfigPath, "w");
        if (f) {
            f.print(_savedDeviceConfig);
            f.close();
            Serial.printf("OTA: restored %s (%u bytes)\n",
                          kDeviceConfigPath, (unsigned)_savedDeviceConfig.length());
        }
        _savedDeviceConfig = "";
    }
    if (_savedSchedule.length() > 0) {
        File f = SPIFFS.open(kSchedulePath, "w");
        if (f) {
            f.print(_savedSchedule);
            f.close();
            Serial.printf("OTA: restored %s (%u bytes)\n",
                          kSchedulePath, (unsigned)_savedSchedule.length());
        }
        _savedSchedule = "";
    }
}

void OTAHandler::setup(AsyncWebServer* server, const char* path) {
    server->on(path, HTTP_POST,
        // Response handler (called after upload completes)
        [this](AsyncWebServerRequest* request) {
            bool success = !Update.hasError();
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json",
                success ? "{\"ok\":true,\"progress\":100}" : "{\"ok\":false,\"error\":\"Update failed\"}");
            response->addHeader("Connection", "close");
            request->send(response);

            if (success) {
                // v2.5.1 — SPIFFS 갱신이었으면 재마운트 후 백업해 둔 설정 파일 복원
                if (_pendingCmd == U_SPIFFS) {
                    restoreSpiffsConfigs();
                }
                Serial.println("OTA: Update successful, rebooting...");
                delay(1000);
                ESP.restart();
            }
            _pendingCmd = U_FLASH;
        },
        // Upload handler (called for each chunk)
        [this](AsyncWebServerRequest* request, const String& filename,
               size_t index, uint8_t* data, size_t len, bool final) {

            if (index == 0) {
                Serial.printf("OTA: Starting update with %s\n", filename.c_str());
                _totalSize = request->contentLength();

                if (_onFilename) _onFilename(filename);

                // v2.5.1 — 파일명 substring 으로 대상 파티션 판정 (규칙 완화).
                //   'spiffs' 또는 'fs.' 가 어디에든 있으면 SPIFFS
                //   예) foo.spiffs.bin, spiffs_20260703.bin, foo_fs.bin, foo-fs.bin
                //   그 외 (.bin) → app 파티션 (firmware)
                int cmd = U_FLASH;
                String lower = filename;
                lower.toLowerCase();
                if (lower.indexOf("spiffs") >= 0 ||
                    lower.indexOf("_fs.")  >= 0 ||
                    lower.indexOf("-fs.")  >= 0 ||
                    lower.indexOf(".fs.")  >= 0) {
                    cmd = U_SPIFFS;
                    Serial.printf("OTA: target = SPIFFS partition (web assets) [%s]\n", filename.c_str());
                } else {
                    Serial.printf("OTA: target = APP partition (firmware) [%s]\n", filename.c_str());
                }
                _pendingCmd = cmd;

                // v2.5.1 — SPIFFS 갱신 전에 유지 대상 설정 파일 백업 (RAM)
                if (cmd == U_SPIFFS) {
                    backupSpiffsConfigs();
                }

                if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
                    Update.printError(Serial);
                    return;
                }
            }

            if (Update.write(data, len) != len) {
                Update.printError(Serial);
                return;
            }

            // Calculate and report progress
            if (_totalSize > 0 && _onProgress) {
                uint8_t progress = (uint8_t)((index + len) * 100 / _totalSize);
                _onProgress(progress);
            }

            if (final) {
                if (Update.end(true)) {
                    Serial.printf("OTA: Update complete (%u bytes)\n", index + len);
                    if (_onProgress) _onProgress(100);
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );
}
