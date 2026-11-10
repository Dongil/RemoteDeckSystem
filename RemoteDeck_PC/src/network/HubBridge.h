#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <mutex>
#include <string>
#include "RcmDeck.h"
#include "config/DeviceConfig.h"

// Design Ref: rcm-hub-v3.0 §5.1 — RCM 허브 deck 서브시스템 래퍼.
// lib/RcmHub를 소유하고, 하드웨어/로그는 Deps로 주입받는다(전역 참조 없음).
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

    // GET /api/hub — 연결 정보·통계 + 최근 송수신 내역(최신이 앞). async_tcp 태스크에서 호출됨.
    String toJson(const HubConfig& cfg);

private:
    void logMsg(const String& msg);
    void onTap(char kind, const char* text, size_t len);   // loop 태스크
    void push(char dir, const char* type, const char* op, const char* id, int8_t ok, const char* detail);
    const char* opForId(const char* id) const;

    RcmDeck _deck;
    Deps _deps;
    bool _started = false;
    bool _incomplete = false;   // enabled인데 host/product_id/device_key 누락
    bool _lastReady = false;
    bool _lastStopped = false;

    // ── 송수신 내역 (링 버퍼, 고정 크기 문자열로 힙 조각화 방지) ──
    static constexpr uint8_t kLogSize = 40;
    struct Entry {
        uint32_t ms;            // millis()
        time_t   epoch;         // NTP 동기 시 실제 시각, 아니면 0
        char     dir;           // 'i' 수신, 'o' 송신, 'c' 연결, 'd' 끊김
        char     type[10];      // challenge/hello/welcome/req/res/evt
        char     op[24];
        char     id[20];
        int8_t   ok;            // res: 1/0, 그 외 -1
        char     detail[96];
    };
    Entry _log[kLogSize];
    uint8_t _head = 0;          // 다음 기록 위치
    uint8_t _count = 0;
    mutable std::mutex _mtx;

    // ── 통계 ──
    uint32_t _rx = 0, _tx = 0, _reqs = 0, _resFail = 0;
    uint32_t _welcomes = 0, _drops = 0;
    uint32_t _lastRxMs = 0, _lastTxMs = 0, _readySinceMs = 0;
    uint32_t _heartbeatMs = 0;
    char _session[40] = "";
    char _room[32] = "";
    char _lastCmd[24] = "";
    int8_t _lastCmdOk = -1;
    uint32_t _lastCmdMs = 0;
};
