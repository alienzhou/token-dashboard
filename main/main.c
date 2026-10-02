#include "token_model.h"
#include "token_ui.h"
#include "token_ble.h"
#include "token_nav.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
static const char *TAG = "token_dashboard";
static token_snapshot_t s_snapshot, s_candidate;
static uint8_t s_packet[TOKEN_PACKET_SIZE], s_checkpoint[TOKEN_PACKET_SIZE];
static QueueHandle_t s_keys;
typedef struct { token_nav_key_t button; token_nav_event_t event; } token_key_t;
static void button_cb(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    token_key_t key;
    switch (btn) {
    case BSP_BTN_UP: key.button = TOKEN_KEY_UP; break;
    case BSP_BTN_DOWN: key.button = TOKEN_KEY_DOWN; break;
    case BSP_BTN_OK: key.button = TOKEN_KEY_OK; break;
    default: return;
    }
    switch (ev) {
    case BSP_BTN_PRESS: key.event = TOKEN_KEY_PRESS; break;
    case BSP_BTN_CLICK: key.event = TOKEN_KEY_CLICK; break;
    case BSP_BTN_DOUBLE: key.event = TOKEN_KEY_DOUBLE; break;
    case BSP_BTN_LONG: key.event = TOKEN_KEY_LONG; break;
    default: return;
    }
    xQueueSend(s_keys, &key, 0);
}
void app_main(void)
{
    /* NVS errors never trigger automatic erasure of user bonds. */
    ESP_ERROR_CHECK(nvs_flash_init());
    nvs_handle_t store;
    ESP_ERROR_CHECK(nvs_open("token_usage", NVS_READWRITE, &store));
    size_t size = sizeof(s_packet);
    bool stored = nvs_get_blob(store, "snapshot", s_packet, &size) == ESP_OK && token_decode(s_packet, size, &s_snapshot);
    ESP_ERROR_CHECK(bsp_display_init());
    if (!bsp_lvgl_init()) { ESP_LOGE(TAG, "LVGL unavailable"); return; }
    esp_err_t gauge = bsp_battery_init();
    if (gauge != ESP_OK) ESP_LOGW(TAG, "battery gauge unavailable");
    s_keys = xQueueCreate(8, sizeof(token_key_t));
    if (!s_keys) { ESP_LOGE(TAG, "key queue unavailable"); return; }
    esp_err_t buttons = bsp_button_init(button_cb, NULL);
    if (buttons != ESP_OK) ESP_LOGW(TAG, "buttons unavailable: %s", esp_err_to_name(buttons));
    if (!bsp_lvgl_lock(1000)) return;
    bool created = token_ui_create(); bsp_lvgl_unlock();
    if (!created) return;
    esp_err_t radio = token_ble_start();
    if (radio != ESP_OK) ESP_LOGE(TAG, "BLE unavailable: %s", esp_err_to_name(radio));
    token_nav_t nav; token_nav_init(&nav);
    int battery = bsp_battery_soc(), last_link = -1;
    bool dirty = true, storage_ok = true, last_pair = false;
    bool save_pending = false;
    int64_t last_save = 0;
    unsigned last_code = UINT32_MAX;
    int64_t last_key = esp_timer_get_time(), last_battery = last_key;
    uint8_t brightness = 75;
    bsp_display_backlight(brightness);
    ESP_LOGI(TAG, "Token Dashboard started; snapshot=%s; heap=%u largest=%u", stored ? "cached" : "empty",
             (unsigned)esp_get_free_heap_size(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    while (true) {
        int64_t now = esp_timer_get_time();
        token_key_t key;
        if (xQueueReceive(s_keys, &key, pdMS_TO_TICKS(100))) {
            bool waking = brightness == 0;
            last_key = now; dirty = true;
            int before = nav.source;
            if (token_nav_handle(&nav, key.button, key.event, waking, token_ble_passkey() != UINT32_MAX))
                token_ble_pair_window();
            if (nav.source != before) ESP_LOGI(TAG, "selected product=%d", nav.source);
        }
        if (token_ble_take_packet(s_packet)) {
            bool valid = token_decode(s_packet, sizeof(s_packet), &s_candidate) && s_candidate.updated >= s_snapshot.updated;
            if (valid) {
                /* NVS checkpoints are rate limited. Receipts acknowledge RAM
                 * acceptance, not a synchronous durable write. */
                save_pending = save_pending || memcmp(&s_snapshot.sources, &s_candidate.sources, sizeof(s_snapshot.sources)) || s_snapshot.end_day != s_candidate.end_day || s_snapshot.longest_task != s_candidate.longest_task;
                s_snapshot = s_candidate;
                memcpy(s_checkpoint, s_packet, sizeof(s_checkpoint));
                token_ble_receipt(s_candidate.sequence, 0);
            } else {
                uint32_t seq = (uint32_t)s_packet[4] | ((uint32_t)s_packet[5] << 8) | ((uint32_t)s_packet[6] << 16) | ((uint32_t)s_packet[7] << 24);
                token_ble_receipt(seq, 1);
            }
            dirty = true;
        }
        if (save_pending && (!last_save || now - last_save >= 60000000)) {
            storage_ok = nvs_set_blob(store, "snapshot", s_checkpoint, sizeof(s_checkpoint)) == ESP_OK && nvs_commit(store) == ESP_OK;
            last_save = now;
            if (storage_ok) save_pending = false;
            dirty = true;
        }
        int link = radio == ESP_OK ? token_ble_link() : 0;
        unsigned code = token_ble_passkey(); bool pair = token_ble_pairing();
        if (last_code != UINT32_MAX && code == UINT32_MAX && link == 2) nav.sync_page = false;
        if (link != last_link || code != last_code || pair != last_pair) dirty = true;
        if (now - last_battery >= 20000000) { battery = bsp_battery_soc(); last_battery = now; dirty = true; }
        uint8_t target = pair || code != UINT32_MAX || now - last_key < 60000000 ? 75 : now - last_key < 300000000 ? 12 : 0;
        if (target != brightness) { bsp_display_backlight(target); brightness = target; }
        if (dirty && bsp_lvgl_lock(1000)) {
            token_ui_update(&s_snapshot, nav.source, nav.older, nav.sync_page, battery, link, code, pair, storage_ok);
            bsp_lvgl_unlock(); dirty = false;
            last_link = link; last_code = code; last_pair = pair;
        }
    }
}
