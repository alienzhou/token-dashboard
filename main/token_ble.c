#include "token_ble.h"
#include "token_pairing.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
void ble_store_config_init(void);
static const char *TAG = "token_ble";
static const ble_uuid128_t s_service = BLE_UUID128_INIT(0x01,0x10,0xbb,0x9c,0x24,0xb0,0x49,0xa7,0x65,0x4d,0x43,0x7a,0x01,0x00,0xd0,0xf2);
static const ble_uuid128_t s_rx_uuid = BLE_UUID128_INIT(0x01,0x10,0xbb,0x9c,0x24,0xb0,0x49,0xa7,0x65,0x4d,0x43,0x7a,0x02,0x00,0xd0,0xf2);
static const ble_uuid128_t s_ack_uuid = BLE_UUID128_INIT(0x01,0x10,0xbb,0x9c,0x24,0xb0,0x49,0xa7,0x65,0x4d,0x43,0x7a,0x03,0x00,0xd0,0xf2);
static atomic_int s_link;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static token_pairing_t s_pair = {.code = TOKEN_NO_PASSKEY};
static uint8_t s_addr_type;
static char s_name[24];
static token_receiver_t s_receiver;
static uint8_t s_pending[TOKEN_PACKET_SIZE], s_ack[5] = {0,0,0,0,255};
static bool s_ready;

bool token_ble_pairing(void)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_mux); bool open = token_pairing_open_at(&s_pair, now); portEXIT_CRITICAL(&s_mux);
    return open;
}
void token_ble_pair_window(void)
{
    int64_t now = esp_timer_get_time(); unsigned random = esp_random();
    portENTER_CRITICAL(&s_mux); token_pairing_open(&s_pair, now, random); portEXIT_CRITICAL(&s_mux);
    ESP_LOGI(TAG, "Physical pairing window opened; screen code ready");
}
int token_ble_link(void) { return atomic_load(&s_link); }
unsigned token_ble_passkey(void)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_mux); unsigned code = token_pairing_visible(&s_pair, now); portEXIT_CRITICAL(&s_mux);
    return code;
}
void token_ble_receipt(uint32_t seq, uint8_t status)
{
    portENTER_CRITICAL(&s_mux);
    for (unsigned i = 0; i < 4; ++i) s_ack[i] = seq >> (i * 8);
    s_ack[4] = status;
    portEXIT_CRITICAL(&s_mux);
}
bool token_ble_take_packet(uint8_t *out)
{
    /* Ready locks the producer out until the one consumer finishes copying. */
    portENTER_CRITICAL(&s_mux); bool ready = s_ready; portEXIT_CRITICAL(&s_mux);
    if (!ready) return false;
    memcpy(out, s_pending, TOKEN_PACKET_SIZE);
    portENTER_CRITICAL(&s_mux); s_ready = false; portEXIT_CRITICAL(&s_mux);
    return true;
}
static int access_cb(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctx, void *arg)
{
    (void)attr;
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(conn, &desc) || !desc.sec_state.encrypted || !desc.sec_state.authenticated || desc.sec_state.key_size != 16)
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    if (arg) {
        uint8_t ack[5];
        portENTER_CRITICAL(&s_mux); memcpy(ack, s_ack, 5); portEXIT_CRITICAL(&s_mux);
        return os_mbuf_append(ctx->om, ack, 5) ? BLE_ATT_ERR_INSUFFICIENT_RES : 0;
    }
    uint16_t len = OS_MBUF_PKTLEN(ctx->om);
    uint8_t chunk[256];
    if (len > sizeof(chunk) || len <= 6) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    portENTER_CRITICAL(&s_mux); bool busy = s_ready; portEXIT_CRITICAL(&s_mux);
    if (busy) return BLE_ATT_ERR_INSUFFICIENT_RES;
    if (ble_hs_mbuf_to_flat(ctx->om, chunk, sizeof(chunk), &len)) return BLE_ATT_ERR_UNLIKELY;
    int result = token_receive(&s_receiver, chunk, len);
    if (result < 0) return BLE_ATT_ERR_INVALID_OFFSET;
    if (result == 1) {
        memcpy(s_pending, s_receiver.bytes, TOKEN_PACKET_SIZE);
        portENTER_CRITICAL(&s_mux); s_ready = true; portEXIT_CRITICAL(&s_mux);
    }
    return 0;
}
static const struct ble_gatt_svc_def s_services[] = {{
    .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &s_service.u,
    .characteristics = (struct ble_gatt_chr_def[]) {
        {.uuid = &s_rx_uuid.u, .access_cb = access_cb,
         .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN},
        {.uuid = &s_ack_uuid.u, .access_cb = access_cb, .arg = (void *)1,
         .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_READ_AUTHEN},
        {0}
    }
}, {0}};
static int gap_cb(struct ble_gap_event *e, void *arg);
static bool bonded_peer(uint16_t handle)
{
    /* Match NimBLE ble_sm_read_bond's identity-address lookup; the stack has
     * already resolved a known rotating peer address in this descriptor. */
    struct ble_gap_conn_desc desc;
    struct ble_store_key_sec key = {0};
    struct ble_store_value_sec bond;
    if (ble_gap_conn_find(handle, &desc)) return false;
    key.peer_addr = desc.peer_id_addr;
    return ble_store_read_peer_sec(&key, &bond) == 0 && bond.ltk_present &&
           bond.authenticated && bond.sc && bond.key_size == 16;
}
static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&s_service; fields.num_uuids128 = 1; fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc) { ESP_LOGE(TAG, "advertising fields: %d", rc); return; }
    struct ble_hs_adv_fields scan = {0};
    scan.name = (const uint8_t *)s_name; scan.name_len = strlen(s_name); scan.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&scan);
    if (rc) { ESP_LOGE(TAG, "scan response: %d", rc); return; }
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND; params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = 800; params.itvl_max = 1200;
    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params, gap_cb, NULL);
    if (rc) ESP_LOGE(TAG, "advertising start: %d", rc);
}
static int gap_cb(struct ble_gap_event *e, void *arg)
{
    (void)arg;
    switch (e->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (e->connect.status) { advertise(); break; }
        atomic_store(&s_link, 1);
        memset(&s_receiver, 0, sizeof(s_receiver));
        /* Reject unknown peers before initiating a security exchange, avoiding
         * an OS password prompt when the device has no physical pairing window. */
        if (!token_pairing_allow_connection(token_ble_pairing(), bonded_peer(e->connect.conn_handle))) {
            ESP_LOGI(TAG, "New peer rejected outside physical pairing window");
            ble_gap_terminate(e->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            break;
        }
        {
            int rc = ble_gap_security_initiate(e->connect.conn_handle);
            if (rc && rc != BLE_HS_EALREADY) {
                ESP_LOGW(TAG, "security initiation: %d", rc);
                ble_gap_terminate(e->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        atomic_store(&s_link, 0);
        portENTER_CRITICAL(&s_mux); token_pairing_disconnect(&s_pair); portEXIT_CRITICAL(&s_mux);
        memset(&s_receiver, 0, sizeof(s_receiver)); advertise(); break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc desc;
        if (!e->enc_change.status && !ble_gap_conn_find(e->enc_change.conn_handle, &desc) &&
            desc.sec_state.encrypted && desc.sec_state.authenticated && desc.sec_state.key_size == 16) {
            atomic_store(&s_link, 2);
            portENTER_CRITICAL(&s_mux); token_pairing_authenticated(&s_pair); portEXIT_CRITICAL(&s_mux);
            ESP_LOGI(TAG, "Authenticated encrypted link established");
        } else ble_gap_terminate(e->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        break;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
            int64_t now = esp_timer_get_time();
            portENTER_CRITICAL(&s_mux);
            unsigned code = token_pairing_start(&s_pair, now, e->passkey.params.action == BLE_SM_IOACT_DISP);
            portEXIT_CRITICAL(&s_mux);
            ESP_LOGI(TAG, "Security IO action=%u; display allowed=%d", e->passkey.params.action, code != TOKEN_NO_PASSKEY);
            if (code == TOKEN_NO_PASSKEY) {
                ble_gap_terminate(e->passkey.conn_handle, BLE_ERR_REM_USER_CONN_TERM); break;
            }
            struct ble_sm_io io = {.action = BLE_SM_IOACT_DISP, .passkey = code};
            return ble_sm_inject_io(e->passkey.conn_handle, &io);
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        if (!token_ble_pairing()) return BLE_GAP_REPEAT_PAIRING_IGNORE;
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(e->repeat_pairing.conn_handle, &desc)) return BLE_GAP_REPEAT_PAIRING_IGNORE;
        return ble_store_util_delete_peer(&desc.peer_id_addr) == 0 ? BLE_GAP_REPEAT_PAIRING_RETRY : BLE_GAP_REPEAT_PAIRING_IGNORE;
    }
    case BLE_GAP_EVENT_ADV_COMPLETE: advertise(); break;
    default: break;
    }
    return 0;
}
static void sync_cb(void)
{
    if (ble_hs_util_ensure_addr(0) || ble_hs_id_infer_auto(0, &s_addr_type)) {
        ESP_LOGE(TAG, "BLE address initialization failed"); return;
    }
    advertise();
}
static void reset_cb(int reason)
{
    atomic_store(&s_link, 0);
    portENTER_CRITICAL(&s_mux); token_pairing_reset(&s_pair); portEXIT_CRITICAL(&s_mux);
    memset(&s_receiver, 0, sizeof(s_receiver)); ESP_LOGW(TAG, "host reset %d", reason);
}
static void host_task(void *arg)
{
    (void)arg; nimble_port_run(); nimble_port_freertos_deinit();
}
esp_err_t token_ble_start(void)
{
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) return err;
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_name, sizeof(s_name), "TokenPassport-%02X%02X", mac[4], mac[5]);
    ble_svc_gap_init(); ble_svc_gatt_init(); ble_svc_gap_device_name_set(s_name);
    ble_hs_cfg.sync_cb = sync_cb; ble_hs_cfg.reset_cb = reset_cb;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1; ble_hs_cfg.sm_mitm = 1; ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    ble_att_set_preferred_mtu(247);
    int rc = ble_gatts_count_cfg(s_services);
    if (!rc) rc = ble_gatts_add_svcs(s_services);
    if (rc) { nimble_port_deinit(); return ESP_FAIL; }
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "Authenticated BLE ready; long OK opens pairing");
    return ESP_OK;
}
