#include "RcmHub.h"
#include <mbedtls/md.h>

// Close codes after which reconnecting would not help (OPS.md "연결 종료 코드").
// links2004/WebSockets does not pass the close code to the app, so the hub also sends
// evt hub.closing {code} right before closing; that is what we look at.
static bool isFinalClose(int code) { return code == 4001 || code == 4009 || code == 4010; }

String RcmHub::hmacHex(const String& key, const String& msg) {
  uint8_t out[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(info, (const uint8_t*)key.c_str(), key.length(), (const uint8_t*)msg.c_str(), msg.length(), out);
  static const char* hex = "0123456789abcdef";
  String s;
  s.reserve(64);
  for (int i = 0; i < 32; i++) {
    s += hex[out[i] >> 4];
    s += hex[out[i] & 0x0f];
  }
  return s;
}

bool RcmHub::selfTest() {
  // handoff/TEST-VECTORS.md
  String k = hmacHex("test-vector-master", "deck|XHR131258");
  String sig = hmacHex(k, "00112233445566778899aabbccddeeff|deck|XHR131258");
  return k == "d1c01658146aa29adabf4c6daf2fbb26dea3fde28358711c96e92376d1753e7f" &&
         sig == "9d9658a5d62d0c9abe955c0cfd366c90dd59fb5e3fd6b918507608109cef2152";
}

void RcmHub::begin(const RcmHubConfig& cfg, StateFn fullState) {
  _cfg = cfg;
  _fullState = fullState;
  _stopped = false;
  _backoffMs = 500;
  if (cfg.tls) _ws.beginSSL(cfg.host.c_str(), cfg.port, cfg.path.c_str());
  else _ws.begin(cfg.host, cfg.port, cfg.path);
  _ws.onEvent([this](WStype_t t, uint8_t* p, size_t l) { onWs(t, p, l); });
  _ws.setReconnectInterval(_backoffMs);
  // our own ping as well: notices a dead hub / half-open TCP within ~2 x 15 s
  _ws.enableHeartbeat(15000, 5000, 2);
}

void RcmHub::loop() {
  if (!_stopped) _ws.loop();
}

void RcmHub::stop() {
  _stopped = true;
  _ready = false;
  _ws.disconnect();
}

void RcmHub::onWs(WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[rcm] connected %s:%u%s\n", _cfg.host.c_str(), _cfg.port, _cfg.path.c_str());
      break;
    case WStype_DISCONNECTED:
      if (_ready) Serial.println("[rcm] disconnected");
      _ready = false;
      // exponential backoff 0.5 s .. 10 s
      _backoffMs = _backoffMs >= 5000 ? 10000 : _backoffMs * 2;
      _ws.setReconnectInterval(_backoffMs);
      break;
    case WStype_TEXT:
      onText((const char*)payload, len);
      break;
    default:
      break;
  }
}

void RcmHub::onText(const char* text, size_t len) {
  DynamicJsonDocument in(2048);
  if (deserializeJson(in, text, len)) {
    Serial.println("[rcm] not JSON from hub");
    return;
  }
  const char* type = in["type"] | "";

  if (!strcmp(type, "challenge")) {
    const char* nonce = in["nonce"] | "";
    StaticJsonDocument<512> hello;
    hello["type"] = "hello";
    hello["v"] = 1;
    hello["role"] = _cfg.role;
    JsonObject auth = hello.createNestedObject("auth");
    auth["productid"] = _cfg.productId;
    auth["sig"] = hmacHex(_cfg.key, String(nonce) + "|" + _cfg.role + "|" + _cfg.productId);
    JsonObject client = hello.createNestedObject("client");
    client["name"] = _cfg.clientName;
    client["version"] = _cfg.clientVersion;
    sendDoc(hello);
    return;
  }

  if (!strcmp(type, "welcome")) {
    _ready = true;
    _backoffMs = 500;
    _ws.setReconnectInterval(_backoffMs);
    Serial.printf("[rcm] ready, room %s\n", (const char*)(in["pair"]["room"] | ""));
    sendFull();
    return;
  }

  if (!strcmp(type, "evt")) {
    if (!strcmp(in["op"] | "", "hub.closing")) {
      int code = in["data"]["code"] | 0;
      Serial.printf("[rcm] hub closing: %d %s\n", code, (const char*)(in["data"]["reason"] | ""));
      if (isFinalClose(code)) {
        _stopped = true;
        _stopReason = String(code) + " " + (const char*)(in["data"]["reason"] | "");
        _ready = false;
        _ws.disconnect();
      }
    }
    return;
  }

  if (!strcmp(type, "req")) {
    RcmReq req;
    req.id = in["id"] | "";
    req.op = in["op"] | "";
    req.data = in["data"].as<JsonObjectConst>();
    req.ttlMs = in["ttl"] | 5000;
    if (req.op == "state.get") {
      DynamicJsonDocument r(1024);
      r["rev"] = _rev;
      _fullState(r.createNestedObject("state"));
      ok(req.id, r.as<JsonObjectConst>());
      return;
    }
    auto it = _handlers.find(req.op);
    if (it == _handlers.end()) {
      fail(req.id, "UNSUPPORTED", req.op);
      return;
    }
    it->second(req);
  }
}

void RcmHub::sendDoc(JsonDocument& doc) {
  String s;
  serializeJson(doc, s);
  _ws.sendTXT(s);
}

void RcmHub::sendFull() {
  DynamicJsonDocument d(1024);
  d["type"] = "evt";
  d["op"] = _cfg.role + ".state";
  JsonObject data = d.createNestedObject("data");
  data["full"] = true;
  _rev = 0;
  data["rev"] = 0;
  _fullState(data.createNestedObject("state"));
  sendDoc(d);
}

void RcmHub::ok(const String& id, JsonObjectConst result) {
  DynamicJsonDocument d(1024);
  d["type"] = "res";
  d["id"] = id;
  d["ok"] = true;
  JsonObject data = d.createNestedObject("data");
  if (!result.isNull())
    for (JsonPairConst kv : result) data[kv.key()] = kv.value();
  sendDoc(d);
}

void RcmHub::ok(const String& id) { ok(id, JsonObjectConst()); }

void RcmHub::fail(const String& id, const char* code, const String& msg) {
  StaticJsonDocument<768> d;
  d["type"] = "res";
  d["id"] = id;
  d["ok"] = false;
  JsonObject e = d.createNestedObject("error");
  e["code"] = code;
  if (msg.length()) e["msg"] = msg.substring(0, 500);
  sendDoc(d);
}

void RcmHub::report(JsonObjectConst patch) {
  if (!_ready || patch.isNull() || patch.size() == 0) return;
  DynamicJsonDocument d(1024);
  d["type"] = "evt";
  d["op"] = _cfg.role + ".state";
  JsonObject data = d.createNestedObject("data");
  data["full"] = false;
  data["rev"] = ++_rev;
  data["state"] = patch;
  sendDoc(d);
}

void RcmHub::event(const char* op, JsonObjectConst payload) {
  if (!_ready) return;
  DynamicJsonDocument d(1024);
  d["type"] = "evt";
  d["op"] = op;
  d["data"] = payload;
  sendDoc(d);
}
