#pragma once
// Design Ref: §4.1 #11 /api/log — Ring buffer 50건
// Plan SC: FR-03 (3탭 — Logs)

#include <Arduino.h>

class WebServer;

class Logger {
public:
    // v2.3 module-webui: DRAM 한계 (dram0_0_seg overflow) 회피 위해 50→30, detail 120→96
    // 절약: 50*148 - 30*124 = 4280 byte
    static constexpr size_t MAX_ENTRIES = 30;
    static constexpr size_t EVENT_MAX = 24;
    static constexpr size_t DETAIL_MAX = 96;

    struct Entry {
        uint32_t ts;                   // millis() 기준
        char event[EVENT_MAX];
        char detail[DETAIL_MAX];
    };

    // WebServer 와 결합 — setLogger 콜백 등록
    void attach(WebServer* ws);

    // 외부에서 직접 호출 (예: main.cpp boot 단계)
    void log(const char* event, const char* detail);

    // /api/log JSON 응답 생성
    String buildJson() const;

private:
    Entry _ring[MAX_ENTRIES];
    size_t _head = 0;    // 다음 쓸 인덱스
    size_t _count = 0;   // 채워진 엔트리 수
};
