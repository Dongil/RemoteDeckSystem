#pragma once
// Design Ref: §5.3 — Web 정적 자산 PROGMEM embed
//   SPIFFS 의 /www/ 가 아닌 펌웨어 Flash 에 인라인.
//   장점: uploadfs 불필요 → 단말 SPIFFS (/images, /deviceconfig 등) 보존 + 펌웨어/UI 버전 일관성.
//   단점: HTML/CSS/JS 수정 시마다 펌웨어 빌드 필요 (≈ +12KB Flash).
// 생성: data/www/{index.html,style.css,app.js} 의 내용을 R"(...)" raw string 으로 단순 임베드.

#include <Arduino.h>

extern const char INDEX_HTML[] PROGMEM;
extern const char STYLE_CSS[]  PROGMEM;
extern const char APP_JS[]     PROGMEM;
