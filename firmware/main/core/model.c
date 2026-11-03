#include "model.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Home Assistant supported_features bits (checked against HA core source)
#define MP_PAUSE 1
#define MP_VOLUME_SET 4
#define MP_PREVIOUS 16
#define MP_NEXT 32
#define MP_PLAY 16384
#define CL_TARGET_TEMPERATURE 1
#define FAN_SET_SPEED 1
#define CV_OPEN 1
#define CV_CLOSE 2
#define CV_SET_POSITION 4
#define CV_STOP 8

static const char *DOMS[D_COUNT] = {"light", "switch", "climate", "media_player", "fan", "cover", "lock", "sensor", "scene", "script"};

static void copy(char *dst, size_t n, const char *src) { snprintf(dst, n, "%s", src ? src : ""); }

const char *dom_name(dom_t d) { return (d >= 0 && d < D_COUNT) ? DOMS[d] : "?"; }

bool dom_from_entity_id(const char *id, dom_t *out)
{
    const char *dot = strchr(id, '.');
    if (!dot) return false;
    size_t n = (size_t)(dot - id);
    for (int i = 0; i < D_COUNT; i++)
        if (strlen(DOMS[i]) == n && !strncmp(DOMS[i], id, n)) { *out = (dom_t)i; return true; }
    return false;
}

void model_clear(model_t *m)
{
    uint32_t v = m->version;
    memset(m, 0, sizeof(*m));
    m->version = v + 1;
}

int model_find(const model_t *m, const char *id)
{
    for (int i = 0; i < m->n; i++) if (!strcmp(m->e[i].id, id)) return i;
    return -1;
}

static int area_index(const model_t *m, const char *area_id)
{
    if (!area_id || !area_id[0]) return -1;
    for (int i = 0; i < m->na; i++) if (!strcmp(m->a[i].id, area_id)) return i;
    return -1;
}

static double num(const cJSON *o, const char *k, double def)
{
    const cJSON *v = cJSON_GetObjectItem(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

static const char *str(const cJSON *o, const char *k)
{
    const cJSON *v = cJSON_GetObjectItem(o, k);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static bool list_has(const cJSON *arr, const char *s)
{
    const cJSON *it;
    cJSON_ArrayForEach(it, arr) if (cJSON_IsString(it) && !strcmp(it->valuestring, s)) return true;
    return false;
}

int model_apply_state(model_t *m, const cJSON *st)
{
    const char *id = str(st, "entity_id");
    dom_t d;
    if (!id || !dom_from_entity_id(id, &d) || strlen(id) >= ID_LEN) return -1;
    const cJSON *at = cJSON_GetObjectItem(st, "attributes");
    const char *state = str(st, "state");
    if (!state) return -1;

    // Sensors: only numeric ones with a unit are shown
    if (d == D_SENSOR) {
        char *end = NULL;
        strtod(state, &end);
        if (!str(at, "unit_of_measurement") || end == state) {
            if (strcmp(state, "unavailable") && strcmp(state, "unknown")) return -1;
        }
    }

    int i = model_find(m, id);
    if (i < 0) {
        if (m->n >= MAX_ENT) return -1;
        i = m->n++;
        memset(&m->e[i], 0, sizeof(entity_t));
        copy(m->e[i].id, sizeof(m->e[i].id), id);
        m->e[i].area = -1;
        m->e[i].cur_temp = NAN;
    }
    entity_t *e = &m->e[i];
    e->dom = d;
    copy(e->state, sizeof(e->state), state);
    e->avail = strcmp(state, "unavailable") != 0;
    const char *fname = str(at, "friendly_name");
    copy(e->name, sizeof(e->name), fname ? fname : id + strlen(dom_name(d)) + 1);
    int feat = (int)num(at, "supported_features", 0);
    e->caps = 0;
    e->lmin = 0; e->lmax = 100; e->lstep = 1;

    switch (d) {
    case D_LIGHT: {
        e->on = !strcmp(state, "on");
        e->caps = CAP_TOGGLE;
        const cJSON *modes = cJSON_GetObjectItem(at, "supported_color_modes");
        bool dimmable = cJSON_IsArray(modes) && !(cJSON_GetArraySize(modes) == 1 && list_has(modes, "onoff"));
        if (dimmable) { e->caps |= CAP_LEVEL; e->lmin = 1; e->lmax = 100; e->lstep = 5; }
        double b = num(at, "brightness", -1);
        if (b >= 0) e->level = (float)round(b * 100.0 / 255.0);
        else if (!e->on) { /* keep last known brightness for the dial */ }
        break;
    }
    case D_SWITCH:
        e->on = !strcmp(state, "on");
        e->caps = CAP_TOGGLE;
        break;
    case D_CLIMATE: {
        e->on = strcmp(state, "off") != 0 && e->avail;
        e->caps = CAP_TOGGLE;
        if (feat & CL_TARGET_TEMPERATURE) {
            e->caps |= CAP_LEVEL;
            e->lmin = (float)num(at, "min_temp", 7);
            e->lmax = (float)num(at, "max_temp", 35);
            e->lstep = (float)num(at, "target_temp_step", 0.5);
            if (e->lstep <= 0) e->lstep = 0.5f;
            double t = num(at, "temperature", NAN);
            if (!isnan(t)) e->level = (float)t;
        }
        e->cur_temp = (float)num(at, "current_temperature", NAN);
        const cJSON *modes = cJSON_GetObjectItem(at, "hvac_modes");
        const char *pick = list_has(modes, "heat") ? "heat" : list_has(modes, "heat_cool") ? "heat_cool" : list_has(modes, "auto") ? "auto" : "heat";
        if (e->on) copy(e->hvac_on_mode, sizeof(e->hvac_on_mode), state);
        else if (!e->hvac_on_mode[0]) copy(e->hvac_on_mode, sizeof(e->hvac_on_mode), pick);
        break;
    }
    case D_MEDIA:
        e->on = !strcmp(state, "playing");
        if (feat & (MP_PAUSE | MP_PLAY)) e->caps |= CAP_TOGGLE;
        if (feat & MP_VOLUME_SET) { e->caps |= CAP_LEVEL; e->lstep = 5; }
        if ((feat & MP_PREVIOUS) && (feat & MP_NEXT)) e->caps |= CAP_PREVNEXT;
        if (cJSON_IsNumber(cJSON_GetObjectItem(at, "volume_level"))) e->level = (float)round(num(at, "volume_level", 0) * 100);
        copy(e->title, sizeof(e->title), str(at, "media_title") ? str(at, "media_title") : "");
        break;
    case D_FAN:
        e->on = !strcmp(state, "on");
        e->caps = CAP_TOGGLE;
        if (feat & FAN_SET_SPEED) { e->caps |= CAP_LEVEL; e->lstep = 10; }
        if (cJSON_IsNumber(cJSON_GetObjectItem(at, "percentage"))) e->level = (float)num(at, "percentage", 0);
        break;
    case D_COVER:
        e->on = strcmp(state, "closed") != 0;
        if (feat & CV_SET_POSITION) { e->caps |= CAP_LEVEL; e->lstep = 10; }
        if ((feat & CV_OPEN) && (feat & CV_CLOSE)) e->caps |= CAP_OPENCLOSE | CAP_TOGGLE;
        if (feat & CV_STOP) e->caps |= CAP_STOP;
        if (cJSON_IsNumber(cJSON_GetObjectItem(at, "current_position"))) e->level = (float)num(at, "current_position", 0);
        else e->level = e->on ? 100 : 0;
        break;
    case D_LOCK:
        e->on = !strcmp(state, "locked") || !strcmp(state, "locking");
        e->caps = CAP_TOGGLE;
        break;
    case D_SENSOR:
        e->caps = CAP_READONLY | CAP_LEVEL;
        e->level = (float)atof(state);
        copy(e->unit, sizeof(e->unit), str(at, "unit_of_measurement"));
        if (!strcmp(e->unit, "°C") || !strcmp(e->unit, "°F")) { e->lmin = -10; e->lmax = 40; }
        else { e->lmin = 0; e->lmax = 100; }
        break;
    case D_SCENE:
    case D_SCRIPT:
        e->caps = CAP_RUN;
        break;
    default: break;
    }
    // A state update is HA's confirmation: clear any pending command
    e->pending_ms = 0;
    e->has_pending_level = false;
    m->version++;
    return i;
}

int model_remove(model_t *m, const char *id)
{
    int i = model_find(m, id);
    if (i < 0) return -1;
    memmove(&m->e[i], &m->e[i + 1], sizeof(entity_t) * (size_t)(m->n - i - 1));
    m->n--;
    m->version++;
    return i;
}

void model_apply_areas(model_t *m, const cJSON *areas)
{
    m->na = 0;
    const cJSON *a;
    cJSON_ArrayForEach(a, areas) {
        if (m->na >= MAX_AREA) break;
        const char *id = str(a, "area_id"), *name = str(a, "name");
        if (!id || !name) continue;
        copy(m->a[m->na].id, sizeof(m->a[0].id), id);
        copy(m->a[m->na].name, sizeof(m->a[0].name), name);
        m->na++;
    }
    for (int i = 0; i < m->n; i++) m->e[i].area = area_index(m, m->e[i].area_id);
    m->version++;
}

void model_apply_registry(model_t *m, const cJSON *entities, const cJSON *devices)
{
    const cJSON *r;
    cJSON_ArrayForEach(r, entities) {
        const char *id = str(r, "entity_id");
        int i = id ? model_find(m, id) : -1;
        if (i < 0) continue;
        entity_t *e = &m->e[i];
        copy(e->device_id, sizeof(e->device_id), str(r, "device_id"));
        const char *area = str(r, "area_id");
        if (area) copy(e->area_id, sizeof(e->area_id), area);
        e->registry_hidden = str(r, "hidden_by") != NULL || str(r, "entity_category") != NULL;
    }
    // Entities without their own area inherit their device's area
    const cJSON *d;
    cJSON_ArrayForEach(d, devices) {
        const char *did = str(d, "id"), *area = str(d, "area_id");
        if (!did || !area) continue;
        for (int i = 0; i < m->n; i++)
            if (!m->e[i].area_id[0] && !strcmp(m->e[i].device_id, did)) copy(m->e[i].area_id, sizeof(m->e[i].area_id), area);
    }
    for (int i = 0; i < m->n; i++) m->e[i].area = area_index(m, m->e[i].area_id);
    m->version++;
}

bool model_visible(const model_t *m, const settings_t *s, int i)
{
    const entity_t *e = &m->e[i];
    if (e->registry_hidden) return false;
    if (settings_has((char (*)[ID_LEN])s->hidden, s->nhidden, e->id)) return false;
    if (!s->auto_add && !settings_has((char (*)[ID_LEN])s->known, s->nknown, e->id)) return false;
    return true;
}

bool model_is_new(const model_t *m, const settings_t *s, int i)
{
    const entity_t *e = &m->e[i];
    return !e->registry_hidden && !settings_has((char (*)[ID_LEN])s->known, s->nknown, e->id);
}

static void fmt_num(char *buf, int len, float v)
{
    if (fabsf(v - roundf(v)) < 0.05f) snprintf(buf, (size_t)len, "%d", (int)roundf(v));
    else snprintf(buf, (size_t)len, "%.1f", v);
}

void model_value(const entity_t *e, float v, char *buf, int len)
{
    char n[16];
    fmt_num(n, sizeof n, v);
    switch (e->dom) {
    case D_CLIMATE: snprintf(buf, (size_t)len, "%s°", n); break;
    case D_SENSOR: snprintf(buf, (size_t)len, "%s%s", n, (!strcmp(e->unit, "°C") || !strcmp(e->unit, "°F")) ? "°" : ""); break;
    case D_COVER: if (v <= 0) snprintf(buf, (size_t)len, "Closed"); else if (v >= 100) snprintf(buf, (size_t)len, "Open"); else snprintf(buf, (size_t)len, "%s%%", n); break;
    case D_MEDIA: snprintf(buf, (size_t)len, "%s", n); break;
    case D_LIGHT: case D_FAN: if (!e->on && !e->has_pending_level) snprintf(buf, (size_t)len, "Off"); else snprintf(buf, (size_t)len, "%s%%", n); break;
    default: snprintf(buf, (size_t)len, "%s", n);
    }
}

void model_status(const entity_t *e, char *buf, int len)
{
    char n[16];
    if (!e->avail) { snprintf(buf, (size_t)len, "Unavailable"); return; }
    switch (e->dom) {
    case D_LIGHT:
        if (!e->on) snprintf(buf, (size_t)len, "Off");
        else if (e->caps & CAP_LEVEL) snprintf(buf, (size_t)len, "On · %d%%", (int)e->level);
        else snprintf(buf, (size_t)len, "On");
        break;
    case D_SWITCH: snprintf(buf, (size_t)len, e->on ? "On" : "Off"); break;
    case D_FAN:
        if (!e->on) snprintf(buf, (size_t)len, "Off");
        else if (e->caps & CAP_LEVEL) snprintf(buf, (size_t)len, "On · %d%%", (int)e->level);
        else snprintf(buf, (size_t)len, "On");
        break;
    case D_CLIMATE: {
        char cur[32] = "";
        if (!isnan(e->cur_temp)) { fmt_num(n, sizeof n, e->cur_temp); snprintf(cur, sizeof cur, " · now %s°", n); }
        fmt_num(n, sizeof n, e->level);
        if (e->on && (e->caps & CAP_LEVEL)) snprintf(buf, (size_t)len, "%s°%s", n, cur);
        else snprintf(buf, (size_t)len, "Off%s", cur);
        break;
    }
    case D_MEDIA:
        if (e->caps & CAP_LEVEL) snprintf(buf, (size_t)len, "%s · Vol %d", e->on ? "Playing" : (!strcmp(e->state, "off") ? "Off" : "Paused"), (int)e->level);
        else snprintf(buf, (size_t)len, "%s", e->on ? "Playing" : "Paused");
        break;
    case D_COVER:
        if (!strcmp(e->state, "opening")) snprintf(buf, (size_t)len, "Opening…");
        else if (!strcmp(e->state, "closing")) snprintf(buf, (size_t)len, "Closing…");
        else if (e->level >= 100) snprintf(buf, (size_t)len, "Open");
        else if (e->level <= 0) snprintf(buf, (size_t)len, "Closed");
        else snprintf(buf, (size_t)len, "%d%% open", (int)e->level);
        break;
    case D_LOCK:
        if (!strcmp(e->state, "jammed")) snprintf(buf, (size_t)len, "Jammed!");
        else snprintf(buf, (size_t)len, e->on ? "Locked" : "Unlocked");
        break;
    case D_SENSOR: fmt_num(n, sizeof n, e->level); snprintf(buf, (size_t)len, "%s %s", n, e->unit); break;
    case D_SCENE: case D_SCRIPT: snprintf(buf, (size_t)len, "Tap to run"); break;
    default: snprintf(buf, (size_t)len, "%s", e->state);
    }
}

static bool name_has(const char *s, const char *w)
{
    char a[64], b[16];
    size_t i;
    for (i = 0; s[i] && i < sizeof a - 1; i++) a[i] = (char)((s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i]);
    a[i] = 0;
    for (i = 0; w[i] && i < sizeof b - 1; i++) b[i] = w[i];
    b[i] = 0;
    return strstr(a, b) != NULL;
}

static int outdoor_temp(const model_t *m)
{
    for (int i = 0; i < m->n; i++) {
        const entity_t *e = &m->e[i];
        if (e->dom != D_SENSOR || !e->avail || strcmp(e->unit, "°C")) continue;
        const char *area = e->area >= 0 ? m->a[e->area].name : "";
        if (name_has(e->name, "outdoor") || name_has(e->name, "outside") || name_has(area, "garden") || name_has(area, "outside"))
            return i;
    }
    return -1;
}

mood_t model_mood(const model_t *m, const settings_t *s, const mood_inputs_t *in)
{
    if (in->charging) return MOOD_CHARGE;
    if (in->battery >= 0 && in->battery <= 15) return MOOD_TIRED;
    if (!in->home_wifi) return MOOD_AWAY;
    if (!in->ha_ok) return MOOD_WORRIED;
    bool playing = false, heating = false, unlocked = false;
    for (int i = 0; i < m->n; i++) {
        if (!model_visible(m, s, i)) continue;
        const entity_t *e = &m->e[i];
        if (e->dom == D_LOCK && e->avail && !e->on && s->alerts) unlocked = true;
        if (e->dom == D_MEDIA && e->on) playing = true;
        if (e->dom == D_CLIMATE && e->on && !strcmp(e->state, "heat")) heating = true;
    }
    if (unlocked) return MOOD_ALERT;
    if (playing) return MOOD_DANCE;
    if (heating) return MOOD_HOT;
    int o = outdoor_temp(m);
    if (o >= 0 && m->e[o].level < 5) return MOOD_COLD;
    return MOOD_IDLE;
}

const char *mood_caption(mood_t mood, const model_t *m, int hour, char *buf, int len)
{
    switch (mood) {
    case MOOD_SLEEP: return "Zzz";
    case MOOD_DANCE: return "Vibing to the music";
    case MOOD_HOT: return "Toasty in here";
    case MOOD_WORRIED: return "Can't reach home";
    case MOOD_TIRED: return "Battery low";
    case MOOD_CHARGE: return "Charging";
    case MOOD_HAPPY: return "All done!";
    case MOOD_SAD: return "That didn't work";
    case MOOD_LISTEN: return "Listening...";
    case MOOD_WAKE: return "Good to see you";
    case MOOD_AWAY: return "Out and about";
    case MOOD_ALERT:
        for (int i = 0; i < m->n; i++)
            if (m->e[i].dom == D_LOCK && m->e[i].avail && !m->e[i].on) { snprintf(buf, (size_t)len, "%s is unlocked", m->e[i].name); return buf; }
        return "A door is unlocked";
    case MOOD_COLD: {
        int o = outdoor_temp(m);
        if (o >= 0) { snprintf(buf, (size_t)len, "Brr... %d° outside", (int)roundf(m->e[o].level)); return buf; }
        return "Brr...";
    }
    default:
        return hour < 0 ? "Hello" : hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening";
    }
}

void model_set_pending(model_t *m, int i, uint32_t now_ms, bool has_level, float level)
{
    if (i < 0 || i >= m->n) return;
    m->e[i].pending_ms = now_ms ? now_ms : 1;
    m->e[i].has_pending_level = has_level;
    m->e[i].pending_level = level;
    m->version++;
}

bool model_expire_pending(model_t *m, uint32_t now_ms, uint32_t timeout_ms)
{
    bool any = false;
    for (int i = 0; i < m->n; i++) {
        if (m->e[i].pending_ms && now_ms - m->e[i].pending_ms > timeout_ms) {
            m->e[i].pending_ms = 0;
            m->e[i].has_pending_level = false;
            any = true;
        }
    }
    if (any) m->version++;
    return any;
}
