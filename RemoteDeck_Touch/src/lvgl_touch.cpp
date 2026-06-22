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

void disable_events(lv_obj_t* obj);  //화면보호시 이벤트 작동안하기
void enable_events(lv_obj_t* obj);   //기존 화면 복원시 이벤트

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
        //화면보호기 사용하면 실행
        if(sleep_using){            
            //화면보호기 부분 추가
            last_touch_time = millis(); // 터치 이벤트 시 시간 갱신
            
            if (screen_protected)
            {
                // 화면 보호 해제
                screen_protected = false;
                if (last_screen)
                {
                    lv_scr_load(last_screen); // 저장된 마지막 화면 복원
                    last_screen = NULL;      // 복원 후 마지막 화면 참조 제거
                    digitalWrite(TFT_BACKLIGHT_ON, HIGH); // 백라이트 끄기
                    return;
                }
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
        digitalWrite(TFT_BACKLIGHT_ON, LOW); // 백라이트 끄기
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

    // 화면 보호 모드 활성화 확인
    if(sleep_using)
    {
        if (!screen_protected && (millis() - last_touch_time > screen_timeout))
        {
            activate_screen_protection();
        }
    }
}
