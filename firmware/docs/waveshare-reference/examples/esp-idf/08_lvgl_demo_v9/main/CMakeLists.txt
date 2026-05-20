set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

idf_component_register(
    SRCS main.c ${LV_DEMOS_SOURCES}
    INCLUDE_DIRS . ${LV_DEMO_DIR})

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(${LVGL_LIB} PRIVATE
    -DLV_LVGL_H_INCLUDE_SIMPLE
    -DLV_USE_DEMO_MUSIC
)

# GCC 15 (IDF v6) errors on conflicting section attrs when LVGL sets
# LV_ATTRIBUTE_FAST_MEM=IRAM_ATTR, because IRAM_ATTR's __COUNTER__ gives the
# header decl and the .c def different .iram1.N sections. Override with a
# stable section name; paired with CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=n.
target_compile_definitions(${LVGL_LIB} PUBLIC
    "LV_ATTRIBUTE_FAST_MEM=__attribute__((section(\".iram1.lvgl_fast\")))"
)
