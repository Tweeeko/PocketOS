// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#pragma once
// Checks that firmware was signed by PocketOS's release key before it is installed.
// Feed every byte of pocketos.bin as it arrives; only the image part is returned for writing.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mbedtls/sha256.h"
#include "update.h"

typedef struct {
    mbedtls_sha256_context img;
    uint32_t total, seen;
    uint8_t tail[UPDATE_TRAILER];
} sig_stream_t;

void sig_begin(sig_stream_t *s, uint32_t total_size);
// How many of these n bytes belong to the image (the rest is the signature trailer).
size_t sig_feed(sig_stream_t *s, const uint8_t *data, size_t n);
// true if the image is signed by the release key. On false, *why says why.
bool sig_finish(sig_stream_t *s, const char **why);
void sig_abort(sig_stream_t *s);
