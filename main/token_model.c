#include "token_model.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static uint16_t u16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return u16(p) | ((uint32_t)u16(p + 2) << 16); }
static uint64_t u64(const uint8_t *p) { return u32(p) | ((uint64_t)u32(p + 4) << 32); }
uint32_t token_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
    }
    return ~crc;
}
bool token_decode(const uint8_t *p, size_t size, token_snapshot_t *out)
{
    if (!p || !out || size != TOKEN_PACKET_SIZE || memcmp(p, "TKD1", 4) ||
        token_crc32(p, size - 4) != u32(p + size - 4)) return false;
    uint32_t updated = u32(p + 8), day = u32(p + 12);
    if (day < TOKEN_DAYS || day != (updated + (uint64_t)28800) / 86400) return false;
    /* Validate without touching the destination: malformed frames never replace
     * the last good snapshot or its persisted copy. */
    uint64_t all = 0;
    for (unsigned t = 0; t < TOKEN_SOURCES; ++t) {
        const uint8_t *meta = p + 32 + t * 21;
        uint64_t total = u64(meta), sum = 0, peak = u64(meta + 8);
        if (meta[20] > TOKEN_ERROR || u16(meta + 16) > day + 1 ||
            u16(meta + 18) > u16(meta + 16) || peak > total || UINT64_MAX - all < total)
            return false;
        all += total;
        for (unsigned d = 0; d < TOKEN_DAYS; ++d) {
            uint32_t n = u32(p + 32 + TOKEN_SOURCES * 21 + (t * TOKEN_DAYS + d) * 4);
            if (n > peak) return false;
            sum += n;
        }
        if (sum > total) return false;
    }
    if (u64(p + 20) > all || u16(p + 30) > u16(p + 28)) return false;
    out->sequence = u32(p + 4); out->updated = updated;
    out->end_day = day; out->longest_task = u32(p + 16);
    out->peak = u64(p + 20); out->longest = u16(p + 28); out->current = u16(p + 30);
    for (unsigned t = 0; t < TOKEN_SOURCES; ++t) {
        const uint8_t *meta = p + 32 + t * 21;
        token_source_t *s = &out->sources[t];
        s->total = u64(meta); s->peak = u64(meta + 8);
        s->longest = u16(meta + 16); s->current = u16(meta + 18); s->status = meta[20];
        for (unsigned d = 0; d < TOKEN_DAYS; ++d)
            s->days[d] = u32(p + 32 + TOKEN_SOURCES * 21 + (t * TOKEN_DAYS + d) * 4);
    }
    return true;
}
int token_receive(token_receiver_t *rx, const uint8_t *p, size_t size)
{
    if (!rx || !p || size <= 6) return -1;
    uint32_t seq = u32(p); size_t off = u16(p + 4), len = size - 6;
    if (off > TOKEN_PACKET_SIZE || len > TOKEN_PACKET_SIZE - off) return -1;
    if (rx->active && rx->sequence == seq && off < rx->received) {
        return off + len <= rx->received && !memcmp(rx->bytes + off, p + 6, len) ? 0 : -1;
    }
    if (off == 0) { rx->sequence = seq; rx->received = 0; rx->active = true; }
    if (!rx->active || seq != rx->sequence || off != rx->received) return -1;
    memcpy(rx->bytes + off, p + 6, len); rx->received += len;
    if (rx->received == TOKEN_PACKET_SIZE) {
        rx->active = false;
        if (memcmp(rx->bytes, "TKD1", 4) || u32(rx->bytes + 4) != seq) return -1;
        return 1;
    }
    return 0;
}
uint64_t token_total(const token_snapshot_t *s, int source)
{
    if (source >= 0 && source < TOKEN_SOURCES) return s->sources[source].total;
    uint64_t n = 0;
    for (unsigned t = 0; t < TOKEN_SOURCES; ++t) n += s->sources[t].total;
    return n;
}
uint64_t token_day(const token_snapshot_t *s, int source, unsigned day)
{
    if (day >= TOKEN_DAYS) return 0;
    if (source >= 0 && source < TOKEN_SOURCES) return s->sources[source].days[day];
    uint64_t n = 0;
    for (unsigned t = 0; t < TOKEN_SOURCES; ++t) n += s->sources[t].days[day];
    return n;
}
unsigned token_heat_level(uint64_t n, uint64_t peak)
{
    if (!n) return 0;
    if (n >= peak) return 4;
    /* Division avoids overflowing n * 4. */
    if (n <= peak / 8) return 1;
    if (n <= peak / 3) return 2;
    if (n <= peak - peak / 3) return 3;
    return 4;
}
void token_format(uint64_t n, char *out, size_t size)
{
    if (n >= 1000000000000ULL) snprintf(out, size, "%.1e", (double)n);
    else if (n >= 100000000ULL) snprintf(out, size, "%.1f亿", (double)n / 100000000);
    else if (n >= 10000) snprintf(out, size, "%.1f万", (double)n / 10000);
    else snprintf(out, size, "%" PRIu64, n);
}
