// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Push-to-talk: hold BOOT, speak, release. Audio goes to Home Assistant's Assist pipeline
// (speech-to-text + intent), which needs a voice pipeline set up in Home Assistant.
// PocketOS shows what was heard and Home Assistant's reply on screen.
#include "ha_proto.h"

void voice_start(void);                    // button held
void voice_stop(void);                     // button released
void voice_on_event(const ha_event_t *ev); // from the Home Assistant client
