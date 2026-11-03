#include "settings.h"
#include <string.h>
#include <stdio.h>

static const char *EYES[EYE_COUNT] = {"blue", "violet", "white"};
static const char *ANIMS[ANIM_COUNT] = {"calm", "lively", "off"};
static const char *UPDS[UPD_COUNT] = {"off", "notify", "auto"};
static const int SLEEPS[] = {15, 30, 60, 0};

// Fixed tile keys and default labels, in default order. The Settings tile (QR code)
// is always first on the device and is not part of this list.
static const char *TILE_KEYS[][2] = {
    {"favourites", "Favourites"}, {"lights", "Lights"}, {"heating", "Heating"},
    {"media", "Media"}, {"locks", "Locks"}, {"rooms", "Rooms"}, {"scenes", "Scenes"},
    {"plugs", "Plugs"}, {"fans", "Fans"}, {"blinds", "Blinds"}, {"sensors", "Sensors"},
};
#define NTILE_KEYS (int)(sizeof(TILE_KEYS) / sizeof(TILE_KEYS[0]))

const char *settings_eye_name(eye_t e) { return (e >= 0 && e < EYE_COUNT) ? EYES[e] : "blue"; }
const char *settings_anim_name(anim_t a) { return (a >= 0 && a < ANIM_COUNT) ? ANIMS[a] : "lively"; }
const char *settings_updates_name(upd_mode_t u) { return (u >= 0 && u < UPD_COUNT) ? UPDS[u] : "notify"; }

static void copy(char *dst, size_t n, const char *src)
{
    snprintf(dst, n, "%s", src ? src : "");
}

void settings_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    copy(s->name, sizeof(s->name), "PocketOS");
    s->brightness = 80;
    s->clock_24h = true;
    s->date_fmt = DATE_DMY;
    s->sleep_s = 30;
    s->eye = EYE_BLUE;
    s->anim = ANIM_LIVELY;
    copy(s->tz, sizeof(s->tz), "GMT0BST,M3.5.0/1,M10.5.0");
    s->ntiles = NTILE_KEYS;
    for (int i = 0; i < NTILE_KEYS; i++) {
        copy(s->tiles[i].key, sizeof(s->tiles[i].key), TILE_KEYS[i][0]);
        copy(s->tiles[i].label, sizeof(s->tiles[i].label), TILE_KEYS[i][1]);
        s->tiles[i].show = i < 7;   // first seven on the main menu, the rest under More
    }
    s->auto_add = false;
    s->alerts = true;
    s->updates = UPD_NOTIFY;
}

bool settings_has(char (*list)[ID_LEN], int n, const char *id)
{
    for (int i = 0; i < n; i++) if (!strcmp(list[i], id)) return true;
    return false;
}

static bool set_in(char (*list)[ID_LEN], int *n, const char *id, bool on)
{
    for (int i = 0; i < *n; i++) {
        if (!strcmp(list[i], id)) {
            if (!on) { memmove(list[i], list[i + 1], (size_t)(*n - i - 1) * ID_LEN); (*n)--; }
            return true;
        }
    }
    if (!on) return true;
    if (*n >= MAX_IDS || strlen(id) >= ID_LEN) return false;
    copy(list[*n], ID_LEN, id);
    (*n)++;
    return true;
}

bool settings_set_flag(settings_t *s, const char *list_name, const char *id, bool on)
{
    if (!strcmp(list_name, "fav")) return set_in(s->fav, &s->nfav, id, on);
    if (!strcmp(list_name, "hidden")) return set_in(s->hidden, &s->nhidden, id, on);
    if (!strcmp(list_name, "known")) return set_in(s->known, &s->nknown, id, on);
    return false;
}

#define FAIL(f, m) do { err->field = (f); err->message = (m); return false; } while (0)

static bool is_int(const cJSON *v) { return cJSON_IsNumber(v) && v->valuedouble == (double)(int)v->valuedouble; }

static bool valid_entity_id(const char *s)
{
    size_t n = strlen(s);
    if (n < 3 || n >= ID_LEN || !strchr(s, '.')) return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) return false;
    }
    return true;
}

static bool load_list(const cJSON *arr, char (*list)[ID_LEN], int *n, const char *field, settings_err_t *err)
{
    if (!cJSON_IsArray(arr)) FAIL(field, "Must be a list of device IDs");
    int k = 0;
    const cJSON *it;
    cJSON_ArrayForEach(it, arr) {
        if (!cJSON_IsString(it) || !valid_entity_id(it->valuestring)) FAIL(field, "Contains an invalid device ID");
        if (k >= MAX_IDS) FAIL(field, "Too many devices in this list");
        copy(list[k++], ID_LEN, it->valuestring);
    }
    *n = k;
    return true;
}

static int tile_index(const char *key)
{
    for (int i = 0; i < NTILE_KEYS; i++) if (!strcmp(TILE_KEYS[i][0], key)) return i;
    return -1;
}

bool settings_patch(settings_t *out, const cJSON *patch, bool trusted, settings_err_t *err)
{
    static settings_t s;   // static: the struct is ~20 KB, too big for a task stack
    err->field = NULL;
    err->message = NULL;
    if (!cJSON_IsObject(patch)) FAIL(NULL, "Expected a JSON object");
    s = *out;

    const cJSON *v;
    cJSON_ArrayForEach(v, patch) {
        const char *k = v->string;
        if (!strcmp(k, "schema")) {
            continue;
        } else if (!strcmp(k, "name")) {
            if (!cJSON_IsString(v)) FAIL(k, "Name must be text");
            size_t n = strlen(v->valuestring);
            if (n < 1 || n > 24) FAIL(k, "Name must be 1 to 24 characters");
            copy(s.name, sizeof(s.name), v->valuestring);
        } else if (!strcmp(k, "brightness")) {
            if (!is_int(v) || v->valueint < 10 || v->valueint > 100) FAIL(k, "Brightness must be 10 to 100");
            s.brightness = v->valueint;
        } else if (!strcmp(k, "clock_24h")) {
            if (!cJSON_IsBool(v)) FAIL(k, "Must be true or false");
            s.clock_24h = cJSON_IsTrue(v);
        } else if (!strcmp(k, "date_format")) {
            if (!cJSON_IsString(v)) FAIL(k, "Must be dmy or mdy");
            if (!strcmp(v->valuestring, "dmy")) s.date_fmt = DATE_DMY;
            else if (!strcmp(v->valuestring, "mdy")) s.date_fmt = DATE_MDY;
            else FAIL(k, "Must be dmy or mdy");
        } else if (!strcmp(k, "sleep_s")) {
            bool ok = false;
            for (size_t i = 0; is_int(v) && i < sizeof(SLEEPS) / sizeof(int); i++) if (SLEEPS[i] == v->valueint) ok = true;
            if (!ok) FAIL(k, "Sleep time must be 15, 30, 60 or 0");
            s.sleep_s = v->valueint;
        } else if (!strcmp(k, "eye")) {
            int f = -1;
            for (int i = 0; cJSON_IsString(v) && i < EYE_COUNT; i++) if (!strcmp(EYES[i], v->valuestring)) f = i;
            if (f < 0) FAIL(k, "Eye colour must be blue, violet or white");
            s.eye = (eye_t)f;
        } else if (!strcmp(k, "anim")) {
            int f = -1;
            for (int i = 0; cJSON_IsString(v) && i < ANIM_COUNT; i++) if (!strcmp(ANIMS[i], v->valuestring)) f = i;
            if (f < 0) FAIL(k, "Animation must be calm, lively or off");
            s.anim = (anim_t)f;
        } else if (!strcmp(k, "tz")) {
            if (!cJSON_IsString(v) || strlen(v->valuestring) < 3 || strlen(v->valuestring) > 47) FAIL(k, "Time zone looks wrong");
            copy(s.tz, sizeof(s.tz), v->valuestring);
        } else if (!strcmp(k, "auto_add")) {
            if (!cJSON_IsBool(v)) FAIL(k, "Must be true or false");
            s.auto_add = cJSON_IsTrue(v);
        } else if (!strcmp(k, "alerts")) {
            if (!cJSON_IsBool(v)) FAIL(k, "Must be true or false");
            s.alerts = cJSON_IsTrue(v);
        } else if (!strcmp(k, "updates")) {
            int f = -1;
            for (int i = 0; cJSON_IsString(v) && i < UPD_COUNT; i++) if (!strcmp(UPDS[i], v->valuestring)) f = i;
            if (f < 0) FAIL(k, "Updates must be off, notify or auto");
            s.updates = (upd_mode_t)f;
        } else if (!strcmp(k, "tiles")) {
            // A full ordered list of {key,label,show}; every known key exactly once.
            if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != NTILE_KEYS) FAIL(k, "Tiles list must include every tile once");
            bool used[NTILE_KEYS] = {0};
            int i = 0;
            const cJSON *t;
            cJSON_ArrayForEach(t, v) {
                const cJSON *key = cJSON_GetObjectItem(t, "key"), *label = cJSON_GetObjectItem(t, "label"), *show = cJSON_GetObjectItem(t, "show");
                int ti = cJSON_IsString(key) ? tile_index(key->valuestring) : -1;
                if (ti < 0 || used[ti]) FAIL(k, "Unknown or repeated tile");
                if (!cJSON_IsString(label) || strlen(label->valuestring) < 1 || strlen(label->valuestring) > 12) FAIL(k, "Tile names must be 1 to 12 characters");
                if (!cJSON_IsBool(show)) FAIL(k, "Tile show must be true or false");
                used[ti] = true;
                copy(s.tiles[i].key, sizeof(s.tiles[i].key), key->valuestring);
                copy(s.tiles[i].label, sizeof(s.tiles[i].label), label->valuestring);
                s.tiles[i].show = cJSON_IsTrue(show);
                i++;
            }
            s.ntiles = i;
        } else if (!strcmp(k, "favourites")) {
            if (!load_list(v, s.fav, &s.nfav, k, err)) return false;
        } else if (!strcmp(k, "hidden")) {
            if (!load_list(v, s.hidden, &s.nhidden, k, err)) return false;
        } else if (!strcmp(k, "known")) {
            if (!load_list(v, s.known, &s.nknown, k, err)) return false;
        } else if (trusted && !strcmp(k, "ha_url")) {
            if (!cJSON_IsString(v) || strlen(v->valuestring) >= sizeof(s.ha_url)) FAIL(k, "Address too long");
            copy(s.ha_url, sizeof(s.ha_url), v->valuestring);
        } else if (trusted && !strcmp(k, "ha_token")) {
            if (!cJSON_IsString(v) || strlen(v->valuestring) >= sizeof(s.ha_token)) FAIL(k, "Token too long");
            copy(s.ha_token, sizeof(s.ha_token), v->valuestring);
        } else {
            FAIL(k, "Unknown setting");
        }
    }
    *out = s;
    return true;
}

static cJSON *list_json(char (*list)[ID_LEN], int n)
{
    cJSON *a = cJSON_CreateArray();
    for (int i = 0; i < n; i++) cJSON_AddItemToArray(a, cJSON_CreateString(list[i]));
    return a;
}

cJSON *settings_to_json(const settings_t *s, bool include_secret)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "schema", SETTINGS_SCHEMA);
    cJSON_AddStringToObject(o, "name", s->name);
    cJSON_AddNumberToObject(o, "brightness", s->brightness);
    cJSON_AddBoolToObject(o, "clock_24h", s->clock_24h);
    cJSON_AddStringToObject(o, "date_format", s->date_fmt == DATE_MDY ? "mdy" : "dmy");
    cJSON_AddNumberToObject(o, "sleep_s", s->sleep_s);
    cJSON_AddStringToObject(o, "eye", settings_eye_name(s->eye));
    cJSON_AddStringToObject(o, "anim", settings_anim_name(s->anim));
    cJSON_AddStringToObject(o, "tz", s->tz);
    cJSON_AddBoolToObject(o, "auto_add", s->auto_add);
    cJSON_AddBoolToObject(o, "alerts", s->alerts);
    cJSON_AddStringToObject(o, "updates", settings_updates_name(s->updates));
    cJSON *t = cJSON_AddArrayToObject(o, "tiles");
    for (int i = 0; i < s->ntiles; i++) {
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "key", s->tiles[i].key);
        cJSON_AddStringToObject(e, "label", s->tiles[i].label);
        cJSON_AddBoolToObject(e, "show", s->tiles[i].show);
        cJSON_AddItemToArray(t, e);
    }
    cJSON_AddItemToObject(o, "favourites", list_json((char (*)[ID_LEN])s->fav, s->nfav));
    cJSON_AddItemToObject(o, "hidden", list_json((char (*)[ID_LEN])s->hidden, s->nhidden));
    cJSON_AddItemToObject(o, "known", list_json((char (*)[ID_LEN])s->known, s->nknown));
    cJSON_AddStringToObject(o, "ha_url", s->ha_url);
    if (include_secret) cJSON_AddStringToObject(o, "ha_token", s->ha_token);
    else cJSON_AddBoolToObject(o, "ha_token_set", s->ha_token[0] != 0);
    return o;
}
