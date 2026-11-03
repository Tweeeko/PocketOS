// Main menu: pages of four glass tiles, swipe up/down. Settings (QR code) is always first so the
// settings website can always be reached; tiles come from the user's choices on the website and
// only appear when the home actually has that kind of device.
#include "ui_priv.h"
#include <string.h>
#include <stdio.h>

#define MAX_ITEMS 16
typedef struct { char key[12]; char label[16]; char sub[40]; bool on; } item_t;
static item_t items[MAX_ITEMS];
static int nitems;
static lv_obj_t *tv, *dots, *sub_lbls[MAX_ITEMS], *tiles[MAX_ITEMS];

bool cat_match(const char *key, const entity_t *e)
{
    if (!strcmp(key, "lights")) return e->dom == D_LIGHT;
    if (!strcmp(key, "heating")) return e->dom == D_CLIMATE;
    if (!strcmp(key, "media")) return e->dom == D_MEDIA;
    if (!strcmp(key, "locks")) return e->dom == D_LOCK;
    if (!strcmp(key, "plugs")) return e->dom == D_SWITCH;
    if (!strcmp(key, "fans")) return e->dom == D_FAN;
    if (!strcmp(key, "blinds")) return e->dom == D_COVER;
    if (!strcmp(key, "sensors")) return e->dom == D_SENSOR;
    if (!strcmp(key, "scenes")) return e->dom == D_SCENE || e->dom == D_SCRIPT;
    return false;
}

// Counts visible entities for a tile and writes its subtitle. Caller holds model+settings locks.
int cat_summary_locked(const model_t *m, const settings_t *s, const char *key, char *buf, int len, bool *on)
{
    int n = 0, cnt_on = 0;
    *on = false;
    if (!strcmp(key, "rooms")) {
        for (int a = 0; a < m->na; a++)
            for (int i = 0; i < m->n; i++) if (m->e[i].area == a && model_visible(m, s, i)) { n++; break; }
        snprintf(buf, (size_t)len, "%d room%s", n, n == 1 ? "" : "s");
        return n;
    }
    if (!strcmp(key, "favourites")) {
        for (int i = 0; i < m->n; i++) if (model_visible(m, s, i) && settings_has((char (*)[ID_LEN])s->fav, s->nfav, m->e[i].id)) n++;
        snprintf(buf, (size_t)len, "%d pinned", n);
        return n;
    }
    const entity_t *first = NULL;
    for (int i = 0; i < m->n; i++) {
        const entity_t *e = &m->e[i];
        if (!cat_match(key, e) || !model_visible(m, s, i)) continue;
        if (!first) first = e;
        n++;
        if (e->dom == D_LOCK ? !e->on : e->on) cnt_on++;
    }
    if (!n) { buf[0] = 0; return 0; }
    if (!strcmp(key, "locks")) snprintf(buf, (size_t)len, cnt_on ? "%d unlocked" : "All locked", cnt_on);
    else if (!strcmp(key, "media")) snprintf(buf, (size_t)len, cnt_on ? "%d playing" : "Idle", cnt_on);
    else if (!strcmp(key, "heating")) { if (cnt_on) model_status(first, buf, len); else snprintf(buf, (size_t)len, "Off"); }
    else if (!strcmp(key, "blinds")) snprintf(buf, (size_t)len, cnt_on ? "%d open" : "All closed", cnt_on);
    else if (!strcmp(key, "sensors")) model_status(first, buf, len);
    else if (!strcmp(key, "scenes")) snprintf(buf, (size_t)len, "%d scene%s", n, n == 1 ? "" : "s");
    else snprintf(buf, (size_t)len, "%d of %d on", cnt_on, n);
    *on = cnt_on > 0 && strcmp(key, "locks") != 0 && strcmp(key, "scenes") != 0 && strcmp(key, "sensors") != 0;
    return n;
}

int cat_summary(const char *key, char *buf, int len)
{
    bool on;
    const settings_t *s = app_settings_lock();
    model_t *m = app_model_lock();
    int n = cat_summary_locked(m, s, key, buf, len, &on);
    app_model_unlock();
    app_settings_unlock();
    return n;
}

static void collect(void)
{
    app_status_t st;
    app_status(&st);
    nitems = 0;
    item_t *it = &items[nitems++];
    snprintf(it->key, sizeof it->key, "settings");
    snprintf(it->label, sizeof it->label, "Settings");
    snprintf(it->sub, sizeof it->sub, st.ha == HA_NOT_SET ? "Set me up" : "Scan on phone");
    it->on = st.ha == HA_NOT_SET;

    const settings_t *s = app_settings_lock();
    model_t *m = app_model_lock();
    bool any_hidden = false;
    for (int t = 0; t < s->ntiles && nitems < MAX_ITEMS - 1; t++) {
        char sub[40];
        bool on;
        int n = cat_summary_locked(m, s, s->tiles[t].key, sub, sizeof sub, &on);
        if (!s->tiles[t].show) { if (n) any_hidden = true; continue; }
        if (!n) continue;
        it = &items[nitems++];
        snprintf(it->key, sizeof it->key, "%s", s->tiles[t].key);
        snprintf(it->label, sizeof it->label, "%s", s->tiles[t].label);
        snprintf(it->sub, sizeof it->sub, "%s", sub);
        it->on = on;
    }
    app_model_unlock();
    app_settings_unlock();
    it = &items[nitems++];
    snprintf(it->key, sizeof it->key, "more");
    snprintf(it->label, sizeof it->label, "More");
    snprintf(it->sub, sizeof it->sub, any_hidden ? "Everything else" : "Companion, device");
    it->on = false;
}

static void tile_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= nitems) return;
    const char *k = items[i].key;
    scr_ref_t r = {.kind = SCR_LIST};
    if (!strcmp(k, "settings")) { ui_toggle_settings_qr(); return; }
    if (!strcmp(k, "more")) r.list = L_MORE;
    else if (!strcmp(k, "rooms")) r.list = L_ROOMS;
    else if (!strcmp(k, "favourites")) r.list = L_FAVS;
    else { r.list = L_CATEGORY; snprintf(r.arg, sizeof r.arg, "%s", k); }
    nav_push(&r);
}

static void style_tile(int i)
{
    lv_obj_t *ic = lv_obj_get_child(tiles[i], 0);
    lv_obj_set_style_image_recolor(ic, lv_color_hex(items[i].on ? C_BLUE : C_INK), 0);
    lv_obj_set_style_text_color(sub_lbls[i], lv_color_hex(items[i].on ? C_BLUE : C_DIM), 0);
    if (items[i].on) lv_obj_add_state(tiles[i], LV_STATE_CHECKED); else lv_obj_remove_state(tiles[i], LV_STATE_CHECKED);
}

static void update_dots(void)
{
    if (!dots) return;
    lv_obj_t *act = lv_tileview_get_tile_active(tv);
    int page = act ? (int)lv_obj_get_index(act) : 0;
    int n = (int)lv_obj_get_child_count(dots);
    for (int i = 0; i < n; i++) {
        lv_obj_t *d = lv_obj_get_child(dots, i);
        lv_obj_set_width(d, i == page ? 22 : 8);
        lv_obj_set_style_bg_opa(d, i == page ? 255 : 70, 0);
    }
}

static void page_cb(lv_event_t *e) { update_dots(); }

void menu_build(lv_obj_t *scr)
{
    collect();
    int pages = (nitems + 3) / 4;
    tv = lv_tileview_create(scr);   // child 0: remembered scroll position
    lv_obj_remove_style_all(tv);
    lv_obj_set_size(tv, SCR_W, SCR_H);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(tv, page_cb, LV_EVENT_VALUE_CHANGED, NULL);
    for (int p = 0; p < pages; p++) {
        lv_dir_t dir = (p > 0 ? LV_DIR_TOP : 0) | (p < pages - 1 ? LV_DIR_BOTTOM : 0);
        lv_obj_t *page = lv_tileview_add_tile(tv, 0, (uint8_t)p, dir);
        for (int k = 0; k < 4 && p * 4 + k < nitems; k++) {
            int i = p * 4 + k;
            lv_obj_t *t = lv_obj_create(page);
            tiles[i] = t;
            lv_obj_remove_style_all(t);
            lv_obj_add_style(t, &st_glass, 0);
            lv_obj_add_style(t, &st_glass_on, LV_STATE_CHECKED);
            lv_obj_add_style(t, &st_pressed, LV_STATE_PRESSED);
            lv_obj_set_style_radius(t, 40, 0);
            lv_obj_set_size(t, 150, 132);
            lv_obj_set_pos(t, k % 2 ? 241 : 75, k < 2 ? 92 : 238);
            lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(t, tile_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            lv_obj_t *ic = mk_icon(t, tile_icon(items[i].key), C_INK);
            lv_obj_set_pos(ic, 20, 18);
            lv_obj_t *l = mk_label(t, &inter_20, C_INK, items[i].label);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_width(l, 116);
            lv_obj_set_pos(l, 20, 70);
            lv_obj_t *sl = mk_label(t, &inter_15, C_DIM, items[i].sub);
            lv_obj_set_style_text_align(sl, LV_TEXT_ALIGN_LEFT, 0);
            lv_label_set_long_mode(sl, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_width(sl, 116);
            lv_obj_set_pos(sl, 20, 96);
            sub_lbls[i] = sl;
            style_tile(i);
        }
    }
    lv_obj_t *t = mk_label(scr, &inter_22, C_DIM, "PocketOS");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
    dots = NULL;
    if (pages > 1) {
        dots = lv_obj_create(scr);
        lv_obj_remove_style_all(dots);
        lv_obj_set_size(dots, LV_SIZE_CONTENT, 8);
        lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(dots, 7, 0);
        lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -46);
        for (int p = 0; p < pages; p++) {
            lv_obj_t *d = lv_obj_create(dots);
            lv_obj_remove_style_all(d);
            lv_obj_set_size(d, 8, 8);
            lv_obj_set_style_radius(d, 4, 0);
            lv_obj_set_style_bg_color(d, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(d, 70, 0);
        }
    }
    scr_ref_t *r = nav_top();
    if (r->scroll) lv_tileview_set_tile_by_index(tv, 0, (uint32_t)(r->scroll / SCR_H), LV_ANIM_OFF);
    update_dots();
}

void menu_refresh(void)
{
    int old = nitems;
    char keys[MAX_ITEMS][12];
    for (int i = 0; i < nitems; i++) memcpy(keys[i], items[i].key, 12);
    collect();
    bool same = old == nitems;
    for (int i = 0; same && i < nitems; i++) if (strcmp(keys[i], items[i].key)) same = false;
    if (!same) {   // tiles appeared/disappeared: rebuild in place
        lv_obj_t *scr = lv_screen_active();
        nav_top()->scroll = lv_obj_get_scroll_y(tv);
        lv_obj_clean(scr);
        menu_build(scr);   // the screen keeps its own swipe-back handler
        return;
    }
    for (int i = 0; i < nitems; i++) { lv_label_set_text(sub_lbls[i], items[i].sub); style_tile(i); }
}
