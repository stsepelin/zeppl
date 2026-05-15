idf_component_register(
    SRCS
        main.c
        vehicle/vehicle_data.c
        simulator/sim_engine.c
        display/boot_screen.c
        display/screen_ride.c
        display/ui_manager.c
        display/widgets/tach_arc.c
        display/widgets/speed_display.c
        display/widgets/gear_indicator.c
        display/widgets/fuel_bar.c
        display/widgets/turn_signals.c
        display/widgets/temp_display.c
        display/widgets/warning_lights.c
        display/widgets/clock_display.c
        display/widgets/odometer_display.c
        display/fonts/jbm_bold_26.c
        display/fonts/jbm_bold_33.c
        display/fonts/jbm_bold_45.c
        display/fonts/jbm_bold_72.c
        display/fonts/jbm_bold_144.c
        display/fonts/mdi_36.c
        display/fonts/mdi_50.c
        display/fonts/mdi_60.c
        display/fonts/mdi_96.c
    INCLUDE_DIRS
        .
        vehicle
        simulator
        display
        display/widgets
        display/fonts
    REQUIRES waveshare__esp32_p4_wifi6_touch_lcd_xc lvgl__lvgl
)

# Generated font files (from lv_font_conv) use `#ifdef LV_LVGL_H_INCLUDE_SIMPLE`
# to pick between `#include "lvgl.h"` and `#include "lvgl/lvgl.h"`. We want the
# simple form so the font .c files compile inside the main component.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

# GCC 15 (IDF v6) rejects LVGL's LV_ATTRIBUTE_FAST_MEM=IRAM_ATTR because
# IRAM_ATTR's __COUNTER__ puts the header decl and the .c def in different
# .iram1.N sections. Override with a stable name; paired with
# CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=n in sdkconfig.defaults.
idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_definitions(${LVGL_LIB} PUBLIC
    "LV_ATTRIBUTE_FAST_MEM=__attribute__((section(\".iram1.lvgl_fast\")))"
)
