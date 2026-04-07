#include "OTAHandler.h"

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
                Serial.println("OTA: Update successful, rebooting...");
                delay(1000);
                ESP.restart();
            }
        },
        // Upload handler (called for each chunk)
        [this](AsyncWebServerRequest* request, const String& filename,
               size_t index, uint8_t* data, size_t len, bool final) {

            if (index == 0) {
                Serial.printf("OTA: Starting update with %s\n", filename.c_str());
                _totalSize = request->contentLength();

                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
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
