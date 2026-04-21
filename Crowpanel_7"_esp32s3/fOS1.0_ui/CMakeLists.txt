SET(SOURCES ui_comp_button1.c
    ui_comp_switch.c
    ui_comp.c
    ui_ScreenHome.c
    ui_ScreenSettings.c
    ui_NoFunktion.c
    ui_ScreenFileManager.c
    ui_ScreenWeb.c
    ui_ScreenText.c
    ui_ScreenRadio.c
    ui_ScreenWeather.c
    ui_ScreenClock.c
    ui_ScreenCalculator.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    ui_img_350490791.c
    ui_img_folder_png.c
    ui_img_2114985472.c
    ui_img_1781796769.c
    ui_img_1607212695.c
    ui_img_1045517857.c
    ui_img_294430296.c
    ui_img_1548352994.c
    ui_img_fos_256x256_png.c)

add_library(ui ${SOURCES})
