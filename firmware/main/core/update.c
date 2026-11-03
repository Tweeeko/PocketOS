// PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
#include "update.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "cJSON.h"

static bool parse(const char *v, long out[3], bool *pre)
{
    if (!v) return false;
    if (*v == 'v' || *v == 'V') v++;
    for (int i = 0; i < 3; i++) {
        if (!isdigit((unsigned char)*v)) return false;
        char *e;
        out[i] = strtol(v, &e, 10);
        if (out[i] < 0 || out[i] > 99999) return false;
        v = e;
        if (i < 2) { if (*v != '.') return false; v++; }
    }
    if (*v && *v != '-' && *v != '+') return false;
    *pre = (*v == '-');
    return true;
}

bool version_valid(const char *v) { long x[3]; bool p; return parse(v, x, &p); }

int version_cmp(const char *a, const char *b)
{
    long x[3], y[3];
    bool pa, pb;
    if (!parse(a, x, &pa) || !parse(b, y, &pb)) return -2;
    for (int i = 0; i < 3; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    if (pa != pb) return pa ? -1 : 1;
    return 0;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

#define FAIL(msg) do { *err = msg; cJSON_Delete(j); return false; } while (0)

bool update_parse_manifest(const char *json, update_manifest_t *m, const char **err)
{
    memset(m, 0, sizeof *m);
    cJSON *j = cJSON_Parse(json);
    if (!cJSON_IsObject(j)) FAIL("Update information is damaged");
    const char *ver = cJSON_GetStringValue(cJSON_GetObjectItem(j, "version"));
    const char *url = cJSON_GetStringValue(cJSON_GetObjectItem(j, "url"));
    const char *sha = cJSON_GetStringValue(cJSON_GetObjectItem(j, "sha256"));
    const char *notes = cJSON_GetStringValue(cJSON_GetObjectItem(j, "notes"));
    const cJSON *size = cJSON_GetObjectItem(j, "size");
    if (!ver || !version_valid(ver) || strlen(ver) >= sizeof m->version) FAIL("Update has no valid version");
    if (!url || strncmp(url, "https://", 8) != 0 || strlen(url) >= sizeof m->url) FAIL("Update address must be https://");
    if (!sha || strlen(sha) != 64) FAIL("Update has no valid checksum");
    for (int i = 0; i < 32; i++) {
        int h = hexval(sha[2 * i]), l = hexval(sha[2 * i + 1]);
        if (h < 0 || l < 0) FAIL("Update has no valid checksum");
        m->sha256[i] = (uint8_t)(h << 4 | l);
    }
    if (!cJSON_IsNumber(size) || size->valuedouble < 1024 || size->valuedouble > UPDATE_MAX_SIZE) FAIL("Update size is wrong");
    snprintf(m->version, sizeof m->version, "%s", ver[0] == 'v' || ver[0] == 'V' ? ver + 1 : ver);
    snprintf(m->url, sizeof m->url, "%s", url);
    m->size = (uint32_t)size->valuedouble;
    snprintf(m->notes, sizeof m->notes, "%s", notes ? notes : "");
    cJSON_Delete(j);
    *err = NULL;
    return true;
}
