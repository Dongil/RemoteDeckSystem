#include "lvgl_touch.h"

#define I2C_SCL -1
#define I2C_SDA -1

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[240 * 320 / 10]; // Adjust based on screen size

FT6236G ct;

TFT_eSPI tft = TFT_eSPI(240, 320); /* TFT instance */

static bool sleep_using;    //화면보호기 사용 여부
static int last_touch_time = 0; // 마지막 터치 시간 기록
static int screen_timeout = 60000; // 1분 타임아웃 (60,000ms)
static bool screen_protected = false; // 화면 보호 상태 플래그
static lv_obj_t* last_screen = NULL; // 마지막 화면 저장 객체

// v2.7: 야간 화면 끄기 (스크린세이버 메커니즘 재사용, 시간대 기반)
static bool night_active = false;              // main 이 NTP 로 판정한 야간 창 상태
static const uint32_t NIGHT_WAKE_MS = 10000;   // 야간 중 터치 wake 후 재off 대기(10s)
void lvgl_set_night_active(bool a) { night_active = a; }

void disable_events(lv_obj_t* obj);  //화면보호시 이벤트 작동안하기
void enable_events(lv_obj_t* obj);   //기존 화면 복원시 이벤트
void activate_screen_protection();     // v2.7: fwd decl
void deactivate_screen_protection();   // v2.7: fwd decl (창 종료/터치 시 복원)

int getTouch(uint16_t *pPoints)
{
  TOUCHINFO ti;
  if (ct.getSamples(&ti) != FT_SUCCESS)
     return 0; // something went wrong
  
  if (pPoints) {
    // swap X/Y since the display is used 90 degrees rotated
    pPoints[0] = ti.x[0];
    pPoints[1] = ti.y[0]; 
    pPoints[2] = ti.x[1];
    pPoints[3] = ti.y[1];
  }
  
  return ti.count;
} /* getTouch() */

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    //uint16_t touchX = 0, touchY = 0;

    uint16_t points[4];
    //int i;
    bool touched =  getTouch(points);//tft.getTouch( &touchX, &touchY, 600 );

    if( !touched )
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        // 화면보호기(무활동) 또는 야간 화면 끄기 중 터치 → wake
        if(sleep_using || night_active){
            last_touch_time = millis(); // 터치 이벤트 시 시간 갱신
            if (screen_protected)
            {
                deactivate_screen_protection();  // 화면 복원 + 백라이트 on
                return;                           // 이 터치는 wake 용으로 소비
            }
        }

        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = points[0];
        data->point.y = points[1];

        //Serial.print( "Data x " );
        //Serial.println( points[0] );

        //Serial.print( "Data y " );
        //Serial.println( points[1] );
    }
}

void activate_screen_protection()
{
    if (!screen_protected)
    {
        screen_protected = true;

        // 현재 화면 저장
        last_screen = lv_scr_act();

        // 블랙 화면 생성
        lv_obj_t *black_screen = lv_obj_create(NULL); // 새로운 화면 생성
        lv_obj_set_style_bg_color(black_screen, lv_color_black(), LV_PART_MAIN); // 검정색 배경
        lv_scr_load(black_screen); // 블랙 화면 로드
        digitalWrite(TFT_BACKLIGHT_ON, LOW); // 백라이트 끄기(off)
    }
}

// v2.7: 화면 보호 해제 (복원 + 백라이트 on) — 터치 wake / 야간 창 종료 시 호출
void deactivate_screen_protection()
{
    if (screen_protected)
    {
        screen_protected = false;
        if (last_screen)
        {
            lv_scr_load(last_screen); // 저장된 마지막 화면 복원
            last_screen = NULL;
        }
        digitalWrite(TFT_BACKLIGHT_ON, HIGH); // 백라이트 on
    }
}

void lvgl_touch_init(uint16_t screenWidth, uint16_t screenHeight)
{
    lv_init();

    tft.begin();
    tft.setRotation(180);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the touchpad*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Touch initialization
    ct.init(-1, -1, false, 400000); // Use actual I2C pins
    delay(100);
}

// v2.6: 웹 설정 모드 정적 안내화면 — TFT 만 init 후 1회 렌더. LVGL/터치/ui 미사용.
//   웹모드 loop 에는 lv_timer_handler 가 없어 이후 tft 무접근 → 웹서버 서비스 중 SPI 경합 없음.
void lcd_show_webmode_info(const char* ip)
{
    tft.begin();
    tft.setRotation(180);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(8, 24);
    tft.println("WEB CONFIG");
    tft.setCursor(8, 48);
    tft.println("MODE");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(8, 96);
    tft.print("URL : http://");
    tft.println(ip);
    tft.setCursor(8, 116);
    tft.println("Auth: admin / 12345");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(8, 152);
    tft.println("Reboot from web to");
    tft.setCursor(8, 168);
    tft.println("return to LCD mode.");
}

void screen_saver_init(int timeout){
    
    if(timeout != 0)
    {   
        sleep_using = true;
        screen_timeout = timeout * 60 * 1000;
    }

    if(sleep_using) {
        last_touch_time = millis(); // 초기화 시 마지막 터치 시간 기록
    }        
}

void lvgl_loop()
{
    lv_timer_handler(); // LVGL 작업 처리
    delay(5);

    // 화면 off 조건: 스크린세이버(무활동) 또는 야간 화면 끄기(시간대). 둘 다 아니면 복원.
    bool wantOff = false;
    if (sleep_using  && (millis() - (uint32_t)last_touch_time > (uint32_t)screen_timeout)) wantOff = true;
    if (night_active && (millis() - (uint32_t)last_touch_time > NIGHT_WAKE_MS))            wantOff = true;
    if (wantOff) {
        if (!screen_protected) activate_screen_protection();
    } else {
        if (screen_protected) deactivate_screen_protection();  // 야간 창 종료/무활동 해제 시 복원
    }
}
