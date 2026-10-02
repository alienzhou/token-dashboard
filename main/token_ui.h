#pragma once
#include "token_model.h"
#include <stdbool.h>
bool token_ui_create(void);
void token_ui_update(const token_snapshot_t *s, int source, bool older, bool sync_page,
                     int battery, int link, unsigned passkey, bool pairing, bool storage_ok);
