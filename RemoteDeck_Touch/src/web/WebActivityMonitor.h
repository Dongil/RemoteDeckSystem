#pragma once
// Design Ref: §3.1 — WebActivityMonitor (시간 분할 핵심 클래스)
// Plan SC: FR-01 (Web active mode), FR-03 (10초 idle 복귀), FR-04 (LCD touch 우선)
//
// 책임:
//   - web active flag + 마지막 활성 timestamp 단독 보유
//   - markActive() : WebServer handler 첫 줄 hook (core 0)
//   - notifyTouch() : main loop touch IRQ (core 1, tap-to-acquire)
//   - shouldFreezeLcd() : main loop 매 tick 비교
//   - onModeChange callback : LCD freeze/resume 진입/종료 hook
//
// 의존: 없음 (순수 state)

#include <Arduino.h>
#include <functional>

class WebActivityMonitor {
public:
    // Design Ref: §7.2 — 사용자 답변 10초
    static constexpr uint32_t IDLE_TIMEOUT_MS = 10000;

    using ModeChangeCb = std::function<void(bool active)>;

    // freezeLCD()/resumeLCD() 진입/종료 시점 알림. nullable.
    void setOnModeChange(ModeChangeCb cb) { _onChange = cb; }

    // WebServer handler 의 requireAuth() 직후 호출 (core 0 httpd task).
    // active=false → true 전환 시 onChange(true) 1회 호출.
    void markActive();

    // main loop 에서 touch IRQ/event 감지 시 호출 (core 1).
    // active=true → false 전환 + 즉시 LCD 복귀 (onChange(false) 호출).
    void notifyTouch();

    // main loop 매 tick 호출. timeout 검사 + state transition.
    // returns: true 면 LVGL/TFT 정지 (lvgl_loop skip), false 면 정상 진행
    bool shouldFreezeLcd();

    bool     isActive() const { return _active; }
    uint32_t lastTs()   const { return _lastTs; }

private:
    volatile bool     _active = false;
    volatile uint32_t _lastTs = 0;
    ModeChangeCb      _onChange = nullptr;

    // reentrancy guard — onChange 콜백 안에서 다시 markActive/notifyTouch 호출 보호
    volatile bool _inCallback = false;
};
