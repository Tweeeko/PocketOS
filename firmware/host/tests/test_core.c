// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
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
    CHECK(E("sensor.kettle_power") == NULL);    // only room readings: power meters etc. would crowd the list
    CHECK(E("sensor.bathroom_humidity") && E("sensor.bathroom_humidity")->level == 61);
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
    CHECK(!strcmp(mood_caption(MOOD_ALERT, &M, NULL, 9, b, sizeof b), "Front door is unlocked"));
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
    CHECK(cJSON_GetObjectItem(o, "schema")->valueint == 4);
    cJSON_Delete(o);
}

static void test_names_and_order(void)
{
    static settings_t s; settings_err_t err; const char *why;
    settings_defaults(&s);
    CHECK(settings_name_for(&s, "light.a") == NULL);
    CHECK(settings_set_name(&s, "light.a", "Reading lamp", &why) && !strcmp(settings_name_for(&s, "light.a"), "Reading lamp"));
    CHECK(settings_set_name(&s, "light.a", "Desk lamp", &why) && s.nnames == 1 && !strcmp(settings_name_for(&s, "light.a"), "Desk lamp"));
    CHECK(!settings_set_name(&s, "light.a", "   ", &why) && why);
    CHECK(!settings_set_name(&s, "light.a", "a name that is far too long for it", &why));
    CHECK(!settings_set_name(&s, "light.a", "two\nlines", &why));
    CHECK(!settings_set_name(&s, "Not An Id", "x", &why));
    CHECK(settings_set_name(&s, "light.a", "", &why) && s.nnames == 0 && settings_name_for(&s, "light.a") == NULL);
    // through a settings patch, and back out as JSON
    cJSON *p = J("{\"names\":[{\"id\":\"light.a\",\"name\":\"Lamp\"},{\"id\":\"lock.b\",\"name\":\"Back door\"}]}");
    CHECK(settings_patch(&s, p, false, &err) && s.nnames == 2);
    cJSON_Delete(p);
    p = J("{\"names\":[{\"id\":\"light.a\",\"name\":\"A\"},{\"id\":\"light.a\",\"name\":\"B\"}]}");
    CHECK(!settings_patch(&s, p, false, &err) && !strcmp(err.field, "names") && s.nnames == 2);
    cJSON_Delete(p);
    cJSON *o = settings_to_json(&s, false);
    CHECK(cJSON_GetArraySize(cJSON_GetObjectItem(o, "names")) == 2);
    cJSON_Delete(o);
    // the chosen name is what PocketOS shows, including in the companion's caption
    entity_t e = {0};
    snprintf(e.id, sizeof e.id, "lock.b"); snprintf(e.name, sizeof e.name, "Rear door lock");
    CHECK(!strcmp(model_name(&s, &e), "Back door") && !strcmp(model_name(NULL, &e), "Rear door lock"));
    // favourites keep the user's order
    settings_set_flag(&s, "fav", "light.a", true); settings_set_flag(&s, "fav", "light.b", true); settings_set_flag(&s, "fav", "light.c", true);
    CHECK(settings_fav_move(&s, "light.c", -1) && !strcmp(s.fav[1], "light.c") && !strcmp(s.fav[2], "light.b"));
    CHECK(settings_fav_move(&s, "light.a", -1) && !strcmp(s.fav[0], "light.a"));   // already first
    CHECK(!settings_fav_move(&s, "light.zz", 1));
}

// What PocketOS asks Home Assistant for, and how it copes with real-world homes
static void test_ha_robustness(void)
{
    ha_event_t ev;
    // sync asks for the compact entity registry
    ha_init(&C);
    char *m = NULL;
    for (int st = 0; st < HA_SYNC_STEPS; st++) { free(m); m = ha_msg_sync(&C, st); if (st == 2) CHECK(strstr(m, "config/entity_registry/list_for_display") != NULL); }
    free(m);

    // the full registry form is still understood
    sync_all();
    cJSON *full = J(FX_ENTITY_REG_FULL_ROWS), *devs = cJSON_CreateArray();
    model_apply_registry(&M, full, devs);
    CHECK(E("switch.kettle_child_lock")->registry_hidden && !E("light.floor_lamp")->registry_hidden);
    cJSON_Delete(full); cJSON_Delete(devs);

    // a home with hundreds of sensors still shows every light: devices are loaded before sensors
    static char big[400000];
    int n = snprintf(big, sizeof big, "{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":[", C.id_states);
    for (int k = 0; k < 300; k++)
        n += snprintf(big + n, sizeof big - n, "{\"entity_id\":\"sensor.t%d\",\"state\":\"20\",\"attributes\":{\"unit_of_measurement\":\"\u00b0C\",\"device_class\":\"temperature\"}},", k);
    for (int k = 0; k < 40; k++)
        n += snprintf(big + n, sizeof big - n, "{\"entity_id\":\"light.l%d\",\"state\":\"off\",\"attributes\":{\"supported_color_modes\":[\"onoff\"]}}%s", k, k < 39 ? "," : "");
    snprintf(big + n, sizeof big - n, "]}");
    ha_handle(&C, &M, big, strlen(big), &ev);
    CHECK(M.n == MAX_ENT && E("light.l0") && E("light.l39"));

    // service calls: Home Assistant's result settles "Sending..." even if nothing changes
    sync_all();
    bool lvl; float nl;
    int lamp = model_find(&M, "light.floor_lamp");
    char *call = ha_msg_action(&C, &M, lamp, ACT_LEVEL, 70, &lvl, &nl);
    int id = (int)cJSON_GetObjectItem(cJSON_Parse(call), "id")->valueint;   // small leak in a test is fine
    free(call);
    model_set_pending(&M, lamp, 1000, lvl, nl);
    feed("{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":{\"context\":{}}}", id, &ev);
    CHECK(ev.kind == HA_EV_CALL_OK && ev.entity == lamp && M.e[lamp].pending_ms == 0);
    CHECK(!model_expire_pending(&M, 99999, 6000));   // no false "No response"
    // ...and a refusal is reported once, with Home Assistant's reason, and the real value comes back
    call = ha_msg_action(&C, &M, model_find(&M, "lock.front_door"), ACT_TOGGLE, 0, &lvl, &nl);
    id = (int)cJSON_GetObjectItem(cJSON_Parse(call), "id")->valueint;
    free(call);
    model_set_pending(&M, model_find(&M, "lock.front_door"), 1000, false, 0);
    feed("{\"id\":%d,\"type\":\"result\",\"success\":false,\"error\":{\"code\":\"home_assistant_error\",\"message\":\"Lock is jammed\"}}", id, &ev);
    CHECK(ev.kind == HA_EV_CALL_FAILED && !strcmp(ev.text, "Lock is jammed") && M.e[model_find(&M, "lock.front_door")].pending_ms == 0);
}

static void test_registry_updates(void)
{
    ha_event_t ev;
    sync_all();
    // the sync subscribes to room and device changes as well as state changes
    CHECK(C.id_regsub[0] && C.id_regsub[1] && C.id_regsub[2]);
    feed("{\"id\":%d,\"type\":\"event\",\"event\":{\"event_type\":\"entity_registry_updated\",\"data\":{\"action\":\"update\",\"entity_id\":\"light.floor_lamp\"}}}", C.id_regsub[1], &ev);
    CHECK(ev.kind == HA_EV_REGISTRY_CHANGED);
    // fetching again applies the move: the lamp now has no room of its own
    char *m = ha_msg_sync(&C, 2); free(m); m = ha_msg_sync(&C, 3); free(m);
    feed("{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":{\"entity_categories\":{},\"entities\":[{\"ei\":\"light.floor_lamp\",\"pl\":\"demo\"}]}}", C.id_entities, &ev);
    feed(FX_DEVICE_REG, C.id_devices, &ev);
    CHECK(E("light.floor_lamp")->area == -1);
}

static void test_tz_validation(void)
{
    settings_t *s = calloc(1, sizeof(settings_t)); settings_defaults(s);
    settings_err_t err;
    cJSON *p = J("{\"tz\":\"<-03>3\"}");
    CHECK(settings_patch(s, p, true, &err) && !strcmp(s->tz, "<-03>3"));
    cJSON_Delete(p);
    p = J("{\"tz\":\"GMT0;rm\"}");
    CHECK(!settings_patch(s, p, true, &err) && !strcmp(s->tz, "<-03>3"));
    cJSON_Delete(p); free(s);
}

static void test_fahrenheit(void)
{
    model_t *fm = calloc(1, sizeof(model_t));
    cJSON *st = J("{\"entity_id\":\"sensor.garden_temperature\",\"state\":\"38\",\"attributes\":{\"unit_of_measurement\":\"\u00b0F\",\"device_class\":\"temperature\",\"friendly_name\":\"Garden temperature\"}}");
    model_apply_state(fm, st);
    CHECK(fm->n == 1 && fm->e[0].lmin == 14 && fm->e[0].lmax == 104);
    settings_t *s = calloc(1, sizeof(settings_t)); settings_defaults(s); s->auto_add = true;
    mood_inputs_t in = {.ha_ok = true, .home_wifi = true, .battery = 80};
    CHECK(model_mood(fm, s, &in) == MOOD_COLD);              // 38 °F is cold
    cJSON_Delete(st);
    st = J("{\"entity_id\":\"sensor.garden_temperature\",\"state\":\"50\",\"attributes\":{\"unit_of_measurement\":\"\u00b0F\",\"device_class\":\"temperature\"}}");
    model_apply_state(fm, st);
    CHECK(model_mood(fm, s, &in) == MOOD_IDLE);              // 50 °F isn't
    cJSON_Delete(st); free(fm); free(s);
}

static void test_light_colour(void)
{
    sync_all();
    bool lvl; float nl;
    const entity_t *b = E("light.bedside");
    CHECK((b->caps & CAP_WARMTH) && (b->caps & CAP_COLOUR) && !(E("light.floor_lamp")->caps & (CAP_WARMTH | CAP_COLOUR)));
    int i = model_find(&M, "light.bedside");
    char *s = ha_msg_action(&C, &M, i, ACT_WARMTH, 2712, &lvl, &nl);
    CHECK(s && strstr(s, "\"color_temp_kelvin\":2700") && strstr(s, "turn_on") && !lvl);
    free(s);
    s = ha_msg_action(&C, &M, i, ACT_HUE, 240, &lvl, &nl);
    CHECK(s && strstr(s, "\"hs_color\":[240,100]"));
    free(s);
    CHECK(ha_msg_action(&C, &M, model_find(&M, "light.floor_lamp"), ACT_HUE, 10, &lvl, &nl) == NULL);   // white-only bulb
    // Home Assistant reports the colour back
    ha_event_t ev;
    feed("{\"id\":%d,\"type\":\"event\",\"event\":{\"event_type\":\"state_changed\",\"data\":{\"entity_id\":\"light.bedside\",\"new_state\":{\"entity_id\":\"light.bedside\",\"state\":\"on\",\"attributes\":{\"friendly_name\":\"Bedside lamp\",\"brightness\":128,\"color_mode\":\"color_temp\",\"color_temp_kelvin\":3000,\"min_color_temp_kelvin\":2200,\"max_color_temp_kelvin\":6500,\"supported_color_modes\":[\"color_temp\",\"hs\"]}}}}}", C.id_sub, &ev);
    b = E("light.bedside");
    CHECK(b->kelvin == 3000 && b->kelvin_min == 2200 && b->sat < 0);
    feed("{\"id\":%d,\"type\":\"event\",\"event\":{\"event_type\":\"state_changed\",\"data\":{\"entity_id\":\"light.bedside\",\"new_state\":{\"entity_id\":\"light.bedside\",\"state\":\"on\",\"attributes\":{\"brightness\":128,\"color_mode\":\"hs\",\"hs_color\":[240,100],\"supported_color_modes\":[\"color_temp\",\"hs\"]}}}}}", C.id_sub, &ev);
    CHECK(E("light.bedside")->hue == 240 && E("light.bedside")->sat == 100);
}

static void test_signed_updates(void)
{
    uint8_t tail[UPDATE_TRAILER] = {0};
    const uint8_t *sig; size_t n;
    CHECK(!update_trailer(tail, &sig, &n));                       // unsigned firmware
    tail[0] = 0x30; tail[1] = 0x44; tail[72] = 70; memcpy(tail + 74, "PSIG01", 6);
    CHECK(update_trailer(tail, &sig, &n) && n == 70 && sig == tail);
    tail[72] = 90; CHECK(!update_trailer(tail, &sig, &n));        // impossible length
    // staged releases: a stable group per device, spread over 0..99
    uint8_t a[6] = {0x7c, 0xdf, 0xa1, 0, 0xab, 0x12}, b[6] = {0x7c, 0xdf, 0xa1, 0, 0xab, 0x13};
    CHECK(update_bucket(a) == update_bucket(a) && update_bucket(a) >= 0 && update_bucket(a) < 100 && update_bucket(a) != update_bucket(b));
    update_manifest_t m; const char *err;
    const char *base = "{\"version\":\"0.2.0\",\"url\":\"https://x/p.bin\",\"size\":2000000,\"sha256\":\"%s\"%s}";
    char js[512], hex[65];
    memset(hex, 'a', 64);
    hex[64] = 0;
    snprintf(js, sizeof js, base, hex, "");
    CHECK(update_parse_manifest(js, &m, &err) && m.rollout == 100);
    snprintf(js, sizeof js, base, hex, ",\"rollout\":20");
    CHECK(update_parse_manifest(js, &m, &err) && m.rollout == 20);
    snprintf(js, sizeof js, base, hex, ",\"rollout\":150");
    CHECK(!update_parse_manifest(js, &m, &err));
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
    test_names_and_order();
    test_ha_robustness();
    test_registry_updates();
    test_signed_updates();
    test_light_colour();
    test_fahrenheit();
    test_tz_validation();
    test_versions();
    test_battery();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
