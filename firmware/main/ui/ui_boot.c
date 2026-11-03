// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// Start-up splash: the PocketOS eye mark (brand/pocketos-mark-gradient.svg) fades in on black,
// opens, blinks once and fades away to reveal the first screen, which is built underneath.
#include "ui_priv.h"

// The mark's eyes are 79 x 153 with a 93 gap; drawn here 120 px tall.
#define EYE_H   120
#define EYE_W   62
#define EYE_GAP 73

static lv_obj_t *splash;

static void set_h(void *o, int32_t v) { lv_obj_set_height((lv_obj_t *)o, v); }
static void set_opa(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, (lv_opa_t)v, 0); }
static void done_cb(lv_anim_t *a) { if (splash) { lv_obj_delete(splash); splash = NULL; } }

static lv_obj_t *eye(lv_obj_t *p, int x, uint32_t c0, uint32_t c1)
{
    lv_obj_t *e = lv_obj_create(p);
    lv_obj_remove_style_all(e);
    lv_obj_set_size(e, EYE_W, 6);
    lv_obj_align(e, LV_ALIGN_CENTER, x, 0);
    lv_obj_set_style_radius(e, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(e, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(e, lv_color_hex(c0), 0);
    lv_obj_set_style_bg_grad_color(e, lv_color_hex(c1), 0);
    lv_obj_set_style_bg_grad_dir(e, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_shadow_color(e, lv_color_hex(0x6AA7FF), 0);
    lv_obj_set_style_shadow_width(e, 34, 0);
    lv_obj_set_style_shadow_opa(e, 110, 0);
    lv_obj_remove_flag(e, LV_OBJ_FLAG_CLICKABLE);
    return e;
}

static void anim(void *o, lv_anim_exec_xcb_t cb, int from, int to, int ms, int delay, int back_ms, lv_anim_path_cb_t path, lv_anim_completed_cb_t done)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, (uint32_t)ms);
    lv_anim_set_delay(&a, (uint32_t)delay);
    if (back_ms) lv_anim_set_reverse_duration(&a, (uint32_t)back_ms);
    lv_anim_set_path_cb(&a, path);
    if (done) lv_anim_set_completed_cb(&a, done);
    // several steps share an object and property: don't cancel the earlier ones or jump ahead
    lv_anim_set_early_apply(&a, false);
    lv_anim_start(&a);
}

void ui_boot_splash(void)
{
    splash = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(splash);
    lv_obj_set_size(splash, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(splash, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, 0);
    lv_obj_add_flag(splash, LV_OBJ_FLAG_CLICKABLE);   // swallow touches until it has gone
    lv_obj_remove_flag(splash, LV_OBJ_FLAG_SCROLLABLE);

    // the logo's gradient runs white -> #56B9FF -> #9860F5 across both eyes
    lv_obj_t *l = eye(splash, -(EYE_GAP / 2 + EYE_W / 2), 0xF5F7FC, 0xA6D8FE);
    lv_obj_t *r = eye(splash, EYE_GAP / 2 + EYE_W / 2, 0x62A9FD, 0x9860F5);
    lv_obj_t *eyes[2] = {l, r};
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_opa(eyes[i], 0, 0);
        anim(eyes[i], set_opa, 0, 255, 300, 150, 0, lv_anim_path_ease_out, NULL);                 // appear
        anim(eyes[i], set_h, 6, EYE_H, 420, 380, 0, lv_anim_path_ease_out, NULL);                 // open
        anim(eyes[i], set_h, EYE_H, 8, 90, 1250, 120, lv_anim_path_ease_in_out, NULL);            // blink
    }
    anim(splash, set_opa, 255, 0, 380, 1800, 0, lv_anim_path_ease_in_out, done_cb);              // reveal
}

bool ui_boot_showing(void) { return splash != NULL; }
