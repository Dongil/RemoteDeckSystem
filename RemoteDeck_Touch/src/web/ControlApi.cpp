// Design Ref: §4.1 #12,13 — Long polling + MQTT bridge
// Plan SC: FR-06, FR-07

#include "ControlApi.h"
#include "WebServer.h"
#include <ArduinoJson.h>

void ControlApi::begin() {
    if (!_eg) _eg = xEventGroupCreate();
}

void ControlApi::attach(WebServer* ws) {
    if (!ws) return;
    if (!_eg) begin();
    ws->setControlGetter([this](uint32_t since, String& body) {
        return waitForChange(since, body);
    });
    ws->setControlSetter([this](const String& body, String& outJson, String& err) {
        return applyToggle(body, outJson, err);
    });
    ws->setControlCurrentGetter([this]() { return currentJson(); });
}

void ControlApi::notifyState(bool in, bool out) {
    if (_in == in && _out == out) return;  // no-op (etag 증가 방지)
    _in = in;
    _out = out;
    _etag++;
    if (_eg) xEventGroupSetBits(_eg, STATE_CHANGED_BIT);
    Serial.printf("ControlApi: state notify in=%d out=%d etag=%u\n",
                  (int)in, (int)out, (unsigned)_etag);
}

bool ControlApi::waitForChange(uint32_t since, String& outJson) {
    if (since != _etag) {
        outJson = buildJson(_etag, _in, _out);
        return true;
    }
    // esp_http_server 는 단일 task — 긴 block 시 다른 요청 모두 block.
    // 500ms slice 로 쪼개 매 slice 마다 task yield → 동시 다른 요청 처리 가능.
    if (_eg) {
        xEventGroupClearBits(_eg, STATE_CHANGED_BIT);
        uint32_t deadline = millis() + LONG_POLL_MS;
        while ((int32_t)(deadline - millis()) > 0) {
            if (since != _etag) break;
            EventBits_t bits = xEventGroupWaitBits(_eg, STATE_CHANGED_BIT,
                                                   pdTRUE, pdFALSE,
                                                   pdMS_TO_TICKS(500));
            if (bits & STATE_CHANGED_BIT) break;
        }
    } else {
        delay(LONG_POLL_MS);
    }
    bool changed = (since != _etag);
    outJson = buildJson(_etag, _in, _out);
    return changed;
}

bool ControlApi::applyToggle(const String& body, String& outJson, String& errOut) {
    StaticJsonDocument<128> doc;
    DeserializationError de = deserializeJson(doc, body);
    if (de) {
        errOut = String("invalid_json:") + de.f_str();
        return false;
    }
    // POST 의 명시된 키를 토글 의도로 해석.
    // "in"=true → IN 상태 (out 자동 false), "out"=true → OUT 상태 (in 자동 false).
    // 명시 안 됐거나 둘 다 false 면 대기 (둘 다 false).
    bool newIn = _in, newOut = _out;
    bool hasIn  = doc.containsKey("in")  && doc["in"].as<bool>();
    bool hasOut = doc.containsKey("out") && doc["out"].as<bool>();
    if (hasIn)       { newIn = true;  newOut = false; }
    else if (hasOut) { newIn = false; newOut = true;  }
    else             { newIn = false; newOut = false; }

    bool changed = (newIn != _in) || (newOut != _out);
    if (changed) {
        _in = newIn;
        _out = newOut;
        _etag++;
        if (_eg) xEventGroupSetBits(_eg, STATE_CHANGED_BIT);
        // MQTT publish (main loop core 1 에서 콜백 등록되어 있음)
        if (_publish) _publish(newIn ? "IN" : "OUT");
        Serial.printf("ControlApi: POST toggle in=%d out=%d etag=%u (mqtt sent)\n",
                      (int)newIn, (int)newOut, (unsigned)_etag);
    }
    outJson = buildJson(_etag, _in, _out);
    return true;
}

String ControlApi::currentJson() const {
    return buildJson(_etag, _in, _out);
}

String ControlApi::buildJson(uint32_t etag, bool in, bool out) const {
    StaticJsonDocument<96> doc;
    doc["etag"] = etag;
    doc["in"]   = in;
    doc["out"]  = out;
    String s;
    serializeJson(doc, s);
    return s;
}
