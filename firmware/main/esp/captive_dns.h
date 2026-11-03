// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Setup mode only: answers every DNS lookup with 192.168.4.1 so a phone that joins
// PocketOS's setup network opens the setup page automatically (captive portal).
void captive_dns_start(void);
