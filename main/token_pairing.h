#pragma once
#include <stdbool.h>
#include <stdint.h>
#define TOKEN_NO_PASSKEY UINT32_MAX
typedef struct {
    int64_t until;
    unsigned code;
    bool authenticating;
} token_pairing_t;
void token_pairing_reset(token_pairing_t *pair);
void token_pairing_open(token_pairing_t *pair, int64_t now, unsigned random);
bool token_pairing_open_at(const token_pairing_t *pair, int64_t now);
unsigned token_pairing_visible(const token_pairing_t *pair, int64_t now);
unsigned token_pairing_start(token_pairing_t *pair, int64_t now, bool display_action);
void token_pairing_disconnect(token_pairing_t *pair);
void token_pairing_authenticated(token_pairing_t *pair);
bool token_pairing_allow_connection(bool window_open, bool valid_bond);
