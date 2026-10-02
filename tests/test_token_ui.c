#include "lvgl.h"
#include "token_ui.h"
#include "token_nav.h"
#include "token_characters.h"
#include "bsp_display.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
LV_FONT_DECLARE(token_font_12);
LV_FONT_DECLARE(token_font_16);
LV_FONT_DECLARE(token_font_20);
LV_FONT_DECLARE(token_font_36);
static uint16_t s_pixels[240 * 320];
static uint8_t s_draw[240 * 40 * 2];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    uint16_t *source = (uint16_t *)pixels;
    for (int y = area->y1; y <= area->y2; ++y) {
        int32_t left, right;
        bool visible = bsp_display_rounded_row_span(y, 240, 320, BSP_LVGL_SCREEN_RADIUS, &left, &right);
        for (int x = area->x1; x <= area->x2; ++x) {
            uint16_t color = *source++;
            s_pixels[y * 240 + x] = visible && x >= left && x <= right ? color : 0;
        }
    }
    lv_display_flush_ready(display);
}

static void snapshot(const char *directory, const char *name)
{
    lv_obj_update_layout(lv_screen_active());
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n240 320\n255\n");
    for (unsigned i = 0; i < sizeof(s_pixels)/sizeof(s_pixels[0]); ++i) {
        uint16_t value = s_pixels[i];
        uint8_t rgb[] = {(uint8_t)(((value >> 11) & 31) * 255 / 31),
                         (uint8_t)(((value >> 5) & 63) * 255 / 63),
                         (uint8_t)((value & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}

static void check_glyphs(const lv_font_t *font)
{
    for (unsigned i = 0; i < sizeof(TOKEN_CODEPOINTS)/sizeof(TOKEN_CODEPOINTS[0]); ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        uint32_t cp = TOKEN_CODEPOINTS[i];
        if (!lv_font_get_glyph_dsc(font, &glyph, cp, 0) || glyph.is_placeholder) {
            fprintf(stderr, "Missing U+%04X\n", cp);
            abort();
        }
    }
    lv_font_glyph_dsc_t glyph = {0};
    bool found = lv_font_get_glyph_dsc(font, &glyph, 0x9F98, 0);
    assert(!found || glyph.is_placeholder); /* known-missing negative case */
}

static void check_labels(lv_obj_t *parent, bool hidden)
{
    hidden = hidden || lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN);
    if (!hidden && lv_obj_check_type(parent, &lv_label_class)) {
        const char *text = lv_label_get_text(parent);
        assert(strcmp(text, "FONT ERROR") != 0);
        lv_area_t rect;
        lv_obj_get_coords(parent, &rect);
        assert(rect.x1 >= 0 && rect.x2 < 240 && rect.y1 >= 0 && rect.y2 < 320);
        /* Also reject text clipped by its composition/field box. */
        lv_area_t outer;
        lv_obj_get_coords(lv_obj_get_parent(parent), &outer);
        assert(rect.y1 >= outer.y1 && rect.y2 <= outer.y2);
        lv_point_t natural;
        lv_text_get_size(&natural, text, lv_obj_get_style_text_font(parent, LV_PART_MAIN),
                         lv_obj_get_style_text_letter_space(parent, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (natural.x > lv_obj_get_width(parent)) {
            fprintf(stderr, "Clipped text: %s (%d > %d)\n", text, (int)natural.x, (int)lv_obj_get_width(parent));
            abort();
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i)
        check_labels(lv_obj_get_child(parent, i), hidden);
}

static void check_active_fonts(lv_obj_t *o)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const lv_font_t *f = lv_obj_get_style_text_font(o, LV_PART_MAIN);
        assert(f == &token_font_12 || f == &token_font_16 || f == &token_font_20 || f == &token_font_36 || f == &lv_font_montserrat_20);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) check_active_fonts(lv_obj_get_child(o, i));
}
static lv_obj_t *find_label(lv_obj_t *o, const char *text)
{
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
        lv_obj_t *found = find_label(lv_obj_get_child(o, i), text);
        if (found) return found;
    }
    return NULL;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, s_draw, NULL, sizeof(s_draw), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    check_glyphs(&token_font_12); check_glyphs(&token_font_16); check_glyphs(&token_font_20); check_glyphs(&token_font_36);
    assert(token_ui_create());
    token_snapshot_t s = {0};
    token_ui_update(&s, -1, false, false, -1, 0, UINT32_MAX, false, true);
    snapshot(argv[1], "empty"); check_labels(lv_screen_active(), false);
    s.updated = 1790910000; s.end_day = (s.updated + 28800) / 86400;
    for (int t = 0; t < TOKEN_SOURCES; ++t) {
        s.sources[t].status = t == 2 ? TOKEN_UNSUPPORTED : TOKEN_LIVE;
        for (unsigned d = 0; d < TOKEN_DAYS; ++d) {
            unsigned n = t == 2 || d < 264 || d % 4 == 0 ? 0 : ((d * 37 + t * 19) % 31) * (d % 13 ? 8500U : 30000U);
            s.sources[t].days[d] = n;
            s.sources[t].total += n;
            if (n > s.sources[t].peak) s.sources[t].peak = n;
        }
        s.sources[t].longest = 13; s.sources[t].current = 3;
    }
    s.peak = 890000; s.longest = 13; s.current = 3;
    token_ui_update(&s, -1, false, false, 88, 2, UINT32_MAX, false, true);
    snapshot(argv[1], "dashboard"); check_labels(lv_screen_active(), false); check_active_fonts(lv_screen_active());
    for (int t = 0; t < TOKEN_SOURCES; ++t) {
        token_ui_update(&s, t, false, false, 88, 2, UINT32_MAX, false, true);
        char name[24]; snprintf(name, sizeof(name), "source-%d", t); snapshot(argv[1], name);
        check_labels(lv_screen_active(), false); check_active_fonts(lv_screen_active());
    }
    /* Verify the navigation state reaches the visible product and its
     * large cumulative number, including leaving the synchronization page. */
    const char *names[] = {"全部产品", "Codex", "Claude Code", "Cursor", "OpenCode", "Gemini CLI"};
    token_nav_t nav; token_nav_init(&nav); nav.sync_page = true;
    for (int i = 1; i <= TOKEN_SOURCES + 1; ++i) {
        token_nav_handle(&nav, TOKEN_KEY_DOWN, TOKEN_KEY_PRESS, false, false);
        token_ui_update(&s, nav.source, nav.older, nav.sync_page, 88, 2, UINT32_MAX, false, true);
        lv_obj_update_layout(lv_screen_active());
        assert(!nav.sync_page && find_label(lv_screen_active(), names[i % (TOKEN_SOURCES + 1)]));
        char value[32];
        if (nav.source == 2) strcpy(value, "--");
        else token_format(token_total(&s, nav.source), value, sizeof(value));
        lv_obj_t *hero = find_label(lv_screen_active(), value);
        assert(hero && lv_obj_get_style_text_font(hero, LV_PART_MAIN) == &token_font_36);
        check_labels(lv_screen_active(), false);
    }
    /* Large valid history must not clip metric fields. */
    token_snapshot_t large = s;
    large.sources[0].total = UINT64_MAX / TOKEN_SOURCES;
    large.sources[0].peak = UINT32_MAX; large.sources[0].current = UINT16_MAX; large.sources[0].longest = UINT16_MAX;
    token_ui_update(&large, 0, false, false, 100, 0, UINT32_MAX, false, true);
    lv_obj_update_layout(lv_screen_active()); check_labels(lv_screen_active(), false);
    snapshot(argv[1], "large-history");
    token_ui_update(&s, -1, false, true, 88, 1, 123456, true, true);
    snapshot(argv[1], "pairing"); check_labels(lv_screen_active(), false);
    token_ui_update(&s, -1, false, true, -1, 0, UINT32_MAX, false, false);
    snapshot(argv[1], "storage-error"); check_labels(lv_screen_active(), false);
    for (int i = 0; i < 600; ++i) {
        s.sources[0].status = i % 4;
        token_ui_update(&s, i % 6 - 1, i % 2, i % 3 == 0, i % 102 - 1, i % 3,
                        i % 17 == 0 ? 999999 : UINT32_MAX, i % 11 == 0, i % 13 != 0);
        lv_obj_update_layout(lv_screen_active()); check_labels(lv_screen_active(), false);
        check_active_fonts(lv_screen_active());
        lv_refr_now(NULL);
    }
    lv_mem_monitor_t memory; lv_mem_monitor(&memory);
    printf("Token UI: PASS (%u glyphs x 4 sizes, all pages, 600 rebuilds; LVGL used %u/%u bytes)\n",
        (unsigned)(sizeof(TOKEN_CODEPOINTS)/sizeof(TOKEN_CODEPOINTS[0])),
        (unsigned)(memory.total_size-memory.free_size), (unsigned)memory.total_size);
    lv_deinit();
    return 0;
}
