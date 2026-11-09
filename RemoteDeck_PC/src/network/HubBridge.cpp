#include "HubBridge.h"

static bool looksLikeIPv4(const std::string& s) {
    IPAddress ip;
    return ip.fromString(s.c_str());
}

void HubBridge::logMsg(const String& msg) {
    Serial.printf("HUB: %s\n", msg.c_str());
    if (_deps.log) _deps.log(msg.c_str());
}

void HubBridge::begin(const HubConfig& cfg, const std::string& fw, const Deps& deps) {
    if (_started) return;
    _deps = deps;
    if (!cfg.enabled) return;

    // Design Ref: §7 — 설정 불완전이면 시작하지 않고 사유만 노출
    if (cfg.host.empty() || cfg.productId.empty() || cfg.deviceKey.empty()) {
        _incomplete = true;
        logMsg("config incomplete (host/product_id/device_key) - not started");
        return;
    }
    if (!looksLikeIPv4(cfg.host)) {
        logMsg(String("host '") + cfg.host.c_str() + "' is not an IP - DNS may fail on Ethernet");
    }

    RcmHubConfig hc;
    hc.host = cfg.host.c_str();
    hc.port = cfg.port;
    hc.path = cfg.path.empty() ? "/v1/ws" : cfg.path.c_str();
    hc.tls = cfg.tls;
    hc.role = "deck";
    hc.productId = cfg.productId.c_str();
    hc.key = cfg.deviceKey.c_str();
    hc.clientName = "RemoteDeck_PC";
    hc.clientVersion = fw.c_str();

    RcmDeckIo io;
    io.pcOn = _deps.pcOn;
    io.getRelay = _deps.getRelay;
    io.setRelay = _deps.setRelay;
    io.pulse = _deps.pulse;
    io.inputs = _deps.inputs;
    io.ip = _deps.ip;
    io.net = _deps.net;
    io.reboot = [] { ESP.restart(); };
    io.relayCount = 2;
    io.powerRelay = 1;
    io.fw = fw.c_str();

    _deck.begin(hc, io);
    _started = true;
    logMsg(String("started -> ") + (cfg.tls ? "wss://" : "ws://") + hc.host + ":" + hc.port + hc.path +
           " (" + hc.productId + ")");
}

void HubBridge::loop() {
    if (!_started) return;
    _deck.loop();

    // 상태 변화 1회씩 로그 (키 값은 절대 출력하지 않음)
    bool ready = _deck.hub().ready();
    bool stopped = _deck.hub().stopped();
    if (ready != _lastReady) {
        _lastReady = ready;
        logMsg(ready ? "connected" : "disconnected");
    }
    if (stopped && !_lastStopped) {
        logMsg(String("stopped: ") + _deck.hub().stopReason());
    }
    _lastStopped = stopped;
}

void HubBridge::fillStatus(JsonDocument& doc, const HubConfig& cfg) {
    doc["hub_enabled"] = cfg.enabled;
    doc["hub_connected"] = _started && _deck.hub().ready();
    doc["hub_stopped"] = _started && _deck.hub().stopped();
    if (_started && _deck.hub().stopped()) doc["hub_stop_reason"] = _deck.hub().stopReason();
    else if (cfg.enabled && _incomplete)   doc["hub_stop_reason"] = "config incomplete";
    else                                   doc["hub_stop_reason"] = "";
    if (cfg.enabled) {
        doc["hub_host"] = String(cfg.host.c_str()) + ":" + cfg.port;
        doc["hub_product_id"] = cfg.productId;
    } else {
        doc["hub_host"] = "";
        doc["hub_product_id"] = "";
    }
}
