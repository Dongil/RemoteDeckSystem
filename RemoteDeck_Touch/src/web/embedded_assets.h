#pragma once
// Design Ref: §5.3 — Web 정적 자산 PROGMEM embed
// v2.3-final-gz:
//   - INDEX_HTML_GZ : inline HTML (CSS+JS 통합) 의 gzip 압축본 + 길이
//                     → Content-Encoding: gzip 헤더 + 단일 httpd_resp_send
//   - STYLE_CSS / APP_JS : 디버깅 용도 raw 유지 (직접 /style.css /app.js GET 가능)
// 생성: tools/embed_www.py

#include <Arduino.h>

extern const uint8_t INDEX_HTML_GZ[] PROGMEM;
extern const size_t  INDEX_HTML_GZ_LEN;
extern const char    STYLE_CSS[]  PROGMEM;
extern const char    APP_JS[]     PROGMEM;
