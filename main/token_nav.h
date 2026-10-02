#pragma once
#include <stdbool.h>

typedef enum { TOKEN_KEY_UP, TOKEN_KEY_DOWN, TOKEN_KEY_OK } token_nav_key_t;
typedef enum { TOKEN_KEY_PRESS, TOKEN_KEY_CLICK, TOKEN_KEY_DOUBLE, TOKEN_KEY_LONG } token_nav_event_t;
typedef struct {
    int source; /* -1 = all products, otherwise TOKEN_SOURCES index */
    bool sync_page;
    bool older;
    int wake_key;
} token_nav_t;

void token_nav_init(token_nav_t *nav);
/* Returns true only when a physical long OK requests a pairing window. */
bool token_nav_handle(token_nav_t *nav, token_nav_key_t key, token_nav_event_t event,
                      bool sleeping, bool passkey_visible);
