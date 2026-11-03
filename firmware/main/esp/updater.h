// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Over-the-air updates from the published releases (see docs/releasing.md).
//
// PocketOS fetches a small manifest.json over HTTPS, and if it names a newer version, downloads
// pocketos.bin, checks its size and SHA-256 against the manifest while writing it to the spare
// app slot, then restarts into it. If the new version doesn't start properly, the bootloader
// goes back to the old one. Nothing about the home or its devices is sent: it's a plain download.
#include <stdbool.h>
#include "cJSON.h"

void updater_start(void);            // background checks (once after start-up, then daily)
bool updater_check_now(void);        // false if a check or install is already running
bool updater_install(const char **why_not);   // starts the download; false + reason if it can't
void updater_info(cJSON *obj);       // adds the "update" object to /info
