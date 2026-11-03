// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
// PocketOS screen simulator: runs the real device UI (firmware/main/ui) on a PC with LVGL 9.5,
// fed by a fake Home Assistant (tests/fixture.h) that confirms commands like the real one does.
// Scripted taps and drags exercise the screens; each step is saved as shots/NN_name.ppm.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "lvgl.h"
#include "ui.h"
#include "ui_priv.h"
#include "ui_app.h"
#include "fixture.h"

static model_t M;
static settings_t S;
static ha_ctx_t C;
static app_status_t ST;
static uint32_t now_ms = 1000;
static uint16_t fb[SCR_W * SCR_H];
static int pt_x, pt_y, pt_down;
static int shot_n, errors;
static int brightness = 80;
static char last_call[512];

/* ---------- app_* implementation for the simulator ---------- */
model_t *app_model_lock(void) { return &M; }
void app_model_unlock(void) {}
const settings_t *app_settings_lock(void) { return &S; }
void app_settings_unlock(void) {}
void app_status(app_status_t *o) { *o = ST; }
uint32_t app_ms(void) { return now_ms; }
void app_brightness(int p) { brightness = p; }
void app_voice(bool start) { printf("  voice %s\n", start ? "start" : "stop"); }

bool app_settings_patch(const char *json)
{
    cJSON *j = cJSON_Parse(json);
    settings_err_t err;
    bool ok = j && settings_patch(&S, j, false, &err);
    cJSON_Delete(j);
    if (ok) ui_settings_changed();
    return ok;
}

// Fake Home Assistant: applies the command and sends back a state change after 300 ms.
typedef struct { uint32_t at; char id[ID_LEN]; act_t act; float v; } pending_t;
static pending_t q[16];
static int nq;

void app_act(const char *id, act_t act, float value)
{
    int i = model_find(&M, id);
    bool has; float nl;
    char *msg = ha_msg_action(&C, &M, i, act, value, &has, &nl);
    if (!msg) { printf("  (no action for %s)\n", id); return; }
    snprintf(last_call, sizeof last_call, "%s", msg);
    printf("  -> HA %s\n", msg);
    free(msg);
    model_set_pending(&M, i, now_ms, has, nl);
    if (nq < 16) { q[nq].at = now_ms + 300; snprintf(q[nq].id, ID_LEN, "%s", id); q[nq].act = act; q[nq].v = has ? nl : value; nq++; }
}

static void ha_confirm(pending_t *p)
{
    int i = model_find(&M, p->id);
    if (i < 0) return;
    entity_t *e = &M.e[i];
    switch (p->act) {
    case ACT_LEVEL: case ACT_STEP_UP: case ACT_STEP_DOWN:
        e->level = p->v;
        if (e->dom == D_LIGHT || e->dom == D_FAN) { e->on = p->v > 0; snprintf(e->state, sizeof e->state, e->on ? "on" : "off"); }
        if (e->dom == D_COVER) e->on = p->v > 0;
        break;
    case ACT_TOGGLE:
        e->on = !e->on;
        if (e->dom == D_LOCK) snprintf(e->state, sizeof e->state, e->on ? "locked" : "unlocked");
        else if (e->dom == D_CLIMATE) snprintf(e->state, sizeof e->state, e->on ? "heat" : "off");
        else if (e->dom == D_MEDIA) snprintf(e->state, sizeof e->state, e->on ? "playing" : "paused");
        else snprintf(e->state, sizeof e->state, e->on ? "on" : "off");
        break;
    case ACT_CLOSE: e->level = 0; e->on = false; snprintf(e->state, sizeof e->state, "closed"); break;
    case ACT_OPEN: e->level = 100; e->on = true; snprintf(e->state, sizeof e->state, "open"); break;
    case ACT_NEXT: snprintf(e->title, sizeof e->title, "Radio 2"); break;
    default: break;
    }
    e->pending_ms = 0;
    e->has_pending_level = false;
    M.version++;
}

/* ---------- LVGL plumbing ---------- */
static uint32_t tick_cb(void) { return now_ms; }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++) memcpy(&fb[y * SCR_W + a->x1], px + (size_t)(y - a->y1) * w * 2, (size_t)w * 2);
    lv_display_flush_ready(d);
}

static void read_cb(lv_indev_t *in, lv_indev_data_t *d)
{
    d->point.x = pt_x;
    d->point.y = pt_y;
    d->state = pt_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void run(int ms)
{
    for (int t = 0; t <= ms; t += 5) {
        now_ms += 5;
        for (int k = 0; k < nq; k++) if ((int32_t)(now_ms - q[k].at) >= 0) { ha_confirm(&q[k]); q[k] = q[--nq]; k--; }
        lv_timer_handler();
    }
}

static void shot(const char *name)
{
    run(20);
    lv_refr_now(NULL);
    char path[128];
    snprintf(path, sizeof path, "shots/%02d_%s.ppm", ++shot_n, name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        uint16_t c = fb[i];
        unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
        if (brightness == 0) rgb[0] = rgb[1] = rgb[2] = 0;
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("shot %s\n", path);
}

static void tap(int x, int y)
{
    pt_x = x; pt_y = y; pt_down = 1; run(80);
    pt_down = 0; run(250);
}

static void drag(int x0, int y0, int x1, int y1, int steps)
{
    pt_x = x0; pt_y = y0; pt_down = 1; run(40);
    for (int i = 1; i <= steps; i++) { pt_x = x0 + (x1 - x0) * i / steps; pt_y = y0 + (y1 - y0) * i / steps; run(16); }
    pt_down = 0; run(60);
}

// Drag around the dial from one fraction of the 270° arc to another
static void dial_drag(float f0, float f1)
{
    pt_down = 1;
    for (int i = 0; i <= 20; i++) {
        float f = f0 + (f1 - f0) * i / 20.0f, a = (135 + f * 270) * 3.14159265f / 180;
        pt_x = 233 + (int)(204 * cosf(a)); pt_y = 233 + (int)(204 * sinf(a));
        run(16);
    }
    pt_down = 0; run(30);
}

static void feed(const char *fmt, int id)
{
    char buf[16384];
    ha_event_t ev;
    snprintf(buf, sizeof buf, fmt, id);
    ha_handle(&C, &M, buf, strlen(buf), &ev);
}

static void expect(bool ok, const char *what)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) errors++;
}

static void open_ref(scr_kind_t k, list_kind_t l, const char *arg)
{
    scr_ref_t r = {.kind = k, .list = l};
    snprintf(r.arg, sizeof r.arg, "%s", arg ? arg : "");
    ui_home();
    run(250);
    nav_push(&r);
    run(300);
}

int main(void)
{
    mkdir("shots", 0755);
    setenv("TZ", "GMT0", 1);
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *d = lv_display_create(SCR_W, SCR_H);
    static uint8_t buf1[SCR_W * 60 * 2];
    lv_display_set_buffers(d, buf1, NULL, sizeof buf1, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush_cb);
    lv_indev_t *in = lv_indev_create();
    lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(in, read_cb);

    settings_defaults(&S);
    snprintf(ST.ap_ssid, sizeof ST.ap_ssid, "PocketOS-A1B2");
    snprintf(ST.ap_pass, sizeof ST.ap_pass, "k7m2qx9p");
    snprintf(ST.host, sizeof ST.host, "pocketos-a1b2.local");
    snprintf(ST.ip, sizeof ST.ip, "192.168.1.42");
    snprintf(ST.pin, sizeof ST.pin, "482913");
    snprintf(ST.firmware, sizeof ST.firmware, "0.2.0");
    ST.battery = 78;
    ST.battery_hours = 14;
    ST.battery_mv = 3950;
    ST.time_ok = true;

    // 1. First power-on: setup mode
    ST.setup_mode = true;
    ui_init();
    run(950);
    shot("boot_splash");
    run(1800);
    shot("setup");

    // 2. Joined home Wi-Fi, Home Assistant not set up yet
    ST.setup_mode = false; ST.wifi = true; ST.ha = HA_NOT_SET;
    run(400);
    shot("home_not_set_up");
    expect(nav_top()->kind == SCR_HOME, "setup screen replaced by companion once on Wi-Fi");

    // 3. Home Assistant connected and synced (fake HA sends its real message shapes)
    ha_init(&C);
    feed(FX_AUTH_REQUIRED, 0); feed(FX_AUTH_OK, 0);
    for (int s = 0; s < HA_SYNC_STEPS; s++) free(ha_msg_sync(&C, s));
    feed(FX_STATES, C.id_states); feed(FX_AREAS, C.id_areas); feed(FX_ENTITY_REG, C.id_entities); feed(FX_DEVICE_REG, C.id_devices);
    for (int i = 0; i < M.n; i++) settings_set_flag(&S, "known", M.e[i].id, true);   // first sync: everything reviewed
    settings_set_flag(&S, "fav", "light.floor_lamp", true);
    settings_set_flag(&S, "fav", "climate.thermostat", true);
    settings_set_flag(&S, "fav", "lock.front_door", true);
    ST.ha = HA_OK;
    run(400);
    shot("home_idle");
    tap(233, 245);
    shot("home_tap_happy");
    run(1500);

    // 4. Menu
    tap(233, 440);
    run(300);
    shot("menu_page1");
    expect(nav_top()->kind == SCR_MENU, "tap outside the companion opens the menu");
    drag(233, 380, 233, 80, 12);
    run(500);
    shot("menu_page2");
    drag(233, 80, 233, 380, 12);
    run(500);

    // 5. Lights list, then a dimmable light
    tap(150 + 0, 92 + 66 + 146 - 146);   // page 1, top-right tile is index 1... use second tile
    run(300);
    if (nav_top()->kind != SCR_LIST) { ui_back(); run(300); tap(316, 158); run(300); }
    shot("list_after_tile");
    open_ref(SCR_LIST, L_CATEGORY, "lights");
    shot("list_lights");
    open_ref(SCR_CONTROL, 0, "light.floor_lamp");
    shot("ctl_light");
    dial_drag(0.70f, 0.30f);
    shot("ctl_light_sending");
    expect(strstr(last_call, "\"brightness_pct\":30") != NULL, "dragging the dial sends light.turn_on brightness_pct=30");
    run(500);
    shot("ctl_light_confirmed");
    expect(fabsf(M.e[model_find(&M, "light.floor_lamp")].level - 30) < 0.1f, "HA confirmation updates the dial");

    // 6. Heating
    open_ref(SCR_CONTROL, 0, "climate.thermostat");
    shot("ctl_heating_off");
    tap(233, 233 + 82);   // flame button
    run(500);
    for (int k = 0; k < 3; k++) { tap(233 + 78, 233 + 82); run(400); }   // + three times
    shot("ctl_heating_on");
    expect(M.e[model_find(&M, "climate.thermostat")].on, "flame button turns heating on");
    expect(fabsf(M.e[model_find(&M, "climate.thermostat")].level - 22.5f) < 0.01f, "+ steps target by 0.5 to 22.5");
    // HA pushes a new room temperature while the screen is open
    feed(FX_EVT_HEAT_ON, C.id_sub);
    run(300);
    shot("ctl_heating_live_update");

    // 7. Media, blinds, lock, plug, sensor, scene
    open_ref(SCR_CONTROL, 0, "media_player.living_room_tv");
    tap(233, 233 + 82);
    run(500);
    shot("ctl_tv_playing");
    open_ref(SCR_CONTROL, 0, "media_player.kitchen_speaker");
    shot("ctl_speaker_play_only");
    open_ref(SCR_CONTROL, 0, "cover.bedroom_blinds");
    shot("ctl_blinds");
    open_ref(SCR_CONTROL, 0, "lock.front_door");
    tap(233, 207);
    run(120);
    shot("ctl_lock_unlocking");
    run(500);
    shot("ctl_lock_unlocked");
    open_ref(SCR_CONTROL, 0, "switch.kettle");
    shot("ctl_plug");
    open_ref(SCR_CONTROL, 0, "light.ceiling");
    shot("ctl_onoff_light");
    open_ref(SCR_CONTROL, 0, "sensor.garden_temperature");
    shot("ctl_sensor");

    // 8. Lists
    open_ref(SCR_LIST, L_ROOMS, "");
    shot("list_rooms");
    open_ref(SCR_LIST, L_ROOM, "2");
    shot("list_bedroom");
    open_ref(SCR_LIST, L_FAVS, "");
    shot("list_favourites");
    open_ref(SCR_LIST, L_CATEGORY, "scenes");
    shot("list_scenes");
    open_ref(SCR_LIST, L_MORE, "");
    shot("list_more");
    ST.update = UPD_AVAILABLE;
    snprintf(ST.update_version, sizeof ST.update_version, "0.3.0");
    open_ref(SCR_LIST, L_INFO, "");
    shot("list_info");
    ST.update = UPD_LATEST;
    open_ref(SCR_LIST, L_COMPANION, "");
    tap(233, 233);   // centred row: eye colour -> violet
    run(300);
    shot("list_companion");
    expect(S.eye == EYE_VIOLET, "changing eye colour on the device saves it");

    // 9. Settings QR (always reachable)
    ui_home(); run(300);
    ui_toggle_settings_qr(); run(300);
    shot("settings_qr");

    // 10. Moods on the home screen
    ui_home(); run(400);
    shot("mood_alert_unlocked_door");   // the lock was unlocked above
    tap(233, 440); run(300); ui_home(); run(300);
    app_act("lock.front_door", ACT_TOGGLE, 0); run(500);
    shot("mood_dance_tv_playing");
    app_act("media_player.living_room_tv", ACT_TOGGLE, 0); run(500);
    shot("mood_hot_heating");
    ST.ha = HA_ERROR; snprintf(ST.ha_error, sizeof ST.ha_error, "Can't reach Home Assistant"); run(300);
    shot("mood_worried_offline");
    ST.ha = HA_OK; ST.battery = 9; run(300);
    shot("mood_tired_battery");
    ST.battery = 78; ST.charging = true; run(300);
    shot("mood_charging");
    ST.charging = false; ST.wifi = false; run(300);
    shot("mood_away");
    ST.wifi = true; run(300);
    ui_set_asleep(true); brightness = 80; run(300);   // render the face that shows on wake
    shot("mood_sleep");
    ui_set_asleep(false); run(200);

    // 11. Voice
    ui_voice(VOICE_LISTENING, NULL); run(600);
    shot("voice_listening");
    ui_voice(VOICE_THINKING, NULL); ui_voice(VOICE_HEARD, "Turn on the floor lamp"); ui_voice(VOICE_REPLY, "Turned on the light"); run(300);
    shot("voice_reply");
    run(5000);
    expect(nav_top()->kind != SCR_VOICE, "voice screen closes itself after the reply");

    // 12. Toast, reset prompt, device removed while open
    ui_toast("Settings updated", 2000);
    ui_reset_prompt(true); run(200);
    shot("reset_prompt");
    ui_reset_prompt(false);
    open_ref(SCR_CONTROL, 0, "switch.kettle");
    feed(FX_EVT_REMOVED, C.id_sub); run(400);
    expect(nav_top()->kind != SCR_CONTROL, "control screen closes when its device is removed");

    // 13. Unconfirmed command times out
    open_ref(SCR_CONTROL, 0, "fan.bedroom_fan");
    model_set_pending(&M, model_find(&M, "fan.bedroom_fan"), now_ms, true, 80);
    run(300);
    shot("ctl_fan_sending");
    run(6500);
    expect(M.e[model_find(&M, "fan.bedroom_fan")].pending_ms == 0, "unconfirmed command stops showing Sending after 6 s");

    printf("%d shots, %d failed checks\n", shot_n, errors);
    return errors ? 1 : 0;
}
