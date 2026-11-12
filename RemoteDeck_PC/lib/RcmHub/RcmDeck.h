// SmartDeck side of the RCM 통합 API (role "deck") on top of RcmHub.
// Hardware is reached only through RcmDeckIo, so RemoteDeck_PC plugs in its RelayController / PCMonitor.
//
// Rules (handoff/SMARTDECK.md, reference rcm-api/sim/deck.js):
//  - pc.powerOn / pc.forceOff: check the PC LED first, press only when needed (pressed:false otherwise),
//    then answer when the LED shows the wanted state, after the press is over. Non-blocking.
//  - report pc on every LED change (the hub's power state comes only from this), relays on every change
//  - relay used for the power button is never latched by deck.relay.set (it would hold the button down)
#pragma once
#include "RcmHub.h"

struct RcmDeckIo {
  std::function<bool()> pcOn;                              // debounced PC power LED (PCMonitor::isPCOn)
  std::function<bool(uint8_t relay)> getRelay;             // relay 1..relayCount
  std::function<void(uint8_t relay, bool on)> setRelay;
  std::function<void(uint8_t relay, uint16_t ms)> pulse;   // non-blocking (RelayController::pulse)
  std::function<void(JsonArray inputs)> inputs;            // optional: GPIO inputs as booleans
  std::function<String()> ip;                              // current LAN IP
  std::function<String()> net;                             // "eth" | "wifi"
  std::function<void()> reboot;                            // ESP.restart
  // v1.1.0 optional: ping result for the paired PC (hub().peerIp()): "up" | "down" | "unknown".
  // Reported as deck.state.peer so the hub can tell "OS not up" from "agent not running" after a power-on.
  // Sent only to a hub that lists "peer" in welcome.caps (v1.1+). Must not block (ping elsewhere, return the last result).
  std::function<const char*()> peerNet;
  uint8_t relayCount = 2;
  uint8_t powerRelay = 1;                                  // relay wired to the PC power button
  String fw;
};

class RcmDeck {
 public:
  void begin(const RcmHubConfig& cfg, const RcmDeckIo& io);
  void loop();
  RcmHub& hub() { return _hub; }

 private:
  void fullState(JsonObject s);
  void addPeer(JsonObject s, const char* net);
  bool peerOn();
  void startPress(RcmReq& req, bool wantOn);
  void finishPress(bool ok);

  RcmHub _hub;
  RcmDeckIo _io;
  bool _lastPc = false;
  uint8_t _lastRelays = 0;  // bit i = relay i+1
  String _lastPeer;         // last reported peer.net
  bool _rebootPending = false;
  uint32_t _rebootAt = 0;

  struct Press {
    bool active = false;
    bool wantOn = false;
    String id;
    uint32_t start = 0;
    uint16_t pressMs = 0;
    uint32_t waitMs = 0;
  } _press;
};
