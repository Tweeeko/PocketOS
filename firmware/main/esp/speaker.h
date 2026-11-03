// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
#include <stdbool.h>

// Plays Home Assistant's spoken reply through the built-in speaker.
// `url` is the full address of the audio (it carries a one-off token, so it is never logged);
// `mime` is Home Assistant's audio type ("audio/mpeg", "audio/wav", "audio/flac").
// Returns at once: the audio is fetched, decoded and played on its own task.
void speaker_play(const char *url, const char *mime, int volume);
void speaker_stop(void);     // stops a reply that is still playing (e.g. the user talks again)
bool speaker_busy(void);
