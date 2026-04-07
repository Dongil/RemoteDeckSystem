#include "WebSocketHandler.h"
#include <ArduinoJson.h>

void WebSocketHandler::begin(AsyncWebServer* server, const char* path) {
    _ws = new AsyncWebSocket(path);
    _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
        this->onEvent(server, client, type, arg, data, len);
    });
    server->addHandler(_ws);
    Serial.printf("WebSocket: Listening on %s\n", path);
}

void WebSocketHandler::loop() {
    if (_ws) _ws->cleanupClients();
}

void WebSocketHandler::broadcastStatus(const char* json) {
    if (!_ws || _ws->count() == 0) return;

    StaticJsonDocument<512> doc;
    doc["type"] = "status";
    doc["data"] = serialized(json);

    String output;
    serializeJson(doc, output);
    _ws->textAll(output);
}

void WebSocketHandler::broadcastLog(const char* event, const char* detail) {
    if (!_ws || _ws->count() == 0) return;

    StaticJsonDocument<256> doc;
    doc["type"] = "log";
    JsonObject data = doc.createNestedObject("data");
    data["event"] = event;
    data["detail"] = detail;

    String output;
    serializeJson(doc, output);
    _ws->textAll(output);
}

void WebSocketHandler::broadcastOTAProgress(uint8_t percent) {
    if (!_ws || _ws->count() == 0) return;

    StaticJsonDocument<128> doc;
    doc["type"] = "ota";
    JsonObject data = doc.createNestedObject("data");
    data["progress"] = percent;
    data["status"] = percent < 100 ? "uploading" : "complete";

    String output;
    serializeJson(doc, output);
    _ws->textAll(output);
}

void WebSocketHandler::onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                               AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket: Client #%u connected from %s\n",
                          client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket: Client #%u disconnected\n", client->id());
            break;
        default:
            break;
    }
}
