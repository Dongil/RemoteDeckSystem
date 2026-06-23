#pragma once
// Design Ref: §5.2 Logs Tab + §11.2 C6 — in-memory ring buffer 50건

#include <Arduino.h>

class TouchWebServer;

class Logger {
public:
    static constexpr uint8_t CAPACITY = 50;
    static constexpr size_t  EVENT_LEN  = 16;
    static constexpr size_t  DETAIL_LEN = 80;

    struct Entry {
        uint32_t ts;            // millis() at insert
        char     event[EVENT_LEN];
        char     detail[DETAIL_LEN];
    };

    void attach(TouchWebServer* ws);   // /api/log GET 등록
    void add(const char* event, const char* detail);
    String toJson() const;
    void clear() { _head = 0; _count = 0; }

private:
    Entry _buf[CAPACITY];
    uint8_t _head = 0;
    uint8_t _count = 0;
    TouchWebServer* _ws = nullptr;

    void handleGet();
};

// ImageApi 등에서 forward-decl 후 호출하는 free helper
void logger_add(Logger* l, const char* event, const char* detail);
