#include "token_nav.h"
#include "token_model.h"

void token_nav_init(token_nav_t *nav)
{
    *nav = (token_nav_t){.source = -1, .wake_key = -1};
}

bool token_nav_handle(token_nav_t *nav, token_nav_key_t key, token_nav_event_t event,
                      bool sleeping, bool passkey_visible)
{
    if (key < TOKEN_KEY_UP || key > TOKEN_KEY_OK ||
        event < TOKEN_KEY_PRESS || event > TOKEN_KEY_LONG) return false;
    if (event == TOKEN_KEY_PRESS) {
        nav->wake_key = sleeping ? (int)key : -1;
    }
    /* A wake gesture must not later toggle a page or open pairing when the
     * same press produces CLICK/LONG. A new PRESS starts a fresh gesture. */
    if (sleeping || nav->wake_key == (int)key || passkey_visible) return false;
    if (key != TOKEN_KEY_OK) {
        if (event == TOKEN_KEY_PRESS) {
            int slot = nav->source + 1;
            slot = (slot + (key == TOKEN_KEY_UP ? TOKEN_SOURCES : 1)) % (TOKEN_SOURCES + 1);
            nav->source = slot - 1;
            nav->sync_page = false;
        }
        return false; /* release/double/hold cannot count the press twice */
    }
    if (event == TOKEN_KEY_CLICK) nav->sync_page = !nav->sync_page;
    else if (event == TOKEN_KEY_DOUBLE) { nav->older = !nav->older; nav->sync_page = false; }
    else if (event == TOKEN_KEY_LONG) { nav->sync_page = true; return true; }
    return false;
}
