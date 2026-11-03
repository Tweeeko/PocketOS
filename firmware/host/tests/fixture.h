#pragma once
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
// A small, realistic Home Assistant installation, as the WebSocket messages it would send.
// Shapes follow home-assistant/core (get_states, config/*_registry/list, state_changed,
// assist_pipeline events). Used by the unit tests and by the screen simulator.

static const char *FX_AUTH_REQUIRED = "{\"type\":\"auth_required\",\"ha_version\":\"2026.10.1\"}";
static const char *FX_AUTH_OK = "{\"type\":\"auth_ok\",\"ha_version\":\"2026.10.1\"}";
static const char *FX_AUTH_BAD = "{\"type\":\"auth_invalid\",\"message\":\"Invalid access token or password\"}";

// %d placeholders are replaced with the request ids by the test helper
static const char *FX_STATES =
"{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":["
"{\"entity_id\":\"light.floor_lamp\",\"state\":\"on\",\"attributes\":{\"friendly_name\":\"Floor lamp\",\"brightness\":179,\"supported_color_modes\":[\"brightness\"]}},"
"{\"entity_id\":\"light.ceiling\",\"state\":\"off\",\"attributes\":{\"friendly_name\":\"Ceiling light\",\"supported_color_modes\":[\"onoff\"]}},"
"{\"entity_id\":\"light.bedside\",\"state\":\"off\",\"attributes\":{\"friendly_name\":\"Bedside lamp\",\"supported_color_modes\":[\"color_temp\",\"hs\"]}},"
"{\"entity_id\":\"climate.thermostat\",\"state\":\"off\",\"attributes\":{\"friendly_name\":\"Thermostat\",\"hvac_modes\":[\"off\",\"heat\"],\"min_temp\":7,\"max_temp\":30,\"target_temp_step\":0.5,\"temperature\":21,\"current_temperature\":19.5,\"supported_features\":385}},"
"{\"entity_id\":\"media_player.living_room_tv\",\"state\":\"paused\",\"attributes\":{\"friendly_name\":\"TV\",\"volume_level\":0.35,\"media_title\":\"Morning Jazz\",\"supported_features\":21437}},"
"{\"entity_id\":\"media_player.kitchen_speaker\",\"state\":\"idle\",\"attributes\":{\"friendly_name\":\"Kitchen speaker\",\"supported_features\":16385}},"
"{\"entity_id\":\"cover.bedroom_blinds\",\"state\":\"open\",\"attributes\":{\"friendly_name\":\"Blinds\",\"current_position\":100,\"supported_features\":15}},"
"{\"entity_id\":\"fan.bedroom_fan\",\"state\":\"off\",\"attributes\":{\"friendly_name\":\"Fan\",\"percentage\":50,\"supported_features\":49}},"
"{\"entity_id\":\"lock.front_door\",\"state\":\"locked\",\"attributes\":{\"friendly_name\":\"Front door\",\"supported_features\":0}},"
"{\"entity_id\":\"switch.kettle\",\"state\":\"off\",\"attributes\":{\"friendly_name\":\"Kettle plug\"}},"
"{\"entity_id\":\"switch.kettle_child_lock\",\"state\":\"off\",\"attributes\":{\"friendly_name\":\"Kettle child lock\"}},"
"{\"entity_id\":\"sensor.garden_temperature\",\"state\":\"12.4\",\"attributes\":{\"friendly_name\":\"Garden temperature\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\"}},"
"{\"entity_id\":\"sensor.router_status\",\"state\":\"online\",\"attributes\":{\"friendly_name\":\"Router status\"}},"
"{\"entity_id\":\"scene.goodnight\",\"state\":\"2026-11-01T22:00:00+00:00\",\"attributes\":{\"friendly_name\":\"Goodnight\"}},"
"{\"entity_id\":\"script.movie_time\",\"state\":\"off\",\"attributes\":{\"friendly_name\":\"Movie time\"}},"
"{\"entity_id\":\"sun.sun\",\"state\":\"above_horizon\",\"attributes\":{\"friendly_name\":\"Sun\"}}"
"]}";

static const char *FX_AREAS =
"{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":["
"{\"area_id\":\"living_room\",\"name\":\"Living room\"},{\"area_id\":\"kitchen\",\"name\":\"Kitchen\"},"
"{\"area_id\":\"bedroom\",\"name\":\"Bedroom\"},{\"area_id\":\"hallway\",\"name\":\"Hallway\"},{\"area_id\":\"garden\",\"name\":\"Garden\"}]}";

static const char *FX_ENTITY_REG =
"{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":["
"{\"entity_id\":\"light.floor_lamp\",\"area_id\":\"living_room\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"light.ceiling\",\"area_id\":null,\"device_id\":\"dev_kitchen_light\",\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"light.bedside\",\"area_id\":\"bedroom\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"climate.thermostat\",\"area_id\":\"living_room\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"media_player.living_room_tv\",\"area_id\":\"living_room\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"media_player.kitchen_speaker\",\"area_id\":\"kitchen\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"cover.bedroom_blinds\",\"area_id\":\"bedroom\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"fan.bedroom_fan\",\"area_id\":\"bedroom\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"lock.front_door\",\"area_id\":\"hallway\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"switch.kettle\",\"area_id\":null,\"device_id\":\"dev_kettle\",\"entity_category\":null,\"hidden_by\":null},"
"{\"entity_id\":\"switch.kettle_child_lock\",\"area_id\":null,\"device_id\":\"dev_kettle\",\"entity_category\":\"config\",\"hidden_by\":null},"
"{\"entity_id\":\"sensor.garden_temperature\",\"area_id\":\"garden\",\"device_id\":null,\"entity_category\":null,\"hidden_by\":null}]}";

static const char *FX_DEVICE_REG =
"{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":["
"{\"id\":\"dev_kitchen_light\",\"area_id\":\"kitchen\",\"name\":\"Kitchen ceiling\"},"
"{\"id\":\"dev_kettle\",\"area_id\":\"kitchen\",\"name\":\"Kettle plug\"}]}";

static const char *FX_SUB_OK = "{\"id\":%d,\"type\":\"result\",\"success\":true,\"result\":null}";

static const char *FX_EVT_TV_PLAYING =
"{\"id\":%d,\"type\":\"event\",\"event\":{\"event_type\":\"state_changed\",\"data\":{\"entity_id\":\"media_player.living_room_tv\","
"\"new_state\":{\"entity_id\":\"media_player.living_room_tv\",\"state\":\"playing\",\"attributes\":{\"friendly_name\":\"TV\",\"volume_level\":0.6,\"media_title\":\"Radio 2\",\"supported_features\":21437}}}}}";

static const char *FX_EVT_DOOR_UNLOCKED =
"{\"id\":%d,\"type\":\"event\",\"event\":{\"event_type\":\"state_changed\",\"data\":{\"entity_id\":\"lock.front_door\","
"\"new_state\":{\"entity_id\":\"lock.front_door\",\"state\":\"unlocked\",\"attributes\":{\"friendly_name\":\"Front door\"}}}}}";

static const char *FX_EVT_HEAT_ON =
"{\"id\":%d,\"type\":\"event\",\"event\":{\"event_type\":\"state_changed\",\"data\":{\"entity_id\":\"climate.thermostat\","
"\"new_state\":{\"entity_id\":\"climate.thermostat\",\"state\":\"heat\",\"attributes\":{\"friendly_name\":\"Thermostat\",\"hvac_modes\":[\"off\",\"heat\"],\"min_temp\":7,\"max_temp\":30,\"target_temp_step\":0.5,\"temperature\":22.5,\"current_temperature\":19.8,\"supported_features\":385}}}}}";

static const char *FX_EVT_REMOVED =
"{\"id\":%d,\"type\":\"event\",\"event\":{\"event_type\":\"state_changed\",\"data\":{\"entity_id\":\"switch.kettle\",\"new_state\":null}}}";

static const char *FX_CALL_FAIL =
"{\"id\":%d,\"type\":\"result\",\"success\":false,\"error\":{\"code\":\"unauthorized\",\"message\":\"Unauthorized\"}}";

static const char *FX_VOICE_START = "{\"id\":%d,\"type\":\"event\",\"event\":{\"type\":\"run-start\",\"data\":{\"pipeline\":\"x\",\"language\":\"en\",\"runner_data\":{\"stt_binary_handler_id\":1,\"timeout\":30}},\"timestamp\":\"t\"}}";
static const char *FX_VOICE_STT = "{\"id\":%d,\"type\":\"event\",\"event\":{\"type\":\"stt-end\",\"data\":{\"stt_output\":{\"text\":\"Turn on the floor lamp\"}},\"timestamp\":\"t\"}}";
static const char *FX_VOICE_INTENT = "{\"id\":%d,\"type\":\"event\",\"event\":{\"type\":\"intent-end\",\"data\":{\"intent_output\":{\"response\":{\"speech\":{\"plain\":{\"speech\":\"Turned on the light\",\"extra_data\":null}},\"response_type\":\"action_done\"},\"conversation_id\":\"c1\"}},\"timestamp\":\"t\"}}";
static const char *FX_VOICE_END = "{\"id\":%d,\"type\":\"event\",\"event\":{\"type\":\"run-end\",\"data\":null,\"timestamp\":\"t\"}}";
