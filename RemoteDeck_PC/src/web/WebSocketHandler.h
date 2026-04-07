#pragma once
#include <ESPAsyncWebServer.h>

class WebSocketHandler {
public:
    void begin(AsyncWebServer* server, const char* path = "/ws");
    void loop();

    void broadcastStatus(const char* json);
    void broadcastLog(const char* event, const char* detail);
    void broadcastOTAProgress(uint8_t percent);

private:
    AsyncWebSocket* _ws = nullptr;

    void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                 AwsEventType type, void* arg, uint8_t* data, size_t len);
};
