// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Home Assistant WebSocket protocol, as plain functions: build outgoing JSON, interpret
// incoming JSON and update the model. No networking here, so it is unit-tested on a PC.
// Protocol details were checked against home-assistant/core (websocket_api, config,
// assist_pipeline).
#include <stdbool.h>
#include <stdint.h>
#include "model.h"

typedef enum { ACT_TOGGLE, ACT_LEVEL, ACT_PREV, ACT_NEXT, ACT_OPEN, ACT_CLOSE, ACT_STOP, ACT_RUN, ACT_STEP_UP, ACT_STEP_DOWN } act_t;

typedef enum {
    HA_EV_NONE, HA_EV_AUTH_REQUIRED, HA_EV_AUTH_OK, HA_EV_AUTH_INVALID,
    HA_EV_SYNCED,          // states + areas + registries loaded and subscribed
    HA_EV_STATE,           // entity changed (index in .entity)
    HA_EV_CALL_FAILED,     // a service call was rejected (.text = reason, .entity = which)
    HA_EV_CALL_OK,         // Home Assistant carried out a service call (.entity)
    HA_EV_VOICE_READY,     // pipeline accepted; stream audio with .handler_id
    HA_EV_VOICE_HEARD,     // .text = what HA heard
    HA_EV_VOICE_REPLY,     // .text = what HA says back
    HA_EV_VOICE_ERROR,     // .text = reason
    HA_EV_VOICE_END,
    HA_EV_ERROR,           // other command failure (.text)
    HA_EV_REGISTRY_CHANGED,// rooms or devices changed in Home Assistant: fetch the registries again
} ha_ev_kind_t;

typedef struct {
    ha_ev_kind_t kind;
    int entity;
    int handler_id;
    char text[160];
} ha_event_t;

typedef struct {
    int next_id;
    int id_states, id_areas, id_entities, id_devices, id_sub, id_voice;
    int id_regsub[3];        // area, entity and device registry updates
    int got;                 // bit flags: 1 states, 2 areas, 4 entity registry, 8 device registry
    bool synced;
    cJSON *reg_entities;     // held until devices arrive too
    cJSON *reg_devices;
    char ha_version[16];
    struct { int id; char entity[ID_LEN]; } calls[8];   // recent service calls, to match their results
    int ncall;
} ha_ctx_t;

void ha_init(ha_ctx_t *c);
void ha_reset(ha_ctx_t *c);   // on disconnect

// Outgoing messages (caller frees with free())
char *ha_msg_auth(const char *token);
char *ha_msg_sync(ha_ctx_t *c, int step);        // 0 states, 1 areas, 2 entity reg, 3 device reg, 4 state changes, 5-7 registry changes
#define HA_SYNC_STEPS 8
// Build a service call for an action on entity i. Returns NULL if the entity can't do it.
// *new_level is set for level-type actions so the UI can show it as pending.
char *ha_msg_action(ha_ctx_t *c, const model_t *m, int i, act_t act, float value, bool *has_level, float *new_level);
char *ha_msg_voice_start(ha_ctx_t *c, int sample_rate);

// Incoming text frame. May produce one event (most messages) per call.
void ha_handle(ha_ctx_t *c, model_t *m, const char *json, size_t len, ha_event_t *ev);
