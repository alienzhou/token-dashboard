#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TOKEN_SOURCES 5
#define TOKEN_DAYS 364
#define TOKEN_PACKET_SIZE (32 + TOKEN_SOURCES * 21 + TOKEN_SOURCES * TOKEN_DAYS * 4 + 4)
enum { TOKEN_MISSING, TOKEN_LIVE, TOKEN_UNSUPPORTED, TOKEN_ERROR };
typedef struct {
    uint64_t total, peak;
    uint16_t longest, current;
    uint8_t status;
    uint32_t days[TOKEN_DAYS]; /* oldest first; end_day is the final day */
} token_source_t;
typedef struct {
    uint32_t sequence, updated, end_day, longest_task;
    uint64_t peak;
    uint16_t longest, current;
    token_source_t sources[TOKEN_SOURCES];
} token_snapshot_t;
typedef struct {
    uint8_t bytes[TOKEN_PACKET_SIZE];
    size_t received;
    uint32_t sequence;
    bool active;
} token_receiver_t;
uint32_t token_crc32(const uint8_t *data, size_t size);
bool token_decode(const uint8_t *bytes, size_t size, token_snapshot_t *out);
/* Fragment: sequence LE32 + offset LE16 + payload. 1 = complete, 0 = partial,
 * -1 = rejected. Exact retransmissions are idempotent. Decode before applying. */
int token_receive(token_receiver_t *rx, const uint8_t *chunk, size_t size);
uint64_t token_total(const token_snapshot_t *s, int source);
uint64_t token_day(const token_snapshot_t *s, int source, unsigned day);
unsigned token_heat_level(uint64_t value, uint64_t peak);
void token_format(uint64_t value, char *out, size_t size);
