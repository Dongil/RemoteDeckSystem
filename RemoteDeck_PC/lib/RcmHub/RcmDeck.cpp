#include "RcmDeck.h"

void RcmDeck::begin(const RcmHubConfig& cfg, const RcmDeckIo& io) {
  _io = io;
  _lastPc = _io.pcOn();
  _lastRelays = 0;
  for (uint8_t r = 1; r <= _io.relayCount; r++)
    if (_io.getRelay(r)) _lastRelays |= 1 << (r - 1);
  _lastPeer = _io.peerNet ? _io.peerNet() : "";

  _hub.on("pc.powerOn", [this](RcmReq& req) { startPress(req, true); });
  _hub.on("pc.forceOff", [this](RcmReq& req) { startPress(req, false); });

  _hub.on("deck.relay.set", [this](RcmReq& req) {
    uint8_t relay = req.data["relay"] | 0;
    bool on = req.data["on"] | false;
    if (relay < 1 || relay > _io.relayCount) return _hub.fail(req.id, "BAD_REQUEST", "relay " + String(relay) + " 없음");
    if (relay == _io.powerRelay && on) return _hub.fail(req.id, "BAD_REQUEST", "전원 버튼 릴레이는 켜 둘 수 없음(pulse 또는 power.on/off 사용)");
    _io.setRelay(relay, on);
    StaticJsonDocument<256> r;
    JsonArray a = r.createNestedArray("relays");
    for (uint8_t i = 1; i <= _io.relayCount; i++) a.add(_io.getRelay(i));
    _hub.ok(req.id, r.as<JsonObjectConst>());
  });

  _hub.on("deck.relay.pulse", [this](RcmReq& req) {
    uint8_t relay = req.data["relay"] | 0;
    uint16_t ms = req.data["ms"] | 0;
    if (relay < 1 || relay > _io.relayCount) return _hub.fail(req.id, "BAD_REQUEST", "relay " + String(relay) + " 없음");
    if (_press.active) return _hub.fail(req.id, "BUSY", "전원 버튼 동작 중");
    _io.pulse(relay, ms);
    _hub.ok(req.id);
  });

  _hub.on("deck.reboot", [this](RcmReq& req) {
    _hub.ok(req.id);
    _rebootPending = true;
    _rebootAt = millis() + 300;  // let the res leave first
  });

  _hub.begin(cfg, [this](JsonObject s) { fullState(s); });
}

void RcmDeck::fullState(JsonObject s) {
  s["pc"] = _io.pcOn() ? "on" : "off";
  JsonArray relays = s.createNestedArray("relays");
  for (uint8_t r = 1; r <= _io.relayCount; r++) relays.add(_io.getRelay(r));
  if (_io.inputs) _io.inputs(s.createNestedArray("inputs"));
  if (_io.fw.length()) s["fw"] = _io.fw.substring(0, 32);
  if (_io.ip) s["ip"] = _io.ip();
  if (_io.net) s["net"] = _io.net();
  if (peerOn()) addPeer(s, _io.peerNet());
}

// peer only to a hub that lists it in welcome.caps (a v1.0 hub would reject the whole deck.state)
bool RcmDeck::peerOn() { return _io.peerNet && _hub.hubHas("peer"); }

// v1.1.0: deck.state.peer {net, ip} - SmartDeck's ping of the paired PC
void RcmDeck::addPeer(JsonObject s, const char* net) {
  JsonObject p = s.createNestedObject("peer");
  p["net"] = net;
  if (_hub.peerIp().length()) p["ip"] = _hub.peerIp().substring(0, 64);
}

void RcmDeck::startPress(RcmReq& req, bool wantOn) {
  uint16_t pressMs = req.data["pressMs"] | (wantOn ? 500 : 6000);
  uint32_t waitMs = req.data["waitMs"] | 15000;
  bool on = _io.pcOn();
  if (on == wantOn) {
    // already in the wanted state: never press (a press would switch it the other way)
    StaticJsonDocument<64> r;
    r["pc"] = on ? "on" : "off";
    r["pressed"] = false;
    return _hub.ok(req.id, r.as<JsonObjectConst>());
  }
  if (_press.active) return _hub.fail(req.id, "BUSY", "전원 버튼 동작 중");
  _press.active = true;
  _press.wantOn = wantOn;
  _press.id = req.id;
  _press.start = millis();
  _press.pressMs = pressMs;
  _press.waitMs = waitMs;
  _io.pulse(_io.powerRelay, pressMs);
}

void RcmDeck::finishPress(bool ok) {
  _press.active = false;
  if (ok) {
    StaticJsonDocument<64> r;
    r["pc"] = _press.wantOn ? "on" : "off";
    r["pressed"] = true;
    _hub.ok(_press.id, r.as<JsonObjectConst>());
  } else {
    _hub.fail(_press.id, "DEVICE_ERROR",
              String(_press.wantOn ? "버튼을 눌렀지만 " : "길게 눌렀지만 ") + String(_press.waitMs) + "ms 안에 PC LED가 " +
                  (_press.wantOn ? "켜지지 않음" : "꺼지지 않음"));
  }
}

void RcmDeck::loop() {
  _hub.loop();

  // report changes as they happen (the hub has no other way to know the PC power state)
  StaticJsonDocument<384> patch;
  bool pc = _io.pcOn();
  if (pc != _lastPc) {
    _lastPc = pc;
    patch["pc"] = pc ? "on" : "off";
  }
  uint8_t bits = 0;
  for (uint8_t r = 1; r <= _io.relayCount; r++)
    if (_io.getRelay(r)) bits |= 1 << (r - 1);
  if (bits != _lastRelays) {
    _lastRelays = bits;
    JsonArray a = patch.createNestedArray("relays");
    for (uint8_t r = 1; r <= _io.relayCount; r++) a.add((bits >> (r - 1)) & 1 ? true : false);
  }
  if (peerOn()) {
    const char* net = _io.peerNet();
    if (_lastPeer != net) {
      _lastPeer = net;
      addPeer(patch.isNull() ? patch.to<JsonObject>() : patch.as<JsonObject>(), net);  // empty doc is not an object yet
    }
  }
  if (patch.size()) _hub.report(patch.as<JsonObjectConst>());

  if (_press.active) {
    uint32_t t = millis() - _press.start;
    if (t >= _press.pressMs && _io.pcOn() == _press.wantOn) finishPress(true);
    else if (t >= (uint32_t)_press.pressMs + _press.waitMs) finishPress(false);
  }

  if (_rebootPending && (int32_t)(millis() - _rebootAt) >= 0) {
    _rebootPending = false;
    if (_io.reboot) _io.reboot();
  }
}
