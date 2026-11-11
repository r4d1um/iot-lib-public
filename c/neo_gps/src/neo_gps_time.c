/*
 * neo_gps_time - Ortszeit mit Zeitzone und Sommerzeit (Teil von neo_gps).
 * Rein 32-Bit-ganzzahlig, gueltig fuer 1970..2105.
 * SPDX-License-Identifier: MIT
 */
#include "neo_gps_time.h"
#include <string.h>

#define DAY_S ((uint32_t)86400UL)

/* Tage seit 1970-01-01 (Howard Hinnant, days_from_civil) */
static uint32_t days_from_civil(uint32_t y, uint32_t m, uint32_t d)
{
    uint32_t era, yoe, doy, doe;
    y -= (m <= 2u);
    era = y / 400u;
    yoe = y - era * 400u;
    doy = (153u * (m > 2u ? m - 3u : m + 9u) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (uint32_t)(era * 146097UL + doe - 719468UL);
}

static uint8_t days_in_month(uint32_t y, uint8_t m)
{
    static const uint8_t dim[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2u && ((y % 4u == 0u && y % 100u != 0u) || y % 400u == 0u)) return 29;
    return dim[m - 1u];
}

uint32_t neo_time_to_unix(uint16_t year, uint8_t month, uint8_t day)
{
    if (year < 1970u || month < 1u || month > 12u || day < 1u) return 0;
    return (uint32_t)(days_from_civil(year, month, day) * 86400UL);
}

uint32_t neo_gps_unix_time(const neo_gps_data_t *d)
{
    const neo_datetime_t *t = &d->time;
    if (!d->date_valid || !d->time_valid || t->year < 1970u || t->month < 1u || t->month > 12u ||
        t->day < 1u || t->hour > 23u || t->minute > 59u || t->second > 60u)
        return 0;
    return (uint32_t)(days_from_civil(t->year, t->month, t->day) * 86400UL +
                      t->hour * 3600UL + t->minute * 60UL + t->second);
}

uint8_t neo_time_from_unix(uint32_t t, neo_datetime_t *out)
{
    /* civil_from_days (Howard Hinnant) */
    uint32_t z = t / DAY_S, s = t % DAY_S;
    uint32_t era, doe, yoe, doy, mp, y, m;
    uint8_t wday = (uint8_t)((z + 4u) % 7u);       /* 1970-01-01 war ein Donnerstag */
    z += 719468UL;
    era = z / 146097UL;
    doe = z - era * (uint32_t)146097UL;
    yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    mp  = (5u * doy + 2u) / 153u;
    m   = mp < 10u ? mp + 3u : mp - 9u;
    y   = yoe + era * 400u + (m <= 2u);
    out->year   = (uint16_t)y;
    out->month  = (uint8_t)m;
    out->day    = (uint8_t)(doy - (153u * mp + 2u) / 5u + 1u);
    out->hour   = (uint8_t)(s / 3600u);
    out->minute = (uint8_t)(s / 60u % 60u);
    out->second = (uint8_t)(s % 60u);
    out->ms     = 0;
    return wday;
}

/* Tag (Tage seit 1970) der Regel "w-ter Wochentag wd im Monat m", w = 5: letzter */
static uint32_t rule_day(uint32_t y, uint8_t m, uint8_t w, uint8_t wd)
{
    uint32_t first = days_from_civil(y, m, 1);
    uint32_t wd1 = (first + 4u) % 7u;
    uint32_t day = first + (wd + 7u - wd1) % 7u + (uint32_t)(w - 1u) * 7u;
    uint32_t end = first + days_in_month(y, m);
    while (day >= end) day -= 7u;
    return day;
}

/* t + Minuten (auch negativ), modulo 2^32 - wohldefiniert fuer unsigned */
static uint32_t add_min(uint32_t t, int32_t minutes) { return t + (uint32_t)(minutes * 60L); }

uint32_t neo_tz_utc_to_local(const neo_tz_t *tz, uint32_t utc, int16_t *offset_min, uint8_t *is_dst)
{
    uint8_t dst = 0;
    int16_t off = tz->std_offset;

    if (tz->dst_offset != tz->std_offset && tz->start_month && tz->end_month) {
        neo_datetime_t lt;
        uint32_t y, s, e;
        neo_time_from_unix(add_min(utc, tz->std_offset), &lt);     /* Jahr in lokaler Normalzeit */
        y = lt.year;
        /* Umschaltzeitpunkte in UTC: Beginn in Normalzeit, Ende in Sommerzeit angegeben */
        s = add_min(rule_day(y, tz->start_month, tz->start_week, tz->start_wday) * DAY_S,
                    (int32_t)tz->start_min - tz->std_offset);
        e = add_min(rule_day(y, tz->end_month, tz->end_week, tz->end_wday) * DAY_S,
                    (int32_t)tz->end_min - tz->dst_offset);
        dst = (s < e) ? (utc >= s && utc < e)          /* Nordhalbkugel */
                      : (utc >= s || utc < e);         /* Suedhalbkugel: ueber den Jahreswechsel */
        if (dst) off = tz->dst_offset;
    }
    if (offset_min) *offset_min = off;
    if (is_dst) *is_dst = dst;
    return add_min(utc, off);
}

int neo_gps_local_time(const neo_gps_data_t *d, const neo_tz_t *tz, neo_local_time_t *out)
{
    uint32_t utc = neo_gps_unix_time(d);
    uint32_t loc;
    if (!utc) return 0;
    loc = neo_tz_utc_to_local(tz, utc, &out->offset_min, &out->is_dst);
    out->weekday = neo_time_from_unix(loc, &out->t);
    out->t.ms = d->time.ms;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* POSIX-TZ-Parser                                                           */
/* ------------------------------------------------------------------------- */

static const char *p_num(const char *s, uint8_t max_digits, int32_t *v)
{
    int32_t n = 0;
    uint8_t i = 0;
    while (i < max_digits && *s >= '0' && *s <= '9') { n = n * 10 + (*s++ - '0'); i++; }
    if (!i) return 0;
    *v = n;
    return s;
}

/* Name: mind. 3 Buchstaben oder <...> */
static const char *p_name(const char *s)
{
    const char *b;
    if (!s) return 0;
    if (*s == '<') {
        while (*s && *s != '>') s++;
        return *s == '>' ? s + 1 : 0;
    }
    b = s;
    while ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) s++;
    return (s - b >= 3) ? s : 0;
}

/* [+-]hh[:mm[:ss]] -> Minuten */
static const char *p_hms(const char *s, int32_t *minutes)
{
    int32_t h, m = 0, sec;
    uint8_t neg = 0;
    if (!s) return 0;
    if (*s == '+' || *s == '-') neg = (*s++ == '-');
    if (!(s = p_num(s, 3, &h)) || h > 167) return 0;
    if (*s == ':') {
        if (!(s = p_num(s + 1, 2, &m)) || m > 59) return 0;
        if (*s == ':' && !(s = p_num(s + 1, 2, &sec))) return 0;   /* Sekunden ignoriert */
    }
    *minutes = neg ? -(h * 60 + m) : h * 60 + m;
    return s;
}

/* Mm.w.d[/time] */
static const char *p_rule(const char *s, uint8_t *mo, uint8_t *wk, uint8_t *wd, int16_t *tm)
{
    int32_t m, w, d, t = 120;                       /* Standard: 02:00 */
    if (!s || *s != 'M') return 0;
    if (!(s = p_num(s + 1, 2, &m)) || m < 1 || m > 12 || *s != '.') return 0;
    if (!(s = p_num(s + 1, 1, &w)) || w < 1 || w > 5 || *s != '.') return 0;
    if (!(s = p_num(s + 1, 1, &d)) || d > 6) return 0;
    if (*s == '/' && !(s = p_hms(s + 1, &t))) return 0;
    *mo = (uint8_t)m; *wk = (uint8_t)w; *wd = (uint8_t)d; *tm = (int16_t)t;
    return s;
}

int neo_tz_parse(neo_tz_t *tz, const char *s)
{
    neo_tz_t r;
    int32_t off;

    if (!s) return 0;
    memset(&r, 0, sizeof(r));

    if (!(s = p_hms(p_name(s), &off)) || off > 24 * 60 || off < -24 * 60) return 0;
    r.std_offset = (int16_t)-off;                   /* POSIX: Vorzeichen umgekehrt (CET-1 = UTC+1) */
    r.dst_offset = r.std_offset;

    if (*s) {
        if (!(s = p_name(s))) return 0;
        r.dst_offset = (int16_t)(r.std_offset + 60);
        if (*s && *s != ',') {
            if (!(s = p_hms(s, &off)) || off > 24 * 60 || off < -24 * 60) return 0;
            r.dst_offset = (int16_t)-off;
        }
        if (*s != ',') return 0;                    /* Sommerzeit ohne Regel: nicht unterstuetzt */
        s = p_rule(s + 1, &r.start_month, &r.start_week, &r.start_wday, &r.start_min);
        if (!s || *s != ',') return 0;
        s = p_rule(s + 1, &r.end_month, &r.end_week, &r.end_wday, &r.end_min);
        if (!s || *s) return 0;
    }
    *tz = r;
    return 1;
}
