#include "token_nav.h"
#include "token_model.h"
#include <assert.h>
#include <stdio.h>

static void press(token_nav_t *n, token_nav_key_t key)
{
    assert(!token_nav_handle(n, key, TOKEN_KEY_PRESS, false, false));
}
int main(void)
{
    token_nav_t n; token_nav_init(&n);
    assert(n.source == -1 && !n.sync_page && !n.older);
    /* Rapid presses must visit every product even if the BSP later emits
     * DOUBLE instead of CLICK. Neither event counts a navigation press twice. */
    for (int round = 0; round < 3; ++round) {
        for (int t = 0; t < TOKEN_SOURCES; ++t) {
            press(&n, TOKEN_KEY_DOWN); assert(n.source == t);
            token_nav_handle(&n, TOKEN_KEY_DOWN, TOKEN_KEY_DOUBLE, false, false);
            token_nav_handle(&n, TOKEN_KEY_DOWN, TOKEN_KEY_CLICK, false, false);
            token_nav_handle(&n, TOKEN_KEY_DOWN, TOKEN_KEY_LONG, false, false);
            assert(n.source == t && !n.older);
        }
        press(&n, TOKEN_KEY_DOWN); assert(n.source == -1);
    }
    for (int t = TOKEN_SOURCES - 1; t >= -1; --t) {
        press(&n, TOKEN_KEY_UP); assert(n.source == t);
    }
    token_nav_handle(&n, TOKEN_KEY_OK, TOKEN_KEY_CLICK, false, false);
    assert(n.sync_page);
    press(&n, TOKEN_KEY_DOWN); assert(n.source == 0 && !n.sync_page);
    token_nav_handle(&n, TOKEN_KEY_OK, TOKEN_KEY_DOUBLE, false, false);
    assert(n.older && !n.sync_page && n.source == 0);
    assert(token_nav_handle(&n, TOKEN_KEY_OK, TOKEN_KEY_LONG, false, false));
    assert(n.sync_page);
    press(&n, TOKEN_KEY_UP); assert(n.source == -1 && !n.sync_page);
    /* No product/page gesture may hide an active passkey. */
    token_nav_t before = n;
    token_nav_handle(&n, TOKEN_KEY_DOWN, TOKEN_KEY_PRESS, false, true);
    assert(n.source == before.source);
    assert(!token_nav_handle(&n, TOKEN_KEY_OK, TOKEN_KEY_LONG, false, true));
    /* Wake consumes all subsequent events of that physical gesture. */
    for (int key = TOKEN_KEY_UP; key <= TOKEN_KEY_OK; ++key) {
        token_nav_init(&n);
        token_nav_handle(&n, key, TOKEN_KEY_PRESS, true, false);
        for (int ev = TOKEN_KEY_CLICK; ev <= TOKEN_KEY_LONG; ++ev) {
            assert(!token_nav_handle(&n, key, ev, false, false));
            assert(n.source == -1 && !n.sync_page && !n.older);
        }
        press(&n, TOKEN_KEY_DOWN); assert(n.source == 0);
    }
    puts("Token navigation: PASS (all products, rapid clicks, sync page, pairing and wake gestures)");
    return 0;
}
