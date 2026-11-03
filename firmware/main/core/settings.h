// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// PocketOS settings (schema v4). Portable C: no ESP-IDF calls, so it is unit-tested on a PC.
// Names, limits and defaults must match docs/config-schema.json.
#include <stdbool.h>
#include "cJSON.h"

#define SETTINGS_SCHEMA 4
#define MAX_TILES  12
#define MAX_IDS    96     // favourites / hidden / known entity lists
#define ID_LEN     64
#define MAX_NAMES  48     // devices the user has renamed
#define NAME_LEN   33     // up to 32 bytes of UTF-8

typedef enum { EYE_BLUE = 0, EYE_VIOLET, EYE_WHITE, EYE_COUNT } eye_t;
typedef enum { ANIM_CALM = 0, ANIM_LIVELY, ANIM_OFF, ANIM_COUNT } anim_t;
typedef enum { DATE_DMY = 0, DATE_MDY } date_fmt_t;
typedef enum { UPD_OFF = 0, UPD_NOTIFY, UPD_AUTO, UPD_COUNT } upd_mode_t;

typedef struct {
    char key[12];     // fixed identifier, e.g. "lights"
    char label[13];   // user's name for the tile
    bool show;        // false = moved to More
} tile_t;

typedef struct { char id[ID_LEN]; char name[NAME_LEN]; } rename_t;   // the user's name for a device

typedef struct {
    char name[25];
    int brightness;          // 10..100
    bool clock_24h;
    date_fmt_t date_fmt;
    int sleep_s;             // 15, 30, 60 or 0 (never)
    eye_t eye;
    anim_t anim;
    char tz[48];             // POSIX TZ
    tile_t tiles[MAX_TILES];
    int ntiles;
    bool auto_add;           // show newly discovered devices without review
    bool alerts;             // companion reacts to unlocked doors etc.
    upd_mode_t updates;      // off, notify (default) or auto (installs while charging at night)
    char ha_url[96];
    char ha_token[320];      // write-only through the API; never returned
    char fav[MAX_IDS][ID_LEN];    int nfav;
    char hidden[MAX_IDS][ID_LEN]; int nhidden;
    char known[MAX_IDS][ID_LEN];  int nknown;   // devices the user has already reviewed
    rename_t names[MAX_NAMES];    int nnames;   // names chosen on PocketOS, shown instead of Home Assistant's
} settings_t;

typedef struct { const char *field; const char *message; } settings_err_t;

void settings_defaults(settings_t *s);

// Validates every field first; applies only if all are valid. Unknown keys are rejected.
// `trusted` allows lists/secrets (used when loading from flash); the web API passes false
// for whole-object import so a token can't be smuggled through /config.
bool settings_patch(settings_t *s, const cJSON *patch, bool trusted, settings_err_t *err);

// include_secret=false for anything sent to a browser.
cJSON *settings_to_json(const settings_t *s, bool include_secret);

// Lists
bool settings_has(char (*list)[ID_LEN], int n, const char *id);
bool settings_set_flag(settings_t *s, const char *list_name, const char *id, bool on);
// Favourites keep the order the user gives them; dir -1 moves one place up, +1 down.
bool settings_fav_move(settings_t *s, const char *id, int dir);

// Device names: the user's name if they set one, otherwise NULL (use Home Assistant's).
const char *settings_name_for(const settings_t *s, const char *id);
// Sets or (with "" or NULL) clears a name. Returns false with a message if it isn't valid.
bool settings_set_name(settings_t *s, const char *id, const char *name, const char **why);

const char *settings_eye_name(eye_t e);
const char *settings_anim_name(anim_t a);
const char *settings_updates_name(upd_mode_t u);
