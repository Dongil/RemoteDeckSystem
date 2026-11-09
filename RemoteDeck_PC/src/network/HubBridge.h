#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <string>
#include "RcmDeck.h"
#include "config/DeviceConfig.h"

// Design Ref: rcm-hub-v3.0 §5.1 — RCM 허브 deck 서브시스템 래퍼.
// lib/RcmHub(원본 그대로)를 소유하고, 하드웨어/로그는 Deps로 주입받는다(전역 참조 없음).
// 설정 변경은 재부팅 후 반영: begin()은 부팅 후 최초 네트워크 연결 시 1회만 호출된다.
class HubBridge {
public:
    struct Deps {
        std::function<bool()> pcOn;                        // PCMonitor 디바운스 값
        std::function<bool(uint8_t)> getRelay;             // 1 기준
        std::function<void(uint8_t, bool)> setRelay;
        std::function<void(uint8_t, uint16_t)> pulse;      // 비블로킹
        std::function<void(JsonArray)> inputs;             // GPIO1~3
        std::function<String()> ip;
        std::function<String()> net;                       // "eth" | "wifi"
        std::function<void(const char*)> log;              // logger.log("HUB", msg)
    };

    // 부팅 시 HMAC known-answer test 결과 (handoff/TEST-VECTORS.md)
    static bool selfTest() { return RcmHub::selfTest(); }

    void begin(const HubConfig& cfg, const std::string& fw, const Deps& deps);
    void loop();
    bool started() const { return _started; }

    // Design Ref: §4.1 — /api/status hub_* 필드
    void fillStatus(JsonDocument& doc, const HubConfig& cfg);

private:
    void logMsg(const String& msg);

    RcmDeck _deck;
    Deps _deps;
    bool _started = false;
    bool _incomplete = false;   // enabled인데 host/product_id/device_key 누락
    bool _lastReady = false;
    bool _lastStopped = false;
};
