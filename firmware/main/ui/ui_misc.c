// Settings QR screen, first-time setup screen and push-to-talk screen.
#include "ui_priv.h"
#include <string.h>
#include <stdio.h>

/* ---------- Settings: QR code to the settings website (always reachable) ---------- */

static lv_obj_t *qr, *qr_host, *qr_pin, *qr_hint;
static char qr_url[64];

static void qr_del(lv_event_t *e) { qr = NULL; }

void qr_build(lv_obj_t *scr)
{
    lv_obj_add_event_cb(scr, qr_del, LV_EVENT_DELETE, NULL);
    mk_title(scr, "Settings", false);
    qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(qr, 172);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(qr, 10, 0);
    lv_obj_set_style_radius(qr, 16, 0);
    lv_obj_align(qr, LV_ALIGN_CENTER, 0, -36);
    qr_url[0] = 0;
    qr_host = mk_label(scr, &inter_18, C_INK, "");
    lv_obj_align(qr_host, LV_ALIGN_CENTER, 0, 76);
    qr_pin = mk_label(scr, &inter_26, C_INK, "");
    lv_obj_align(qr_pin, LV_ALIGN_CENTER, 0, 112);
    qr_hint = mk_label(scr, &inter_15, C_DIM, "");
    lv_obj_align(qr_hint, LV_ALIGN_CENTER, 0, 148);
    qr_refresh();
}

void qr_refresh(void)
{
    if (!qr) return;
    app_status_t st;
    app_status(&st);
    char url[64], host[64], pin[32];
    const char *hint;
    if (st.setup_mode) {
        snprintf(url, sizeof url, "http://192.168.4.1/");
        snprintf(host, sizeof host, "Join %s first", st.ap_ssid);
        hint = "then scan this code";
    } else if (st.wifi) {
        snprintf(url, sizeof url, "http://%s/", st.ip);
        snprintf(host, sizeof host, "%s", st.host);
        hint = "Scan on your phone, on home Wi-Fi";
    } else {
        snprintf(url, sizeof url, "http://%s/", st.host);
        snprintf(host, sizeof host, "Not on Wi-Fi right now");
        hint = "Reconnecting...";
    }
    snprintf(pin, sizeof pin, "PIN  %s", st.pin);
    if (strcmp(url, qr_url)) {
        snprintf(qr_url, sizeof qr_url, "%s", url);
        lv_qrcode_update(qr, url, (uint32_t)strlen(url));
    }
    lv_label_set_text(qr_host, host);
    lv_label_set_text(qr_pin, pin);
    lv_label_set_text(qr_hint, hint);
}

/* ---------- First-time setup ---------- */

static lv_obj_t *su_qr, *su_txt, *su_title;
static char su_data[96];

static void su_del(lv_event_t *e) { su_qr = NULL; }

void setup_build(lv_obj_t *scr)
{
    lv_obj_add_event_cb(scr, su_del, LV_EVENT_DELETE, NULL);
    su_title = mk_label(scr, &inter_26, C_INK, "Hi, I'm PocketOS");
    lv_obj_align(su_title, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_t *s = mk_label(scr, &inter_18, C_DIM, "Scan to join my Wi-Fi");
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 84);
    su_qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(su_qr, 168);
    lv_qrcode_set_dark_color(su_qr, lv_color_black());
    lv_qrcode_set_light_color(su_qr, lv_color_white());
    lv_obj_set_style_border_color(su_qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(su_qr, 10, 0);
    lv_obj_set_style_radius(su_qr, 16, 0);
    lv_obj_align(su_qr, LV_ALIGN_CENTER, 0, -4);
    su_data[0] = 0;
    su_txt = mk_label(scr, &inter_18, C_INK, "");
    lv_obj_align(su_txt, LV_ALIGN_CENTER, 0, 134);
    setup_refresh();
}

void setup_refresh(void)
{
    if (!su_qr) return;
    app_status_t st;
    app_status(&st);
    char d[96], t[160];
    // Standard Wi-Fi QR format understood by iPhone and Android cameras
    snprintf(d, sizeof d, "WIFI:T:WPA;S:%s;P:%s;;", st.ap_ssid, st.ap_pass);
    if (strcmp(d, su_data)) { snprintf(su_data, sizeof su_data, "%s", d); lv_qrcode_update(su_qr, d, (uint32_t)strlen(d)); }
    snprintf(t, sizeof t, "%s  ·  %s\nthen the setup page opens\n(or go to 192.168.4.1)", st.ap_ssid, st.ap_pass);
    lv_label_set_text(su_txt, t);
}

/* ---------- Voice (hold BOOT to talk) ---------- */

static lv_obj_t *v_ring[2], *v_mic, *v_state, *v_heard, *v_reply;
static lv_timer_t *v_close;

static void ring_anim(void *o, int32_t v)
{
    lv_obj_t *r = (lv_obj_t *)o;
    lv_obj_set_size(r, v, v);
    lv_obj_center(r);
    lv_obj_set_style_border_opa(r, (lv_opa_t)LV_CLAMP(0, 255 - (v - 150) * 255 / 230, 255), 0);
}

static void v_del(lv_event_t *e) { v_mic = NULL; if (v_close) { lv_timer_delete(v_close); v_close = NULL; } }
static void v_close_cb(lv_timer_t *t) { v_close = NULL; ui_back(); }

void voice_build(lv_obj_t *scr)
{
    lv_obj_add_event_cb(scr, v_del, LV_EVENT_DELETE, NULL);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *r = lv_obj_create(scr);
        lv_obj_remove_style_all(r);
        lv_obj_set_style_radius(r, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(r, lv_color_hex(C_BLUE), 0);
        lv_obj_set_style_border_width(r, 3, 0);
        lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
        v_ring[i] = r;
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, r);
        lv_anim_set_exec_cb(&a, ring_anim);
        lv_anim_set_values(&a, 150, 380);
        lv_anim_set_duration(&a, 1400);
        lv_anim_set_delay(&a, (uint32_t)(i * 700));
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
    v_mic = mk_round_btn(scr, 150, &ic_mic_84, true);
    lv_obj_remove_flag(v_mic, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(v_mic);
    v_state = mk_label(scr, &inter_22, C_INK, "Listening...");
    lv_obj_align(v_state, LV_ALIGN_TOP_MID, 0, 58);
    v_heard = mk_label(scr, &inter_18, C_DIM, "");
    lv_label_set_long_mode(v_heard, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(v_heard, 300);
    lv_obj_align(v_heard, LV_ALIGN_BOTTOM_MID, 0, -112);
    v_reply = mk_label(scr, &inter_20, C_INK, "");
    lv_label_set_long_mode(v_reply, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(v_reply, 300);
    lv_obj_align(v_reply, LV_ALIGN_BOTTOM_MID, 0, -56);
}

void voice_set(voice_state_t st, const char *text)
{
    if (!v_mic) return;
    switch (st) {
    case VOICE_LISTENING: lv_label_set_text(v_state, "Listening..."); break;
    case VOICE_THINKING: lv_label_set_text(v_state, "Thinking..."); for (int i = 0; i < 2; i++) lv_anim_delete(v_ring[i], NULL); break;
    case VOICE_HEARD: { char b[200]; snprintf(b, sizeof b, "\"%s\"", text ? text : ""); lv_label_set_text(v_heard, b); break; }
    case VOICE_REPLY:
        lv_label_set_text(v_state, "Done");
        lv_label_set_text(v_reply, text ? text : "");
        break;
    case VOICE_ERROR:
        lv_label_set_text(v_state, "Sorry");
        lv_label_set_text(v_reply, text ? text : "Voice didn't work");
        break;
    case VOICE_IDLE: break;
    }
    if (st == VOICE_REPLY || st == VOICE_ERROR || st == VOICE_IDLE) {
        for (int i = 0; i < 2; i++) lv_anim_delete(v_ring[i], NULL);
        if (v_close) lv_timer_delete(v_close);
        v_close = lv_timer_create(v_close_cb, st == VOICE_IDLE ? 600 : 4500, NULL);
        lv_timer_set_repeat_count(v_close, 1);
    }
}
