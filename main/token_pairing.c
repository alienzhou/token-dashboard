#include "token_pairing.h"
void token_pairing_reset(token_pairing_t *p)
{
    *p = (token_pairing_t){.code = TOKEN_NO_PASSKEY};
}
void token_pairing_open(token_pairing_t *p, int64_t now, unsigned random)
{
    *p = (token_pairing_t){.until = now + 60000000, .code = random % 1000000};
}
bool token_pairing_open_at(const token_pairing_t *p, int64_t now) { return now < p->until; }
unsigned token_pairing_visible(const token_pairing_t *p, int64_t now)
{
    return token_pairing_open_at(p, now) || p->authenticating ? p->code : TOKEN_NO_PASSKEY;
}
unsigned token_pairing_start(token_pairing_t *p, int64_t now, bool display_action)
{
    if (!display_action || !token_pairing_open_at(p, now)) return TOKEN_NO_PASSKEY;
    p->authenticating = true;
    return p->code;
}
void token_pairing_disconnect(token_pairing_t *p) { p->authenticating = false; }
void token_pairing_authenticated(token_pairing_t *p) { token_pairing_reset(p); }
bool token_pairing_allow_connection(bool window_open, bool valid_bond) { return window_open || valid_bond; }
