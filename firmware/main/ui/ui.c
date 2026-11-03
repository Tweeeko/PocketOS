// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Screen stack, live refresh, toasts and overlays.
#include "ui_priv.h"
#include <string.h>

#define MAX_DEPTH 8
static scr_ref_t stack[MAX_DEPTH];
static int depth;                    // 0 = home
static uint32_t seen_version = 0xFFFFFFFF;
static app_status_t last_st;
static lv_obj_t *toast_lbl, *reset_lbl;
static lv_timer_t *toast_t;
static bool asleep;

scr_ref_t *nav_top(void) { return &stack[depth]; }
bool ui_asleep_flag(void) { return asleep; }

static void gesture_cb(lv_event_t *e)
{
    lv_indev_t *in = lv_indev_active();
    if (in && lv_indev_get_gesture_dir(in) == LV_DIR_RIGHT && depth > 0) {
        lv_indev_wait_release(in);
        ui_back();
    }
}

void nav_gesture_back(lv_obj_t *scr) { lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL); }

static void build(bool animate, bool forward)
{
    scr_ref_t *r = &stack[depth];
    lv_obj_t *scr = mk_screen();
    switch (r->kind) {
    case SCR_HOME: home_build(scr); break;
    case SCR_MENU: menu_build(scr); break;
    case SCR_LIST: list_build(scr, r); break;
    case SCR_CONTROL: ctl_build(scr, r); break;
    case SCR_QR: qr_build(scr); break;
    case SCR_SETUP: setup_build(scr); break;
    case SCR_VOICE: voice_build(scr); break;
    }
    if (r->kind != SCR_HOME && r->kind != SCR_SETUP) nav_gesture_back(scr);
    // a new screen starts without the previous screen's message
    if (toast_lbl) { lv_obj_add_flag(toast_lbl, LV_OBJ_FLAG_HIDDEN); if (toast_t) { lv_timer_delete(toast_t); toast_t = NULL; } }
    if (animate) lv_screen_load_anim(scr, forward ? LV_SCREEN_LOAD_ANIM_FADE_IN : LV_SCREEN_LOAD_ANIM_FADE_IN, 180, 0, true);
    else lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
    seen_version = 0xFFFFFFFF;   // refresh contents on the next tick
}

void nav_push(const scr_ref_t *r)
{
    if (depth >= MAX_DEPTH - 1) return;
    if (stack[depth].kind == SCR_LIST || stack[depth].kind == SCR_MENU) {
        // remember where we were scrolled to
        lv_obj_t *cur = lv_screen_active();
        lv_obj_t *scroller = lv_obj_get_child(cur, 0);
        if (scroller) stack[depth].scroll = lv_obj_get_scroll_y(scroller);
    }
    stack[++depth] = *r;
    build(true, true);
}

void ui_back(void)
{
    if (depth == 0) return;
    depth--;
    build(true, false);
}

void nav_replace_home(void)
{
    app_status_t st;
    app_status(&st);
    depth = 0;
    memset(&stack[0], 0, sizeof(stack[0]));
    stack[0].kind = st.setup_mode ? SCR_SETUP : SCR_HOME;
    build(true, false);
}

void ui_home(void) { nav_replace_home(); }

void ui_open_menu(void)
{
    scr_ref_t r = {.kind = SCR_MENU};
    if (stack[depth].kind == SCR_MENU) return;
    depth = 0;
    nav_push(&r);
}

void ui_toggle_settings_qr(void)
{
    if (stack[depth].kind == SCR_QR) { ui_back(); return; }
    scr_ref_t r = {.kind = SCR_QR};
    nav_push(&r);
}

/* ---------- live refresh ---------- */

static void refresh_cb(lv_timer_t *t)
{
    app_status_t st;
    app_status(&st);
    model_t *m = app_model_lock();
    // Commands that Home Assistant never confirmed: stop showing "Sending..."
    bool expired = model_expire_pending(m, app_ms(), 6000);
    uint32_t v = m->version;
    app_model_unlock();
    if (expired) ui_toast("No response from Home Assistant", 2500);

    bool status_changed = memcmp(&st, &last_st, sizeof st) != 0;
    // Setup mode finished (or reset): swap the root screen
    if (st.setup_mode != last_st.setup_mode && depth == 0) { last_st = st; nav_replace_home(); return; }
    last_st = st;
    if (v == seen_version && !status_changed) return;
    seen_version = v;
    switch (stack[depth].kind) {
    case SCR_HOME: home_refresh(); break;
    case SCR_MENU: menu_refresh(); break;
    case SCR_LIST: list_refresh(); break;
    case SCR_CONTROL: ctl_refresh(); break;
    case SCR_QR: qr_refresh(); break;
    case SCR_SETUP: setup_refresh(); break;
    default: break;
    }
}

/* ---------- overlays ---------- */

static void toast_hide(lv_timer_t *t) { lv_obj_add_flag(toast_lbl, LV_OBJ_FLAG_HIDDEN); toast_t = NULL; }

void ui_toast(const char *text, int ms)
{
    lv_label_set_text(toast_lbl, text);
    lv_obj_remove_flag(toast_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(toast_lbl);
    if (toast_t) { lv_timer_delete(toast_t); toast_t = NULL; }
    if (ms > 0) { toast_t = lv_timer_create(toast_hide, (uint32_t)ms, NULL); lv_timer_set_repeat_count(toast_t, 1); }
}

void ui_reset_prompt(bool on)
{
    if (on) { lv_obj_remove_flag(reset_lbl, LV_OBJ_FLAG_HIDDEN); ui_wake(); }
    else lv_obj_add_flag(reset_lbl, LV_OBJ_FLAG_HIDDEN);
}

void ui_set_asleep(bool s)
{
    if (s == asleep) return;
    asleep = s;
    if (s) {
        // Fall back to the companion so the device wakes to a friendly face
        if (stack[depth].kind != SCR_SETUP && stack[depth].kind != SCR_VOICE) { depth = 0; stack[0].kind = SCR_HOME; build(false, false); }
        app_brightness(0);
    } else {
        const settings_t *s2 = app_settings_lock();
        int b = s2->brightness;
        app_settings_unlock();
        app_brightness(b);
        if (stack[depth].kind == SCR_HOME) home_mood_override(MOOD_WAKE, 900);
    }
}

bool ui_is_asleep(void) { return asleep; }
void ui_wake(void) { lv_display_trigger_activity(NULL); if (asleep) ui_set_asleep(false); }

void ui_settings_changed(void)
{
    const settings_t *s = app_settings_lock();
    int b = s->brightness;
    app_settings_unlock();
    if (!asleep) app_brightness(b);
    seen_version = 0xFFFFFFFF;
    // Rebuild the current screen so tiles, names and colours update
    build(false, false);
    ui_toast("Settings updated", 1500);
}

void ui_react_happy(void) { if (stack[depth].kind == SCR_HOME) home_happy(1500); }

void ui_voice(voice_state_t st, const char *text)
{
    if (st == VOICE_LISTENING && stack[depth].kind != SCR_VOICE) {
        ui_wake();
        scr_ref_t r = {.kind = SCR_VOICE};
        nav_push(&r);
    }
    if (stack[depth].kind == SCR_VOICE) voice_set(st, text);
}

void ui_init(void)
{
    theme_init();
    toast_lbl = mk_label(lv_layer_top(), &inter_20, C_INK, "");
    lv_obj_set_style_bg_color(toast_lbl, lv_color_hex(0x14162A), 0);
    lv_obj_set_style_bg_opa(toast_lbl, 235, 0);
    lv_obj_set_style_radius(toast_lbl, 22, 0);
    lv_obj_set_style_pad_hor(toast_lbl, 20, 0);
    lv_obj_set_style_pad_ver(toast_lbl, 10, 0);
    lv_obj_set_style_border_color(toast_lbl, lv_color_white(), 0);
    lv_obj_set_style_border_opa(toast_lbl, 50, 0);
    lv_obj_set_style_border_width(toast_lbl, 1, 0);
    lv_obj_set_style_max_width(toast_lbl, 320, 0);
    lv_label_set_long_mode(toast_lbl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(toast_lbl, LV_ALIGN_BOTTOM_MID, 0, -96);
    lv_obj_add_flag(toast_lbl, LV_OBJ_FLAG_HIDDEN);

    reset_lbl = mk_label(lv_layer_top(), &inter_26, 0xFFFFFF, "Press the Boot button\nto erase everything");
    lv_obj_set_style_bg_color(reset_lbl, lv_color_hex(0x3A0F18), 0);
    lv_obj_set_style_bg_opa(reset_lbl, 245, 0);
    lv_obj_set_style_radius(reset_lbl, 30, 0);
    lv_obj_set_style_pad_all(reset_lbl, 22, 0);
    lv_obj_set_style_border_color(reset_lbl, lv_color_hex(C_BAD), 0);
    lv_obj_set_style_border_width(reset_lbl, 2, 0);
    lv_obj_center(reset_lbl);
    lv_obj_add_flag(reset_lbl, LV_OBJ_FLAG_HIDDEN);

    app_status(&last_st);
    memset(stack, 0, sizeof stack);
    depth = 0;
    stack[0].kind = last_st.setup_mode ? SCR_SETUP : SCR_HOME;
    build(false, false);
    lv_timer_create(refresh_cb, 150, NULL);
    ui_boot_splash();
}
