// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Firmware update helpers: version comparison and checking the release manifest.
// Portable C (unit-tested on a PC). The download itself is in esp/updater.c.
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define UPDATE_MAX_SIZE (6 * 1024 * 1024)   // one app slot (partitions.csv)

typedef struct {
    char version[16];      // "0.2.0"
    char url[256];         // https:// address of pocketos.bin
    uint8_t sha256[32];    // SHA-256 of the whole pocketos.bin file
    uint32_t size;         // bytes
    char notes[400];       // what's new, plain text
    int rollout;           // 0..100: share of devices offered it automatically (staged release); 100 if absent
} update_manifest_t;

// Compares "1.2.3" style versions (a leading "v" is ignored). Returns <0, 0 or >0.
// A version with a suffix ("1.2.3-beta") counts as older than the same version without one.
// Returns -2 if either isn't a valid version.
int version_cmp(const char *a, const char *b);
bool version_valid(const char *v);

// Parses manifest.json from a release. Everything is checked; on failure *err says why.
bool update_parse_manifest(const char *json, update_manifest_t *m, const char **err);

// Signed firmware: pocketos.bin = the app image followed by an 80-byte trailer
//   [0..71] ECDSA P-256 signature (DER, zero padded) over SHA-256 of the image
//   [72..73] signature length, little-endian   [74..79] "PSIG01"
// The key that signs releases is kept by Leeko; devices hold only the public half.
#define UPDATE_TRAILER 80
bool update_trailer(const uint8_t tail[UPDATE_TRAILER], const uint8_t **sig, size_t *sig_len);

// Staged releases: which of 100 groups this device is in (from its MAC address), so the same
// device always gets the same answer.
int update_bucket(const uint8_t mac[6]);
