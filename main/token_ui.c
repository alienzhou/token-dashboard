#include "token_ui.h"
#include "lvgl.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
LV_FONT_DECLARE(token_font_12);
LV_FONT_DECLARE(token_font_16);
LV_FONT_DECLARE(token_font_20);
LV_FONT_DECLARE(token_font_36);
static lv_obj_t *s_screen;
static token_snapshot_t s_ui_data;
static const token_snapshot_t *s_data;
static int s_source;
static bool s_older;
static const char *s_names[] = {"Codex", "Claude Code", "Cursor", "OpenCode", "Gemini CLI"};
static const char *s_status[] = {"未发现记录", "自动采集", "暂不支持", "采集异常"};
static const uint32_t s_colors[] = {0x178B77, 0xC58552, 0x8995AC, 0x7376CD, 0x428BC4};
static const uint32_t s_heat[] = {0xE2E9E7, 0xC0E9DC, 0x7ACDB5, 0x38AB8A, 0x16775F};

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static int text_width(const char *txt, const lv_font_t *font)
{
    lv_point_t size;
    lv_text_get_size(&size, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}
static lv_obj_t *label(lv_obj_t *p, const char *txt, int x, int y, int w, const lv_font_t *f, uint32_t color)
{
    /* Large historical totals/streaks still fit their fields. */
    if (text_width(txt, f) > w && f == &token_font_36) f = &token_font_20;
    if (text_width(txt, f) > w && f == &token_font_20) f = &token_font_16;
    if (text_width(txt, f) > w && f == &token_font_16) f = &token_font_12;
    lv_obj_t *o = lv_label_create(p);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, w);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(o, f, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_text(o, txt);
    return o;
}
static void heat(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a; lv_obj_get_coords(lv_event_get_target(e), &a);
    uint64_t peak = 0;
    for (unsigned d = 0; d < TOKEN_DAYS; ++d) {
        uint64_t n = token_day(s_data, s_source, d);
        if (n > peak) peak = n;
    }
    /* Monday-first calendar columns, aligned to actual calendar weekdays. */
    unsigned base = s_older ? 0 : 182;
    uint32_t first_day = s_data->end_day ? s_data->end_day - TOKEN_DAYS + 1 + base : 0;
    unsigned shift = (first_day + 3) % 7; /* Unix epoch was Thursday */
    for (unsigned i = 0; i < 189; ++i) {
        int d = (int)i - (int)shift;
        uint64_t n = d >= 0 && d < 182 ? token_day(s_data, s_source, base + (unsigned)d) : 0;
        lv_draw_rect_dsc_t r; lv_draw_rect_dsc_init(&r);
        r.bg_color = lv_color_hex(s_heat[token_heat_level(n, peak)]);
        r.bg_opa = d >= 0 && d < 182 ? LV_OPA_COVER : LV_OPA_20; r.radius = 1;
        lv_area_t cell = {a.x1 + (int)(i / 7) * 7, a.y1 + (int)(i % 7) * 6,
                          a.x1 + (int)(i / 7) * 7 + 5, a.y1 + (int)(i % 7) * 6 + 4};
        lv_draw_rect(layer, &r, &cell);
    }
}
bool token_ui_create(void)
{
    s_screen = lv_obj_create(NULL);
    if (!s_screen) return false;
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0xF6F8F5), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(s_screen);
    return true;
}
void token_ui_update(const token_snapshot_t *s, int source, bool older, bool sync_page,
                     int battery, int link, unsigned passkey, bool pairing, bool storage_ok)
{
    /* Owned copy is changed only under the LVGL lock; draw callbacks never
     * race the application task while a new BLE snapshot is being accepted. */
    if (source < -1 || source >= TOKEN_SOURCES) source = -1;
    s_ui_data = *s; s_data = &s_ui_data; s_source = source; s_older = older;
    lv_obj_clean(s_screen);
    char text[64], value[32];
    label(s_screen, sync_page ? "蓝牙同步" : "TOKEN 用量", 20, 12, 150, &token_font_12, 0x73827A);
    if (battery < 0) snprintf(text, sizeof(text), "--%%");
    else snprintf(text, sizeof(text), "%d%%", battery);
    lv_obj_t *battery_label = label(s_screen, text, 180, 12, 42, &token_font_12, 0x73827A);
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
    if (sync_page || passkey != UINT32_MAX) {
        box(s_screen, 14, 47, 212, 103, 0x173F35, 12);
        label(s_screen, passkey != UINT32_MAX ? "电脑配对码" : "AI Passport", 27, 56, 185, &token_font_12, 0xBDE8D8);
        if (passkey != UINT32_MAX) snprintf(text, sizeof(text), "%06u", passkey);
        else snprintf(text, sizeof(text), "%s", link == 2 ? "已加密连接" : pairing ? "等待配对" : "等待电脑");
        label(s_screen, text, 27, 81, 185, passkey != UINT32_MAX ? &token_font_36 : &token_font_20, 0xFFFFFF);
        label(s_screen, pairing ? "配对窗口开启 · 60秒有效" : "长按确认键开启配对", 16, 168, 212, &token_font_16, 0x243C36);
        label(s_screen, passkey != UINT32_MAX ? "在电脑弹窗输入上方六位码" : "电脑自动汇总本地用量", 16, 203, 212, &token_font_12, 0x6A7C75);
        label(s_screen, "双击确认切换半年活动图", 16, 225, 212, &token_font_12, 0x6A7C75);
        if (s->updated) {
            time_t when = (time_t)s->updated + 28800;
            struct tm date; gmtime_r(&when, &date);
            snprintf(text, sizeof(text), "同步 %02d/%02d %02d:%02d", date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min);
        } else snprintf(text, sizeof(text), "尚未同步，等待真实数据");
        label(s_screen, text, 16, 253, 212, &token_font_12, 0x6A7C75);
        lv_obj_t *hint = label(s_screen, !storage_ok ? "保存失败，请重新同步" : passkey != UINT32_MAX ? "配对成功后自动返回用量页" : "上下切产品 · 确认返回", 24, 293, 192, &token_font_12, storage_ok ? 0x6A7C75 : 0xB04C35);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }
    uint32_t accent = source < 0 ? 0x173F35 : s_colors[source];
    lv_obj_t *selector = box(s_screen, 14, 36, 212, 36, 0xFFFFFF, 10);
    lv_obj_set_style_border_width(selector, 1, 0);
    lv_obj_set_style_border_color(selector, lv_color_hex(0xDDE6DF), 0);
    label(selector, "↑", 10, 5, 20, &token_font_16, accent);
    label(selector, "↓", 186, 5, 20, &token_font_16, accent);
    lv_obj_t *product = label(selector, source < 0 ? "全部产品" : s_names[source], 32, source < 0 ? 6 : 3, 148,
                              source < 0 ? &token_font_16 : &lv_font_montserrat_20, 0x203B30);
    if (source < 0) lv_obj_set_style_text_letter_space(product, 1, 0);
    lv_obj_set_style_text_align(product, LV_TEXT_ALIGN_CENTER, 0);
    int dot_x = 88;
    for (int i = 0; i <= TOKEN_SOURCES; ++i) {
        bool selected = i == source + 1;
        box(s_screen, dot_x, 78, selected ? 12 : 4, 3, selected ? accent : 0xD5DED7, 2);
        dot_x += (selected ? 12 : 4) + 6;
    }
    label(s_screen, "累计 Token", 14, 87, 132, &token_font_12, 0x73827A);
    box(s_screen, 172, 94, 4, 4, link == 2 ? 0x178B77 : 0xBBC6BE, 2);
    label(s_screen, link == 2 ? "已连接" : link == 1 ? "连接中" : "离线", 181, 87, 45, &token_font_12, 0x73827A);
    if (s->updated && (source < 0 || s->sources[source].status == TOKEN_LIVE))
        token_format(token_total(s, source), value, sizeof(value));
    else strcpy(value, "--");
    label(s_screen, value, 13, 104, 214, &token_font_36, 0x173F35);
    if (source >= 0 && s->sources[source].status != TOKEN_LIVE)
        label(s_screen, s_status[s->sources[source].status], 112, 129, 114, &token_font_12, 0x73827A);
    bool missing = !s->updated || (source >= 0 && s->sources[source].status != TOKEN_LIVE && !s->sources[source].total);
    uint64_t peak = source < 0 ? s->peak : s->sources[source].peak;
    unsigned current = source < 0 ? s->current : s->sources[source].current;
    unsigned longest = source < 0 ? s->longest : s->sources[source].longest;
    const char *captions[] = {"单日峰值", "连续天数", "最长连续"};
    for (int i = 0; i < 3; ++i) {
        int x = 14 + i * 74;
        if (i) box(s_screen, x - 8, 168, 1, 42, 0xDCE5DE, 0);
        if (missing) strcpy(value, "--");
        else if (i == 0) token_format(peak, value, sizeof(value));
        else snprintf(value, sizeof(value), "%u天", i == 1 ? current : longest);
        label(s_screen, value, x, 163, 64, &token_font_20, 0x203B30);
        label(s_screen, captions[i], x, 196, 64, &token_font_12, 0x73827A);
    }
    label(s_screen, older ? "前半年活动" : "近半年活动", 14, 225, 88, &token_font_12, 0x203B30);
    lv_obj_t *grid = box(s_screen, 25, 249, 189, 42, 0xF6F8F5, 0);
    lv_obj_add_event_cb(grid, heat, LV_EVENT_DRAW_MAIN, NULL);
    if (!s->updated) snprintf(text, sizeof(text), "等待同步");
    else if (source >= 0 && s->sources[source].status != TOKEN_LIVE) snprintf(text, sizeof(text), "无可用明细");
    else {
        token_format(token_day(s, source, TOKEN_DAYS - 1), value, sizeof(value));
        snprintf(text, sizeof(text), link == 2 ? "今日 %s" : "同步日 %s", value);
    }
    lv_obj_t *day = label(s_screen, text, 104, 225, 122, &token_font_12, 0x73827A);
    lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *hint = label(s_screen, storage_ok ? "上下切产品 · 确认同步" : "保存失败，请重新同步", 24, 297, 192, &token_font_12, storage_ok ? 0x73827A : 0xB04C35);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
}
