// Unit tests for the portable core: settings, model, Home Assistant protocol.
// Build and run: make -C firmware/host test
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "settings.h"
#include "model.h"
#include "ha_proto.h"
#include "battery.h"
#include "update.h"
#include "fixture.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static model_t M;
static settings_t S;
static ha_ctx_t C;

static void feed(const char *fmt, int id, ha_event_t *ev)
{
    char buf[16384];
    snprintf(buf, sizeof buf, fmt, id);
    ha_handle(&C, &M, buf, strlen(buf), ev);
}

static const entity_t *E(const char *id) { int i = model_find(&M, id); return i >= 0 ? &M.e[i] : NULL; }

static cJSON *J(const char *s) { cJSON *j = cJSON_Parse(s); if (!j) { printf("bad json in test\n"); exit(1); } return j; }

static void test_settings(void)
{
    settings_t s;
    settings_defaults(&s);
    settings_err_t err;
    CHECK(s.brightness == 80 && s.clock_24h && s.ntiles == 11 && s.tiles[0].show && !s.tiles[8].show);

    cJSON *p = J("{\"brightness\":55,\"eye\":\"violet\",\"name\":\"Dad's PocketOS\"}");
    CHECK(settings_patch(&s, p, false, &err));
    CHECK(s.brightness == 55 && s.eye == EYE_VIOLET && !strcmp(s.name, "Dad's PocketOS"));
    cJSON_Delete(p);

    // invalid value: nothing changes, field reported
    p = J("{\"brightness\":40,\"sleep_s\":45}");
    CHECK(!settings_patch(&s, p, false, &err));
    CHECK(err.field && !strcmp(err.field, "sleep_s"));
    CHECK(s.brightness == 55);
    cJSON_Delete(p);

    // token cannot be set through the untrusted path, and is never returned to browsers
    p = J("{\"ha_token\":\"secret\"}");
    CHECK(!settings_patch(&s, p, false, &err));
    cJSON_Delete(p);
    p = J("{\"ha_token\":\"secret\",\"ha_url\":\"http://ha.local:8123\"}");
    CHECK(settings_patch(&s, p, true, &err));
    cJSON_Delete(p);
    cJSON *out = settings_to_json(&s, false);
    char *txt = cJSON_PrintUnformatted(out);
    CHECK(strstr(txt, "secret") == NULL && strstr(txt, "\"ha_token_set\":true"));
    free(txt);
    cJSON_Delete(out);

    // round trip through JSON (what NVS stores)
    out = settings_to_json(&s, true);
    settings_t s2;
    settings_defaults(&s2);
    CHECK(settings_patch(&s2, out, true, &err));
    CHECK(!strcmp(s2.ha_token, "secret") && s2.brightness == 55 && !strcmp(s2.name, s.name));
    cJSON_Delete(out);

    // tiles: reorder + rename, must contain every key once
    p = J("{\"tiles\":[{\"key\":\"heating\",\"label\":\"Warmth\",\"show\":true},{\"key\":\"lights\",\"label\":\"Lights\",\"show\":true},"
          "{\"key\":\"favourites\",\"label\":\"Faves\",\"show\":true},{\"key\":\"media\",\"label\":\"Media\",\"show\":false},"
          "{\"key\":\"locks\",\"label\":\"Locks\",\"show\":true},{\"key\":\"rooms\",\"label\":\"Rooms\",\"show\":true},"
          "{\"key\":\"scenes\",\"label\":\"Scenes\",\"show\":true},{\"key\":\"plugs\",\"label\":\"Plugs\",\"show\":false},"
          "{\"key\":\"fans\",\"label\":\"Fans\",\"show\":false},{\"key\":\"blinds\",\"label\":\"Blinds\",\"show\":true},"
          "{\"key\":\"sensors\",\"label\":\"Sensors\",\"show\":false}]}");
    CHECK(settings_patch(&s, p, false, &err));
    CHECK(!strcmp(s.tiles[0].key, "heating") && !strcmp(s.tiles[0].label, "Warmth") && !s.tiles[3].show);
    cJSON_Delete(p);
    p = J("{\"tiles\":[{\"key\":\"heating\",\"label\":\"x\",\"show\":true}]}");
    CHECK(!settings_patch(&s, p, false, &err));
    cJSON_Delete(p);

    // favourites validation
    p = J("{\"favourites\":[\"light.floor_lamp\",\"BAD ID\"]}");
    CHECK(!settings_patch(&s, p, false, &err));
    cJSON_Delete(p);
    CHECK(settings_set_flag(&s, "fav", "light.floor_lamp", true));
    CHECK(settings_set_flag(&s, "fav", "light.floor_lamp", true) && s.nfav == 1);
    CHECK(settings_set_flag(&s, "fav", "light.floor_lamp", false) && s.nfav == 0);
}

static void sync_all(void)
{
    ha_event_t ev;
    ha_init(&C);
    model_clear(&M);
    feed(FX_AUTH_REQUIRED, 0, &ev);
    CHECK(ev.kind == HA_EV_AUTH_REQUIRED && !strcmp(C.ha_version, "2026.10.1"));
    char *a = ha_msg_auth("tok123");
    CHECK(strstr(a, "\"type\":\"auth\"") && strstr(a, "\"access_token\":\"tok123\""));
    free(a);
    feed(FX_AUTH_OK, 0, &ev);
    CHECK(ev.kind == HA_EV_AUTH_OK);
    for (int st = 0; st < HA_SYNC_STEPS; st++) {
        char *m = ha_msg_sync(&C, st);
        CHECK(m != NULL);
        free(m);
    }
    feed(FX_STATES, C.id_states, &ev);
    CHECK(ev.kind == HA_EV_NONE);
    feed(FX_AREAS, C.id_areas, &ev);
    feed(FX_ENTITY_REG, C.id_entities, &ev);
    CHECK(ev.kind == HA_EV_NONE);
    feed(FX_DEVICE_REG, C.id_devices, &ev);
    CHECK(ev.kind == HA_EV_SYNCED);
    feed(FX_SUB_OK, C.id_sub, &ev);
    CHECK(ev.kind == HA_EV_NONE);
}

static void test_model(void)
{
    settings_defaults(&S);
    sync_all();

    // which entities are kept
    CHECK(E("light.floor_lamp") && E("sensor.garden_temperature"));
    CHECK(E("sensor.router_status") == NULL);   // non-numeric sensor dropped
    CHECK(E("sun.sun") == NULL);                // unsupported domain
    CHECK(M.na == 5);

    // capabilities only where supported
    CHECK(E("light.floor_lamp")->caps & CAP_LEVEL);
    CHECK(!(E("light.ceiling")->caps & CAP_LEVEL));      // onoff-only bulb: no dial
    CHECK(E("light.bedside")->caps & CAP_LEVEL);
    CHECK(fabsf(E("light.floor_lamp")->level - 70) < 0.6f);
    CHECK((E("climate.thermostat")->caps & CAP_LEVEL) && E("climate.thermostat")->lmax == 30 && E("climate.thermostat")->lstep == 0.5f);
    CHECK(E("media_player.living_room_tv")->caps & CAP_LEVEL && E("media_player.living_room_tv")->caps & CAP_PREVNEXT);
    CHECK(!(E("media_player.kitchen_speaker")->caps & CAP_LEVEL));   // no VOLUME_SET bit
    CHECK(E("cover.bedroom_blinds")->caps & CAP_STOP);
    CHECK(E("fan.bedroom_fan")->caps & CAP_LEVEL);

    // rooms: own area, or inherited from the device
    const entity_t *k = E("light.ceiling");
    CHECK(k->area >= 0 && !strcmp(M.a[k->area].name, "Kitchen"));
    CHECK(E("switch.kettle")->area >= 0 && !strcmp(M.a[E("switch.kettle")->area].name, "Kitchen"));
    CHECK(E("switch.kettle_child_lock")->registry_hidden);   // config entity not shown

    // visibility: nothing until reviewed, unless auto_add
    int lamp = model_find(&M, "light.floor_lamp");
    CHECK(!model_visible(&M, &S, lamp) && model_is_new(&M, &S, lamp));
    settings_set_flag(&S, "known", "light.floor_lamp", true);
    CHECK(model_visible(&M, &S, lamp) && !model_is_new(&M, &S, lamp));
    S.auto_add = true;
    CHECK(model_visible(&M, &S, model_find(&M, "lock.front_door")));
    CHECK(!model_visible(&M, &S, model_find(&M, "switch.kettle_child_lock")));
    settings_set_flag(&S, "hidden", "lock.front_door", true);
    CHECK(!model_visible(&M, &S, model_find(&M, "lock.front_door")));
    settings_set_flag(&S, "hidden", "lock.front_door", false);

    // status texts
    char b[64];
    model_status(E("light.floor_lamp"), b, sizeof b); CHECK(!strcmp(b, "On · 70%"));
    model_status(E("climate.thermostat"), b, sizeof b); CHECK(!strcmp(b, "Off · now 19.5°"));
    model_status(E("media_player.living_room_tv"), b, sizeof b); CHECK(!strcmp(b, "Paused · Vol 35"));
    model_status(E("cover.bedroom_blinds"), b, sizeof b); CHECK(!strcmp(b, "Open"));
    model_status(E("sensor.garden_temperature"), b, sizeof b); CHECK(!strcmp(b, "12.4 °C"));
    model_value(E("climate.thermostat"), 21.5f, b, sizeof b); CHECK(!strcmp(b, "21.5°"));

    // moods
    mood_inputs_t in = {.ha_ok = true, .home_wifi = true, .battery = 80, .charging = false};
    CHECK(model_mood(&M, &S, &in) == MOOD_IDLE);
    ha_event_t ev;
    feed(FX_EVT_HEAT_ON, C.id_sub, &ev);
    CHECK(ev.kind == HA_EV_STATE && E("climate.thermostat")->on && E("climate.thermostat")->level == 22.5f);
    CHECK(model_mood(&M, &S, &in) == MOOD_HOT);
    feed(FX_EVT_TV_PLAYING, C.id_sub, &ev);
    CHECK(E("media_player.living_room_tv")->on && fabsf(E("media_player.living_room_tv")->level - 60) < 0.1f);
    CHECK(model_mood(&M, &S, &in) == MOOD_DANCE);
    feed(FX_EVT_DOOR_UNLOCKED, C.id_sub, &ev);
    CHECK(model_mood(&M, &S, &in) == MOOD_ALERT);
    CHECK(!strcmp(mood_caption(MOOD_ALERT, &M, 9, b, sizeof b), "Front door is unlocked"));
    in.ha_ok = false;  CHECK(model_mood(&M, &S, &in) == MOOD_WORRIED);
    in.home_wifi = false; CHECK(model_mood(&M, &S, &in) == MOOD_AWAY);
    in.charging = true; CHECK(model_mood(&M, &S, &in) == MOOD_CHARGE);

    // entity removed
    feed(FX_EVT_REMOVED, C.id_sub, &ev);
    CHECK(E("switch.kettle") == NULL);
}

static void test_actions(void)
{
    sync_all();
    bool lvl; float nl;
    int i = model_find(&M, "light.floor_lamp");
    char *s = ha_msg_action(&C, &M, i, ACT_LEVEL, 42, &lvl, &nl);
    CHECK(s && strstr(s, "\"domain\":\"light\"") && strstr(s, "\"service\":\"turn_on\"") && strstr(s, "\"brightness_pct\":40") && strstr(s, "\"entity_id\":\"light.floor_lamp\""));
    CHECK(lvl && nl == 40);   // snapped to the 5% step
    free(s);

    s = ha_msg_action(&C, &M, model_find(&M, "light.ceiling"), ACT_LEVEL, 50, &lvl, &nl);
    CHECK(s == NULL);   // on/off-only bulb has no brightness
    s = ha_msg_action(&C, &M, model_find(&M, "light.ceiling"), ACT_TOGGLE, 0, &lvl, &nl);
    CHECK(s && strstr(s, "\"service\":\"toggle\""));
    free(s);

    int t = model_find(&M, "climate.thermostat");
    s = ha_msg_action(&C, &M, t, ACT_LEVEL, 40, &lvl, &nl);
    CHECK(s && strstr(s, "set_temperature") && strstr(s, "\"temperature\":30") && nl == 30);   // clamped to max
    free(s);
    s = ha_msg_action(&C, &M, t, ACT_STEP_UP, 0, &lvl, &nl);
    CHECK(s && nl == 21.5f);
    free(s);
    s = ha_msg_action(&C, &M, t, ACT_TOGGLE, 0, &lvl, &nl);
    CHECK(s && strstr(s, "set_hvac_mode") && strstr(s, "\"hvac_mode\":\"heat\""));
    free(s);

    int tv = model_find(&M, "media_player.living_room_tv");
    s = ha_msg_action(&C, &M, tv, ACT_LEVEL, 60, &lvl, &nl);
    CHECK(s && strstr(s, "volume_set") && strstr(s, "\"volume_level\":0.6"));
    free(s);
    s = ha_msg_action(&C, &M, tv, ACT_NEXT, 0, &lvl, &nl);
    CHECK(s && strstr(s, "media_next_track"));
    free(s);
    CHECK(ha_msg_action(&C, &M, model_find(&M, "media_player.kitchen_speaker"), ACT_NEXT, 0, &lvl, &nl) == NULL);

    s = ha_msg_action(&C, &M, model_find(&M, "cover.bedroom_blinds"), ACT_STOP, 0, &lvl, &nl);
    CHECK(s && strstr(s, "stop_cover"));
    free(s);
    s = ha_msg_action(&C, &M, model_find(&M, "lock.front_door"), ACT_TOGGLE, 0, &lvl, &nl);
    CHECK(s && strstr(s, "\"service\":\"unlock\""));
    free(s);
    s = ha_msg_action(&C, &M, model_find(&M, "scene.goodnight"), ACT_RUN, 0, &lvl, &nl);
    CHECK(s && strstr(s, "\"domain\":\"scene\"") && strstr(s, "turn_on"));
    free(s);
    CHECK(ha_msg_action(&C, &M, model_find(&M, "sensor.garden_temperature"), ACT_LEVEL, 3, &lvl, &nl) == NULL);

    // pending: cleared by HA's state update, or expires
    model_set_pending(&M, tv, 1000, true, 60);
    CHECK(M.e[tv].pending_ms);
    ha_event_t ev;
    feed(FX_EVT_TV_PLAYING, C.id_sub, &ev);
    CHECK(M.e[model_find(&M, "media_player.living_room_tv")].pending_ms == 0);
    int lamp = model_find(&M, "light.floor_lamp");
    model_set_pending(&M, lamp, 1000, false, 0);
    CHECK(!model_expire_pending(&M, 3000, 5000));
    CHECK(model_expire_pending(&M, 7000, 5000) && M.e[lamp].pending_ms == 0);

    // a refused call is reported
    feed(FX_CALL_FAIL, 999, &ev);
    CHECK(ev.kind == HA_EV_CALL_FAILED && !strcmp(ev.text, "Unauthorized"));
}

static void test_voice(void)
{
    sync_all();
    char *s = ha_msg_voice_start(&C, 16000);
    CHECK(s && strstr(s, "assist_pipeline/run") && strstr(s, "\"start_stage\":\"stt\"") && strstr(s, "\"sample_rate\":16000"));
    free(s);
    ha_event_t ev;
    feed(FX_VOICE_START, C.id_voice, &ev);  CHECK(ev.kind == HA_EV_VOICE_READY && ev.handler_id == 1);
    feed(FX_VOICE_STT, C.id_voice, &ev);    CHECK(ev.kind == HA_EV_VOICE_HEARD && !strcmp(ev.text, "Turn on the floor lamp"));
    feed(FX_VOICE_INTENT, C.id_voice, &ev); CHECK(ev.kind == HA_EV_VOICE_REPLY && !strcmp(ev.text, "Turned on the light"));
    feed(FX_VOICE_END, C.id_voice, &ev);    CHECK(ev.kind == HA_EV_VOICE_END);
}

static void test_auth_fail(void)
{
    ha_event_t ev;
    ha_init(&C);
    feed(FX_AUTH_BAD, 0, &ev);
    CHECK(ev.kind == HA_EV_AUTH_INVALID && strstr(ev.text, "Invalid"));
}


static void test_updates_setting(void)
{
    settings_t s; settings_err_t err;
    settings_defaults(&s);
    CHECK(s.updates == UPD_NOTIFY);
    cJSON *p = J("{\"updates\":\"auto\"}");
    CHECK(settings_patch(&s, p, false, &err) && s.updates == UPD_AUTO);
    cJSON_Delete(p);
    p = J("{\"updates\":\"sometimes\"}");
    CHECK(!settings_patch(&s, p, false, &err) && !strcmp(err.field, "updates") && s.updates == UPD_AUTO);
    cJSON_Delete(p);
    cJSON *o = settings_to_json(&s, false);
    CHECK(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(o, "updates")), "auto"));
    CHECK(cJSON_GetObjectItem(o, "schema")->valueint == 3);
    cJSON_Delete(o);
}

static void test_versions(void)
{
    CHECK(version_cmp("0.1.0", "0.2.0") < 0);
    CHECK(version_cmp("v0.10.0", "0.9.9") > 0);
    CHECK(version_cmp("1.2.3", "1.2.3") == 0);
    CHECK(version_cmp("1.2.3-beta", "1.2.3") < 0);
    CHECK(version_cmp("1.2", "1.2.0") == -2);
    CHECK(version_cmp("1.2.x", "1.2.0") == -2);
    CHECK(!version_valid("") && version_valid("v2.0.1"));

    update_manifest_t m; const char *err;
    const char *good = "{\"version\":\"v0.2.0\",\"url\":\"https://github.com/Tweeeko/PocketOS/releases/download/v0.2.0/pocketos.bin\","
        "\"sha256\":\"00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF\",\"size\":1843200,\"notes\":\"Faster menus\"}";
    CHECK(update_parse_manifest(good, &m, &err));
    CHECK(!strcmp(m.version, "0.2.0") && m.size == 1843200 && m.sha256[0] == 0x00 && m.sha256[31] == 0xff && !strcmp(m.notes, "Faster menus"));
    char bad[600];
    snprintf(bad, sizeof bad, "%s", good); memcpy(strstr(bad, "https"), "http:", 5);
    CHECK(!update_parse_manifest(bad, &m, &err));   // plain http refused
    snprintf(bad, sizeof bad, "%s", good); *strstr(bad, "00112233") = 'g';
    CHECK(!update_parse_manifest(bad, &m, &err));   // bad checksum
    CHECK(!update_parse_manifest("{\"version\":\"0.2.0\",\"url\":\"https://x/y\",\"sha256\":\"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\",\"size\":99999999}", &m, &err));
    CHECK(!update_parse_manifest("<html>", &m, &err) && err);
}

static batt_log_t BL;
static void test_battery(void)
{
    const uint32_t T0 = 1790000000u;   // 2026
    batt_log_init(&BL);
    CHECK(batt_log_valid(&BL));
    CHECK(!batt_log_add(&BL, 1000, 50, 3900, false, false));          // clock not set: ignored
    CHECK(batt_log_add(&BL, T0, 100, 4150, false, true));              // plugged, full
    CHECK(BL.last_full_t == T0);
    CHECK(!batt_log_add(&BL, T0 + 60, 100, 4150, false, true));        // too soon
    CHECK(batt_log_add(&BL, T0 + 120, 100, 4150, false, false));       // unplugged: recorded at once
    batt_est_t e = batt_estimate(&BL, T0 + 120, 100, false);
    CHECK(!e.known && e.hours_left < 0);                               // still learning
    // 6 hours on battery, losing 5 % an hour
    for (int i = 1; i <= 36; i++) batt_log_add(&BL, T0 + 120 + i * 600, 100 - i * 5 / 6, 4100 - i * 5, false, false);
    e = batt_estimate(&BL, T0 + 120 + 36 * 600, 70, false);
    CHECK(e.known && fabsf(e.pct_per_h - 5.0f) < 0.6f);
    CHECK(fabsf(e.hours_left - 14.0f) < 2.0f && fabsf(e.full_h - 20.0f) < 2.5f && e.basis_h > 5.5f);
    // Plugged in again: keeps the estimate from the last stretch, no "time left"
    uint32_t t = T0 + 120 + 37 * 600;
    CHECK(batt_log_add(&BL, t, 70, 4000, true, true));
    e = batt_estimate(&BL, t, 70, true);
    CHECK(e.known && e.hours_left < 0 && fabsf(e.full_h - 20.0f) < 2.5f);
    // Clock going backwards is ignored
    CHECK(!batt_log_add(&BL, t - 5000, 70, 4000, true, true));
    // Ring buffer wraps after 48 h and keeps order
    batt_log_init(&BL);
    for (int i = 0; i < 400; i++) batt_log_add(&BL, T0 + i * 600, 50, 3800, false, false);
    batt_sample_t a, b;
    CHECK(batt_log_count(&BL) == BATT_MAX_SAMPLES);
    CHECK(batt_log_get(&BL, 0, &a) && batt_log_get(&BL, BATT_MAX_SAMPLES - 1, &b) && b.t - a.t == (BATT_MAX_SAMPLES - 1) * 600u && b.t == T0 + 399 * 600);
    e = batt_estimate(&BL, b.t, 50, false);
    CHECK(!e.known);                                                   // no drop: no guess
    // A long gap (switched off) splits stretches
    batt_log_init(&BL);
    for (int i = 0; i < 4; i++) batt_log_add(&BL, T0 + i * 600, 90 - i, 4000, false, false);
    for (int i = 0; i < 4; i++) batt_log_add(&BL, T0 + 30000 + i * 600, 60 - i, 3900, false, false);
    e = batt_estimate(&BL, T0 + 32000, 57, false);
    CHECK(!e.known);
}

int main(void)
{
    test_settings();
    test_model();
    test_actions();
    test_voice();
    test_auth_fail();
    test_updates_setting();
    test_versions();
    test_battery();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
