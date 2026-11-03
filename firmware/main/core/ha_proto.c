// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "ha_proto.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static char *print(cJSON *o)
{
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}

void ha_init(ha_ctx_t *c)
{
    memset(c, 0, sizeof(*c));
    c->next_id = 1;
}

void ha_reset(ha_ctx_t *c)
{
    cJSON_Delete(c->reg_entities);
    cJSON_Delete(c->reg_devices);
    ha_init(c);
}

char *ha_msg_auth(const char *token)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "auth");
    cJSON_AddStringToObject(o, "access_token", token);
    return print(o);
}

static cJSON *cmd(ha_ctx_t *c, const char *type, int *id_out)
{
    cJSON *o = cJSON_CreateObject();
    int id = c->next_id++;
    cJSON_AddNumberToObject(o, "id", id);
    cJSON_AddStringToObject(o, "type", type);
    if (id_out) *id_out = id;
    return o;
}

char *ha_msg_sync(ha_ctx_t *c, int step)
{
    switch (step) {
    case 0: c->got = 0; c->synced = false; return print(cmd(c, "get_states", &c->id_states));
    case 1: return print(cmd(c, "config/area_registry/list", &c->id_areas));
    case 2: return print(cmd(c, "config/entity_registry/list_for_display", &c->id_entities));   // compact
    case 3: return print(cmd(c, "config/device_registry/list", &c->id_devices));
    case 4: {
        cJSON *o = cmd(c, "subscribe_events", &c->id_sub);
        cJSON_AddStringToObject(o, "event_type", "state_changed");
        return print(o);
    }
    case 5: case 6: case 7: {   // allowed for non-admin users (SUBSCRIBE_ALLOWLIST)
        static const char *EV[] = {"area_registry_updated", "entity_registry_updated", "device_registry_updated"};
        cJSON *o = cmd(c, "subscribe_events", &c->id_regsub[step - 5]);
        cJSON_AddStringToObject(o, "event_type", EV[step - 5]);
        return print(o);
    }
    }
    return NULL;
}

static char *service(ha_ctx_t *c, const entity_t *e, const char *domain, const char *svc, cJSON *data)
{
    int id;
    cJSON *o = cmd(c, "call_service", &id);
    int k = c->ncall++ % 8;
    c->calls[k].id = id;
    snprintf(c->calls[k].entity, ID_LEN, "%s", e->id);
    cJSON_AddStringToObject(o, "domain", domain);
    cJSON_AddStringToObject(o, "service", svc);
    if (data) cJSON_AddItemToObject(o, "service_data", data);
    cJSON *t = cJSON_AddObjectToObject(o, "target");
    cJSON_AddStringToObject(t, "entity_id", e->id);
    return print(o);
}

static cJSON *one(const char *k, double v)
{
    cJSON *d = cJSON_CreateObject();
    cJSON_AddNumberToObject(d, k, v);
    return d;
}

char *ha_msg_action(ha_ctx_t *c, const model_t *m, int i, act_t act, float value, bool *has_level, float *new_level)
{
    if (i < 0 || i >= m->n) return NULL;
    const entity_t *e = &m->e[i];
    *has_level = false;
    if (act == ACT_STEP_UP || act == ACT_STEP_DOWN) {
        if (!(e->caps & CAP_LEVEL) || (e->caps & CAP_READONLY)) return NULL;
        float base = e->has_pending_level ? e->pending_level : e->level;
        value = base + (act == ACT_STEP_UP ? e->lstep : -e->lstep);
        act = ACT_LEVEL;
    }
    if (act == ACT_LEVEL) {
        if (!(e->caps & CAP_LEVEL) || (e->caps & CAP_READONLY)) return NULL;
        if (value < e->lmin) value = e->lmin;
        if (value > e->lmax) value = e->lmax;
        value = roundf(value / e->lstep) * e->lstep;
        *has_level = true;
        *new_level = value;
    }
    switch (e->dom) {
    case D_LIGHT:
        if (act == ACT_LEVEL) return service(c, e, "light", "turn_on", one("brightness_pct", value));
        if (act == ACT_TOGGLE) return service(c, e, "light", "toggle", NULL);
        break;
    case D_SWITCH:
        if (act == ACT_TOGGLE) return service(c, e, "switch", "toggle", NULL);
        break;
    case D_FAN:
        if (act == ACT_LEVEL) return service(c, e, "fan", "set_percentage", one("percentage", value));
        if (act == ACT_TOGGLE) return service(c, e, "fan", "toggle", NULL);
        break;
    case D_CLIMATE:
        if (act == ACT_LEVEL) return service(c, e, "climate", "set_temperature", one("temperature", value));
        if (act == ACT_TOGGLE) {
            cJSON *d = cJSON_CreateObject();
            cJSON_AddStringToObject(d, "hvac_mode", e->on ? "off" : (e->hvac_on_mode[0] ? e->hvac_on_mode : "heat"));
            return service(c, e, "climate", "set_hvac_mode", d);
        }
        break;
    case D_MEDIA:
        if (act == ACT_LEVEL) return service(c, e, "media_player", "volume_set", one("volume_level", value / 100.0));
        if (act == ACT_TOGGLE && (e->caps & CAP_TOGGLE)) return service(c, e, "media_player", "media_play_pause", NULL);
        if (act == ACT_PREV && (e->caps & CAP_PREVNEXT)) return service(c, e, "media_player", "media_previous_track", NULL);
        if (act == ACT_NEXT && (e->caps & CAP_PREVNEXT)) return service(c, e, "media_player", "media_next_track", NULL);
        break;
    case D_COVER:
        if (act == ACT_LEVEL) return service(c, e, "cover", "set_cover_position", one("position", value));
        if (act == ACT_TOGGLE && (e->caps & CAP_OPENCLOSE)) return service(c, e, "cover", e->level > 0 ? "close_cover" : "open_cover", NULL);
        if (act == ACT_OPEN && (e->caps & CAP_OPENCLOSE)) return service(c, e, "cover", "open_cover", NULL);
        if (act == ACT_CLOSE && (e->caps & CAP_OPENCLOSE)) return service(c, e, "cover", "close_cover", NULL);
        if (act == ACT_STOP && (e->caps & CAP_STOP)) return service(c, e, "cover", "stop_cover", NULL);
        break;
    case D_LOCK:
        if (act == ACT_TOGGLE) return service(c, e, "lock", e->on ? "unlock" : "lock", NULL);
        break;
    case D_SCENE:
        if (act == ACT_RUN || act == ACT_TOGGLE) return service(c, e, "scene", "turn_on", NULL);
        break;
    case D_SCRIPT:
        if (act == ACT_RUN || act == ACT_TOGGLE) return service(c, e, "script", "turn_on", NULL);
        break;
    default: break;
    }
    *has_level = false;
    return NULL;
}

char *ha_msg_voice_start(ha_ctx_t *c, int sample_rate)
{
    cJSON *o = cmd(c, "assist_pipeline/run", &c->id_voice);
    cJSON_AddStringToObject(o, "start_stage", "stt");
    cJSON_AddStringToObject(o, "end_stage", "intent");
    cJSON *in = cJSON_AddObjectToObject(o, "input");
    cJSON_AddNumberToObject(in, "sample_rate", sample_rate);
    cJSON_AddNumberToObject(o, "timeout", 30);
    return print(o);
}

static void text(ha_event_t *ev, const char *s)
{
    snprintf(ev->text, sizeof(ev->text), "%s", s ? s : "");
}

static void maybe_registry(ha_ctx_t *c, model_t *m)
{
    if (c->reg_entities && c->reg_devices) {
        model_apply_registry(m, c->reg_entities, c->reg_devices);
        cJSON_Delete(c->reg_entities);
        cJSON_Delete(c->reg_devices);
        c->reg_entities = c->reg_devices = NULL;
    }
}

static void handle_voice_event(const cJSON *event, ha_event_t *ev)
{
    const char *t = cJSON_GetStringValue(cJSON_GetObjectItem(event, "type"));
    const cJSON *data = cJSON_GetObjectItem(event, "data");
    if (!t) return;
    if (!strcmp(t, "run-start")) {
        const cJSON *rd = cJSON_GetObjectItem(data, "runner_data");
        const cJSON *h = cJSON_GetObjectItem(rd, "stt_binary_handler_id");
        if (cJSON_IsNumber(h)) { ev->kind = HA_EV_VOICE_READY; ev->handler_id = h->valueint; }
    } else if (!strcmp(t, "stt-end")) {
        ev->kind = HA_EV_VOICE_HEARD;
        text(ev, cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(data, "stt_output"), "text")));
    } else if (!strcmp(t, "intent-end")) {
        const cJSON *speech = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(
            cJSON_GetObjectItem(data, "intent_output"), "response"), "speech"), "plain"), "speech");
        ev->kind = HA_EV_VOICE_REPLY;
        text(ev, cJSON_IsString(speech) ? speech->valuestring : "Done.");
    } else if (!strcmp(t, "error")) {
        ev->kind = HA_EV_VOICE_ERROR;
        text(ev, cJSON_GetStringValue(cJSON_GetObjectItem(data, "message")));
    } else if (!strcmp(t, "run-end")) {
        ev->kind = HA_EV_VOICE_END;
    }
}

void ha_handle(ha_ctx_t *c, model_t *m, const char *json, size_t len, ha_event_t *ev)
{
    memset(ev, 0, sizeof(*ev));
    ev->entity = -1;
    cJSON *o = cJSON_ParseWithLength(json, len);
    if (!o) return;
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(o, "type"));
    int id = cJSON_IsNumber(cJSON_GetObjectItem(o, "id")) ? cJSON_GetObjectItem(o, "id")->valueint : -1;
    if (!type) goto done;

    if (!strcmp(type, "auth_required")) {
        ev->kind = HA_EV_AUTH_REQUIRED;
        text(ev, cJSON_GetStringValue(cJSON_GetObjectItem(o, "ha_version")));
        snprintf(c->ha_version, sizeof(c->ha_version), "%.15s", ev->text);
    } else if (!strcmp(type, "auth_ok")) {
        ev->kind = HA_EV_AUTH_OK;
    } else if (!strcmp(type, "auth_invalid")) {
        ev->kind = HA_EV_AUTH_INVALID;
        text(ev, cJSON_GetStringValue(cJSON_GetObjectItem(o, "message")));
    } else if (!strcmp(type, "result")) {
        bool ok = cJSON_IsTrue(cJSON_GetObjectItem(o, "success"));
        cJSON *res = cJSON_GetObjectItem(o, "result");
        const char *msg = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(o, "error"), "message"));
        if (id == c->id_states) {
            if (!ok || !cJSON_IsArray(res)) { ev->kind = HA_EV_ERROR; text(ev, msg ? msg : "Couldn't read devices"); goto done; }
            uint32_t v = m->version;
            m->n = 0;   // full refresh: things you control first, then room sensors, so a home
            const cJSON *st;   // with many sensors can't crowd out its lights
            cJSON_ArrayForEach(st, res) {
                const char *eid = cJSON_GetStringValue(cJSON_GetObjectItem(st, "entity_id"));
                if (eid && strncmp(eid, "sensor.", 7)) model_apply_state(m, st);
            }
            cJSON_ArrayForEach(st, res) {
                const char *eid = cJSON_GetStringValue(cJSON_GetObjectItem(st, "entity_id"));
                if (eid && !strncmp(eid, "sensor.", 7)) model_apply_state(m, st);
            }
            m->version = v + 1;
            c->got |= 1;
        } else if (id == c->id_areas) {
            // Registries are optional: without them devices simply show without rooms.
            if (ok && cJSON_IsArray(res)) model_apply_areas(m, res);
            c->got |= 2;
        } else if (id == c->id_entities) {
            c->reg_entities = (ok && (cJSON_IsArray(res) || cJSON_IsObject(res))) ? cJSON_DetachItemFromObject(o, "result") : cJSON_CreateArray();
            c->got |= 4;
            maybe_registry(c, m);
        } else if (id == c->id_devices) {
            c->reg_devices = (ok && cJSON_IsArray(res)) ? cJSON_DetachItemFromObject(o, "result") : cJSON_CreateArray();
            c->got |= 8;
            maybe_registry(c, m);
        } else if (id == c->id_sub || id == c->id_regsub[0] || id == c->id_regsub[1] || id == c->id_regsub[2]) {
            if (!ok) { ev->kind = HA_EV_ERROR; text(ev, msg ? msg : "Couldn't subscribe to updates"); }
            goto done;
        } else if (id == c->id_voice) {
            if (!ok) { ev->kind = HA_EV_VOICE_ERROR; text(ev, msg ? msg : "Voice isn't set up in Home Assistant"); }
            goto done;
        } else {
            // A service call's result: Home Assistant has done it, or refused it
            for (int k = 0; k < 8; k++) {
                if (c->calls[k].id != id || !c->calls[k].entity[0]) continue;
                int i = model_find(m, c->calls[k].entity);
                c->calls[k].entity[0] = 0;
                ev->entity = i;
                if (i >= 0) {
                    entity_t *e = &m->e[i];
                    e->pending_ms = 0;   // confirmed (or refused): stop showing "Sending..."
                    if (!ok) e->has_pending_level = false;   // refused: show the real value again
                    m->version++;
                }
                break;
            }
            if (!ok) { ev->kind = HA_EV_CALL_FAILED; text(ev, msg ? msg : "Home Assistant refused that"); }
            else ev->kind = HA_EV_CALL_OK;
            goto done;
        }
        if (c->got == 15 && !c->synced) { c->synced = true; ev->kind = HA_EV_SYNCED; }
    } else if (!strcmp(type, "event")) {
        const cJSON *event = cJSON_GetObjectItem(o, "event");
        if (id == c->id_voice) { handle_voice_event(event, ev); goto done; }
        if (id > 0 && (id == c->id_regsub[0] || id == c->id_regsub[1] || id == c->id_regsub[2])) { ev->kind = HA_EV_REGISTRY_CHANGED; goto done; }
        const char *et = cJSON_GetStringValue(cJSON_GetObjectItem(event, "event_type"));
        if (et && !strcmp(et, "state_changed")) {
            const cJSON *data = cJSON_GetObjectItem(event, "data");
            const cJSON *ns = cJSON_GetObjectItem(data, "new_state");
            const char *eid = cJSON_GetStringValue(cJSON_GetObjectItem(data, "entity_id"));
            if (cJSON_IsObject(ns)) {
                int i = model_apply_state(m, ns);
                if (i >= 0) { ev->kind = HA_EV_STATE; ev->entity = i; }
            } else if (eid) {
                if (model_remove(m, eid) >= 0) ev->kind = HA_EV_STATE;
            }
        }
    }
done:
    cJSON_Delete(o);
}
