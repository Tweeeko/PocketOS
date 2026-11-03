// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Device control screen. Anything with a level gets a 270° glass dial around the edge; on/off-only
// devices get one big glass button. Values update live from Home Assistant; while a command is
// on its way the screen says "Sending..." and only shows the new value once HA confirms it.
#include "ui_priv.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static lv_obj_t *arc, *val_lbl, *sub_lbl, *btn_row, *big_btn, *big_lbl;
static lv_obj_t *b_tog, *b_l, *b_r, *b_warm, *b_col;
static int dmode;          // lights: 0 brightness, 1 warmth (white), 2 colour
static float held = -1;    // warmth/colour just chosen, shown until Home Assistant reports it
static uint32_t held_at;
static char ent_id[ID_LEN];
static entity_t snap;      // copy of the entity, refreshed from the model
static int scale = 1;      // arc works in integers: climate uses tenths

static uint32_t dial_color(dom_t d)
{
    switch (d) {
    case D_LIGHT: return 0xFFC46B;
    case D_CLIMATE: return 0xFF7A50;
    case D_MEDIA: return 0xE07BFF;
    case D_FAN: return 0x6AD3C0;
    case D_COVER: return 0x8FA8FF;
    default: return 0x8FD3FF;
    }
}

static bool load(void)
{
    model_t *m = app_model_lock();
    int i = model_find(m, ent_id);
    if (i >= 0) snap = m->e[i];
    app_model_unlock();
    return i >= 0;
}

static float shown_level(void) { return snap.has_pending_level ? snap.pending_level : snap.level; }

/* ---------- lights: warmth and colour on the same dial ---------- */
static uint32_t mix(uint32_t a, uint32_t b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    uint32_t r = 0;
    for (int sh = 0; sh <= 16; sh += 8) r |= (uint32_t)(((a >> sh) & 0xFF) + (((int)((b >> sh) & 0xFF) - (int)((a >> sh) & 0xFF)) * t)) << sh;
    return r;
}
static uint32_t kelvin_rgb(float k) { return mix(0xFFB15A, 0xDDEBFF, (k - snap.kelvin_min) / (snap.kelvin_max - snap.kelvin_min)); }
static uint32_t hue_rgb(float h)
{
    h = fmodf(h, 360.0f) / 60.0f;
    float x = 1 - fabsf(fmodf(h, 2.0f) - 1);
    float r = 0, g = 0, b = 0;
    if (h < 1) { r = 1; g = x; } else if (h < 2) { r = x; g = 1; } else if (h < 3) { g = 1; b = x; }
    else if (h < 4) { g = x; b = 1; } else if (h < 5) { r = x; b = 1; } else { r = 1; b = x; }
    return (uint32_t)(r * 255) << 16 | (uint32_t)(g * 255) << 8 | (uint32_t)(b * 255);
}
static const char *hue_name(float h)
{
    static const char *N[] = {"Red", "Orange", "Yellow", "Lime", "Green", "Teal", "Cyan", "Sky", "Blue", "Purple", "Pink", "Rose"};
    return N[((int)roundf(fmodf(h + 360.0f, 360.0f) / 30.0f)) % 12];
}
static float mode_value(void)
{
    if (held >= 0 && (snap.pending_ms || app_ms() - held_at < 3000)) return held;
    if (dmode == 1) return snap.kelvin > 0 ? snap.kelvin : (snap.kelvin_min + snap.kelvin_max) / 2;
    if (dmode == 2) return snap.sat >= 0 ? snap.hue : 0;
    return shown_level();
}
static void mode_text(float v)
{
    char b[24];
    if (dmode == 1) snprintf(b, sizeof b, "%dK", (int)roundf(v / 50) * 50);
    else snprintf(b, sizeof b, "%s", hue_name(v));
    lv_label_set_text(val_lbl, b);
    uint32_t c = dmode == 1 ? kelvin_rgb(v) : hue_rgb(v);
    lv_obj_set_style_arc_color(arc, lv_color_hex(c), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(arc, lv_color_hex(c), LV_PART_KNOB);
}

static void set_sub(void)
{
    char b[96];
    if (snap.pending_ms) { lv_label_set_text(sub_lbl, "Sending..."); return; }
    if (!snap.avail) { lv_label_set_text(sub_lbl, "Unavailable"); return; }
    switch (snap.dom) {
    case D_CLIMATE:
        if (!isnan(snap.cur_temp)) snprintf(b, sizeof b, "%s · now %.1f°", snap.on ? "Heating" : "Heating off", (double)snap.cur_temp);
        else snprintf(b, sizeof b, "%s", snap.on ? "Heating" : "Heating off");
        break;
    case D_LIGHT: snprintf(b, sizeof b, "%s", dmode == 1 ? "Warmth · drag the ring" : dmode == 2 ? "Colour · drag the ring" : snap.on ? "Brightness" : "Off · drag the ring"); break;
    case D_FAN: snprintf(b, sizeof b, "%s", snap.on ? "Fan speed" : "Off · drag the ring"); break;
    case D_MEDIA: snprintf(b, sizeof b, "Volume%s%s%s", snap.title[0] ? " · " : "", snap.title, snap.on ? "" : " (paused)"); break;
    case D_COVER: model_status(&snap, b, sizeof b); if (!strcmp(b, "Open") || !strcmp(b, "Closed")) snprintf(b, sizeof b, "Position"); break;
    case D_SENSOR: { model_t *m = app_model_lock(); snprintf(b, sizeof b, "%s · live", snap.area >= 0 ? m->a[snap.area].name : "Sensor"); app_model_unlock(); break; }
    default: b[0] = 0;
    }
    lv_label_set_text(sub_lbl, b);
}

static void show_value(float v)
{
    char b[24];
    model_value(&snap, v, b, sizeof b);
    lv_label_set_text(val_lbl, b);
}

static void arc_changed(lv_event_t *e)
{
    if (dmode) { mode_text((float)lv_arc_get_value(arc)); return; }
    float v = (float)lv_arc_get_value(arc) / (float)scale;
    entity_t tmp = snap;
    tmp.has_pending_level = true;   // show "70%" rather than "Off" while dragging
    char b[24];
    model_value(&tmp, v, b, sizeof b);
    lv_label_set_text(val_lbl, b);
}

static void arc_released(lv_event_t *e)
{
    float v = (float)lv_arc_get_value(arc) / (float)scale;
    if (dmode) { v = (float)lv_arc_get_value(arc); held = v; held_at = app_ms(); }
    app_act(ent_id, dmode == 1 ? ACT_WARMTH : dmode == 2 ? ACT_HUE : ACT_LEVEL, v);
    ui_wake();
}

static void apply_mode(void);
static void mode_cb(lv_event_t *e)
{
    int m = (int)(intptr_t)lv_event_get_user_data(e);
    dmode = dmode == m ? 0 : m;   // tap again to go back to brightness
    held = -1;
    apply_mode();
    ui_wake();
}

static void btn_cb(lv_event_t *e)
{
    act_t a = (act_t)(intptr_t)lv_event_get_user_data(e);
    app_act(ent_id, a, 0);
    ui_wake();
}

static lv_obj_t *button(lv_obj_t *p, int size, const lv_image_dsc_t *ic, act_t a, bool on)
{
    lv_obj_t *b = mk_round_btn(p, size, ic, on);
    lv_obj_add_style(b, &st_btn_on, LV_STATE_CHECKED);
    if (on) lv_obj_add_state(b, LV_STATE_CHECKED);
    lv_obj_add_event_cb(b, btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)a);
    return b;
}

static void update_buttons(void)
{
    if (b_tog) {
        bool on = snap.on;
        if (on) lv_obj_add_state(b_tog, LV_STATE_CHECKED); else lv_obj_remove_state(b_tog, LV_STATE_CHECKED);
        if (snap.dom == D_MEDIA) lv_image_set_src(lv_obj_get_child(b_tog, 0), on ? &ic_pause_30 : &ic_play_30);
    }
    if (big_btn) {
        bool lock = snap.dom == D_LOCK;
        if (snap.on) lv_obj_add_state(big_btn, LV_STATE_CHECKED); else lv_obj_remove_state(big_btn, LV_STATE_CHECKED);
        if (lock) lv_image_set_src(lv_obj_get_child(big_btn, 0), snap.on ? &ic_lock_84 : &ic_unlock_84);
        if (snap.dom == D_MEDIA) lv_image_set_src(lv_obj_get_child(big_btn, 0), snap.on ? &ic_pause_84 : &ic_play_84);
        char b[48];
        if (snap.pending_ms) snprintf(b, sizeof b, lock ? (snap.on ? "Unlocking..." : "Locking...") : "Sending...");
        else model_status(&snap, b, sizeof b);
        lv_label_set_text(big_lbl, b);
    }
}

static void opts_cb(lv_event_t *e) { ui_device_options(ent_id); }

static void del_cb(lv_event_t *e) { arc = NULL; val_lbl = sub_lbl = btn_row = big_btn = big_lbl = NULL; b_tog = b_l = b_r = b_warm = b_col = NULL; }

// Point the dial at brightness, warmth or colour
static void apply_mode(void)
{
    if (!arc) return;
    if (b_warm) { if (dmode == 1) lv_obj_add_state(b_warm, LV_STATE_CHECKED); else lv_obj_remove_state(b_warm, LV_STATE_CHECKED); }
    if (b_col) { if (dmode == 2) lv_obj_add_state(b_col, LV_STATE_CHECKED); else lv_obj_remove_state(b_col, LV_STATE_CHECKED); }
    if (dmode == 1) lv_arc_set_range(arc, (int32_t)snap.kelvin_min, (int32_t)snap.kelvin_max);
    else if (dmode == 2) lv_arc_set_range(arc, 0, 359);
    else {
        lv_arc_set_range(arc, (int32_t)roundf(snap.lmin * scale), (int32_t)roundf(snap.lmax * scale));
        lv_obj_set_style_arc_color(arc, lv_color_hex(dial_color(snap.dom)), LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(arc, lv_color_hex(dial_color(snap.dom)), LV_PART_KNOB);
    }
    ctl_refresh();
}

void ctl_build(lv_obj_t *scr, scr_ref_t *r)
{
    snprintf(ent_id, sizeof ent_id, "%s", r->arg);
    arc = val_lbl = sub_lbl = btn_row = big_btn = big_lbl = NULL;
    b_tog = b_l = b_r = b_warm = b_col = NULL;
    dmode = 0;
    held = -1;
    lv_obj_add_event_cb(scr, del_cb, LV_EVENT_DELETE, NULL);
    if (!load()) {
        lv_obj_center(mk_label(scr, &inter_20, C_DIM, "This device is no longer\nin Home Assistant"));
        mk_title(scr, "Back", true);
        return;
    }
    bool dial = (snap.caps & CAP_LEVEL) != 0;
    if (dial) {
        scale = (snap.lstep < 1.0f) ? 10 : 1;
        arc = lv_arc_create(scr);
        lv_obj_set_size(arc, 430, 430);
        lv_obj_center(arc);
        lv_arc_set_rotation(arc, 135);
        lv_arc_set_bg_angles(arc, 0, 270);
        lv_arc_set_range(arc, (int32_t)roundf(snap.lmin * scale), (int32_t)roundf(snap.lmax * scale));
        lv_arc_set_value(arc, (int32_t)roundf(shown_level() * scale));
        uint32_t col = dial_color(snap.dom);
        lv_obj_set_style_arc_width(arc, 22, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, 28, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 22, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_hex(col), LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(arc, lv_color_white(), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc, 5, LV_PART_KNOB);
        lv_obj_set_style_shadow_width(arc, 16, LV_PART_KNOB);
        lv_obj_set_style_shadow_color(arc, lv_color_hex(col), LV_PART_KNOB);
        lv_obj_set_style_shadow_opa(arc, 160, LV_PART_KNOB);
        if (snap.caps & CAP_READONLY) {
            lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        } else {
            lv_obj_add_event_cb(arc, arc_changed, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_add_event_cb(arc, arc_released, LV_EVENT_RELEASED, NULL);
        }

        val_lbl = mk_label(scr, &inter_76, C_INK, "");
        lv_obj_align(val_lbl, LV_ALIGN_CENTER, 0, -46);
        sub_lbl = mk_label(scr, &inter_18, C_DIM, "");
        lv_label_set_long_mode(sub_lbl, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(sub_lbl, 280);
        lv_obj_align(sub_lbl, LV_ALIGN_CENTER, 0, 8);

        btn_row = lv_obj_create(scr);
        lv_obj_remove_style_all(btn_row);
        lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btn_row, 16, 0);
        lv_obj_align(btn_row, LV_ALIGN_CENTER, 0, 82);
        lv_obj_add_flag(btn_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        switch (snap.dom) {
        case D_CLIMATE:
            b_l = button(btn_row, 52, &ic_minus_30, ACT_STEP_DOWN, false);
            if (snap.caps & CAP_TOGGLE) b_tog = button(btn_row, 72, &ic_flame_30, ACT_TOGGLE, snap.on);
            b_r = button(btn_row, 52, &ic_plus_30, ACT_STEP_UP, false);
            break;
        case D_LIGHT:
            // Warmth and colour, for bulbs that have them: they switch what the dial sets
            if (snap.caps & CAP_WARMTH) { b_warm = mk_round_btn(btn_row, 52, &ic_sun_30, false); lv_obj_add_style(b_warm, &st_btn_on, LV_STATE_CHECKED); lv_obj_add_event_cb(b_warm, mode_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1); }
            b_tog = button(btn_row, 72, &ic_power_30, ACT_TOGGLE, snap.on);
            if (snap.caps & CAP_COLOUR) { b_col = mk_round_btn(btn_row, 52, &ic_palette_30, false); lv_obj_add_style(b_col, &st_btn_on, LV_STATE_CHECKED); lv_obj_add_event_cb(b_col, mode_cb, LV_EVENT_CLICKED, (void *)(intptr_t)2); }
            break;
        case D_FAN:
            b_tog = button(btn_row, 72, &ic_power_30, ACT_TOGGLE, snap.on);
            break;
        case D_MEDIA:
            if (snap.caps & CAP_PREVNEXT) b_l = button(btn_row, 52, &ic_prev_30, ACT_PREV, false);
            if (snap.caps & CAP_TOGGLE) b_tog = button(btn_row, 72, snap.on ? &ic_pause_30 : &ic_play_30, ACT_TOGGLE, snap.on);
            if (snap.caps & CAP_PREVNEXT) b_r = button(btn_row, 52, &ic_next_30, ACT_NEXT, false);
            break;
        case D_COVER:
            if (snap.caps & CAP_OPENCLOSE) button(btn_row, 52, &ic_down_30, ACT_CLOSE, false);
            if (snap.caps & CAP_STOP) button(btn_row, 52, &ic_stop_30, ACT_STOP, false);
            if (snap.caps & CAP_OPENCLOSE) button(btn_row, 52, &ic_up_30, ACT_OPEN, false);
            break;
        default: break;
        }
    } else {
        // On/off-only: lock, plug, simple light or fan, play/pause-only speaker
        const lv_image_dsc_t *ic = snap.dom == D_LOCK ? (snap.on ? &ic_lock_84 : &ic_unlock_84)
                                 : snap.dom == D_MEDIA ? (snap.on ? &ic_pause_84 : &ic_play_84) : &ic_power_84;
        big_btn = mk_round_btn(scr, 200, ic, false);
        lv_obj_add_style(big_btn, &st_btn_on, LV_STATE_CHECKED);
        lv_obj_align(big_btn, LV_ALIGN_CENTER, 0, -26);
        lv_obj_add_event_cb(big_btn, btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ACT_TOGGLE);
        big_lbl = mk_label(scr, &inter_26, C_INK, "");
        lv_obj_align(big_lbl, LV_ALIGN_CENTER, 0, 108);
        if (!(snap.caps & CAP_TOGGLE)) lv_obj_remove_flag(big_btn, LV_OBJ_FLAG_CLICKABLE);
    }
    {
        const settings_t *s = app_settings_lock();
        char nm[NAME_LEN + 8];
        snprintf(nm, sizeof nm, "%s", model_name(s, &snap));
        app_settings_unlock();
        lv_obj_t *t = mk_title(scr, nm, true);
        lv_obj_add_event_cb(t, opts_cb, LV_EVENT_LONG_PRESSED, NULL);
    }
    // "..." at the top: favourite, rename or hide this device
    lv_obj_t *o = mk_round_btn(scr, 44, &ic_dots_40, false);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_ext_click_area(o, 10);
    lv_obj_add_event_cb(o, opts_cb, LV_EVENT_CLICKED, NULL);
    ctl_refresh();
}

void ctl_refresh(void)
{
    if (!val_lbl && !big_lbl) return;
    if (!load()) { ui_back(); ui_toast("That device was removed", 2000); return; }
    if (arc) {
        bool dragging = lv_obj_has_state(arc, LV_STATE_PRESSED);
        if (!dragging && dmode) {
            lv_arc_set_value(arc, (int32_t)roundf(mode_value()));
            mode_text(mode_value());
        } else if (!dragging) {
            lv_arc_set_value(arc, (int32_t)roundf(shown_level() * scale));
            show_value(shown_level());
        }
        bool dim = !dmode && (snap.dom == D_LIGHT || snap.dom == D_FAN || snap.dom == D_CLIMATE) && !snap.on && !snap.has_pending_level;
        lv_obj_set_style_arc_opa(arc, dim ? 90 : 255, LV_PART_INDICATOR);
        set_sub();
    }
    update_buttons();
}
