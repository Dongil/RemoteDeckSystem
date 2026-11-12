#include "HubBridge.h"

static bool looksLikeIPv4(const std::string& s) {
    IPAddress ip;
    return ip.fromString(s.c_str());
}

// 잘린 UTF-8 다중 바이트 글자(한글 등)의 앞부분이 끝에 남지 않도록 정리
static void utf8Trim(char* s) {
    size_t i = strlen(s);
    uint8_t back = 0;
    while (i > 0 && back < 4 && ((uint8_t)s[i - 1] & 0xC0) == 0x80) { i--; back++; }
    if (i == 0) return;
    uint8_t lead = (uint8_t)s[i - 1];
    uint8_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (need > 1 && back + 1 < need) s[i - 1] = '\0';
}

static void copyStr(char* dst, size_t n, const char* src) {
    if (!src) src = "";
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
    utf8Trim(dst);
}

// JSON 값을 짧은 한 줄로 (잘리면 글자 경계에서 자르고 ".." 표시)
static void compact(JsonVariantConst v, char* out, size_t n) {
    if (v.isNull()) { out[0] = '\0'; return; }
    size_t w = serializeJson(v, out, n);
    if (w >= n - 1 && n > 3) { out[n - 3] = '\0'; utf8Trim(out); strcat(out, ".."); }
}

void HubBridge::logMsg(const String& msg) {
    Serial.printf("HUB: %s\n", msg.c_str());
    if (_deps.log) _deps.log(msg.c_str());
}

void HubBridge::begin(const HubConfig& cfg, const std::string& fw, const Deps& deps) {
    if (_started) return;
    _deps = deps;
    if (!cfg.enabled) return;

    // Design Ref: §7 — 설정 불완전이면 시작하지 않고 사유만 노출 (재연결마다 반복 로그 방지)
    if (cfg.host.empty() || cfg.productId.empty() || cfg.deviceKey.empty()) {
        if (!_incomplete) logMsg("config incomplete (host/product_id/device_key) - not started");
        _incomplete = true;
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
    // v3.1.0 rcm-power-diag: 짝 PC ping 결과 → deck.state.peer (허브의 부팅 실패 원인 추정용)
    io.peerNet = [this]() { return peerNet(); };

    // 송수신 내역 수집 (라이브러리 동작에는 영향 없음)
    _deck.hub().setTap([this](char kind, const char* text, size_t len) { onTap(kind, text, len); });
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
        if (ready) _readySinceMs = millis();
        logMsg(ready ? "connected" : "disconnected");
    }
    if (stopped && !_lastStopped) {
        logMsg(String("stopped: ") + _deck.hub().stopReason());
    }
    _lastStopped = stopped;

    updatePeerProbe();
}

// ─── v3.1.0 rcm-power-diag: 짝 PC ping ────────────────

const char* HubBridge::peerNet() const {
    if (_pingIp.length() == 0) return "unknown";
    switch (_peerState.load()) {
        case 1: return "up";
        case 2: return "down";
        default: return "unknown";
    }
}

// ping 태스크에서 호출됨 — atomic만 건드림
void HubBridge::onPingSuccess(esp_ping_handle_t, void* args) {
    auto* self = static_cast<HubBridge*>(args);
    self->_peerMisses = 0;
    self->_peerReplies++;
    self->_peerState = 1;
}

void HubBridge::onPingTimeout(esp_ping_handle_t, void* args) {
    auto* self = static_cast<HubBridge*>(args);
    uint8_t m = self->_peerMisses.load();
    if (m < 255) self->_peerMisses = m + 1;
    if (m + 1 >= 3) self->_peerState = 2;   // 연속 3회(약 30초) 무응답
}

// 짝 IP(welcome마다 갱신)가 바뀌면 세션을 다시 만든다. loop에서 호출, 비블로킹.
void HubBridge::updatePeerProbe() {
    const String& ip = _deck.hub().peerIp();
    if (ip == _pingIp) return;
    if (_ping) {
        esp_ping_stop(_ping);
        esp_ping_delete_session(_ping);
        _ping = nullptr;
    }
    _pingIp = ip;
    _peerState = 0;
    _peerMisses = 0;
    _peerReplies = 0;
    if (ip.length() == 0) {
        logMsg("peer ping off (no paired PC IP)");
        return;
    }
    ip_addr_t target;
    if (!ipaddr_aton(ip.c_str(), &target)) {
        logMsg(String("peer ping: bad IP ") + ip);
        return;
    }
    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr = target;
    cfg.count = ESP_PING_COUNT_INFINITE;
    cfg.interval_ms = 10000;
    cfg.timeout_ms = 1000;
    cfg.data_size = 32;
    esp_ping_callbacks_t cbs = {};
    cbs.cb_args = this;
    cbs.on_ping_success = onPingSuccess;
    cbs.on_ping_timeout = onPingTimeout;
    if (esp_ping_new_session(&cfg, &cbs, &_ping) != ESP_OK || esp_ping_start(_ping) != ESP_OK) {
        logMsg(String("peer ping start failed: ") + ip);
        if (_ping) {
            esp_ping_delete_session(_ping);
            _ping = nullptr;
        }
        return;
    }
    logMsg(String("peer ping -> ") + ip + " (10s)");
}

// ─── 송수신 내역 ───────────────────────────────────────

const char* HubBridge::opForId(const char* id) const {
    // 최근 수신 req 중 같은 id의 op (res에 op 이름을 붙이기 위함). _mtx 보유 상태에서 호출.
    for (uint8_t k = 0; k < _count; k++) {
        const Entry& e = _log[(_head + kLogSize - 1 - k) % kLogSize];
        if (e.dir == 'i' && !strcmp(e.type, "req") && !strcmp(e.id, id)) return e.op;
    }
    return "";
}

void HubBridge::push(char dir, const char* type, const char* op, const char* id, int8_t ok, const char* detail) {
    std::lock_guard<std::mutex> lk(_mtx);
    Entry& e = _log[_head];
    e.ms = millis();
    time_t now = time(nullptr);
    e.epoch = now > 1600000000 ? now : 0;
    e.dir = dir;
    copyStr(e.type, sizeof(e.type), type);
    copyStr(e.id, sizeof(e.id), id);
    // res에는 해당 req의 op를 붙인다
    if ((!op || !*op) && dir == 'o' && !strcmp(type, "res")) op = opForId(id);
    copyStr(e.op, sizeof(e.op), op);
    e.ok = ok;
    copyStr(e.detail, sizeof(e.detail), detail);
    _head = (_head + 1) % kLogSize;
    if (_count < kLogSize) _count++;

    if (dir == 'o' && !strcmp(type, "res")) {
        copyStr(_lastCmd, sizeof(_lastCmd), e.op);
        _lastCmdOk = ok;
        _lastCmdMs = e.ms;
    }
}

void HubBridge::onTap(char kind, const char* text, size_t len) {
    if (kind == 'c') { push('c', "socket", "", "", -1, "연결됨 (인증 대기)"); return; }
    if (kind == 'd') {
        _drops++;
        push('d', "socket", "", "", -1, "연결 끊김");
        return;
    }

    if (kind == 'i') { _rx++; _lastRxMs = millis(); }
    else             { _tx++; _lastTxMs = millis(); }

    // 필요한 키만 파싱 (서명·키 등은 읽지 않음)
    StaticJsonDocument<256> filter;
    filter["type"] = true; filter["op"] = true; filter["id"] = true; filter["ok"] = true;
    filter["error"] = true; filter["data"] = true; filter["session"] = true;
    filter["heartbeatMs"] = true; filter["pair"]["room"] = true; filter["by"]["sub"] = true;
    filter["auth"]["productid"] = true; filter["client"] = true;
    DynamicJsonDocument d(1536);
    if (deserializeJson(d, text, len, DeserializationOption::Filter(filter))) {
        push(kind, "?", "", "", -1, "JSON 아님");
        return;
    }
    const char* type = d["type"] | "?";
    const char* op = d["op"] | "";
    const char* id = d["id"] | "";
    char detail[96] = "";

    if (!strcmp(type, "challenge")) {
        copyStr(detail, sizeof(detail), "인증 요청");
    } else if (!strcmp(type, "hello")) {
        snprintf(detail, sizeof(detail), "deck %s, %s %s", (const char*)(d["auth"]["productid"] | ""),
                 (const char*)(d["client"]["name"] | ""), (const char*)(d["client"]["version"] | ""));
    } else if (!strcmp(type, "welcome")) {
        {
            std::lock_guard<std::mutex> lk(_mtx);   // toJson(async_tcp)과 동시 접근
            _welcomes++;
            _heartbeatMs = d["heartbeatMs"] | 0;
            copyStr(_session, sizeof(_session), d["session"] | "");
            copyStr(_room, sizeof(_room), d["pair"]["room"] | "");
        }
        snprintf(detail, sizeof(detail), "방 %s, heartbeat %lus", _room, (unsigned long)(_heartbeatMs / 1000));
    } else if (!strcmp(type, "req")) {
        _reqs++;
        char data[56];
        compact(d["data"], data, sizeof(data));
        const char* by = d["by"]["sub"] | "";
        if (*by) snprintf(detail, sizeof(detail), "%s (by %s)", data, by);
        else copyStr(detail, sizeof(detail), data);
    } else if (!strcmp(type, "res")) {
        bool ok = d["ok"] | false;
        if (!ok) _resFail++;
        if (ok) compact(d["data"], detail, sizeof(detail));
        else snprintf(detail, sizeof(detail), "%s %s", (const char*)(d["error"]["code"] | ""),
                      (const char*)(d["error"]["msg"] | ""));
        push(kind, type, op, id, ok ? 1 : 0, detail);
        return;
    } else if (!strcmp(type, "evt")) {
        if (!strcmp(op, "hub.closing")) {
            snprintf(detail, sizeof(detail), "%d %s", (int)(d["data"]["code"] | 0), (const char*)(d["data"]["reason"] | ""));
        } else if (d["data"]["full"] | false) {
            snprintf(detail, sizeof(detail), "전체 상태 (rev %lu)", (unsigned long)(d["data"]["rev"] | 0));
        } else {
            // 패치: data.state(바뀐 키만)가 있으면 그것만, 없으면 rev·full을 뺀 data
            JsonVariantConst st = d["data"]["state"];
            if (!st.isNull()) {
                compact(st, detail, sizeof(detail));
            } else {
                DynamicJsonDocument p(512);
                for (JsonPairConst kv : d["data"].as<JsonObjectConst>()) {
                    if (!strcmp(kv.key().c_str(), "rev") || !strcmp(kv.key().c_str(), "full")) continue;
                    p[kv.key()] = kv.value();
                }
                compact(p.as<JsonVariantConst>(), detail, sizeof(detail));
            }
        }
    }
    push(kind, type, op, id, -1, detail);
}

String HubBridge::toJson(const HubConfig& cfg) {
    DynamicJsonDocument doc(20480);   // 40건 × 약 300B
    uint32_t now = millis();
    doc["enabled"] = cfg.enabled;
    doc["started"] = _started;
    doc["connected"] = _started && _deck.hub().ready();
    doc["stopped"] = _started && _deck.hub().stopped();
    if (_started && _deck.hub().stopped()) doc["stop_reason"] = _deck.hub().stopReason();
    else if (cfg.enabled && _incomplete)   doc["stop_reason"] = "config incomplete";
    else                                   doc["stop_reason"] = "";
    doc["url"] = String(cfg.tls ? "wss://" : "ws://") + cfg.host.c_str() + ":" + cfg.port +
                 (cfg.path.empty() ? "/v1/ws" : cfg.path.c_str());
    doc["product_id"] = cfg.productId;

    std::lock_guard<std::mutex> lk(_mtx);
    doc["session"] = _session;
    doc["room"] = _room;
    doc["heartbeat_ms"] = _heartbeatMs;
    doc["connected_for_s"] = (_started && _deck.hub().ready()) ? (now - _readySinceMs) / 1000 : 0;
    doc["welcomes"] = _welcomes;
    doc["drops"] = _drops;
    doc["rx"] = _rx;
    doc["tx"] = _tx;
    doc["reqs"] = _reqs;
    doc["res_fail"] = _resFail;
    doc["last_rx_s"] = _lastRxMs ? (int32_t)((now - _lastRxMs) / 1000) : -1;
    doc["last_tx_s"] = _lastTxMs ? (int32_t)((now - _lastTxMs) / 1000) : -1;
    doc["last_cmd"] = _lastCmd;
    doc["last_cmd_ok"] = _lastCmdOk;
    doc["last_cmd_s"] = _lastCmdMs ? (int32_t)((now - _lastCmdMs) / 1000) : -1;
    // v3.1.0: 짝 PC ping 상태 (점검용, 웹 UI는 표시하지 않음)
    doc["peer_ip"] = _pingIp;
    doc["peer_net"] = peerNet();
    doc["peer_replies"] = _peerReplies.load();

    JsonArray arr = doc.createNestedArray("log");
    for (uint8_t k = 0; k < _count; k++) {
        const Entry& e = _log[(_head + kLogSize - 1 - k) % kLogSize];
        JsonObject o = arr.createNestedObject();
        o["t"] = (uint32_t)e.epoch;
        o["ago"] = (now - e.ms) / 1000;
        o["dir"] = String(e.dir);
        o["type"] = e.type;
        o["op"] = e.op;
        o["id"] = e.id;
        if (e.ok >= 0) o["ok"] = e.ok == 1;
        o["detail"] = e.detail;
    }
    String out;
    serializeJson(doc, out);
    return out;
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
