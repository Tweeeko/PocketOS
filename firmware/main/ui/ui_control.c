// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Device control screen. Anything with a level gets a 270° glass dial around the edge; on/off-only
// devices get one big glass button. Values update live from Home Assistant; while a command is
// on its way the screen says "Sending..." and only shows the new value once HA confirms it.
#include "ui_priv.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static lv_obj_t *arc, *val_lbl, *sub_lbl, *btn_row, *big_btn, *big_lbl;
static lv_obj_t *b_tog, *b_l, *b_r;
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
    case D_LIGHT: snprintf(b, sizeof b, "%s", snap.on ? "Brightness" : "Off · drag the ring"); break;
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
    app_act(ent_id, ACT_LEVEL, v);
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

static void del_cb(lv_event_t *e) { arc = NULL; val_lbl = sub_lbl = btn_row = big_btn = big_lbl = NULL; b_tog = b_l = b_r = NULL; }

void ctl_build(lv_obj_t *scr, scr_ref_t *r)
{
    snprintf(ent_id, sizeof ent_id, "%s", r->arg);
    arc = val_lbl = sub_lbl = btn_row = big_btn = big_lbl = NULL;
    b_tog = b_l = b_r = NULL;
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
        case D_LIGHT: case D_FAN:
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
    mk_title(scr, snap.name, true);
    ctl_refresh();
}

void ctl_refresh(void)
{
    if (!val_lbl && !big_lbl) return;
    if (!load()) { ui_back(); ui_toast("That device was removed", 2000); return; }
    if (arc) {
        bool dragging = lv_obj_has_state(arc, LV_STATE_PRESSED);
        if (!dragging) {
            lv_arc_set_value(arc, (int32_t)roundf(shown_level() * scale));
            show_value(shown_level());
        }
        bool dim = (snap.dom == D_LIGHT || snap.dom == D_FAN || snap.dom == D_CLIMATE) && !snap.on && !snap.has_pending_level;
        lv_obj_set_style_arc_opa(arc, dim ? 90 : 255, LV_PART_INDICATOR);
        set_sub();
    }
    update_buttons();
}
