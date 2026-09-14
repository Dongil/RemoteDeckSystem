#ifndef LVGL_TOUCH_H
#define LVGL_TOUCH_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <FT6236G.h>
#include <ui.h>

void lvgl_touch_init(uint16_t screenWidth, uint16_t screenHeight);
void lcd_show_webmode_info(const char* ip);   // v2.6: 웹 설정 모드 정적 안내화면 (TFT 만, LVGL 미사용)
void screen_saver_init(int timeout);
void lvgl_loop();

extern FT6236G ct;

#endif // LVGL_TOUCH_H
