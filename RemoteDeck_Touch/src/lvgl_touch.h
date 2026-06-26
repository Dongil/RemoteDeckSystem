#ifndef LVGL_TOUCH_H
#define LVGL_TOUCH_H

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <FT6236G.h>
#include <ui.h>

void lvgl_touch_init(uint16_t screenWidth, uint16_t screenHeight);
void screen_saver_init(int timeout);
void lvgl_loop();

// v2.4: main.cpp 에서 LCD freeze 시 touch tap-to-acquire 감지용 직접 polling
int  getTouch(uint16_t *pPoints);

extern FT6236G ct;

#endif // LVGL_TOUCH_H
