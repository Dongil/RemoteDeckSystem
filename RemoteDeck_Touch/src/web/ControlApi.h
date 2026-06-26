#pragma once
// Design Ref: §4.1 #12,13 — /api/control GET (Long polling 10s + ETag) / POST (토글)
// Plan SC: FR-06 (Long polling, v2.2 부팅 hang 회피), FR-07 (MQTT 양방향 미러)
//
// 동작:
//   GET /api/control?since=N
//     - state.etag != N : 즉시 200 응답
//     - state.etag == N : EventGroup wait 10초
//                         bit set → 즉시 응답, timeout → 304 (same state)
//   POST /api/control {in|out}
//     - state 갱신 + etag++ + EventGroup setBits + MQTT publish 콜백 호출
//
// MQTT 외부 메시지 수신:
//   main.cpp message_process() → controlApi.notifyState(in, out) → etag++ + setBits

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

class WebServer;

class ControlApi {
public:
    static constexpr uint32_t STATE_CHANGED_BIT = (1 << 0);
    // esp_http_server 단일 task — 긴 polling block 시 다른 요청 block.
    // 1초 polling 도 max_open_sockets=4 한계에서 socket 압박 누적 → hang.
    // 즉시 응답 (0ms) + 클라이언트 측 throttle (3s setTimeout) 으로 본질 해결.
    static constexpr uint32_t LONG_POLL_MS      = 0;

    using MqttPublishCb = std::function<void(const char* status)>;  // "IN"/"OUT" publish 콜백

    void begin();                     // EventGroup 생성
    void attach(WebServer* ws);

    // MQTT publish 콜백 등록 — POST /api/control 시 호출
    void setMqttPublisher(MqttPublishCb cb) { _publish = cb; }

    // 외부 (MQTT 수신, LCD 토글) 에서 state 변경 통보
    void notifyState(bool in, bool out);

    // /api/control GET — Long polling
    //   since: 클라이언트가 마지막으로 본 etag
    //   out_json: 응답 JSON
    //   returns true if state was new (200), false if timeout w/ same etag (304-equivalent)
    bool waitForChange(uint32_t since, String& outJson);

    // /api/control POST — 토글 적용
    //   body JSON: {"in": true} 또는 {"out": true} (양쪽 동시 가능)
    //   returns 응답 JSON
    bool applyToggle(const String& body, String& outJson, String& errOut);

    // 현재 state JSON (인스턴스 메서드, snapshot)
    String currentJson() const;

    // accessors
    uint32_t etag() const { return _etag; }
    bool isIn()    const { return _in;   }
    bool isOut()   const { return _out;  }

private:
    EventGroupHandle_t _eg = nullptr;
    volatile uint32_t  _etag = 1;     // 0 은 reserved (since=0 일 때 즉시 응답 유도)
    volatile bool      _in  = false;
    volatile bool      _out = false;
    MqttPublishCb _publish = nullptr;

    String buildJson(uint32_t etag, bool in, bool out) const;
};
