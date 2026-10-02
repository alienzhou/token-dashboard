#include "token_pairing.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    token_pairing_t p; token_pairing_reset(&p);
    assert(!token_pairing_open_at(&p, 100) && token_pairing_visible(&p, 100) == TOKEN_NO_PASSKEY);
    assert(!token_pairing_allow_connection(false, false));
    assert(token_pairing_allow_connection(false, true));
    assert(token_pairing_allow_connection(true, false));
    token_pairing_open(&p, 1000, 1000123);
    assert(token_pairing_visible(&p, 1000) == 123); /* visible before the OS asks */
    assert(token_pairing_start(&p, 1001, false) == TOKEN_NO_PASSKEY);
    assert(token_pairing_start(&p, 1002, true) == 123);
    token_pairing_disconnect(&p);
    assert(token_pairing_visible(&p, 1003) == 123); /* retries reuse the screen code */
    assert(token_pairing_start(&p, 1004, true) == 123);
    assert(!token_pairing_open_at(&p, 60001000));
    assert(token_pairing_visible(&p, 60001000) == 123); /* finish the pending exchange */
    token_pairing_disconnect(&p);
    assert(token_pairing_visible(&p, 60001000) == TOKEN_NO_PASSKEY);
    assert(token_pairing_start(&p, 60001000, true) == TOKEN_NO_PASSKEY);
    token_pairing_open(&p, 70000000, 0);
    assert(token_pairing_start(&p, 70000001, true) == 0); /* 000000 is valid */
    token_pairing_authenticated(&p);
    assert(!token_pairing_open_at(&p, 70000002));
    assert(token_pairing_visible(&p, 70000002) == TOKEN_NO_PASSKEY);
    puts("Token pairing: PASS (immediate code, closed-window rejection, bonds, retries, expiry and completion)");
    return 0;
}
