# IDF v6 split `driver` into per-peripheral components; declare each one used.
idf_component_register(
    SRCS "esp32_p4_wifi6_touch_lcd_xc.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_driver_i2s esp_driver_sdmmc
    PRIV_REQUIRES esp_lcd usb spiffs fatfs
)
