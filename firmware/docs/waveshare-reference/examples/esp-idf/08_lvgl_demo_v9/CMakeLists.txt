# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# bsp_extra (audio demo glue) pulls in chmorgan/esp-audio-player, which still
# uses the pre-IDF-v6 driver/i2s_std.h path. We don't need audio for this demo.
set(EXCLUDE_COMPONENTS
    bsp_extra
    chmorgan__esp-audio-player
    chmorgan__esp-file-iterator
    chmorgan__esp-libhelix-mp3
)

add_compile_options(-Wno-format)
project(lvgl_demo_v9)
