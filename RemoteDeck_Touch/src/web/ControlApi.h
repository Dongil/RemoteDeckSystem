#pragma once
// Design Ref: Control 탭 추가 (사용자 요청 2026-06-23)
// LCD 미러링 + IN/OUT 토글 (단말 ibtnRoom_Click 과 동일 동작)

#include <Arduino.h>

class TouchWebServer;
class Logger;

class ControlApi {
public:
    void attach(TouchWebServer* ws, Logger* logger);

private:
    TouchWebServer* _ws = nullptr;
    Logger* _logger = nullptr;

    void handleGetState();
    void handlePostState();   // body: {"action":"toggle"} or {"status":"IN"|"OUT"}
};
