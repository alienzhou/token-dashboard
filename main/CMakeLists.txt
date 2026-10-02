idf_component_register(
    SRCS "main.c" "token_model.c" "token_ble.c" "token_ui.c" "token_nav.c" "token_pairing.c"
         "../assets/fonts/token_font_12.c" "../assets/fonts/token_font_16.c"
         "../assets/fonts/token_font_20.c" "../assets/fonts/token_font_36.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)
