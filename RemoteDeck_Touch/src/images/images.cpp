// Design Ref: §5.3, §12.1 — Decoder Layer (BMP 24/32bit + PNG) + heap-aware + fail-soft
// Plan SC: FR-04, FR-05, FR-06 (downloadFile 안정화는 main.cpp에서 별도)

#include "images.h"
#include <Arduino.h>

// 최소 디코드 가용 heap (디코드 직전 체크)
static constexpr size_t MIN_DECODE_HEAP = 30 * 1024;

// 전역 LVGL 이미지 디스크립터 (외부 참조용 - 기존 코드 호환)
lv_img_dsc_t img_photo = {};
lv_img_dsc_t img_name  = {};
lv_img_dsc_t img_title = {};

uint16_t convert_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void free_img_dsc(lv_img_dsc_t* img_dsc) {
    // Design Ref: §12.1 — img_dsc.data malloc 한 것만 안전하게 free
    if (img_dsc && img_dsc->data) {
        free((void*)img_dsc->data);
        img_dsc->data = nullptr;
        img_dsc->data_size = 0;
        img_dsc->header.w = 0;
        img_dsc->header.h = 0;
    }
}

// 파일명 끝 확장자 소문자 비교 (대소문자 무관)
static bool endsWithCI(const char* s, const char* suffix) {
    if (!s || !suffix) return false;
    size_t sl = strlen(s), su = strlen(suffix);
    if (su > sl) return false;
    for (size_t i = 0; i < su; ++i) {
        char a = s[sl - su + i]; if (a >= 'A' && a <= 'Z') a += 32;
        char b = suffix[i];      if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }
    return true;
}

bool decode_image_from_spiffs(const char* filepath, lv_img_dsc_t* img_dsc) {
    if (!filepath || !img_dsc) return false;
    if (ESP.getFreeHeap() < MIN_DECODE_HEAP) {
        Serial.printf("Decode skip: low heap %u < %u\n", ESP.getFreeHeap(), (unsigned)MIN_DECODE_HEAP);
        return false;
    }
    if (endsWithCI(filepath, ".png")) {
        return read_png_from_spiffs(filepath, img_dsc);
    }
    return read_bmp_from_spiffs(filepath, img_dsc);
}

// Design Ref: §3.1 — 24bit / 32bit BMP, top-down / bottom-up 모두 지원
bool read_bmp_from_spiffs(const char* filepath, lv_img_dsc_t* img_dsc) {
    File bmpFile = SPIFFS.open(filepath, "r");
    if (!bmpFile) {
        Serial.printf("BMP open failed: %s\n", filepath);
        return false;
    }

    uint8_t hdr[54];
    if (bmpFile.read(hdr, 54) != 54) {
        Serial.println("BMP header read failed");
        bmpFile.close();
        return false;
    }

    if (hdr[0] != 'B' || hdr[1] != 'M') {
        Serial.println("BMP signature mismatch");
        bmpFile.close();
        return false;
    }

    uint32_t pixelDataOffset = *(uint32_t*)&hdr[10];
    int32_t  widthSigned     = *(int32_t*)&hdr[18];
    int32_t  heightSigned    = *(int32_t*)&hdr[22];
    uint16_t bpp             = *(uint16_t*)&hdr[28];
    uint32_t compression     = *(uint32_t*)&hdr[30];

    bool topDown = heightSigned < 0;
    uint32_t width  = (uint32_t)(widthSigned  < 0 ? -widthSigned  : widthSigned);
    uint32_t height = (uint32_t)(heightSigned < 0 ? -heightSigned : heightSigned);

    if ((bpp != 24 && bpp != 32) || compression != 0) {
        Serial.printf("BMP unsupported: bpp=%u comp=%u\n", bpp, compression);
        bmpFile.close();
        return false;
    }

    size_t imgSize = (size_t)width * height * 2; // RGB565 = 2 bytes/pixel
    uint16_t* pixelData = (uint16_t*)malloc(imgSize);
    if (!pixelData) {
        Serial.printf("BMP malloc failed: %u bytes (free=%u)\n", (unsigned)imgSize, ESP.getFreeHeap());
        bmpFile.close();
        return false;
    }

    // 행당 패딩 계산 (4-byte 정렬, 24bit BMP에 한함)
    uint32_t bytesPerPixel = bpp / 8;
    uint32_t rowSize       = ((bytesPerPixel * width + 3) / 4) * 4;
    uint32_t padding       = rowSize - (bytesPerPixel * width);

    bmpFile.seek(pixelDataOffset, SeekSet);

    uint8_t px[4]; // BGR(A)
    bool ok = true;
    for (uint32_t y = 0; y < height && ok; y++) {
        uint32_t dstY = topDown ? y : (height - 1 - y);
        for (uint32_t x = 0; x < width; x++) {
            if (bmpFile.read(px, bytesPerPixel) != (int)bytesPerPixel) { ok = false; break; }
            // px[0]=B, px[1]=G, px[2]=R, (px[3]=A) — RGB565는 알파 무시
            pixelData[dstY * width + x] = convert_rgb888_to_rgb565(px[2], px[1], px[0]);
        }
        if (ok && padding > 0) bmpFile.seek(padding, SeekCur);
    }

    bmpFile.close();

    if (!ok) {
        Serial.println("BMP pixel read failed");
        free(pixelData);
        return false;
    }

    img_dsc->header.always_zero = 0;
    img_dsc->header.w           = width;
    img_dsc->header.h           = height;
    img_dsc->header.cf          = LV_IMG_CF_TRUE_COLOR;
    img_dsc->data_size          = imgSize;
    img_dsc->data               = (const uint8_t*)pixelData;
    return true;
}

#if LV_USE_PNG
// lv_conf.h 에서 LV_USE_PNG=1 설정 시 lvgl/extra/libs/png/lodepng 사용 가능
// lodepng.h 는 자체적으로 namespace lodepng + C linkage 처리하므로 extern "C" 래핑 금지
#include "extra/libs/png/lodepng.h"

bool read_png_from_spiffs(const char* filepath, lv_img_dsc_t* img_dsc) {
    File f = SPIFFS.open(filepath, "r");
    if (!f) {
        Serial.printf("PNG open failed: %s\n", filepath);
        return false;
    }

    size_t fileSize = f.size();
    if (fileSize == 0) {
        f.close();
        return false;
    }

    uint8_t* fileBuf = (uint8_t*)malloc(fileSize);
    if (!fileBuf) {
        Serial.printf("PNG file malloc failed: %u (free=%u)\n", (unsigned)fileSize, ESP.getFreeHeap());
        f.close();
        return false;
    }
    if (f.read(fileBuf, fileSize) != (int)fileSize) {
        Serial.println("PNG file read failed");
        free(fileBuf);
        f.close();
        return false;
    }
    f.close();

    unsigned char* rgba = nullptr;
    unsigned w = 0, h = 0;
    unsigned err = lodepng_decode32(&rgba, &w, &h, fileBuf, fileSize);
    free(fileBuf);

    if (err) {
        Serial.printf("PNG decode failed: %s\n", lodepng_error_text(err));
        if (rgba) free(rgba);
        return false;
    }

    size_t imgSize = (size_t)w * h * 2;
    uint16_t* pixelData = (uint16_t*)malloc(imgSize);
    if (!pixelData) {
        Serial.printf("PNG out malloc failed: %u (free=%u)\n", (unsigned)imgSize, ESP.getFreeHeap());
        free(rgba);
        return false;
    }

    // RGBA8888 → RGB565 (알파 무시, 추후 알파 합성 필요시 배경색과 블렌딩)
    for (unsigned i = 0; i < w * h; ++i) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        pixelData[i] = convert_rgb888_to_rgb565(r, g, b);
    }
    free(rgba);

    img_dsc->header.always_zero = 0;
    img_dsc->header.w           = w;
    img_dsc->header.h           = h;
    img_dsc->header.cf          = LV_IMG_CF_TRUE_COLOR;
    img_dsc->data_size          = imgSize;
    img_dsc->data               = (const uint8_t*)pixelData;
    return true;
}
#else
bool read_png_from_spiffs(const char* filepath, lv_img_dsc_t* img_dsc) {
    Serial.println("PNG decode disabled (LV_USE_PNG=0)");
    return false;
}
#endif

// /download/ 우선, fallback /images/ — 디코드 후 LVGL src 갱신
// Design Ref: §12.1 — 새 dsc 할당 전 기존 data free
static void try_set(lv_obj_t* target, lv_img_dsc_t* dsc, const char* role) {
    // 두 후보 경로 (PNG 우선, BMP fallback)
    const char* candidates[4] = {
        nullptr, nullptr, nullptr, nullptr
    };
    char buf[4][64];
    snprintf(buf[0], sizeof(buf[0]), "/download/%s.png", role);
    snprintf(buf[1], sizeof(buf[1]), "/download/%s.bmp", role);
    snprintf(buf[2], sizeof(buf[2]), "/images/%s.png",   role);
    snprintf(buf[3], sizeof(buf[3]), "/images/%s.bmp",   role);
    candidates[0] = buf[0];
    candidates[1] = buf[1];
    candidates[2] = buf[2];
    candidates[3] = buf[3];

    lv_img_dsc_t tmp = {};
    bool ok = false;
    for (int i = 0; i < 4; ++i) {
        if (!SPIFFS.exists(candidates[i])) continue;
        if (decode_image_from_spiffs(candidates[i], &tmp)) {
            ok = true;
            Serial.printf("Image loaded [%s]: %s (%ux%u, %u bytes)\n",
                          role, candidates[i], tmp.header.w, tmp.header.h, (unsigned)tmp.data_size);
            break;
        }
    }

    if (!ok) {
        Serial.printf("Image load failed for role: %s\n", role);
        return;
    }

    // 기존 데이터 해제 후 새 데이터 바인딩
    free_img_dsc(dsc);
    *dsc = tmp;
    lv_img_set_src(target, dsc);
    lv_obj_invalidate(target);
}

void images_update() {
    Serial.printf("images_update start (free heap=%u)\n", ESP.getFreeHeap());
    try_set(ui_ibtnLogo,   &img_title, "title");
    try_set(ui_ImagePhoto, &img_photo, "photo");
    try_set(ui_ImageName,  &img_name,  "name");
    Serial.printf("images_update done  (free heap=%u)\n", ESP.getFreeHeap());
}
