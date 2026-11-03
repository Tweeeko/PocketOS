#include "battery.h"
#include <string.h>

#define PLAUSIBLE_T   1704067200u   // 2024-01-01: anything earlier means the clock isn't set
#define MAX_GAP_S     (2 * 3600)    // a longer gap (switched off) splits a stretch on battery
#define MIN_SPAN_S    3600          // need at least an hour on battery...
#define MIN_DROP_PCT  2             // ...and a visible drop before estimating
#define MAX_SPAN_S    (24 * 3600)   // only the latest day of a stretch counts

void batt_log_init(batt_log_t *l)
{
    memset(l, 0, sizeof *l);
    l->magic = BATT_MAGIC;
}

bool batt_log_valid(const batt_log_t *l)
{
    return l->magic == BATT_MAGIC && l->n <= BATT_MAX_SAMPLES && l->head < BATT_MAX_SAMPLES;
}

int batt_log_count(const batt_log_t *l) { return l->n; }

bool batt_log_get(const batt_log_t *l, int i, batt_sample_t *out)
{
    if (i < 0 || i >= l->n) return false;
    int idx = (l->head - l->n + i + BATT_MAX_SAMPLES) % BATT_MAX_SAMPLES;
    *out = l->s[idx];
    return true;
}

static const batt_sample_t *newest(const batt_log_t *l)
{
    return l->n ? &l->s[(l->head + BATT_MAX_SAMPLES - 1) % BATT_MAX_SAMPLES] : NULL;
}

bool batt_log_add(batt_log_t *l, uint32_t now, int pct, int mv, bool charging, bool plugged)
{
    if (!batt_log_valid(l)) batt_log_init(l);
    if (now < PLAUSIBLE_T || pct < 0 || pct > 100) return false;
    if (plugged && pct >= 100) l->last_full_t = now;
    const batt_sample_t *last = newest(l);
    uint8_t flags = (charging ? BATT_CHARGING : 0) | (plugged ? BATT_PLUGGED : 0);
    if (last) {
        if (now < last->t) return false;   // clock went backwards: ignore until it catches up
        bool plug_changed = (last->flags & BATT_PLUGGED) != (flags & BATT_PLUGGED);
        if (!plug_changed && now - last->t < BATT_SAMPLE_S) return false;
    }
    batt_sample_t *s = &l->s[l->head];
    s->t = now;
    s->pct = (uint8_t)pct;
    s->mv = (uint16_t)(mv > 0 && mv < 65535 ? mv : 0);
    s->flags = flags;
    l->head = (uint16_t)((l->head + 1) % BATT_MAX_SAMPLES);
    if (l->n < BATT_MAX_SAMPLES) l->n++;
    return true;
}

// Least-squares drain over samples [a, b] (oldest-first indices), limited to the last 24 h.
static bool fit(const batt_log_t *l, int a, int b, float *rate, float *span_h)
{
    batt_sample_t last = {0}, s = {0};
    batt_log_get(l, b, &last);
    while (a < b) { batt_log_get(l, a, &s); if (last.t - s.t <= MAX_SPAN_S) break; a++; }
    batt_log_get(l, a, &s);
    if (b - a < 2 || last.t - s.t < MIN_SPAN_S) return false;
    int hi = 0, lo = 100;
    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = a; i <= b; i++) {
        batt_log_get(l, i, &s);
        double x = ((double)s.t - (double)last.t) / 3600.0, y = s.pct;   // hours before the newest sample (negative)
        n++; sx += x; sy += y; sxx += x * x; sxy += x * y;
        if (s.pct > hi) hi = s.pct;
        if (s.pct < lo) lo = s.pct;
    }
    double den = n * sxx - sx * sx;
    if (hi - lo < MIN_DROP_PCT || den <= 0) return false;
    double slope = (n * sxy - sx * sy) / den;   // % per hour (negative while draining)
    if (slope >= -0.01) return false;
    *rate = (float)-slope;
    batt_log_get(l, a, &s);
    *span_h = (float)(last.t - s.t) / 3600.0f;
    return true;
}

batt_est_t batt_estimate(const batt_log_t *l, uint32_t now, int pct_now, bool plugged_now)
{
    batt_est_t e = {.known = false, .pct_per_h = 0, .hours_left = -1, .full_h = -1, .basis_h = 0};
    (void)now;
    int n = batt_log_count(l);
    // Walk stretches on battery from newest to oldest; use the first one that gives a fit.
    int end = n - 1;
    while (end >= 1) {
        batt_sample_t s = {0}, p = {0};
        batt_log_get(l, end, &s);
        if (s.flags & BATT_PLUGGED) { end--; continue; }
        int start = end;
        while (start > 0) {
            batt_log_get(l, start - 1, &p);
            batt_log_get(l, start, &s);
            if ((p.flags & BATT_PLUGGED) || s.t - p.t > MAX_GAP_S) break;
            start--;
        }
        float rate, span;
        if (fit(l, start, end, &rate, &span)) {
            e.known = true;
            e.pct_per_h = rate;
            e.basis_h = span;
            e.full_h = 100.0f / rate;
            if (!plugged_now && pct_now >= 0) e.hours_left = (float)pct_now / rate;
            if (e.full_h > 999) e.full_h = 999;
            if (e.hours_left > 999) e.hours_left = 999;
            return e;
        }
        end = start - 1;
    }
    return e;
}
