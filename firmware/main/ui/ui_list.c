// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Scrolling glass lists. The row nearest the centre is highlighted and the rest fade towards the
// round edge. Rows: rooms, devices in a room or category, favourites, More, device info, companion.
#include "ui_priv.h"
#include "portable.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ROWS 64
typedef enum { R_ENTITY, R_ALL, R_ROOM, R_TILE, R_SCREEN, R_INFO, R_SETTING } row_kind_t;
typedef struct { row_kind_t kind; char id[ID_LEN]; char label[48]; char sub[80]; bool on; } row_t;

PSRAM_BSS static row_t rows[MAX_ROWS];   // ~13 KB
static int nrows;
static lv_obj_t *scroller, *row_obj[MAX_ROWS], *sub_obj[MAX_ROWS], *empty_lbl;
static scr_ref_t *ref;

static void add(row_kind_t k, const char *id, const char *label, const char *sub, bool on)
{
    if (nrows >= MAX_ROWS) return;
    row_t *r = &rows[nrows++];
    r->kind = k;
    snprintf(r->id, sizeof r->id, "%s", id ? id : "");
    snprintf(r->label, sizeof r->label, "%s", label);
    snprintf(r->sub, sizeof r->sub, "%s", sub ? sub : "");
    r->on = on;
}

static const char *tile_label(const settings_t *s, const char *key)
{
    for (int i = 0; i < s->ntiles; i++) if (!strcmp(s->tiles[i].key, key)) return s->tiles[i].label;
    return key;
}

static void entity_row(const model_t *m, int i, bool with_area)
{
    const entity_t *e = &m->e[i];
    char st[48], sub[96];
    model_status(e, st, sizeof st);
    if (e->pending_ms) snprintf(st, sizeof st, "Sending...");
    if (with_area && e->area >= 0) snprintf(sub, sizeof sub, "%s · %s", m->a[e->area].name, st);
    else snprintf(sub, sizeof sub, "%s", st);
    bool on = (e->dom == D_LOCK) ? false : e->on;
    add(R_ENTITY, e->id, e->name, sub, on && e->dom != D_SENSOR);
}

static void collect(void)
{
    nrows = 0;
    app_status_t st;
    app_status(&st);
    const settings_t *s = app_settings_lock();
    model_t *m = app_model_lock();
    char sub[48];
    switch (ref->list) {
    case L_CATEGORY: {
        const char *k = ref->arg;
        int on = 0, n = 0;
        for (int i = 0; i < m->n; i++) if (cat_match(k, &m->e[i]) && model_visible(m, s, i)) { n++; if (m->e[i].dom == D_LOCK ? !m->e[i].on : m->e[i].on) on++; }
        if (n > 1 && on && !strcmp(k, "lights")) add(R_ALL, k, "Turn all off", "Every light at once", false);
        if (n > 1 && on && !strcmp(k, "media")) add(R_ALL, k, "Pause all", "Every speaker and TV", false);
        if (n > 1 && on && !strcmp(k, "locks")) add(R_ALL, k, "Lock all", "Every door", false);
        if (n > 1 && on && !strcmp(k, "plugs")) add(R_ALL, k, "Turn all off", "Every plug", false);
        for (int i = 0; i < m->n; i++) if (cat_match(k, &m->e[i]) && model_visible(m, s, i)) entity_row(m, i, true);
        break;
    }
    case L_ROOMS:
        for (int a = 0; a < m->na; a++) {
            int n = 0, on = 0;
            for (int i = 0; i < m->n; i++) if (m->e[i].area == a && model_visible(m, s, i)) { n++; if (m->e[i].on && m->e[i].dom != D_LOCK && m->e[i].dom != D_SENSOR) on++; }
            if (!n) continue;
            char id[12];
            snprintf(id, sizeof id, "%d", a);
            snprintf(sub, sizeof sub, "%d device%s · %d on", n, n == 1 ? "" : "s", on);
            add(R_ROOM, id, m->a[a].name, sub, on > 0);
        }
        break;
    case L_ROOM: {
        int a = atoi(ref->arg);
        for (int i = 0; i < m->n; i++) if (m->e[i].area == a && model_visible(m, s, i)) entity_row(m, i, false);
        break;
    }
    case L_FAVS:
        for (int i = 0; i < m->n; i++) if (model_visible(m, s, i) && settings_has((char (*)[ID_LEN])s->fav, s->nfav, m->e[i].id)) entity_row(m, i, true);
        break;
    case L_MORE:
        for (int t = 0; t < s->ntiles; t++) {
            if (s->tiles[t].show) continue;
            char sb[40];
            bool on = false;
            int n = cat_summary_locked(m, s, s->tiles[t].key, sb, sizeof sb, &on);
            if (n) add(R_TILE, s->tiles[t].key, s->tiles[t].label, sb, false);
        }
        add(R_SCREEN, "companion", "Companion", "Eyes, animation, brightness", false);
        add(R_SCREEN, "info", "This PocketOS", "Wi-Fi, battery, version", false);
        add(R_SCREEN, "settings", "Settings website", "Scan the code on your phone", false);
        break;
    case L_INFO:
        if (st.update == UPD_AVAILABLE) {
            snprintf(sub, sizeof sub, "%s ready · install on website", st.update_version);
            add(R_INFO, "", "Update", sub, true);
        } else if (st.update == UPD_DOWNLOADING) {
            snprintf(sub, sizeof sub, "Downloading %d%%", st.update_progress);
            add(R_INFO, "", "Update", sub, true);
        }
        snprintf(sub, sizeof sub, "%s", st.setup_mode ? "Setup mode" : st.wifi ? st.ip : "Not connected");
        add(R_INFO, "", "Wi-Fi", sub, st.wifi);
        add(R_INFO, "", "Home Assistant", st.ha == HA_OK ? "Connected" : st.ha == HA_CONNECTING ? "Connecting..." : st.ha == HA_ERROR ? (st.ha_error[0] ? st.ha_error : "Problem connecting") : "Not set up yet", st.ha == HA_OK);
        if (st.battery < 0) snprintf(sub, sizeof sub, "%s", st.plugged ? "No battery · on USB power" : "Unknown");
        else if (st.charging) snprintf(sub, sizeof sub, "%d%% · charging", st.battery);
        else if (st.plugged) snprintf(sub, sizeof sub, "%d%% · plugged in", st.battery);
        else if (st.battery_hours >= 0) snprintf(sub, sizeof sub, "%d%% · about %d h left", st.battery, st.battery_hours);
        else snprintf(sub, sizeof sub, "%d%%", st.battery);
        add(R_INFO, "", "Battery", sub, false);
        add(R_INFO, "", "Address", st.host, false);
        add(R_INFO, "", "Firmware", st.firmware, false);
        add(R_INFO, "", "Created by", "Leeko", false);
        snprintf(sub, sizeof sub, "%d devices, %d rooms", m->n, m->na);
        add(R_INFO, "", "Home", sub, false);
        break;
    case L_COMPANION: {
        static const char *EYE_N[] = {"Blue", "Violet", "White"}, *ANIM_N[] = {"Calm", "Lively", "Off"};
        add(R_SETTING, "eye", "Eye colour", EYE_N[s->eye], false);
        add(R_SETTING, "anim", "Animation", ANIM_N[s->anim], false);
        snprintf(sub, sizeof sub, "%d%%", s->brightness);
        add(R_SETTING, "brightness", "Brightness", sub, false);
        snprintf(sub, sizeof sub, s->sleep_s ? "%d seconds" : "Never", s->sleep_s);
        add(R_SETTING, "sleep", "Sleep after", sub, false);
        break;
    }
    }
    app_model_unlock();
    app_settings_unlock();
    (void)tile_label;
}

static void all_off(const char *key)
{
    // Turn off / pause / lock every visible device in the category that is "on"
    char ids[MAX_ROWS][ID_LEN];
    int n = 0;
    const settings_t *s = app_settings_lock();
    model_t *m = app_model_lock();
    for (int i = 0; i < m->n && n < MAX_ROWS; i++) {
        const entity_t *e = &m->e[i];
        if (!cat_match(key, e) || !model_visible(m, s, i)) continue;
        if (e->dom == D_LOCK ? !e->on : e->on) snprintf(ids[n++], ID_LEN, "%s", e->id);
    }
    app_model_unlock();
    app_settings_unlock();
    for (int i = 0; i < n; i++) app_act(ids[i], ACT_TOGGLE, 0);
    ui_toast("Sent to every device", 1500);
}

static void cycle_setting(const char *what)
{
    const settings_t *s = app_settings_lock();
    char json[64];
    static const char *EY[] = {"blue", "violet", "white"}, *AN[] = {"calm", "lively", "off"};
    static const int SL[] = {15, 30, 60, 0};
    if (!strcmp(what, "eye")) snprintf(json, sizeof json, "{\"eye\":\"%s\"}", EY[(s->eye + 1) % 3]);
    else if (!strcmp(what, "anim")) snprintf(json, sizeof json, "{\"anim\":\"%s\"}", AN[(s->anim + 1) % 3]);
    else if (!strcmp(what, "brightness")) snprintf(json, sizeof json, "{\"brightness\":%d}", s->brightness >= 100 ? 40 : s->brightness < 40 ? 40 : s->brightness >= 70 ? 100 : 70);
    else { int k = 0; for (int i = 0; i < 4; i++) if (SL[i] == s->sleep_s) k = i; snprintf(json, sizeof json, "{\"sleep_s\":%d}", SL[(k + 1) % 4]); }
    app_settings_unlock();
    if (!app_settings_patch(json)) ui_toast("Couldn't save that", 2000);
}

static void row_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= nrows) return;
    row_t *r = &rows[i];
    scr_ref_t n = {0};
    switch (r->kind) {
    case R_ENTITY: {
        model_t *m = app_model_lock();
        int k = model_find(m, r->id);
        bool scene = k >= 0 && (m->e[k].caps & CAP_RUN);
        app_model_unlock();
        if (scene) { app_act(r->id, ACT_RUN, 0); ui_toast("Running scene", 1500); return; }
        n.kind = SCR_CONTROL;
        snprintf(n.arg, sizeof n.arg, "%s", r->id);
        nav_push(&n);
        return;
    }
    case R_ALL: all_off(r->id); return;
    case R_ROOM: n.kind = SCR_LIST; n.list = L_ROOM; snprintf(n.arg, sizeof n.arg, "%s", r->id); nav_push(&n); return;
    case R_TILE:
        n.kind = SCR_LIST;
        if (!strcmp(r->id, "rooms")) n.list = L_ROOMS;
        else if (!strcmp(r->id, "favourites")) n.list = L_FAVS;
        else { n.list = L_CATEGORY; snprintf(n.arg, sizeof n.arg, "%s", r->id); }
        nav_push(&n);
        return;
    case R_SCREEN:
        if (!strcmp(r->id, "settings")) { ui_toggle_settings_qr(); return; }
        n.kind = SCR_LIST;
        n.list = !strcmp(r->id, "info") ? L_INFO : L_COMPANION;
        nav_push(&n);
        return;
    case R_SETTING: cycle_setting(r->id); return;
    case R_INFO: return;
    }
}

static void highlight(void)
{
    if (!scroller) return;
    lv_area_t sa;
    lv_obj_get_coords(scroller, &sa);
    int32_t cy = (sa.y1 + sa.y2) / 2, best = 0x7FFF, bi = -1;
    for (int i = 0; i < nrows; i++) {
        lv_area_t a;
        lv_obj_get_coords(row_obj[i], &a);
        int32_t d = LV_ABS((a.y1 + a.y2) / 2 - cy);
        if (d < best) { best = d; bi = i; }
        int32_t opa = 255 - d * 255 / 200;
        lv_obj_set_style_opa(row_obj[i], (lv_opa_t)LV_CLAMP(40, opa, 255), 0);
    }
    for (int i = 0; i < nrows; i++) {
        if (i == bi) lv_obj_add_state(row_obj[i], LV_STATE_FOCUSED); else lv_obj_remove_state(row_obj[i], LV_STATE_FOCUSED);
    }
}

static void scroll_cb(lv_event_t *e) { highlight(); }

static void fill(void)
{
    lv_obj_clean(scroller);
    for (int i = 0; i < nrows; i++) {
        lv_obj_t *r = lv_obj_create(scroller);
        row_obj[i] = r;
        lv_obj_remove_style_all(r);
        lv_obj_add_style(r, &st_glass, 0);
        lv_obj_add_style(r, &st_glass_sel, LV_STATE_FOCUSED);
        lv_obj_add_style(r, &st_pressed, LV_STATE_PRESSED);
        lv_obj_set_style_radius(r, 38, 0);
        lv_obj_set_size(r, 340, 76);
        lv_obj_set_style_pad_hor(r, 28, 0);
        lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SNAPPABLE);
        lv_obj_add_event_cb(r, row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = mk_label(r, &inter_20, C_INK, rows[i].label);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(l, 284);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, rows[i].sub[0] ? -12 : 0);
        lv_obj_t *s = mk_label(r, &inter_15, rows[i].on ? C_BLUE : C_DIM, rows[i].sub);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(s, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(s, 284);
        lv_obj_align(s, LV_ALIGN_LEFT_MID, 0, 14);
        sub_obj[i] = s;
    }
    if (empty_lbl) lv_obj_add_flag(empty_lbl, LV_OBJ_FLAG_HIDDEN);
    if (!nrows && empty_lbl) lv_obj_remove_flag(empty_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(scroller);
    highlight();
}

static void del_cb(lv_event_t *e) { scroller = NULL; empty_lbl = NULL; }

static const char *title_for(const scr_ref_t *r, char *buf, int len)  // buf >= 64
{
    switch (r->list) {
    case L_ROOMS: return "Rooms";
    case L_FAVS: return "Favourites";
    case L_MORE: return "More";
    case L_INFO: return "This PocketOS";
    case L_COMPANION: return "Companion";
    case L_ROOM: {
        model_t *m = app_model_lock();
        int a = atoi(r->arg);
        snprintf(buf, (size_t)len, "%s", (a >= 0 && a < m->na) ? m->a[a].name : "Room");
        app_model_unlock();
        return buf;
    }
    case L_CATEGORY: {
        const settings_t *s = app_settings_lock();
        snprintf(buf, (size_t)len, "%s", tile_label(s, r->arg));
        app_settings_unlock();
        return buf;
    }
    }
    return "";
}

void list_build(lv_obj_t *scr, scr_ref_t *r)
{
    ref = r;
    collect();
    scroller = lv_obj_create(scr);   // child 0: remembered scroll position
    lv_obj_remove_style_all(scroller);
    lv_obj_set_size(scroller, SCR_W, SCR_H);
    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroller, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scroller, 12, 0);
    lv_obj_set_style_pad_ver(scroller, SCR_H / 2 - 38, 0);
    lv_obj_set_scroll_dir(scroller, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(scroller, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(scroller, scroll_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(scroller, del_cb, LV_EVENT_DELETE, NULL);
    char tb[80];
    mk_title(scr, title_for(r, tb, sizeof tb), false);
    empty_lbl = mk_label(scr, &inter_18, C_DIM, r->list == L_FAVS ? "No favourites yet.\nStar devices on the\nsettings website." : "Nothing here yet.\nAdd devices in Home Assistant,\nthen review them on the\nsettings website.");
    lv_obj_center(empty_lbl);
    fill();
    if (r->scroll) lv_obj_scroll_to_y(scroller, r->scroll, LV_ANIM_OFF);
    else if (nrows) lv_obj_scroll_to_view(row_obj[0], LV_ANIM_OFF);
    highlight();
}

void list_refresh(void)
{
    if (!scroller) return;
    char ids[MAX_ROWS][ID_LEN];
    int old = nrows;
    for (int i = 0; i < nrows; i++) memcpy(ids[i], rows[i].id, ID_LEN);
    collect();
    bool same = old == nrows;
    for (int i = 0; same && i < nrows; i++) if (strcmp(ids[i], rows[i].id)) same = false;
    if (!same) {
        int32_t y = lv_obj_get_scroll_y(scroller);
        fill();
        lv_obj_scroll_to_y(scroller, y, LV_ANIM_OFF);
        return;
    }
    for (int i = 0; i < nrows; i++) {
        lv_label_set_text(sub_obj[i], rows[i].sub);
        lv_obj_set_style_text_color(sub_obj[i], lv_color_hex(rows[i].on ? C_BLUE : C_DIM), 0);
        lv_label_set_text(lv_obj_get_child(row_obj[i], 0), rows[i].label);
    }
}
