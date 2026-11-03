// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "sigcheck.h"
#include <string.h>
#include "mbedtls/pk.h"
#include "esp_log.h"

static const char *TAG = "sig";
extern const char signing_key_pem_start[] asm("_binary_signing_key_pem_start");
extern const char signing_key_pem_end[] asm("_binary_signing_key_pem_end");

void sig_begin(sig_stream_t *s, uint32_t total_size)
{
    memset(s, 0, sizeof *s);
    s->total = total_size;
    mbedtls_sha256_init(&s->img);
    mbedtls_sha256_starts(&s->img, 0);
}

size_t sig_feed(sig_stream_t *s, const uint8_t *data, size_t n)
{
    uint32_t img_end = s->total > UPDATE_TRAILER ? s->total - UPDATE_TRAILER : 0;
    size_t img = 0;
    if (s->seen < img_end) img = (img_end - s->seen) < n ? (img_end - s->seen) : n;
    if (img) mbedtls_sha256_update(&s->img, data, img);
    for (size_t i = img; i < n; i++) {
        uint32_t at = s->seen + (uint32_t)i - img_end;
        if (at < UPDATE_TRAILER) s->tail[at] = data[i];
    }
    s->seen += (uint32_t)n;
    return img;
}

bool sig_finish(sig_stream_t *s, const char **why)
{
    unsigned char hash[32];
    mbedtls_sha256_finish(&s->img, hash);
    mbedtls_sha256_free(&s->img);
    const uint8_t *sig;
    size_t len;
    if (s->seen != s->total || !update_trailer(s->tail, &sig, &len)) { *why = "This firmware isn't signed by PocketOS"; return false; }
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int r = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)signing_key_pem_start,
                                        (size_t)(signing_key_pem_end - signing_key_pem_start));
    if (r == 0) r = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof hash, sig, len);
    mbedtls_pk_free(&pk);
    if (r != 0) { ESP_LOGW(TAG, "signature check failed (%d)", r); *why = "This firmware isn't signed by PocketOS"; return false; }
    return true;
}

void sig_abort(sig_stream_t *s) { mbedtls_sha256_free(&s->img); }
