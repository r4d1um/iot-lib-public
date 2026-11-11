/*
 * neo_gps - u-blox NEO-6M / NEO-7M / NEO-M8N Library
 * SPDX-License-Identifier: MIT
 */
#include "neo_gps.h"
#include <string.h>

/* Parser-Zustaende */
enum {
    ST_IDLE = 0,
    ST_NMEA, ST_NMEA_CK1, ST_NMEA_CK2,
    ST_UBX_SYNC2, ST_UBX_CLASS, ST_UBX_ID, ST_UBX_LEN1, ST_UBX_LEN2,
    ST_UBX_PAYLOAD, ST_UBX_CKA, ST_UBX_CKB
};

void neo_gps_init(neo_gps_t *gps, neo_write_fn write, void *user)
{
    memset(gps, 0, sizeof(*gps));
    gps->write = write;
    gps->user  = user;
}

uint16_t neo_gps_updates(neo_gps_t *gps)
{
    uint16_t u = gps->data.updated;
    gps->data.updated = 0;
    return u;
}

/* ========================================================================= */
/* Positionsuebernahme (Filter haengt sich per Funktionszeiger ein, damit er  */
/* nur gelinkt wird, wenn neo_gps_filter_* auch benutzt wird)                */
/* ========================================================================= */
#define NO_TIME NEO_GPS_TIME_UNKNOWN

static uint8_t pos_commit(neo_gps_t *g, int32_t lat, int32_t lon, uint32_t t, uint32_t period)
{
#if NEO_GPS_ENABLE_FILTER
    g->data.raw_lat = lat;
    g->data.raw_lon = lon;
    if (g->f_fn) return g->f_fn(g, lat, lon, t, period);
#else
    (void)t; (void)period;
#endif
    g->data.lat = lat; g->data.lon = lon;
    g->data.updated |= NEO_UPD_POSITION;
    return 1;
}

/* ========================================================================= */
/* NMEA                                                                      */
/* ========================================================================= */
#if NEO_GPS_ENABLE_NMEA

enum { T_NONE = 0, T_RMC, T_GGA, T_GSA, T_VTG, T_GSV };

#define H_TIME    0x0001u
#define H_LAT     0x0002u
#define H_LON     0x0004u
#define H_ALT     0x0008u
#define H_SEP     0x0010u
#define H_SPEED   0x0020u
#define H_COURSE  0x0040u
#define H_HDOP    0x0080u
#define H_PDOP    0x0100u
#define H_VDOP    0x0200u
#define H_DATE    0x0400u
#define H_NUMSV   0x0800u
#define H_QUAL    0x1000u
#define H_NAVMODE 0x2000u
#define H_INVIEW  0x4000u

#define NMEA_MAX_LEN 100u   /* Standard: 82, etwas Reserve fuer NMEA 4.1 */

/* "123.4567" -> Ganzzahl mit genau 'dec' Nachkommastellen (abschneiden/auffuellen). */
static uint8_t parse_ufixed(const char *s, uint8_t n, uint8_t dec, uint32_t *out)
{
    uint32_t v = 0;
    uint8_t i, d = 0, dot = 0, any = 0;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            if (dot) { if (d >= dec) continue; d++; }
            v = v * 10u + (uint32_t)(c - '0');
            any = 1;
        } else if (c == '.' && !dot) {
            dot = 1;
        } else {
            return 0;
        }
    }
    if (!any) return 0;
    while (d < dec) { v *= 10u; d++; }
    *out = v;
    return 1;
}

static uint8_t parse_sfixed(const char *s, uint8_t n, uint8_t dec, int32_t *out)
{
    uint32_t v;
    if (n && s[0] == '-') {
        if (!parse_ufixed(s + 1, (uint8_t)(n - 1), dec, &v)) return 0;
        *out = -(int32_t)v;
        return 1;
    }
    if (!parse_ufixed(s, n, dec, &v)) return 0;
    *out = (int32_t)v;
    return 1;
}

/* "dddmm.mmmmm" -> 1e-7 Grad, rein ganzzahlig: Grad*1e7 + Minuten_e5 * 5/3 */
static uint8_t parse_coord(const char *s, uint8_t n, int32_t *out)
{
    uint32_t v, deg, min_e5;
    if (!parse_ufixed(s, n, 5, &v)) return 0;
    deg    = v / 10000000UL;
    min_e5 = v % 10000000UL;
    if (min_e5 >= 6000000UL || deg > 180u) return 0;
    *out = (int32_t)(deg * 10000000UL + (min_e5 * 5UL + 1UL) / 3UL);
    return 1;
}

static uint8_t hexval(uint8_t c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0xFF;
}

/* Feld 0 (Adresse) auswerten. 0 = Satz uninteressant -> verwerfen */
static uint8_t nmea_address(neo_gps_t *g, const char *b, uint8_t n)
{
    uint8_t t;
    if (n != 5) return 0;
    if      (b[2] == 'R' && b[3] == 'M' && b[4] == 'C') t = T_RMC;
    else if (b[2] == 'G' && b[3] == 'G' && b[4] == 'A') t = T_GGA;
    else if (b[2] == 'G' && b[3] == 'S' && b[4] == 'A') t = T_GSA;
    else if (b[2] == 'V' && b[3] == 'T' && b[4] == 'G') t = T_VTG;
    else if (b[2] == 'G' && b[3] == 'S' && b[4] == 'V') t = T_GSV;
    else return 0;

    g->n_type = t;
    /* Talker fuer GSV: GP=0, GL=1, GA=2, GB/BD=3 */
    if (b[0] == 'G' && b[1] == 'P')      g->n_talker = 0;
    else if (b[0] == 'G' && b[1] == 'L') g->n_talker = 1;
    else if (b[0] == 'G' && b[1] == 'A') g->n_talker = 2;
    else if ((b[0] == 'G' && b[1] == 'B') || (b[0] == 'B' && b[1] == 'D')) g->n_talker = 3;
    else g->n_talker = 0xFF;
    return 1;
}

/* Ein komplettes Feld verarbeiten. Leere Felder werden einfach ignoriert. */
static uint8_t nmea_field(neo_gps_t *g)
{
    const char *b = g->n_buf;
    uint8_t n = g->n_flen;
    uint8_t f = g->n_field;
    uint32_t u;

    if (f == 0) return nmea_address(g, b, n);
    if (n == 0 || n > NEO_GPS_NMEA_FIELD_MAX) return 1;

    switch (g->n_type) {
    case T_RMC:
        switch (f) {
        case 1: if (parse_ufixed(b, n, 3, &g->n.time_raw)) g->n.have |= H_TIME; break;
        case 2: g->n.status = (uint8_t)b[0]; break;
        case 3: if (parse_coord(b, n, &g->n.lat)) g->n.have |= H_LAT; break;
        case 4: if (b[0] == 'S') g->n.lat = -g->n.lat; break;
        case 5: if (parse_coord(b, n, &g->n.lon)) g->n.have |= H_LON; break;
        case 6: if (b[0] == 'W') g->n.lon = -g->n.lon; break;
        case 7: if (parse_ufixed(b, n, 3, &g->n.speed_raw)) g->n.have |= H_SPEED; break;
        case 8: if (parse_sfixed(b, n, 5, &g->n.course)) g->n.have |= H_COURSE; break;
        case 9: if (parse_ufixed(b, n, 0, &g->n.date_raw)) g->n.have |= H_DATE; break;
        case 12: g->n.mode = (uint8_t)b[0]; break;
        default: break;
        }
        break;

    case T_GGA:
        switch (f) {
        case 1: if (parse_ufixed(b, n, 3, &g->n.time_raw)) g->n.have |= H_TIME; break;
        case 2: if (parse_coord(b, n, &g->n.lat)) g->n.have |= H_LAT; break;
        case 3: if (b[0] == 'S') g->n.lat = -g->n.lat; break;
        case 4: if (parse_coord(b, n, &g->n.lon)) g->n.have |= H_LON; break;
        case 5: if (b[0] == 'W') g->n.lon = -g->n.lon; break;
        case 6: g->n.quality = (uint8_t)(b[0] - '0'); g->n.have |= H_QUAL; break;
        case 7: if (parse_ufixed(b, n, 0, &u)) { g->n.num_sv = (uint8_t)u; g->n.have |= H_NUMSV; } break;
        case 8: if (parse_ufixed(b, n, 2, &u)) { g->n.hdop = (uint16_t)u; g->n.have |= H_HDOP; } break;
        case 9: if (parse_sfixed(b, n, 3, &g->n.alt)) g->n.have |= H_ALT; break;
        case 11: if (parse_sfixed(b, n, 3, &g->n.sep)) g->n.have |= H_SEP; break;
        default: break;
        }
        break;

    case T_GSA:
        switch (f) {
        case 2: g->n.nav_mode = (uint8_t)(b[0] - '0'); g->n.have |= H_NAVMODE; break;
        case 15: if (parse_ufixed(b, n, 2, &u)) { g->n.pdop = (uint16_t)u; g->n.have |= H_PDOP; } break;
        case 16: if (parse_ufixed(b, n, 2, &u)) { g->n.hdop = (uint16_t)u; g->n.have |= H_HDOP; } break;
        case 17: if (parse_ufixed(b, n, 2, &u)) { g->n.vdop = (uint16_t)u; g->n.have |= H_VDOP; } break;
        default: break;
        }
        break;

    case T_VTG:
        switch (f) {
        case 1: if (parse_sfixed(b, n, 5, &g->n.course)) g->n.have |= H_COURSE; break;
        case 5: if (parse_ufixed(b, n, 3, &g->n.speed_raw)) g->n.have |= H_SPEED; break;
        case 9: g->n.mode = (uint8_t)b[0]; break;
        default: break;
        }
        break;

    case T_GSV:
        if (f == 3 && parse_ufixed(b, n, 0, &u)) { g->n.in_view = (uint8_t)u; g->n.have |= H_INVIEW; }
        break;

    default: break;
    }
    return 1;
}

static void set_time(neo_gps_data_t *d, uint32_t t)
{
    /* t = hhmmss * 1000 + ms */
    d->time.ms     = (uint16_t)(t % 1000u);  t /= 1000u;
    d->time.second = (uint8_t)(t % 100u);    t /= 100u;
    d->time.minute = (uint8_t)(t % 100u);
    d->time.hour   = (uint8_t)(t / 100u);
    d->time_valid  = 1;
    d->updated    |= NEO_UPD_TIME;
}

static void set_date(neo_gps_data_t *d, uint32_t v)
{
    /* ddmmyy */
    d->time.year  = (uint16_t)(2000u + v % 100u);
    d->time.month = (uint8_t)((v / 100u) % 100u);
    d->time.day   = (uint8_t)(v / 10000u);
    d->date_valid = 1;
    d->updated   |= NEO_UPD_DATE;
}

/* Knoten*1e3 -> mm/s  (1 kn = 1852/3600 m/s = 463/900 m/s) */
static uint32_t knots_to_mms(uint32_t k_e3) { return (uint32_t)((k_e3 * 463UL + 450UL) / 900UL); }

/* Pruefsumme stimmte: Werte uebernehmen */
/* hhmmss*1000+ms -> ms seit Mitternacht (fuer den Filter) */
static uint32_t nmea_ms_of_day(const neo_gps_t *g)
{
    uint32_t t = g->n.time_raw;
    if (!(g->n.have & H_TIME)) return NO_TIME;
    return (uint32_t)((t / 10000000UL) * 3600000UL + (t / 100000UL % 100u) * 60000UL +
                      (t / 1000u % 100u) * 1000u + t % 1000u);
}

static neo_event_t nmea_commit(neo_gps_t *g)
{
    neo_gps_data_t *d = &g->data;
    uint16_t h = g->n.have;

    switch (g->n_type) {
    case T_RMC: {
        uint8_t ok = (g->n.status == 'A') && (g->n.mode != 'N');
        if (h & H_TIME) set_time(d, g->n.time_raw);
        if (h & H_DATE) set_date(d, g->n.date_raw);
        if (ok && (h & H_SPEED))  { d->speed = knots_to_mms(g->n.speed_raw); d->updated |= NEO_UPD_SPEED; }
        if (ok && (h & H_COURSE)) { d->course = g->n.course; d->updated |= NEO_UPD_COURSE; }
        if (d->valid != ok) { d->valid = ok; d->updated |= NEO_UPD_FIX; }
        if (!ok && d->fix != NEO_FIX_NONE) { d->fix = NEO_FIX_NONE; d->updated |= NEO_UPD_FIX; }
        if (ok && (h & H_LAT) && (h & H_LON))
            pos_commit(g, g->n.lat, g->n.lon, nmea_ms_of_day(g), 86400000UL);
        return NEO_EV_NMEA_RMC;
    }
    case T_GGA: {
        uint8_t q = (h & H_QUAL) ? g->n.quality : 0;
        if (h & H_TIME) set_time(d, g->n.time_raw);
        d->quality = q;
        if (h & H_NUMSV) { d->num_sv = g->n.num_sv; d->updated |= NEO_UPD_SATS; }
        if (h & H_HDOP)  { d->hdop = g->n.hdop; d->updated |= NEO_UPD_DOP; }
        if (d->valid != (q != 0)) { d->valid = (q != 0); d->updated |= NEO_UPD_FIX; }
        if (q == 6) d->fix = NEO_FIX_DR;
        else if (!q) d->fix = NEO_FIX_NONE;
        {
            uint8_t acc = 1;
            if (q && (h & H_LAT) && (h & H_LON))
                acc = pos_commit(g, g->n.lat, g->n.lon, nmea_ms_of_day(g), 86400000UL);
            if (q && acc && (h & H_ALT)) {
                d->alt_msl = g->n.alt;
                if (h & H_SEP) d->geoid_sep = g->n.sep;
                d->updated |= NEO_UPD_ALTITUDE;
            }
        }
        return NEO_EV_NMEA_GGA;
    }
    case T_GSA:
        if (h & H_NAVMODE) {
            uint8_t m = g->n.nav_mode;
            uint8_t fx = (m == 2) ? NEO_FIX_2D : (m == 3) ? NEO_FIX_3D : NEO_FIX_NONE;
            if (d->fix != fx && !(d->fix == NEO_FIX_DR && fx == NEO_FIX_NONE)) {
                d->fix = fx; d->updated |= NEO_UPD_FIX;
            }
        }
        if (h & H_PDOP) d->pdop = g->n.pdop;
        if (h & H_HDOP) d->hdop = g->n.hdop;
        if (h & H_VDOP) d->vdop = g->n.vdop;
        if (h & (H_PDOP | H_HDOP | H_VDOP)) d->updated |= NEO_UPD_DOP;
        return NEO_EV_NMEA_GSA;

    case T_VTG:
        if (g->n.mode != 'N') {
            if (h & H_SPEED)  { d->speed = knots_to_mms(g->n.speed_raw); d->updated |= NEO_UPD_SPEED; }
            if (h & H_COURSE) { d->course = g->n.course; d->updated |= NEO_UPD_COURSE; }
        }
        return NEO_EV_NMEA_VTG;

    case T_GSV:
        if ((h & H_INVIEW) && g->n_talker < 4) {
            g->gsv_view[g->n_talker] = g->n.in_view;
            d->sats_in_view = (uint8_t)(g->gsv_view[0] + g->gsv_view[1] +
                                        g->gsv_view[2] + g->gsv_view[3]);
            d->updated |= NEO_UPD_SATS;
        }
        return NEO_EV_NMEA_GSV;

    default:
        return NEO_EV_NONE;
    }
}

static void nmea_start(neo_gps_t *g)
{
    g->state  = ST_NMEA;
    g->n_chk  = 0;
    g->n_field = 0;
    g->n_flen = 0;
    g->n_type = T_NONE;
    g->n_len  = 0;
    memset(&g->n, 0, sizeof(g->n));
}

#endif /* NEO_GPS_ENABLE_NMEA */

/* ========================================================================= */
/* UBX                                                                       */
/* ========================================================================= */
#if NEO_GPS_ENABLE_UBX

#define UBX_SYNC1 0xB5u
#define UBX_SYNC2 0x62u

#define CLS_NAV 0x01u
#define CLS_ACK 0x05u
#define CLS_CFG 0x06u
#define CLS_MON 0x0Au

/* Little-Endian lesen, alignment-sicher (auch auf Cortex-M0 / AVR) */
static uint16_t rd_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t rd_i32(const uint8_t *p) { return (int32_t)rd_u32(p); }

static void ubx_set_datetime(neo_gps_data_t *d, const uint8_t *p, int32_t nano, uint8_t date_ok, uint8_t time_ok)
{
    /* p zeigt auf year(U2) month day hour min sec */
    if (date_ok) {
        d->time.year  = rd_u16(p);
        d->time.month = p[2];
        d->time.day   = p[3];
        d->date_valid = 1;
        d->updated   |= NEO_UPD_DATE;
    }
    if (time_ok) {
        int32_t ms = (int32_t)((nano + 500000L) / 1000000L);   /* auf ms runden */
        if (ms < 0) ms = 0;
        if (ms > 999) ms = 999;
        d->time.hour   = p[4];
        d->time.minute = p[5];
        d->time.second = p[6];
        d->time.ms     = (uint16_t)ms;
        d->time_valid  = 1;
        d->updated    |= NEO_UPD_TIME;
    }
}

static void set_fix(neo_gps_data_t *d, uint8_t fix, uint8_t ok)
{
    if (d->fix != fix || d->valid != ok) d->updated |= NEO_UPD_FIX;
    d->fix = fix;
    d->valid = ok;
}

static neo_event_t ubx_dispatch(neo_gps_t *g)
{
    neo_gps_data_t *d = &g->data;
    const uint8_t *p = g->u_buf;
    uint16_t len = g->u_len;
    uint8_t cls = g->u_cls, id = g->u_id;

    if (g->ubx_handler)
        g->ubx_handler(g->user, cls, id, p, len < NEO_GPS_UBX_MAX_PAYLOAD ? len : NEO_GPS_UBX_MAX_PAYLOAD);

    if (cls == CLS_NAV) {
        switch (id) {
        case 0x07: /* NAV-PVT: 84 Byte (u-blox 7) / 92 Byte (M8) */
            if (len >= 84) {
                uint8_t ok = (p[21] & 0x01u) != 0;
                d->itow = rd_u32(p);
                ubx_set_datetime(d, p + 4, rd_i32(p + 16), (p[11] & 0x01u) != 0, (p[11] & 0x02u) != 0);
                set_fix(d, p[20], ok);
                d->num_sv = p[23];
                d->h_acc = rd_u32(p + 40);
                d->v_acc = rd_u32(p + 44);
                d->s_acc = rd_u32(p + 68);
                d->pdop  = rd_u16(p + 76);
                d->updated |= NEO_UPD_SATS | NEO_UPD_ACCURACY | NEO_UPD_DOP;
                if (ok) {
                    d->vel_n     = rd_i32(p + 48);
                    d->vel_e     = rd_i32(p + 52);
                    d->vel_d     = rd_i32(p + 56);
                    d->speed     = (uint32_t)rd_i32(p + 60);
                    d->course    = rd_i32(p + 64);
                    d->updated |= NEO_UPD_SPEED | NEO_UPD_COURSE | NEO_UPD_VELNED;
                    if (pos_commit(g, rd_i32(p + 28), rd_i32(p + 24), d->itow, 604800000UL)) {
                        d->alt_msl   = rd_i32(p + 36);
                        d->geoid_sep = rd_i32(p + 32) - d->alt_msl;
                        d->updated |= NEO_UPD_ALTITUDE;
                    }
                }
                return NEO_EV_UBX_NAV_PVT;
            }
            break;

        case 0x02: /* NAV-POSLLH (NEO-6) */
            if (len >= 28) {
                d->itow      = rd_u32(p);
                d->h_acc     = rd_u32(p + 20);
                d->v_acc     = rd_u32(p + 24);
                d->updated |= NEO_UPD_ACCURACY;
                if (pos_commit(g, rd_i32(p + 8), rd_i32(p + 4), d->itow, 604800000UL)) {
                    d->alt_msl   = rd_i32(p + 16);
                    d->geoid_sep = rd_i32(p + 12) - d->alt_msl;
                    d->updated |= NEO_UPD_ALTITUDE;
                }
                return NEO_EV_UBX_NAV_POSLLH;
            }
            break;

        case 0x12: /* NAV-VELNED (NEO-6), Einheiten cm/s */
            if (len >= 36) {
                d->itow   = rd_u32(p);
                d->vel_n  = rd_i32(p + 4) * 10;
                d->vel_e  = rd_i32(p + 8) * 10;
                d->vel_d  = rd_i32(p + 12) * 10;
                d->speed  = rd_u32(p + 20) * 10u;
                d->course = rd_i32(p + 24);
                d->s_acc  = rd_u32(p + 28) * 10u;
                d->updated |= NEO_UPD_SPEED | NEO_UPD_COURSE | NEO_UPD_VELNED;
                return NEO_EV_UBX_NAV_VELNED;
            }
            break;

        case 0x21: /* NAV-TIMEUTC */
            if (len >= 20) {
                uint8_t utc = (p[19] & 0x04u) != 0;
                d->itow = rd_u32(p);
                ubx_set_datetime(d, p + 12, rd_i32(p + 8), utc, utc);
                return NEO_EV_UBX_NAV_TIMEUTC;
            }
            break;

        case 0x06: /* NAV-SOL (NEO-6) */
            if (len >= 52) {
                d->itow = rd_u32(p);
                set_fix(d, p[10], (p[11] & 0x01u) != 0);
                d->pdop   = rd_u16(p + 44);
                d->num_sv = p[47];
                d->updated |= NEO_UPD_SATS | NEO_UPD_DOP;
                return NEO_EV_UBX_NAV_SOL;
            }
            break;

        case 0x04: /* NAV-DOP */
            if (len >= 18) {
                d->itow = rd_u32(p);
                d->pdop = rd_u16(p + 6);
                d->vdop = rd_u16(p + 10);
                d->hdop = rd_u16(p + 12);
                d->updated |= NEO_UPD_DOP;
                return NEO_EV_UBX_NAV_DOP;
            }
            break;

        default: break;
        }
    } else if (cls == CLS_ACK && len >= 2) {
        if (g->ack_state == NEO_ACK_PENDING && p[0] == g->ack_cls && p[1] == g->ack_id)
            g->ack_state = (id == 0x01) ? NEO_ACK_OK : NEO_ACK_NAK;
        return (id == 0x01) ? NEO_EV_UBX_ACK : NEO_EV_UBX_NAK;
    } else if (cls == CLS_MON && id == 0x04 && len >= 40) {
        /* MON-VER: hwVersion (10 Zeichen) ab Offset 30, z.B. "00040007" */
        const uint8_t *hw = p + 30;
        if (hw[0] == '0' && hw[1] == '0' && hw[2] == '0') {
            if (hw[3] == '4')      g->model = NEO_MODEL_NEO6;
            else if (hw[3] == '7') g->model = NEO_MODEL_NEO7;
            else if (hw[3] == '8') g->model = NEO_MODEL_M8;
        }
        return NEO_EV_UBX_MON_VER;
    }
    return NEO_EV_UBX_OTHER;
}

#endif /* NEO_GPS_ENABLE_UBX */

/* ========================================================================= */
/* Byte-Eingang                                                              */
/* ========================================================================= */

static neo_event_t start_frame(neo_gps_t *g, uint8_t c)
{
#if NEO_GPS_ENABLE_NMEA
    if (c == '$') { nmea_start(g); return NEO_EV_NONE; }
#endif
#if NEO_GPS_ENABLE_UBX
    if (c == UBX_SYNC1) { g->state = ST_UBX_SYNC2; return NEO_EV_NONE; }
#endif
    g->state = ST_IDLE;
    return NEO_EV_NONE;
}

static neo_event_t fail(neo_gps_t *g)
{
    g->msgs_err++;
    g->state = ST_IDLE;
    return NEO_EV_CHECKSUM_ERROR;
}

neo_event_t neo_gps_feed(neo_gps_t *g, uint8_t c)
{
    switch (g->state) {
#if NEO_GPS_ENABLE_NMEA
    case ST_NMEA:
        if (c == '*') {
            if (!nmea_field(g)) { g->state = ST_IDLE; return NEO_EV_NONE; }
            g->state = ST_NMEA_CK1;
            return NEO_EV_NONE;
        }
        if (c < 0x20 || c > 0x7E || c == '$' || ++g->n_len > NMEA_MAX_LEN)
            return start_frame(g, c);           /* kaputter Satz -> neu synchronisieren */
        g->n_chk ^= c;
        if (c == ',') {
            g->n_buf[g->n_flen <= NEO_GPS_NMEA_FIELD_MAX ? g->n_flen : NEO_GPS_NMEA_FIELD_MAX] = '\0';
            if (!nmea_field(g)) { g->state = ST_IDLE; return NEO_EV_NONE; }  /* uninteressant */
            g->n_field++;
            g->n_flen = 0;
        } else {
            if (g->n_flen < NEO_GPS_NMEA_FIELD_MAX) g->n_buf[g->n_flen] = (char)c;
            if (g->n_flen <= NEO_GPS_NMEA_FIELD_MAX) g->n_flen++;   /* > MAX = Ueberlauf */
        }
        return NEO_EV_NONE;

    case ST_NMEA_CK1: {
        uint8_t v = hexval(c);
        if (v == 0xFF) return fail(g);
        g->n_rx_chk = (uint8_t)(v << 4);
        g->state = ST_NMEA_CK2;
        return NEO_EV_NONE;
    }
    case ST_NMEA_CK2: {
        uint8_t v = hexval(c);
        g->state = ST_IDLE;
        if (v == 0xFF || (uint8_t)(g->n_rx_chk | v) != g->n_chk) return fail(g);
        g->msgs_ok++;
        return nmea_commit(g);
    }
#endif

#if NEO_GPS_ENABLE_UBX
    case ST_UBX_SYNC2:
        if (c == UBX_SYNC2) { g->state = ST_UBX_CLASS; return NEO_EV_NONE; }
        return start_frame(g, c);

    case ST_UBX_CLASS:
        g->u_cls = c; g->u_cka = c; g->u_ckb = c;
        g->state = ST_UBX_ID;
        return NEO_EV_NONE;

    case ST_UBX_ID:
        g->u_id = c; g->u_cka = (uint8_t)(g->u_cka + c); g->u_ckb = (uint8_t)(g->u_ckb + g->u_cka);
        g->state = ST_UBX_LEN1;
        return NEO_EV_NONE;

    case ST_UBX_LEN1:
        g->u_len = c; g->u_cka = (uint8_t)(g->u_cka + c); g->u_ckb = (uint8_t)(g->u_ckb + g->u_cka);
        g->state = ST_UBX_LEN2;
        return NEO_EV_NONE;

    case ST_UBX_LEN2:
        g->u_len |= (uint16_t)((uint16_t)c << 8); g->u_cka = (uint8_t)(g->u_cka + c); g->u_ckb = (uint8_t)(g->u_ckb + g->u_cka);
        if (g->u_len > NEO_GPS_UBX_LEN_LIMIT) return fail(g);
        g->u_idx = 0;
        g->state = g->u_len ? ST_UBX_PAYLOAD : ST_UBX_CKA;
        return NEO_EV_NONE;

    case ST_UBX_PAYLOAD:
        if (g->u_idx < NEO_GPS_UBX_MAX_PAYLOAD) g->u_buf[g->u_idx] = c;
        g->u_cka = (uint8_t)(g->u_cka + c); g->u_ckb = (uint8_t)(g->u_ckb + g->u_cka);
        if (++g->u_idx >= g->u_len) g->state = ST_UBX_CKA;
        return NEO_EV_NONE;

    case ST_UBX_CKA:
        if (c != g->u_cka) return fail(g);
        g->state = ST_UBX_CKB;
        return NEO_EV_NONE;

    case ST_UBX_CKB:
        g->state = ST_IDLE;
        if (c != g->u_ckb) return fail(g);
        g->msgs_ok++;
        return ubx_dispatch(g);
#endif

    default:
        return start_frame(g, c);
    }
}

uint16_t neo_gps_feed_buf(neo_gps_t *gps, const uint8_t *buf, uint16_t len)
{
    uint16_t n = 0;
    while (len--) {
        neo_event_t e = neo_gps_feed(gps, *buf++);
        if (e != NEO_EV_NONE && e != NEO_EV_CHECKSUM_ERROR) n++;
    }
    return n;
}

/* ========================================================================= */
/* UBX senden / konfigurieren                                                */
/* ========================================================================= */
#if NEO_GPS_ENABLE_UBX

#define TX_INLINE_MAX 40u

static void wr_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr_u32(uint8_t *p, uint32_t v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

void neo_gps_ubx_send(neo_gps_t *g, uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len)
{
    uint8_t f[6 + TX_INLINE_MAX + 2];
    uint8_t a = 0, b = 0;
    uint16_t i;

    if (!g->write) return;

    f[0] = UBX_SYNC1; f[1] = UBX_SYNC2; f[2] = cls; f[3] = id;
    wr_u16(&f[4], len);
    for (i = 2; i < 6; i++) { a = (uint8_t)(a + f[i]); b = (uint8_t)(b + a); }
    for (i = 0; i < len; i++) { a = (uint8_t)(a + payload[i]); b = (uint8_t)(b + a); }

    if (cls == CLS_CFG) { g->ack_cls = cls; g->ack_id = id; g->ack_state = NEO_ACK_PENDING; }

    if (len <= TX_INLINE_MAX) {           /* ein einziger write()-Aufruf - DMA-freundlich */
        if (len) memcpy(&f[6], payload, len);
        f[6 + len] = a; f[7 + len] = b;
        g->write(g->user, f, (uint16_t)(len + 8u));
    } else {
        uint8_t ck[2];
        ck[0] = a; ck[1] = b;
        g->write(g->user, f, 6);
        g->write(g->user, payload, len);
        g->write(g->user, ck, 2);
    }
}

neo_ack_t neo_gps_ack_status(const neo_gps_t *g) { return (neo_ack_t)g->ack_state; }

void neo_gps_detect_model(neo_gps_t *g) { neo_gps_ubx_poll(g, CLS_MON, 0x04); }

void neo_gps_cfg_port(neo_gps_t *g, uint32_t baud, uint8_t out_proto)
{
    uint8_t p[20];
    memset(p, 0, sizeof(p));
    p[0] = 1;                              /* UART1 */
    wr_u32(&p[4], 0x000008D0UL);           /* 8N1 */
    wr_u32(&p[8], baud);
    wr_u16(&p[12], NEO_PROTO_UBX | NEO_PROTO_NMEA);   /* Eingang: immer beides */
    wr_u16(&p[14], out_proto);
    neo_gps_ubx_send(g, CLS_CFG, 0x00, p, sizeof(p));
}

void neo_gps_cfg_rate(neo_gps_t *g, uint16_t meas_ms)
{
    uint8_t p[6];
    wr_u16(&p[0], meas_ms);
    wr_u16(&p[2], 1);                      /* navRate: immer 1 */
    wr_u16(&p[4], 1);                      /* timeRef: GPS-Zeit */
    neo_gps_ubx_send(g, CLS_CFG, 0x08, p, sizeof(p));
}

void neo_gps_cfg_msg(neo_gps_t *g, uint8_t cls, uint8_t id, uint8_t rate)
{
    uint8_t p[3];
    p[0] = cls; p[1] = id; p[2] = rate;    /* Kurzform: gilt fuer den aktuellen Port */
    neo_gps_ubx_send(g, CLS_CFG, 0x01, p, sizeof(p));
}

void neo_gps_cfg_output(neo_gps_t *g, neo_output_t out)
{
    /* NMEA-Standardsaetze: Klasse 0xF0 - GGA=0 GLL=1 GSA=2 GSV=3 RMC=4 VTG=5 */
    static const uint8_t nmea_ids[6] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
    static const uint8_t def_on[6]   = { 1, 1, 1, 1, 1, 1 };
    static const uint8_t min_on[6]   = { 1, 0, 1, 0, 1, 0 };
    static const uint8_t ubx_on[6]   = { 0, 0, 0, 0, 0, 0 };
    const uint8_t *on = (out == NEO_OUT_UBX) ? ubx_on : (out == NEO_OUT_NMEA_MINIMAL) ? min_on : def_on;
    uint8_t i, pvt = (out == NEO_OUT_UBX && g->model != NEO_MODEL_NEO6);
    uint8_t legacy = (out == NEO_OUT_UBX && g->model == NEO_MODEL_NEO6);

    /* Erst UBX einschalten, dann NMEA aus - so gibt es nie eine Luecke */
    neo_gps_cfg_msg(g, CLS_NAV, 0x07, pvt);             /* NAV-PVT     */
    neo_gps_cfg_msg(g, CLS_NAV, 0x02, legacy);          /* NAV-POSLLH  */
    neo_gps_cfg_msg(g, CLS_NAV, 0x12, legacy);          /* NAV-VELNED  */
    neo_gps_cfg_msg(g, CLS_NAV, 0x21, legacy);          /* NAV-TIMEUTC */
    neo_gps_cfg_msg(g, CLS_NAV, 0x06, legacy);          /* NAV-SOL     */
    neo_gps_cfg_msg(g, CLS_NAV, 0x04, (uint8_t)(out == NEO_OUT_UBX)); /* NAV-DOP */
    for (i = 0; i < 6; i++)
        neo_gps_cfg_msg(g, 0xF0, nmea_ids[i], on[i]);
}

void neo_gps_cfg_dynmodel(neo_gps_t *g, neo_dyn_model_t model)
{
    uint8_t p[36];
    memset(p, 0, sizeof(p));
    wr_u16(&p[0], 0x0001);                 /* Maske: nur dynModel aendern */
    p[2] = (uint8_t)model;
    neo_gps_ubx_send(g, CLS_CFG, 0x24, p, sizeof(p));
}

void neo_gps_cfg_power_save(neo_gps_t *g, uint8_t enable)
{
    uint8_t p[2];
    p[0] = 8;                              /* reserved, laut Doku immer 8 */
    p[1] = enable ? 1 : 0;
    neo_gps_ubx_send(g, CLS_CFG, 0x11, p, sizeof(p));
}

static void cfg_cfg(neo_gps_t *g, uint32_t clear, uint32_t save, uint32_t load)
{
    uint8_t p[13];
    wr_u32(&p[0], clear);
    wr_u32(&p[4], save);
    wr_u32(&p[8], load);
    p[12] = 0x17;                          /* BBR + Flash + EEPROM + SPI-Flash */
    neo_gps_ubx_send(g, CLS_CFG, 0x09, p, sizeof(p));
}

void neo_gps_cfg_save(neo_gps_t *g)          { cfg_cfg(g, 0, 0x0000FFFFUL, 0); }
void neo_gps_cfg_factory_reset(neo_gps_t *g) { cfg_cfg(g, 0x0000FFFFUL, 0, 0x0000FFFFUL); }

void neo_gps_reset(neo_gps_t *g, neo_start_t type)
{
    uint8_t p[4];
    uint16_t bbr = (type == NEO_START_COLD) ? 0xFFFFu : (type == NEO_START_WARM) ? 0x0001u : 0x0000u;
    wr_u16(&p[0], bbr);
    p[2] = 0x02;                           /* kontrollierter Software-Reset (nur GNSS) */
    p[3] = 0;
    neo_gps_ubx_send(g, CLS_CFG, 0x04, p, sizeof(p));
    g->ack_state = NEO_ACK_NONE;           /* CFG-RST wird nicht bestaetigt */
}

#endif /* NEO_GPS_ENABLE_UBX */
