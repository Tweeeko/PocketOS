// Home screen: the companion. Its face follows the home's state (see model_mood) and reacts to
// touches. Shapes follow the browser prototype's SVG face, scaled to a 250 px orb.
#include "ui_priv.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ORB 250
static lv_obj_t *orb, *face, *time_lbl, *bat_lbl, *wifi_ic, *cap_lbl, *cap_dot, *cap;
static lv_obj_t *eyes[2];
static int eye_h;
static mood_t cur_mood = (mood_t)-1, override = (mood_t)-1;
static uint32_t override_until;
static lv_timer_t *blink_t, *look_t;
static anim_t anim_level = ANIM_LIVELY;

static const uint32_t EYE_COLORS[EYE_COUNT] = {0x6AA7FF, 0x9B7BFF, 0xE8ECFF};

// prototype face coordinates (100-unit SVG, orb centre at 50,55, radius 40) -> pixels
#define PX(x) ((int)(((x) - 50) * 3.125f + ORB / 2))
#define PY(y) ((int)(((y) - 55) * 3.125f + ORB / 2))
#define U(v)  ((int)((v) * 3.125f))

static lv_obj_t *ellipse(lv_obj_t *p, int cx, int cy, int w, int h, uint32_t col)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, cx - w / 2, cy - h / 2);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_color(o, lv_color_hex(col), 0);
    lv_obj_set_style_shadow_width(o, 18, 0);
    lv_obj_set_style_shadow_opa(o, 120, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// Curved stroke (eyelid ^, smile, closed eye) with an lv_arc showing only the indicator
static lv_obj_t *stroke_arc(lv_obj_t *p, int cx, int cy, int r, int a0, int a1, int w, uint32_t col)
{
    lv_obj_t *a = lv_arc_create(p);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, r * 2, r * 2);
    lv_obj_set_pos(a, cx - r, cy - r);
    lv_arc_set_bg_angles(a, (lv_value_precise_t)a0, (lv_value_precise_t)a1);
    lv_arc_set_angles(a, (lv_value_precise_t)a0, (lv_value_precise_t)a1);
    lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(col), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    return a;
}

static lv_point_precise_t pts[12][3];
static int npts;
static lv_obj_t *stroke_line(lv_obj_t *p, int x0, int y0, int x1, int y1, int x2, int y2, int n, int w, uint32_t col)
{
    if (npts >= 12) return NULL;
    lv_point_precise_t *q = pts[npts++];
    q[0].x = x0; q[0].y = y0; q[1].x = x1; q[1].y = y1; q[2].x = x2; q[2].y = y2;
    lv_obj_t *l = lv_line_create(p);
    lv_line_set_points(l, q, (uint32_t)n);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, lv_color_hex(col), 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static lv_obj_t *half_eye(lv_obj_t *p, int cx, int cy, int w, uint32_t col)
{
    // lower half of a circle (sleepy / hot eyes) + an eyelid line
    lv_obj_t *clip = lv_obj_create(p);
    lv_obj_remove_style_all(clip);
    lv_obj_set_size(clip, w, w / 2);
    lv_obj_set_pos(clip, cx - w / 2, cy);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE);
    ellipse(clip, w / 2, 0, w, w, col);
    stroke_line(p, cx - w / 2 - 6, cy, cx + w / 2 + 6, cy, 0, 0, 2, 7, col);
    return clip;
}

static lv_obj_t *glyph(lv_obj_t *p, const char *txt, const lv_font_t *f, int x, int y, uint32_t col)
{
    lv_obj_t *l = mk_label(p, f, col, txt);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void float_anim(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *)o, v, 0); }
static void bob_x(void *o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *)o, v, 0); }

static void anim_loop(lv_obj_t *o, lv_anim_exec_xcb_t cb, int from, int to, int ms, bool back)
{
    if (anim_level == ANIM_OFF) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, (uint32_t)(anim_level == ANIM_CALM ? ms * 3 / 2 : ms));
    if (back) lv_anim_set_reverse_duration(&a, (uint32_t)(anim_level == ANIM_CALM ? ms * 3 / 2 : ms));
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static uint32_t mood_color(mood_t m, uint32_t eye)
{
    switch (m) {
    case MOOD_HOT: return 0xFFA070;
    case MOOD_DANCE: return 0xFF7BD5;
    case MOOD_COLD: return 0x8FD3FF;
    case MOOD_TIRED: return 0xFFC46B;
    case MOOD_ALERT: return 0xFFD166;
    case MOOD_CHARGE: return 0x7BE3A4;
    case MOOD_WORRIED: return 0xB6A8FF;
    default: return eye;
    }
}

static void draw_face(mood_t m, uint32_t eye)
{
    lv_anim_delete(orb, NULL);
    lv_obj_set_style_translate_y(orb, 0, 0);
    lv_obj_set_style_translate_x(orb, 0, 0);
    if (face) lv_obj_delete(face);
    npts = 0;
    eyes[0] = eyes[1] = NULL;
    face = lv_obj_create(orb);
    lv_obj_remove_style_all(face);
    lv_obj_set_size(face, ORB, ORB);
    lv_obj_remove_flag(face, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(face, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(face, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    uint32_t c = mood_color(m, eye);
    lv_obj_set_style_border_color(orb, lv_color_hex(m == MOOD_HOT ? 0xFF8C5A : 0xFFFFFF), 0);
    lv_obj_set_style_border_opa(orb, m == MOOD_HOT ? 160 : 40, 0);
    lv_obj_set_style_shadow_color(orb, lv_color_hex(c), 0);

    switch (m) {
    case MOOD_SLEEP:
        stroke_arc(face, PX(34), PY(46), U(9), 30, 150, 11, c);
        stroke_arc(face, PX(66), PY(46), U(9), 30, 150, 11, c);
        glyph(face, "z", &inter_26, PX(72), PY(22), c);
        glyph(face, "z", &inter_18, PX(80), PY(14), c);
        break;
    case MOOD_HAPPY: case MOOD_DANCE: case MOOD_CHARGE:
        stroke_arc(face, PX(34), PY(58), U(9), 200, 340, 13, c);
        stroke_arc(face, PX(66), PY(58), U(9), 200, 340, 13, c);
        if (m == MOOD_DANCE) {
            lv_obj_t *mo = ellipse(face, PX(50), PY(68), U(11), U(8), c);
            anim_loop(mo, float_anim, 0, -4, 350, true);
            lv_obj_t *n1 = glyph(face, LV_SYMBOL_AUDIO, &lv_font_montserrat_28, PX(6), PY(26), c);
            lv_obj_t *n2 = glyph(face, LV_SYMBOL_AUDIO, &lv_font_montserrat_20, PX(84), PY(32), c);
            anim_loop(n1, float_anim, 6, -14, 900, true);
            anim_loop(n2, float_anim, -10, 8, 800, true);
            anim_loop(orb, float_anim, 0, -10, 330, true);
            anim_loop(orb, bob_x, -6, 6, 660, true);
        } else {
            stroke_arc(face, PX(50), PY(60), U(10), 30, 150, 9, c);
            if (m == MOOD_CHARGE) glyph(face, LV_SYMBOL_CHARGE, &lv_font_montserrat_28, PX(76), PY(22), c);
        }
        break;
    case MOOD_HOT: case MOOD_TIRED:
        half_eye(face, PX(34), PY(52), U(16), c);
        half_eye(face, PX(66), PY(52), U(16), c);
        if (m == MOOD_HOT) {
            lv_obj_t *mo = ellipse(face, PX(50), PY(69), U(9), U(6), c);
            anim_loop(mo, float_anim, 0, 3, 500, true);
            ellipse(face, PX(21), PY(63), U(10), U(10), 0xFF5A4A);
            ellipse(face, PX(79), PY(63), U(10), U(10), 0xFF5A4A);
            lv_obj_set_style_bg_opa(lv_obj_get_child(face, -1), 80, 0);
            lv_obj_set_style_bg_opa(lv_obj_get_child(face, -2), 80, 0);
            lv_obj_t *drop = ellipse(face, PX(80), PY(32), U(6), U(9), 0x8FD3FF);
            anim_loop(drop, float_anim, 0, 26, 1600, false);
        } else {
            stroke_line(face, PX(43), PY(71), PX(57), PY(71), 0, 0, 2, 9, c);
        }
        break;
    case MOOD_SAD: case MOOD_WORRIED:
        stroke_line(face, PX(25), PY(43), PX(41), PY(39), 0, 0, 2, 10, c);
        stroke_line(face, PX(75), PY(43), PX(59), PY(39), 0, 0, 2, 10, c);
        eyes[0] = ellipse(face, PX(34), PY(56), U(12), U(16), c);
        eyes[1] = ellipse(face, PX(66), PY(56), U(12), U(16), c);
        eye_h = U(16);
        stroke_arc(face, PX(50), PY(78), U(10), 220, 320, 9, c);
        if (m == MOOD_WORRIED) glyph(face, "?", &inter_26, PX(78), PY(22), c);
        break;
    case MOOD_ALERT:
        stroke_line(face, PX(26), PY(38), PX(40), PY(42), 0, 0, 2, 10, c);
        stroke_line(face, PX(74), PY(38), PX(60), PY(42), 0, 0, 2, 10, c);
        eyes[0] = ellipse(face, PX(34), PY(54), U(15), U(21), c);
        eyes[1] = ellipse(face, PX(66), PY(54), U(15), U(21), c);
        eye_h = U(21);
        ellipse(face, PX(50), PY(72), U(7), U(6), c);
        glyph(face, "!", &inter_40, PX(78), PY(16), c);
        break;
    case MOOD_COLD:
        stroke_line(face, PX(26), PY(47), PX(39), PY(53), PX(26), PY(59), 3, 12, c);
        stroke_line(face, PX(74), PY(47), PX(61), PY(53), PX(74), PY(59), 3, 12, c);
        stroke_line(face, PX(40), PY(71), PX(50), PY(68), PX(60), PY(71), 3, 8, c);
        anim_loop(orb, bob_x, -3, 3, 90, true);
        break;
    default: {   // idle, wake, listen, away
        int h = U(m == MOOD_LISTEN ? 20 : 17), w = U(13);
        eyes[0] = ellipse(face, PX(34), PY(52), w, h, c);
        eyes[1] = ellipse(face, PX(66), PY(52), w, h, c);
        eye_h = h;
        if (m == MOOD_AWAY) stroke_arc(face, PX(50), PY(66), U(7), 40, 140, 8, c);
        if (m != MOOD_WAKE) anim_loop(orb, float_anim, 0, -5, 2200, true);   // breathing
        break;
    }
    }
}

/* blink and look-around */
static void unblink_cb(lv_timer_t *t)
{
    for (int i = 0; i < 2; i++) if (eyes[i]) { int y = lv_obj_get_y(eyes[i]) + lv_obj_get_height(eyes[i]) / 2; lv_obj_set_height(eyes[i], eye_h); lv_obj_set_y(eyes[i], y - eye_h / 2); }
}

static void blink_cb(lv_timer_t *t)
{
    lv_timer_set_period(t, 2500 + lv_rand(0, 3500));
    if (anim_level == ANIM_OFF || !eyes[0] || ui_asleep_flag()) return;
    for (int i = 0; i < 2; i++) { int y = lv_obj_get_y(eyes[i]) + eye_h / 2; lv_obj_set_height(eyes[i], 6); lv_obj_set_y(eyes[i], y - 3); }
    lv_timer_t *u = lv_timer_create(unblink_cb, 120, NULL);
    lv_timer_set_repeat_count(u, 1);
}

static void look_cb(lv_timer_t *t)
{
    lv_timer_set_period(t, 3500 + lv_rand(0, 3500));
    if (anim_level != ANIM_LIVELY || !eyes[0] || !face) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, face);
    lv_anim_set_exec_cb(&a, bob_x);
    lv_anim_set_values(&a, 0, (int32_t)lv_rand(0, 28) - 14);
    lv_anim_set_duration(&a, 450);
    lv_anim_set_reverse_delay(&a, 900);
    lv_anim_set_reverse_duration(&a, 450);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void orb_cb(lv_event_t *e) { home_happy(1300); lv_event_stop_bubbling(e); }
static void open_menu_cb(lv_event_t *e) { if (lv_event_get_target(e) == lv_event_get_current_target(e)) ui_open_menu(); }
static void home_gesture(lv_event_t *e)
{
    lv_indev_t *in = lv_indev_active();
    if (in && (lv_indev_get_gesture_dir(in) == LV_DIR_TOP || lv_indev_get_gesture_dir(in) == LV_DIR_LEFT)) { lv_indev_wait_release(in); ui_open_menu(); }
}

static void del_cb(lv_event_t *e)
{
    if (blink_t) { lv_timer_delete(blink_t); blink_t = NULL; }
    if (look_t) { lv_timer_delete(look_t); look_t = NULL; }
    orb = face = NULL;
    eyes[0] = eyes[1] = NULL;
}

void home_build(lv_obj_t *scr)
{
    cur_mood = (mood_t)-1;
    face = NULL;
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, open_menu_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scr, home_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(scr, del_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    wifi_ic = mk_icon(row, &ic_wifi_22, C_DIM);
    bat_lbl = mk_label(row, &inter_15, C_DIM, "");

    time_lbl = mk_label(scr, &inter_40, C_INK, "--:--");
    lv_obj_align(time_lbl, LV_ALIGN_TOP_MID, 0, 50);

    orb = lv_obj_create(scr);
    lv_obj_remove_style_all(orb);
    lv_obj_set_size(orb, ORB, ORB);
    lv_obj_align(orb, LV_ALIGN_CENTER, 0, 12);
    lv_obj_set_style_radius(orb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(orb, lv_color_hex(0x2A2F55), 0);
    lv_obj_set_style_bg_grad_color(orb, lv_color_hex(0x0C0E1C), 0);
    lv_obj_set_style_bg_grad_dir(orb, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(orb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(orb, 1, 0);
    lv_obj_set_style_shadow_width(orb, 60, 0);
    lv_obj_set_style_shadow_opa(orb, 60, 0);
    lv_obj_remove_flag(orb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(orb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(orb, orb_cb, LV_EVENT_CLICKED, NULL);
    // glossy highlight
    lv_obj_t *hl = lv_obj_create(orb);
    lv_obj_remove_style_all(hl);
    lv_obj_set_size(hl, 150, 70);
    lv_obj_set_pos(hl, 30, 14);
    lv_obj_set_style_radius(hl, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hl, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_color(hl, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(hl, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_opa(hl, 34, 0);
    lv_obj_set_style_bg_grad_opa(hl, 0, 0);
    lv_obj_set_style_bg_grad_dir(hl, LV_GRAD_DIR_VER, 0);
    lv_obj_remove_flag(hl, LV_OBJ_FLAG_CLICKABLE);

    cap = lv_obj_create(scr);
    lv_obj_remove_style_all(cap);
    lv_obj_add_style(cap, &st_glass, 0);
    lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(cap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(cap, 16, 0);
    lv_obj_set_style_pad_ver(cap, 7, 0);
    lv_obj_set_style_pad_column(cap, 8, 0);
    lv_obj_set_flex_flow(cap, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(cap, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_remove_flag(cap, LV_OBJ_FLAG_CLICKABLE);
    cap_dot = lv_obj_create(cap);
    lv_obj_remove_style_all(cap_dot);
    lv_obj_set_size(cap_dot, 9, 9);
    lv_obj_set_style_radius(cap_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(cap_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(cap_dot, 10, 0);
    cap_lbl = mk_label(cap, &inter_18, C_INK, "");

    blink_t = lv_timer_create(blink_cb, 3000, NULL);
    look_t = lv_timer_create(look_cb, 4000, NULL);
    home_refresh();
}

void home_mood_override(int mood, int ms)
{
    override = (mood_t)mood;
    override_until = app_ms() + (uint32_t)ms;
    home_refresh();
}

void home_happy(int ms) { home_mood_override(MOOD_HAPPY, ms); }

void home_refresh(void)
{
    if (!orb) return;
    app_status_t st;
    app_status(&st);
    const settings_t *s = app_settings_lock();
    eye_t eye = s->eye;
    anim_level = s->anim;
    bool h24 = s->clock_24h;
    model_t *m = app_model_lock();
    mood_inputs_t in = {.ha_ok = st.ha == HA_OK || st.ha == HA_NOT_SET, .home_wifi = st.wifi, .battery = st.battery, .charging = st.charging};
    mood_t md = model_mood(m, s, &in);
    char capbuf[64];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    const char *caption;
    if (override != (mood_t)-1 && (int32_t)(override_until - app_ms()) > 0) md = override;
    else override = (mood_t)-1;
    if (ui_asleep_flag()) md = MOOD_SLEEP;
    caption = mood_caption(md, m, st.time_ok ? tm.tm_hour : -1, capbuf, sizeof capbuf);
    if (st.ha == HA_NOT_SET && md == MOOD_IDLE) caption = "Hello! Scan Settings to set me up";
    app_model_unlock();
    app_settings_unlock();

    if (md != cur_mood) { cur_mood = md; draw_face(md, EYE_COLORS[eye]); }
    lv_label_set_text(cap_lbl, caption);
    bool bad = st.ha == HA_ERROR || !st.wifi;
    lv_obj_set_style_bg_color(cap_dot, lv_color_hex(bad ? C_BAD : C_OK), 0);
    lv_obj_set_style_shadow_color(cap_dot, lv_color_hex(bad ? C_BAD : C_OK), 0);

    char tb[16];
    if (!st.time_ok) lv_snprintf(tb, sizeof tb, "--:--");
    else if (h24) lv_snprintf(tb, sizeof tb, "%02d:%02d", tm.tm_hour, tm.tm_min);
    else lv_snprintf(tb, sizeof tb, "%d:%02d", tm.tm_hour % 12 ? tm.tm_hour % 12 : 12, tm.tm_min);
    lv_label_set_text(time_lbl, tb);

    char bb[24];
    if (st.battery < 0) bb[0] = 0;
    else lv_snprintf(bb, sizeof bb, "%s%d%%", st.charging ? "+ " : "", st.battery);
    lv_label_set_text(bat_lbl, bb);
    lv_obj_set_style_image_recolor(wifi_ic, lv_color_hex(st.wifi ? C_DIM : C_BAD), 0);
}
