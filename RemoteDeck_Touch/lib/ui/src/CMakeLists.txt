SET(SOURCES screens/ui_ScreenLogo.c
    screens/ui_ScreenMain.c
    screens/ui_ScreenDevice.c
    screens/ui_ScreenEthernet.c
    screens/ui_ScreenWifi.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_logo_png.c
    images/ui_img_img01_png.c
    images/ui_img_img02_png.c
    images/ui_img_photo_png.c
    images/ui_img_name_png.c
    images/ui_img_title_png.c
    images/ui_img_in_png.c
    images/ui_img_out_png.c
    images/ui_img_ethernet_png.c
    images/ui_img_set_png.c
    images/ui_img_wifi2_png.c
    fonts/ui_font_nanumB15.c
    fonts/ui_font_nanumB20.c)

add_library(ui ${SOURCES})
