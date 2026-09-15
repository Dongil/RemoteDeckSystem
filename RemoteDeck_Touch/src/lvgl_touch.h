#ifndef LVGL_TOUCH_H
#define LVGL_TOUCH_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <FT6236G.h>
#include <ui.h>

void lvgl_touch_init(uint16_t screenWidth, uint16_t screenHeight);
void lcd_show_webmode_info(const char* ip);   // v2.6: 웹 설정 모드 정적 안내화면 (TFT 만, LVGL 미사용)
void screen_saver_init(int timeout);
void lvgl_set_night_active(bool active);       // v2.7: 야간 화면 끄기 활성 상태 (main 이 NTP 판정 후 전달)
void lvgl_loop();

extern FT6236G ct;

#endif // LVGL_TOUCH_H
