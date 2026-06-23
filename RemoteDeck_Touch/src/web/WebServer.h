#pragma once
// Design Ref: §2.0 Option C — sync WebServer (Arduino 내장) + 협력적 yield
// Design Ref: §12.1 — Phase 1 PoC (sync + W5500 + MQTT 동시 동작 검증)
//
// AsyncWebServer (v2.1, mathieucarbou) 가 W5500 + MQTT 환경에서 task slot 충돌로 실패.
// sync WebServer 는 별도 task 생성 안 함 → handleClient() 를 main loop 에서 호출.

#include <WebServer.h>     // ESP32 Arduino 내장 (대소문자 주의: 소문자 'w' 아님)
#include <functional>

struct TouchAuth {
    const char* user = "admin";
    const char* pass = "12345";
};

class TouchWebServer {
public:
    void begin(uint16_t port, const TouchAuth* auth);
    void handleClient();   // main loop 에서 매 iteration 호출

    using StatusGetter = std::function<String()>;
    using LogCallback  = std::function<void(const char* event, const char* detail)>;

    void setStatusGetter(StatusGetter cb) { _getStatus = cb; }
    void setLogger(LogCallback cb)        { _onLog = cb; }

    // 노출 (Phase 2 에서 ImageApi 등이 직접 등록 시 사용)
    ::WebServer& server() { return _server; }

private:
    ::WebServer _server { 80 };          // sync WebServer, 기본 80
    const TouchAuth* _auth = nullptr;
    StatusGetter _getStatus = nullptr;
    LogCallback _onLog = nullptr;

    bool requireAuth();
    void setupRoutes();
    void logEvent(const char* event, const char* detail);
};
