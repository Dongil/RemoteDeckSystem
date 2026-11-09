// RCM 통합 API device connection for ESP32 (Arduino-ESP32 3.x, links2004/WebSockets 2.6, ArduinoJson 6).
// Protocol: rcm-api/docs/OPS.md. Reference behaviour: rcm-api/lib/client.js + rcm-api/sim/deck.js.
//
//   1. connect ws(s)://host:port/v1/ws    2. challenge {nonce}
//   3. hello {role, auth:{productid, sig = hex HMAC-SHA256(key, nonce|role|productid)}}
//   4. welcome -> send <role>.state {full:true, rev:0} at once
//   5. answer every req with a res (same id); patches go out as <role>.state {full:false, rev+1}
//
// Never block loop(): handlers that need time (pressing the power button and waiting for the LED) keep
// the request id and answer later with ok()/fail() from their own loop. While loop() is blocked the
// library cannot answer the hub's pings and the hub drops the connection after one missed heartbeat.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <functional>
#include <map>

struct RcmHubConfig {
  String host;              // RCM server (IP recommended: W5500 setups often have no DNS)
  uint16_t port = 8891;
  String path = "/v1/ws";
  bool tls = false;         // wss:// (needs ~40 KB heap more)
  String role = "deck";
  String productId;         // same value as the paired recorder (tbl_lecture.l_productid)
  String key;               // device key from tools/device-key.js --role deck
  String clientName = "SmartDeck";
  String clientVersion;     // firmware version
};

// Valid only during the handler call. Copy what you need if you answer later.
struct RcmReq {
  String id;
  String op;
  JsonObjectConst data;
  uint32_t ttlMs;
};

class RcmHub {
 public:
  using ReqHandler = std::function<void(RcmReq& req)>;
  using StateFn = std::function<void(JsonObject state)>;  // fill the full state

  void begin(const RcmHubConfig& cfg, StateFn fullState);
  void loop();
  void stop();  // close and stay closed

  bool ready() const { return _ready; }
  // true after the hub closed with 4001 (bad key/productid), 4009 (another device with this id) or 4010
  bool stopped() const { return _stopped; }
  const String& stopReason() const { return _stopReason; }

  void on(const char* op, ReqHandler h) { _handlers[op] = h; }

  // Answers. result may be nullptr for {}.
  void ok(const String& id, JsonObjectConst result);
  void ok(const String& id);
  void fail(const String& id, const char* code, const String& msg);

  // <role>.state patch (only changed keys). Dropped while offline: the next welcome sends the full state.
  void report(JsonObjectConst patch);
  void event(const char* op, JsonObjectConst data);

  // Known-answer test for the HMAC code (handoff/TEST-VECTORS.md). Call once at boot, log the result.
  static bool selfTest();
  static String hmacHex(const String& key, const String& msg);

 private:
  void onWs(WStype_t type, uint8_t* payload, size_t len);
  void onText(const char* text, size_t len);
  void sendDoc(JsonDocument& doc);
  void sendFull();

  WebSocketsClient _ws;
  RcmHubConfig _cfg;
  StateFn _fullState;
  std::map<String, ReqHandler> _handlers;
  bool _ready = false;
  bool _stopped = false;
  String _stopReason;
  uint32_t _rev = 0;
  uint32_t _backoffMs = 500;
};
