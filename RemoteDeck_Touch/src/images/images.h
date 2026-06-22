#pragma once
// Design Ref: §5.3 Component List — Decoder Layer
// Plan SC: FR-04 (BMP 24/32bit + top/bottom-up), FR-05 (PNG → RGB565)

#include <ui.h>
#include <SPIFFS.h>

uint16_t convert_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);

// Design Ref: §12.1 — heap-aware decode, fail-soft (실패 시 기존 dsc 유지)
//   filepath: SPIFFS 절대 경로 (.bmp or .png)
//   img_dsc : LVGL 디스크립터 (성공 시 data 새로 malloc, 호출자가 lv_img_set_src 후 free 책임)
//   반환     : 성공 시 true, 실패 시 false (img_dsc 미변경)
bool decode_image_from_spiffs(const char* filepath, lv_img_dsc_t* img_dsc);

// 24/32bit BMP 디코더 (top-down/bottom-up 자동 감지)
bool read_bmp_from_spiffs(const char* filepath, lv_img_dsc_t* img_dsc);

// LV_USE_PNG 활성 시 PNG → RGB565 디코더
bool read_png_from_spiffs(const char* filepath, lv_img_dsc_t* img_dsc);

// Design Ref: §12.1 — 새 dsc 할당 전 기존 data free
void free_img_dsc(lv_img_dsc_t* img_dsc);

// /download/ 우선, fallback /images/ — 핫리로드 시 재호출
void images_update();
