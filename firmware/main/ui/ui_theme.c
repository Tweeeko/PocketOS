// Liquid Glass styling for LVGL: frosted gradient panels, fine bright borders, soft glows.
// Real blur is too expensive on the ESP32-S3, so "glass" is a translucent white gradient over a
// pre-rendered glowing background image.
#include "ui_priv.h"
#include <string.h>

lv_style_t st_screen, st_glass, st_glass_sel, st_glass_on, st_title, st_dim, st_btn_round, st_btn_on, st_pressed;

void theme_init(void)
{
    lv_style_init(&st_screen);
    lv_style_set_bg_color(&st_screen, lv_color_black());
    lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);
    lv_style_set_bg_image_src(&st_screen, &img_bg);
    lv_style_set_text_color(&st_screen, lv_color_hex(C_INK));
    lv_style_set_text_font(&st_screen, &inter_20);
    lv_style_set_pad_all(&st_screen, 0);
    lv_style_set_border_width(&st_screen, 0);

    lv_style_init(&st_glass);
    lv_style_set_radius(&st_glass, 30);
    lv_style_set_bg_color(&st_glass, lv_color_white());
    lv_style_set_bg_opa(&st_glass, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&st_glass, lv_color_white());
    lv_style_set_bg_main_opa(&st_glass, 40);
    lv_style_set_bg_grad_opa(&st_glass, 10);
    lv_style_set_bg_grad_dir(&st_glass, LV_GRAD_DIR_VER);
    lv_style_set_border_color(&st_glass, lv_color_white());
    lv_style_set_border_opa(&st_glass, 38);
    lv_style_set_border_width(&st_glass, 1);
    lv_style_set_pad_all(&st_glass, 0);
    lv_style_set_shadow_width(&st_glass, 0);
    lv_style_set_text_color(&st_glass, lv_color_hex(C_INK));

    lv_style_init(&st_glass_sel);   // brighter edge + blue glow (focused tile / centre row)
    lv_style_set_bg_main_opa(&st_glass_sel, 58);
    lv_style_set_border_opa(&st_glass_sel, 110);
    lv_style_set_shadow_color(&st_glass_sel, lv_color_hex(C_BLUE));
    lv_style_set_shadow_width(&st_glass_sel, 26);
    lv_style_set_shadow_opa(&st_glass_sel, 70);

    lv_style_init(&st_glass_on);    // something is on: blue tint
    lv_style_set_bg_color(&st_glass_on, lv_color_hex(C_BLUE));
    lv_style_set_bg_grad_color(&st_glass_on, lv_color_hex(C_VIOLET));
    lv_style_set_bg_main_opa(&st_glass_on, 110);
    lv_style_set_bg_grad_opa(&st_glass_on, 60);
    lv_style_set_border_opa(&st_glass_on, 120);
    lv_style_set_shadow_color(&st_glass_on, lv_color_hex(C_BLUE));
    lv_style_set_shadow_width(&st_glass_on, 34);
    lv_style_set_shadow_opa(&st_glass_on, 110);

    lv_style_init(&st_title);
    lv_style_set_text_font(&st_title, &inter_22);
    lv_style_set_text_color(&st_title, lv_color_hex(C_DIM));

    lv_style_init(&st_dim);
    lv_style_set_text_color(&st_dim, lv_color_hex(C_DIM));

    lv_style_init(&st_btn_round);
    lv_style_set_radius(&st_btn_round, LV_RADIUS_CIRCLE);
    lv_style_set_bg_main_opa(&st_btn_round, 30);
    lv_style_set_bg_grad_opa(&st_btn_round, 24);
    lv_style_set_border_opa(&st_btn_round, 60);

    lv_style_init(&st_btn_on);
    lv_style_set_bg_color(&st_btn_on, lv_color_hex(C_BLUE));
    lv_style_set_bg_grad_color(&st_btn_on, lv_color_hex(C_VIOLET));
    lv_style_set_bg_main_opa(&st_btn_on, 150);
    lv_style_set_bg_grad_opa(&st_btn_on, 120);
    lv_style_set_border_opa(&st_btn_on, 130);
    lv_style_set_shadow_color(&st_btn_on, lv_color_hex(C_BLUE));
    lv_style_set_shadow_width(&st_btn_on, 30);
    lv_style_set_shadow_opa(&st_btn_on, 130);

    lv_style_init(&st_pressed);
    lv_style_set_transform_scale(&st_pressed, 236);   // 92 %
    lv_style_set_bg_main_opa(&st_pressed, 80);
}

lv_obj_t *mk_screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_remove_style_all(s);
    lv_obj_add_style(s, &st_screen, 0);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

lv_obj_t *mk_label(lv_obj_t *p, const lv_font_t *f, uint32_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, txt ? txt : "");
    return l;
}

lv_obj_t *mk_icon(lv_obj_t *p, const lv_image_dsc_t *img, uint32_t color)
{
    lv_obj_t *i = lv_image_create(p);
    lv_image_set_src(i, img);
    lv_obj_set_style_image_recolor(i, lv_color_hex(color), 0);
    lv_obj_set_style_image_recolor_opa(i, LV_OPA_COVER, 0);
    lv_obj_remove_flag(i, LV_OBJ_FLAG_CLICKABLE);
    return i;
}

lv_obj_t *mk_round_btn(lv_obj_t *p, int size, const lv_image_dsc_t *icon, bool on)
{
    lv_obj_t *b = lv_obj_create(p);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &st_glass, 0);
    lv_obj_add_style(b, &st_btn_round, 0);
    if (on) lv_obj_add_style(b, &st_btn_on, 0);
    lv_obj_add_style(b, &st_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(b, size, size);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_ext_click_area(b, 8);
    if (icon) lv_obj_center(mk_icon(b, icon, C_INK));
    return b;
}

static void title_cb(lv_event_t *e) { ui_back(); }

lv_obj_t *mk_title(lv_obj_t *scr, const char *text, bool bottom)
{
    char buf[64];
    lv_snprintf(buf, sizeof buf, "< %s", text);
    lv_obj_t *l = mk_label(scr, &inter_22, C_DIM, buf);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(l, 260);
    lv_obj_align(l, bottom ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_TOP_MID, 0, bottom ? -34 : 36);
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(l, 14);
    lv_obj_add_event_cb(l, title_cb, LV_EVENT_CLICKED, NULL);
    return l;
}

const lv_image_dsc_t *tile_icon(const char *key)
{
    if (!strcmp(key, "settings")) return &ic_qr_40;
    if (!strcmp(key, "favourites")) return &ic_star_40;
    if (!strcmp(key, "lights")) return &ic_bulb_40;
    if (!strcmp(key, "heating")) return &ic_flame_40;
    if (!strcmp(key, "media")) return &ic_play_40;
    if (!strcmp(key, "locks")) return &ic_lock_40;
    if (!strcmp(key, "rooms")) return &ic_home_40;
    if (!strcmp(key, "scenes")) return &ic_sparkles_40;
    if (!strcmp(key, "plugs")) return &ic_plug_40;
    if (!strcmp(key, "fans")) return &ic_fan_40;
    if (!strcmp(key, "blinds")) return &ic_blinds_40;
    if (!strcmp(key, "sensors")) return &ic_thermo_40;
    if (!strcmp(key, "more")) return &ic_dots_40;
    return &ic_gear_40;
}
