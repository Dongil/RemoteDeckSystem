#include "ControlApi.h"
#include "WebServer.h"
#include "Logger.h"
#include "../config/DeviceConfig.h"
#include "../config/ServerConfig.h"
#include "../mqtt/ethernet_mqtt.h"
#include "../mqtt/MQTTHandler.h"
#include <ArduinoJson.h>

// main.cpp 의 전역 변수 + 헬퍼 사용
extern bool room;
extern bool ethernet_conn;
extern bool wifi_conn;
extern bool httpRequestUsing;
extern MQTTHandler mqttHandler;
extern DeviceConfig deviceConfig;
extern ServerConfig serverConfig;
extern void sendHttpMessage(const char* msg);
extern void message_process(String msg);

void ControlApi::attach(TouchWebServer* ws, Logger* logger) {
    _ws = ws;
    _logger = logger;
    if (!_ws) return;
    ::WebServer& s = _ws->server();
    s.on("/api/state", HTTP_GET,  [this]() { handleGetState(); });
    s.on("/api/state", HTTP_POST, [this]() { handlePostState(); });
}

void ControlApi::handleGetState() {
    if (!_ws) return;
    // imagesconfig 의 layout 정보 + 현재 IN/OUT 상태 반환
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"status\":\"%s\",\"room\":%s,\"device_id\":\"%s\"}",
        room ? "IN" : "OUT",
        room ? "true" : "false",
        deviceConfig.deviceID.c_str());
    _ws->server().send(200, "application/json", buf);
}

void ControlApi::handlePostState() {
    if (!_ws) return;

    // body 파싱: {"action":"toggle"} 또는 {"status":"IN"|"OUT"}
    String body = _ws->server().hasArg("plain") ? _ws->server().arg("plain") : String("");
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, body)) {
        _ws->server().send(400, "application/json",
            "{\"ok\":false,\"error\":\"bad json\"}");
        return;
    }

    // 목표 상태 결정
    bool targetIn;
    if (doc.containsKey("status")) {
        const char* s = doc["status"];
        if (!s) { _ws->server().send(400); return; }
        if (strcmp(s, "IN") == 0)       targetIn = true;
        else if (strcmp(s, "OUT") == 0) targetIn = false;
        else { _ws->server().send(400); return; }
    } else {
        // 기본 toggle
        targetIn = !room;
    }

    // 단말 IN/OUT publish 로직 (main.cpp ibtnRoom_Click 미러링)
    const char* msg = targetIn ? "IN" : "OUT";
    bool sent = false;

    if (httpRequestUsing) {
        // HTTP REST 모드 — sendHttpMessage 가 응답 받아서 message_process 까지 호출
        if (ethernet_conn) {
            sendHttpMessage(msg);
            sent = true;
        }
    } else {
        // MQTT 모드
        if (ethernet_conn) {
            mqttEthernet_publish(msg);
            sent = true;
        }
        if (wifi_conn) {
            mqttHandler.xenoMqttPublish(msg);
            sent = true;
        }
    }

    if (_logger) _logger->add("CTRL", msg);

    char resp[160];
    snprintf(resp, sizeof(resp),
        "{\"ok\":%s,\"sent\":\"%s\",\"current\":\"%s\"}",
        sent ? "true" : "false", msg, room ? "IN" : "OUT");
    _ws->server().send(sent ? 200 : 503, "application/json", resp);
}
