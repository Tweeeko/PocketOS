#pragma once
// PocketOS view of the home: entities from Home Assistant, grouped by area, with exactly the
// controls each entity supports. Portable C (unit-tested on a PC). Not thread-safe by itself:
// the firmware guards it with a mutex (see esp/app.c).
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "settings.h"

#define MAX_ENT  128
#define MAX_AREA 32

typedef enum { D_LIGHT, D_SWITCH, D_CLIMATE, D_MEDIA, D_FAN, D_COVER, D_LOCK, D_SENSOR, D_SCENE, D_SCRIPT, D_COUNT } dom_t;

// What the control screen can do for an entity (derived from supported_features/attributes)
enum {
    CAP_TOGGLE   = 1 << 0,   // centre button on/off (or lock/unlock, play/pause...)
    CAP_LEVEL    = 1 << 1,   // dial
    CAP_PREVNEXT = 1 << 2,   // media skip
    CAP_OPENCLOSE= 1 << 3,   // cover buttons
    CAP_STOP     = 1 << 4,
    CAP_RUN      = 1 << 5,   // scenes/scripts: one tap
    CAP_READONLY = 1 << 6,   // sensors
};

typedef struct {
    char id[ID_LEN];
    char name[40];
    dom_t dom;
    int area;                 // index into areas, -1 = none
    char area_id[40], device_id[40];
    char state[20];
    bool avail;
    bool on;                  // light/switch/fan on, media playing, cover open, lock locked, climate heating mode
    // levels (stored in the units shown to the user)
    float level;              // light brightness %, media volume %, fan %, cover position %, climate target, sensor value
    float cur_temp;           // climate current temperature (NAN if unknown)
    float lmin, lmax, lstep;  // dial range
    char unit[8];             // sensor unit
    char title[48];           // media title
    char hvac_on_mode[12];    // climate mode used for "on" (heat, auto...)
    uint32_t caps;
    bool registry_hidden;     // hidden or config/diagnostic in HA
    uint32_t pending_ms;      // when a command was sent (0 = none)
    float pending_level;
    bool has_pending_level;
} entity_t;

typedef struct { char id[40]; char name[32]; } area_t;

typedef struct {
    entity_t e[MAX_ENT];
    int n;
    area_t a[MAX_AREA];
    int na;
    uint32_t version;         // bumped on every change; the UI redraws when it changes
} model_t;

typedef enum {
    MOOD_IDLE, MOOD_HAPPY, MOOD_SAD, MOOD_SLEEP, MOOD_WAKE, MOOD_LISTEN,
    MOOD_DANCE, MOOD_HOT, MOOD_COLD, MOOD_ALERT, MOOD_WORRIED, MOOD_TIRED, MOOD_CHARGE, MOOD_AWAY,
} mood_t;

typedef struct { bool ha_ok; bool home_wifi; int battery; bool charging; } mood_inputs_t;

void model_clear(model_t *m);
const char *dom_name(dom_t d);
bool dom_from_entity_id(const char *entity_id, dom_t *out);

// Home Assistant payloads (see ha_proto.c)
int  model_apply_state(model_t *m, const cJSON *state);          // returns index or -1 if ignored
int  model_remove(model_t *m, const char *entity_id);
void model_apply_areas(model_t *m, const cJSON *areas);
void model_apply_registry(model_t *m, const cJSON *entities, const cJSON *devices);
int  model_find(const model_t *m, const char *entity_id);

// Visibility for the device UI
bool model_visible(const model_t *m, const settings_t *s, int i);
bool model_is_new(const model_t *m, const settings_t *s, int i);   // not yet reviewed by the user

// Display helpers
void model_status(const entity_t *e, char *buf, int len);     // "On · 70%", "21° · now 19.5°"
void model_value(const entity_t *e, float v, char *buf, int len); // big number on the dial
mood_t model_mood(const model_t *m, const settings_t *s, const mood_inputs_t *in);
const char *mood_caption(mood_t mood, const model_t *m, int hour, char *buf, int len);

// Pending commands: set when sent, cleared by the next state update for that entity
void model_set_pending(model_t *m, int i, uint32_t now_ms, bool has_level, float level);
bool model_expire_pending(model_t *m, uint32_t now_ms, uint32_t timeout_ms);   // true if any expired
