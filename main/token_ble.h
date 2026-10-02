#pragma once
#include "esp_err.h"
#include "token_model.h"
#include <stdbool.h>
esp_err_t token_ble_start(void);
void token_ble_pair_window(void);
int token_ble_link(void);
unsigned token_ble_passkey(void);
bool token_ble_pairing(void);
bool token_ble_take_packet(uint8_t *out);
void token_ble_receipt(uint32_t sequence, uint8_t status);
