cmake_minimum_required(VERSION 3.16)
project(token_ui_host C)
set(CMAKE_C_STANDARD 11)
if(NOT EXISTS "${LVGL_PATH}/lvgl.h")
    message(FATAL_ERROR "LVGL_PATH must point to the pinned LVGL")
endif()
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_PATH}/src/*.c")
add_library(token_lvgl STATIC ${LVGL_SOURCES})
target_include_directories(token_lvgl PUBLIC "${LVGL_PATH}")
target_compile_definitions(token_lvgl PUBLIC LV_CONF_PATH="${CMAKE_CURRENT_LIST_DIR}/lv_conf.h")
add_executable(test_token_ui ../test_token_ui.c ../../main/token_ui.c ../../main/token_model.c ../../main/token_nav.c
    ../../components/bsp/src/bsp_display_rounding.c
    ../../assets/fonts/token_font_12.c ../../assets/fonts/token_font_16.c
    ../../assets/fonts/token_font_20.c ../../assets/fonts/token_font_36.c)
target_include_directories(test_token_ui PRIVATE ../../main ../../assets/fonts ../bsp_stubs
    ../../components/bsp/include ../../components/bsp/src)
target_link_libraries(test_token_ui PRIVATE token_lvgl)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_token_ui PRIVATE m)
endif()
target_compile_options(test_token_ui PRIVATE -Wall -Wextra -Werror -UNDEBUG)
