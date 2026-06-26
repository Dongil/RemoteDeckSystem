// Design Ref: §3.1, §2.2 — Web active state + LCD freeze hook 구현
// Plan SC: FR-01, FR-03, FR-04

#include "WebActivityMonitor.h"

void WebActivityMonitor::markActive() {
    _lastTs = millis();
    if (_active) return;  // 이미 active — 콜백 재호출 안 함
    _active = true;
    if (_onChange && !_inCallback) {
        _inCallback = true;
        _onChange(true);
        _inCallback = false;
    }
}

void WebActivityMonitor::notifyTouch() {
    if (!_active) return;
    _active = false;
    _lastTs = 0;
    if (_onChange && !_inCallback) {
        _inCallback = true;
        _onChange(false);
        _inCallback = false;
    }
}

bool WebActivityMonitor::shouldFreezeLcd() {
    if (!_active) return false;
    // timeout 검사 — millis() rollover (49.7일) 는 임베디드 운영에서 무시
    if ((millis() - _lastTs) > IDLE_TIMEOUT_MS) {
        _active = false;
        if (_onChange && !_inCallback) {
            _inCallback = true;
            _onChange(false);
            _inCallback = false;
        }
        return false;
    }
    return true;
}
