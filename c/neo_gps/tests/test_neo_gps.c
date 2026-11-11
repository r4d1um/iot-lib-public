/* Host-Tests fuer neo_gps:  make test */
#include "neo_gps.h"
#include "neo_gps_util.h"
#include "neo_gps_time.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { long long _a = (long long)(a), _b = (long long)(b); checks++; \
    if (_a != _b) { fails++; printf("FAIL %s:%d: %s == %lld, erwartet %lld\n", __FILE__, __LINE__, #a, _a, _b); } } while (0)

/* --- Hilfen ------------------------------------------------------------- */

/* Body ohne $ und *CS -> kompletter Satz mit korrekter Pruefsumme */
static neo_event_t feed_nmea(neo_gps_t *g, const char *body)
{
    char s[128];
    unsigned cs = 0;
    const char *p;
    neo_event_t ev = NEO_EV_NONE, e;
    size_t i;
    for (p = body; *p; p++) cs ^= (unsigned char)*p;
    snprintf(s, sizeof(s), "$%s*%02X\r\n", body, cs);
    for (i = 0; s[i]; i++) if ((e = neo_gps_feed(g, (uint8_t)s[i])) != NEO_EV_NONE) ev = e;
    return ev;
}

static neo_event_t feed_str(neo_gps_t *g, const char *s)
{
    neo_event_t ev = NEO_EV_NONE, e;
    for (; *s; s++) if ((e = neo_gps_feed(g, (uint8_t)*s)) != NEO_EV_NONE) ev = e;
    return ev;
}

static size_t make_ubx(uint8_t *out, uint8_t cls, uint8_t id, const uint8_t *pl, uint16_t len)
{
    uint8_t a = 0, b = 0;
    size_t i;
    out[0] = 0xB5; out[1] = 0x62; out[2] = cls; out[3] = id;
    out[4] = (uint8_t)len; out[5] = (uint8_t)(len >> 8);
    memcpy(out + 6, pl, len);
    for (i = 2; i < 6u + len; i++) { a += out[i]; b += a; }
    out[6 + len] = a; out[7 + len] = b;
    return 8u + len;
}

static neo_event_t feed_ubx(neo_gps_t *g, uint8_t cls, uint8_t id, const uint8_t *pl, uint16_t len)
{
    static uint8_t f[2048];
    size_t n = make_ubx(f, cls, id, pl, len), i;
    neo_event_t ev = NEO_EV_NONE, e;
    for (i = 0; i < n; i++) if ((e = neo_gps_feed(g, f[i])) != NEO_EV_NONE) ev = e;
    return ev;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static uint8_t tx[4096];
static size_t tx_len;
static void tx_cb(void *u, const uint8_t *d, uint16_t n) { (void)u; memcpy(tx + tx_len, d, n); tx_len += n; }

/* --- Tests -------------------------------------------------------------- */

static void test_rmc(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    /* Beispiel aus dem u-blox Protokollhandbuch */
    CHECK_EQ(feed_str(&g, "$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*57\r\n"), NEO_EV_NMEA_RMC);
    CHECK_EQ(g.data.lat, 472852395);
    CHECK_EQ(g.data.lon, 85652537);
    CHECK_EQ(g.data.speed, 2);
    CHECK_EQ(g.data.course, 7752000);
    CHECK_EQ(g.data.time.hour, 8);
    CHECK_EQ(g.data.time.minute, 35);
    CHECK_EQ(g.data.time.second, 59);
    CHECK_EQ(g.data.time.ms, 0);
    CHECK_EQ(g.data.time.day, 9);
    CHECK_EQ(g.data.time.month, 12);
    CHECK_EQ(g.data.time.year, 2002);
    CHECK(g.data.valid);
    {
        uint16_t u = neo_gps_updates(&g);
        CHECK(u & NEO_UPD_POSITION);
        CHECK(u & NEO_UPD_DATE);
        CHECK_EQ(neo_gps_updates(&g), 0);
    }
    /* 10 kn -> 5144 mm/s,  Sued/West */
    CHECK_EQ(feed_nmea(&g, "GNRMC,235959.40,A,3352.12800,S,15112.55800,W,10.000,359.99,311226,,,D"), NEO_EV_NMEA_RMC);
    CHECK_EQ(g.data.lat, -338688000);
    CHECK_EQ(g.data.lon, -1512093000);
    CHECK_EQ(g.data.speed, 5144);
    CHECK_EQ(g.data.course, 35999000);
    CHECK_EQ(g.data.time.ms, 400);
    CHECK_EQ(g.data.time.year, 2026);
    /* Kein Fix: Position bleibt erhalten, valid = 0 */
    CHECK_EQ(feed_nmea(&g, "GPRMC,120000.00,V,,,,,,,301026,,,N"), NEO_EV_NMEA_RMC);
    CHECK(!g.data.valid);
    CHECK_EQ(g.data.fix, NEO_FIX_NONE);
    CHECK_EQ(g.data.lat, -338688000);
    CHECK_EQ(g.data.time.hour, 12);
}

static void test_gga_gsa_gsv_vtg(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    CHECK_EQ(feed_str(&g, "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B\r\n"), NEO_EV_NMEA_GGA);
    CHECK_EQ(g.data.lat, 472852332);
    CHECK_EQ(g.data.lon, 85652650);
    CHECK_EQ(g.data.alt_msl, 499600);
    CHECK_EQ(g.data.geoid_sep, 48000);
    CHECK_EQ(g.data.num_sv, 8);
    CHECK_EQ(g.data.hdop, 101);
    CHECK_EQ(g.data.quality, 1);
    CHECK(g.data.valid);

    CHECK_EQ(feed_nmea(&g, "GNGGA,092725.00,4717.11399,N,00833.91590,E,1,12,0.7,-12.3,M,48.0,M,,"), NEO_EV_NMEA_GGA);
    CHECK_EQ(g.data.alt_msl, -12300);
    CHECK_EQ(g.data.hdop, 70);

    CHECK_EQ(feed_nmea(&g, "GPGSA,A,3,23,29,07,08,09,18,26,28,,,,,1.94,1.18,1.54"), NEO_EV_NMEA_GSA);
    CHECK_EQ(g.data.fix, NEO_FIX_3D);
    CHECK_EQ(g.data.pdop, 194);
    CHECK_EQ(g.data.hdop, 118);
    CHECK_EQ(g.data.vdop, 154);

    CHECK_EQ(feed_nmea(&g, "GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00"), NEO_EV_NMEA_GSV);
    CHECK_EQ(feed_nmea(&g, "GLGSV,2,1,07,65,10,047,,66,49,016,19,67,48,288,22,74,22,203,"), NEO_EV_NMEA_GSV);
    CHECK_EQ(g.data.sats_in_view, 18);

    CHECK_EQ(feed_nmea(&g, "GPVTG,77.52,T,,M,0.004,N,0.008,K,A"), NEO_EV_NMEA_VTG);
    CHECK_EQ(g.data.course, 7752000);
    CHECK_EQ(g.data.speed, 2);

    /* GGA ohne Fix */
    CHECK_EQ(feed_nmea(&g, "GPGGA,092726.00,,,,,0,00,99.99,,,,,,"), NEO_EV_NMEA_GGA);
    CHECK(!g.data.valid);
    CHECK_EQ(g.data.fix, NEO_FIX_NONE);
}

static void test_errors_and_resync(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    /* falsche Pruefsumme -> keine Uebernahme */
    CHECK_EQ(feed_str(&g, "$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*58\r\n"), NEO_EV_CHECKSUM_ERROR);
    CHECK_EQ(g.data.lat, 0);
    CHECK_EQ(g.msgs_err, 1);
    /* abgebrochener Satz + Muell + gueltiger Satz */
    CHECK_EQ(feed_str(&g, "$GPRMC,0835\xff\x01garbage$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*57\r\n"),
             NEO_EV_NMEA_RMC);
    CHECK_EQ(g.data.lat, 472852395);
    /* unbekannte Saetze werden still ignoriert */
    CHECK_EQ(feed_nmea(&g, "GPTXT,01,01,02,u-blox ag - www.u-blox.com"), NEO_EV_NONE);
    CHECK_EQ(feed_nmea(&g, "PUBX,00,081350.00,4717.113210,N"), NEO_EV_NONE);
    /* ueberlanges Feld wird ignoriert, Rest des Satzes zaehlt */
    CHECK_EQ(feed_nmea(&g, "GPGGA,092725.00,4717.1139999999999999,N,00833.91590,E,1,05,1.01,10.0,M,48.0,M,,"), NEO_EV_NMEA_GGA);
    CHECK_EQ(g.data.num_sv, 5);
    CHECK_EQ(g.data.lat, 472852395); /* unveraendert, Feld war zu lang */
}

static void test_ubx_pvt(void)
{
    neo_gps_t g;
    uint8_t p[92];
    neo_gps_init(&g, NULL, NULL);
    memset(p, 0, sizeof(p));
    put32(p + 0, 123456000);       /* iTOW */
    put16(p + 4, 2026); p[6] = 10; p[7] = 30; p[8] = 14; p[9] = 5; p[10] = 59;
    p[11] = 0x07;                  /* validDate|validTime|fullyResolved */
    put32(p + 16, (uint32_t)199999500L);   /* nano -> 200 ms */
    p[20] = 3; p[21] = 0x01; p[23] = 14;
    put32(p + 24, (uint32_t)-1234567890L); /* lon */
    put32(p + 28, 512345678);      /* lat */
    put32(p + 32, 150000);         /* height ellipsoid */
    put32(p + 36, 102000);         /* hMSL */
    put32(p + 40, 1500); put32(p + 44, 2500);
    put32(p + 48, (uint32_t)-100); put32(p + 52, 200); put32(p + 56, (uint32_t)-5);
    put32(p + 60, 13888);          /* 50 km/h */
    put32(p + 64, 27012345);
    put32(p + 68, 300);
    put16(p + 76, 132);
    CHECK_EQ(feed_ubx(&g, 0x01, 0x07, p, 92), NEO_EV_UBX_NAV_PVT);
    CHECK_EQ(g.data.lat, 512345678);
    CHECK_EQ(g.data.lon, -1234567890);
    CHECK_EQ(g.data.alt_msl, 102000);
    CHECK_EQ(g.data.geoid_sep, 48000);
    CHECK_EQ(g.data.speed, 13888);
    CHECK_EQ(g.data.course, 27012345);
    CHECK_EQ(g.data.vel_n, -100);
    CHECK_EQ(g.data.h_acc, 1500);
    CHECK_EQ(g.data.num_sv, 14);
    CHECK_EQ(g.data.fix, NEO_FIX_3D);
    CHECK_EQ(g.data.pdop, 132);
    CHECK_EQ(g.data.time.year, 2026);
    CHECK_EQ(g.data.time.second, 59);
    CHECK_EQ(g.data.time.ms, 200);
    CHECK(g.data.valid);

    /* u-blox 7: NAV-PVT nur 84 Byte */
    p[23] = 9;
    CHECK_EQ(feed_ubx(&g, 0x01, 0x07, p, 84), NEO_EV_UBX_NAV_PVT);
    CHECK_EQ(g.data.num_sv, 9);

    /* gnssFixOK = 0: Position bleibt, valid = 0 */
    p[21] = 0; put32(p + 28, 1);
    CHECK_EQ(feed_ubx(&g, 0x01, 0x07, p, 92), NEO_EV_UBX_NAV_PVT);
    CHECK(!g.data.valid);
    CHECK_EQ(g.data.lat, 512345678);

    /* kaputte UBX-Pruefsumme */
    {
        uint8_t f[128];
        size_t n = make_ubx(f, 0x01, 0x07, p, 92), i;
        neo_event_t ev = NEO_EV_NONE, e;
        f[n - 1] ^= 0x55;
        for (i = 0; i < n; i++) if ((e = neo_gps_feed(&g, f[i])) != NEO_EV_NONE) ev = e;
        CHECK_EQ(ev, NEO_EV_CHECKSUM_ERROR);
    }
}

static void test_ubx_neo6(void)
{
    neo_gps_t g;
    uint8_t p[52];
    neo_gps_init(&g, NULL, NULL);

    memset(p, 0, sizeof(p));
    put32(p + 4, 85652537); put32(p + 8, 472852395); put32(p + 12, 547600); put32(p + 16, 499600);
    put32(p + 20, 2100); put32(p + 24, 3300);
    CHECK_EQ(feed_ubx(&g, 0x01, 0x02, p, 28), NEO_EV_UBX_NAV_POSLLH);
    CHECK_EQ(g.data.lat, 472852395);
    CHECK_EQ(g.data.geoid_sep, 48000);
    CHECK_EQ(g.data.v_acc, 3300);

    memset(p, 0, sizeof(p));
    put32(p + 4, 100); put32(p + 20, 1389); put32(p + 24, 9000000); put32(p + 28, 50);
    CHECK_EQ(feed_ubx(&g, 0x01, 0x12, p, 36), NEO_EV_UBX_NAV_VELNED);
    CHECK_EQ(g.data.speed, 13890);
    CHECK_EQ(g.data.vel_n, 1000);
    CHECK_EQ(g.data.course, 9000000);

    memset(p, 0, sizeof(p));
    put32(p + 8, (uint32_t)-300000L);
    put16(p + 12, 2026); p[14] = 1; p[15] = 2; p[16] = 3; p[17] = 4; p[18] = 5; p[19] = 0x07;
    CHECK_EQ(feed_ubx(&g, 0x01, 0x21, p, 20), NEO_EV_UBX_NAV_TIMEUTC);
    CHECK_EQ(g.data.time.month, 1);
    CHECK_EQ(g.data.time.second, 5);
    CHECK_EQ(g.data.time.ms, 0);
    CHECK(g.data.date_valid);

    memset(p, 0, sizeof(p));
    p[10] = 2; p[11] = 0x0D; put16(p + 44, 250); p[47] = 6;
    CHECK_EQ(feed_ubx(&g, 0x01, 0x06, p, 52), NEO_EV_UBX_NAV_SOL);
    CHECK_EQ(g.data.fix, NEO_FIX_2D);
    CHECK(g.data.valid);
    CHECK_EQ(g.data.num_sv, 6);

    memset(p, 0, sizeof(p));
    put16(p + 6, 180); put16(p + 10, 150); put16(p + 12, 95);
    CHECK_EQ(feed_ubx(&g, 0x01, 0x04, p, 18), NEO_EV_UBX_NAV_DOP);
    CHECK_EQ(g.data.hdop, 95);
}

static void test_model_detect(void)
{
    neo_gps_t g;
    uint8_t p[40 + 30 * 6];
    neo_gps_init(&g, NULL, NULL);
    memset(p, 0, sizeof(p));
    memcpy(p, "7.03 (45969)", 12); memcpy(p + 30, "00040007", 8);
    CHECK_EQ(feed_ubx(&g, 0x0A, 0x04, p, 40), NEO_EV_UBX_MON_VER);
    CHECK_EQ(neo_gps_model(&g), NEO_MODEL_NEO6);
    memcpy(p + 30, "00070000", 8);
    CHECK_EQ(feed_ubx(&g, 0x0A, 0x04, p, 40), NEO_EV_UBX_MON_VER);
    CHECK_EQ(neo_gps_model(&g), NEO_MODEL_NEO7);
    /* M8 mit Extensions -> laenger als Puffer, trotzdem erkannt */
    memcpy(p + 30, "00080000", 8);
    memcpy(p + 40, "PROTVER=18.00", 13);
    CHECK_EQ(feed_ubx(&g, 0x0A, 0x04, p, sizeof(p)), NEO_EV_UBX_MON_VER);
    CHECK_EQ(neo_gps_model(&g), NEO_MODEL_M8);
}

static void test_tx_and_ack(void)
{
    neo_gps_t g;
    uint8_t ack[2] = { 0x06, 0x08 };
    static const uint8_t rate5hz[] = { 0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A };
    static const uint8_t poll_ver[] = { 0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34 };
    neo_gps_init(&g, tx_cb, NULL);

    tx_len = 0;
    neo_gps_cfg_rate(&g, 200);
    CHECK_EQ(tx_len, sizeof(rate5hz));
    CHECK(memcmp(tx, rate5hz, sizeof(rate5hz)) == 0);
    CHECK_EQ(neo_gps_ack_status(&g), NEO_ACK_PENDING);
    CHECK_EQ(feed_ubx(&g, 0x05, 0x01, ack, 2), NEO_EV_UBX_ACK);
    CHECK_EQ(neo_gps_ack_status(&g), NEO_ACK_OK);

    neo_gps_cfg_dynmodel(&g, NEO_DYN_AUTOMOTIVE);
    ack[1] = 0x24;
    CHECK_EQ(feed_ubx(&g, 0x05, 0x00, ack, 2), NEO_EV_UBX_NAK);
    CHECK_EQ(neo_gps_ack_status(&g), NEO_ACK_NAK);

    tx_len = 0;
    neo_gps_detect_model(&g);
    CHECK(tx_len == sizeof(poll_ver) && memcmp(tx, poll_ver, sizeof(poll_ver)) == 0);

    /* alle gesendeten Frames muessen vom eigenen Parser als gueltig erkannt werden */
    tx_len = 0;
    neo_gps_set_model(&g, NEO_MODEL_M8);
    neo_gps_cfg_port(&g, 115200, NEO_PROTO_UBX);
    neo_gps_cfg_output(&g, NEO_OUT_UBX);
    neo_gps_cfg_power_save(&g, 1);
    neo_gps_cfg_save(&g);
    neo_gps_cfg_factory_reset(&g);
    neo_gps_reset(&g, NEO_START_COLD);
    {
        neo_gps_t rx;
        neo_gps_init(&rx, NULL, NULL);
        neo_gps_feed_buf(&rx, tx, (uint16_t)tx_len);
        CHECK_EQ(rx.msgs_err, 0);
        CHECK_EQ(rx.msgs_ok, 1 + 12 + 1 + 1 + 1 + 1);
    }
}

static void test_mixed_stream(void)
{
    neo_gps_t g;
    uint8_t buf[512], p[18];
    size_t n = 0;
    const char *s1 = "$GPGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*5B\r\n";
    const char *s2 = "$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*57\r\n";
    neo_gps_init(&g, NULL, NULL);
    memset(p, 0, sizeof(p));
    p[0] = '$'; p[5] = '$'; p[6] = 0xB5;   /* Steuerzeichen in Binaerdaten duerfen nicht stoeren */
    put16(p + 12, 77);
    memcpy(buf + n, s1, strlen(s1)); n += strlen(s1);
    n += make_ubx(buf + n, 0x01, 0x04, p, 18);
    buf[n++] = 0xB5;                        /* falscher Sync */
    memcpy(buf + n, s2, strlen(s2)); n += strlen(s2);
    CHECK_EQ(neo_gps_feed_buf(&g, buf, (uint16_t)n), 3);
    CHECK_EQ(g.data.hdop, 77);
    CHECK_EQ(g.data.lat, 472852395);
}

static void test_util(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    feed_str(&g, "$GPRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*57\r\n");
    CHECK_EQ(neo_gps_unix_time(&g.data), 1039422959UL);
    g.data.time.year = 2026; g.data.time.month = 10; g.data.time.day = 30;
    g.data.time.hour = 14; g.data.time.minute = 5;
    CHECK_EQ(neo_gps_unix_time(&g.data), 1793369159UL);
    /* Berlin Brandenburger Tor -> Muenchen Marienplatz ~ 504.2 km */
    {
        float d = neo_gps_distance_m(525162940, 133777030, 481373260, 115754840);
        float f = neo_gps_distance_fast_m(525162940, 133777030, 481373260, 115754840);
        float b = neo_gps_bearing_deg(525162940, 133777030, 481373260, 115754840);
        CHECK(fabsf(d - 504200.0f) < 1500.0f);
        CHECK(fabsf(f - d) / d < 0.005f);
        CHECK(b > 190.0f && b < 200.0f);
        /* Datumsgrenze */
        CHECK(neo_gps_distance_m(0, 1799999999, 0, -1799999999) < 30.0f);
    }
}

/* --- Filter ------------------------------------------------------------ */

/* NAV-PVT mit den fuer den Filter relevanten Feldern */
static neo_event_t pvt(neo_gps_t *g, uint32_t itow, int32_t lat, int32_t lon,
                       uint32_t speed_mm, uint32_t hacc_mm, uint8_t sv)
{
    uint8_t p[92];
    memset(p, 0, sizeof(p));
    put32(p, itow);
    put16(p + 4, 2026); p[6] = 10; p[7] = 30;
    p[11] = 0x03; p[20] = 3; p[21] = 0x01; p[23] = sv;
    put32(p + 24, (uint32_t)lon); put32(p + 28, (uint32_t)lat);
    put32(p + 32, 548000); put32(p + 36, 500000);
    put32(p + 40, hacc_mm); put32(p + 60, speed_mm);
    return feed_ubx(g, 0x01, 0x07, p, 92);
}

#define M_LAT 90   /* ~1 m Breite in 1e-7 Grad (1e-7 Grad = 11.132 mm) */

static void test_filter_off(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    pvt(&g, 1000, 481000000, 115000000, 0, 2000, 10);
    pvt(&g, 2000, 482000000, 115000000, 0, 2000, 10);       /* 11 km Sprung */
    CHECK_EQ(g.data.lat, 482000000);                          /* ohne Filter: durchgereicht */
    CHECK_EQ(g.data.raw_lat, 482000000);
    CHECK_EQ(g.data.rejected, 0);
}

static void test_filter_jump(void)
{
    neo_gps_t g;
    uint32_t t = 100000, i;
    int32_t lat = 481000000;
    neo_gps_init(&g, NULL, NULL);
    neo_gps_filter_preset(&g, NEO_FILTER_PEDESTRIAN);

    /* gehen mit 1.4 m/s nach Norden, 1 Hz */
    for (i = 0; i < 5; i++, t += 1000, lat += 126)
        pvt(&g, t, lat, 115000000, 1400, 3000, 10);
    CHECK_EQ(g.data.lat, lat - 126);
    neo_gps_updates(&g);

    /* Multipath-Sprung um 80 m nach Osten */
    pvt(&g, t, lat, 115000000 + 80 * 134, 1400, 3000, 10);
    {
        uint16_t u = neo_gps_updates(&g);
        CHECK(u & NEO_UPD_REJECTED);
        CHECK(!(u & NEO_UPD_POSITION));
        CHECK(!(u & NEO_UPD_ALTITUDE));
    }
    CHECK_EQ(g.data.reject_reason, NEO_REJECT_JUMP);
    CHECK_EQ(g.data.rejected, 1);
    CHECK_EQ(g.data.lat, lat - 126);                          /* alte Position bleibt */
    CHECK_EQ(g.data.raw_lon, 115000000 + 80 * 134);

    /* naechste Epoche wieder normal -> akzeptiert (2 s seit letzter guter Position) */
    t += 1000; lat += 126;
    pvt(&g, t, lat + 126, 115000000, 1400, 3000, 10);
    CHECK(neo_gps_updates(&g) & NEO_UPD_POSITION);
    CHECK_EQ(g.data.lat, lat + 126);
}

static void test_filter_reanchor(void)
{
    neo_gps_t g;
    uint32_t t = 5000, i;
    neo_gps_init(&g, NULL, NULL);
    neo_gps_filter_preset(&g, NEO_FILTER_PEDESTRIAN);
    pvt(&g, t, 481000000, 115000000, 0, 3000, 10);
    /* Modul war "falsch" verankert, echte Position 500 m weiter: nach 5 Epochen uebernommen */
    for (i = 1; i <= 5; i++) {
        pvt(&g, t + i * 1000, 481000000 + 500 * M_LAT, 115000000, 0, 3000, 10);
        if (i < 5) CHECK_EQ(g.data.lat, 481000000);
    }
    CHECK_EQ(g.data.lat, 481000000 + 500 * M_LAT);
    CHECK_EQ(g.data.rejected, 4);
}

static void test_filter_quality(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    neo_gps_filter_preset(&g, NEO_FILTER_AUTOMOTIVE);
    pvt(&g, 1000, 481000000, 115000000, 0, 3000, 10);
    pvt(&g, 2000, 481000100, 115000000, 0, 45000, 10);        /* h_acc 45 m */
    CHECK_EQ(g.data.reject_reason, NEO_REJECT_ACCURACY);
    pvt(&g, 3000, 481000100, 115000000, 0, 3000, 3);          /* 3 Satelliten */
    CHECK_EQ(g.data.reject_reason, NEO_REJECT_SATS);
    CHECK_EQ(g.data.lat, 481000000);
    CHECK_EQ(g.data.rejected, 2);
    /* Qualitaets-Ablehnungen zaehlen nicht zum Neu-Verankern; gute Daten kommen durch */
    pvt(&g, 4000, 481000100, 115000000, 1000, 3000, 10);    /* in Bewegung -> ungeglaettet */
    CHECK_EQ(g.data.lat, 481000100);
}

static void test_filter_tunnel(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    neo_gps_filter_preset(&g, NEO_FILTER_AUTOMOTIVE);
    pvt(&g, 10000, 481000000, 115000000, 15000, 3000, 10);   /* 54 km/h */
    /* 60 s keine Position, dann 900 m weiter -> plausibel */
    pvt(&g, 70000, 481000000 + 900 * M_LAT, 115000000, 15000, 3000, 10);
    CHECK_EQ(g.data.lat, 481000000 + 900 * M_LAT);
    CHECK_EQ(g.data.rejected, 0);
    /* Wochenwechsel von iTOW (604800000 -> 0) */
    pvt(&g, 604799000, 481000000 + 900 * M_LAT, 115000000, 0, 3000, 10);
    pvt(&g, 0, 481000000 + 900 * M_LAT, 115000000, 0, 3000, 10);
    CHECK_EQ(g.data.rejected, 0);
    /* ueber die Datumsgrenze im Stand */
    neo_gps_filter_reset(&g);
    pvt(&g, 1000, 0, 1799999990, 0, 2000, 10);
    pvt(&g, 2000, 0, -1799999990, 0, 2000, 10);              /* ~2 m entfernt */
    CHECK_EQ(g.data.rejected, 0);
    CHECK(g.data.lon > 1799999980 || g.data.lon < -1799999980);
}

static void test_filter_smoothing(void)
{
    neo_gps_t g;
    uint32_t i;
    int32_t maxdev = 0;
    neo_gps_init(&g, NULL, NULL);
    neo_gps_filter_preset(&g, NEO_FILTER_STATIONARY);
    /* im Stand +/- 3 m Rauschen um einen festen Punkt */
    for (i = 0; i < 60; i++) {
        int32_t noise = (i & 1) ? 3 * M_LAT : -3 * M_LAT;
        int32_t dev;
        pvt(&g, 1000 + i * 1000, 481000000 + noise, 115000000, 0, 2500, 10);
        dev = g.data.lat - 481000000;
        if (dev < 0) dev = -dev;
        if (i > 45 && dev > maxdev) maxdev = dev;   /* nach dem Einschwingen */
    }
    CHECK(maxdev < M_LAT / 2 + 1);          /* geglaettet: < 0.5 m statt 3 m */
    CHECK_EQ(g.data.rejected, 0);
    /* bei Bewegung keine Glaettung (keine Verzoegerung) */
    pvt(&g, 70000, 481000000 + 2 * M_LAT, 115000000, 2000, 2500, 10);
    CHECK_EQ(g.data.lat, 481000000 + 2 * M_LAT);
}

static void test_filter_nmea(void)
{
    neo_gps_t g;
    neo_gps_init(&g, NULL, NULL);
    neo_gps_filter_preset(&g, NEO_FILTER_PEDESTRIAN);
    feed_nmea(&g, "GNRMC,100000.00,A,4800.00000,N,01130.00000,E,2.000,0.0,301026,,,A");
    feed_nmea(&g, "GNGGA,100000.00,4800.00000,N,01130.00000,E,1,09,0.9,500.0,M,48.0,M,,");
    feed_nmea(&g, "GNGSA,A,3,01,02,03,04,05,06,07,08,09,,,,1.5,0.9,1.2");
    CHECK_EQ(g.data.lat, 480000000);
    /* 200 m Sprung in RMC + GGA derselben Epoche: einmal verworfen, Hoehe bleibt */
    feed_nmea(&g, "GNRMC,100001.00,A,4800.10800,N,01130.00000,E,2.000,0.0,301026,,,A");
    feed_nmea(&g, "GNGGA,100001.00,4800.10800,N,01130.00000,E,1,09,0.9,650.0,M,48.0,M,,");
    CHECK_EQ(g.data.lat, 480000000);
    CHECK_EQ(g.data.alt_msl, 500000);
    CHECK_EQ(g.data.rejected, 1);
    CHECK_EQ(g.data.reject_reason, NEO_REJECT_JUMP);
    /* normale Epoche: RMC und GGA akzeptiert, Hoehe aktualisiert */
    feed_nmea(&g, "GNRMC,100002.00,A,4800.00060,N,01130.00000,E,2.000,0.0,301026,,,A");
    feed_nmea(&g, "GNGGA,100002.00,4800.00060,N,01130.00000,E,1,09,0.9,501.0,M,48.0,M,,");
    CHECK_EQ(g.data.lat, 480000100);
    CHECK_EQ(g.data.alt_msl, 501000);
    /* Mitternacht: Zeit springt 23:59:59 -> 00:00:00 */
    neo_gps_filter_reset(&g);
    feed_nmea(&g, "GNRMC,235959.00,A,4800.00060,N,01130.00000,E,2.000,0.0,301026,,,A");
    feed_nmea(&g, "GNRMC,000000.00,A,4800.00090,N,01130.00000,E,2.000,0.0,311026,,,A");
    CHECK_EQ(g.data.rejected, 1);           /* unveraendert */
    CHECK_EQ(g.data.lat, 480000150);
}

/* --- Zeitzonen --------------------------------------------------------- */

static void set_utc(neo_gps_t *g, uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
{
    g->data.time.year = y; g->data.time.month = mo; g->data.time.day = d;
    g->data.time.hour = h; g->data.time.minute = mi; g->data.time.second = s;
    g->data.time.ms = 250;
    g->data.time_valid = g->data.date_valid = 1;
}

#define CHECK_LT(lt, Y, MO, D, H, MI, S, DST) do { \
    CHECK_EQ((lt).t.year, Y); CHECK_EQ((lt).t.month, MO); CHECK_EQ((lt).t.day, D); \
    CHECK_EQ((lt).t.hour, H); CHECK_EQ((lt).t.minute, MI); CHECK_EQ((lt).t.second, S); \
    CHECK_EQ((lt).is_dst, DST); } while (0)

static void test_time_berlin(void)
{
    static const neo_tz_t berlin = NEO_TZ_BERLIN;
    neo_gps_t g;
    neo_local_time_t lt;
    neo_gps_init(&g, NULL, NULL);

    /* noch keine gueltige Zeit */
    CHECK_EQ(neo_gps_local_time(&g.data, &berlin, &lt), 0);

    /* Winterzeit: UTC+1 */
    set_utc(&g, 2026, 1, 15, 12, 0, 0);
    CHECK_EQ(neo_gps_local_time(&g.data, &berlin, &lt), 1);
    CHECK_LT(lt, 2026, 1, 15, 13, 0, 0, 0);
    CHECK_EQ(lt.offset_min, 60);
    CHECK_EQ(lt.t.ms, 250);
    CHECK_EQ(lt.weekday, 4);                       /* Donnerstag */

    /* Sommerzeitbeginn 29.03.2026: 02:00 MEZ -> 03:00 MESZ (= 01:00 UTC) */
    set_utc(&g, 2026, 3, 29, 0, 59, 59);
    neo_gps_local_time(&g.data, &berlin, &lt);
    CHECK_LT(lt, 2026, 3, 29, 1, 59, 59, 0);
    set_utc(&g, 2026, 3, 29, 1, 0, 0);
    neo_gps_local_time(&g.data, &berlin, &lt);
    CHECK_LT(lt, 2026, 3, 29, 3, 0, 0, 1);
    CHECK_EQ(lt.offset_min, 120);

    /* Sommerzeitende 25.10.2026: 03:00 MESZ -> 02:00 MEZ (= 01:00 UTC) */
    set_utc(&g, 2026, 10, 25, 0, 59, 59);
    neo_gps_local_time(&g.data, &berlin, &lt);
    CHECK_LT(lt, 2026, 10, 25, 2, 59, 59, 1);
    set_utc(&g, 2026, 10, 25, 1, 0, 0);
    neo_gps_local_time(&g.data, &berlin, &lt);
    CHECK_LT(lt, 2026, 10, 25, 2, 0, 0, 0);

    /* Tageswechsel durch die Umrechnung: 30.10.2026 23:30 UTC = Sa 31.10. 00:30 MEZ */
    set_utc(&g, 2026, 10, 30, 23, 30, 0);
    neo_gps_local_time(&g.data, &berlin, &lt);
    CHECK_LT(lt, 2026, 10, 31, 0, 30, 0, 0);
    CHECK_EQ(lt.weekday, 6);
    /* Silvester -> Neujahr, Schaltjahr */
    set_utc(&g, 2027, 12, 31, 23, 0, 0);
    neo_gps_local_time(&g.data, &berlin, &lt);
    CHECK_LT(lt, 2028, 1, 1, 0, 0, 0, 0);
    set_utc(&g, 2028, 2, 28, 23, 30, 0);
    neo_gps_local_time(&g.data, &berlin, &lt);
    CHECK_LT(lt, 2028, 2, 29, 0, 30, 0, 0);

    /* direkt aus NMEA */
    neo_gps_init(&g, NULL, NULL);
    feed_nmea(&g, "GNRMC,103000.50,A,5231.00000,N,01324.00000,E,0.0,0.0,150726,,,A");
    CHECK_EQ(neo_gps_local_time(&g.data, &berlin, &lt), 1);
    CHECK_LT(lt, 2026, 7, 15, 12, 30, 0, 1);
    CHECK_EQ(lt.t.ms, 500);
}

static int tz_eq(const neo_tz_t *a, const neo_tz_t *b)
{
    return a->std_offset == b->std_offset && a->dst_offset == b->dst_offset &&
           a->start_month == b->start_month && a->start_week == b->start_week &&
           a->start_wday == b->start_wday && a->start_min == b->start_min &&
           a->end_month == b->end_month && a->end_week == b->end_week &&
           a->end_wday == b->end_wday && a->end_min == b->end_min;
}

static void test_time_parse(void)
{
    static const neo_tz_t berlin = NEO_TZ_BERLIN, london = NEO_TZ_LONDON, hel = NEO_TZ_HELSINKI;
    neo_tz_t tz;
    CHECK(neo_tz_parse(&tz, "CET-1CEST,M3.5.0,M10.5.0/3") && tz_eq(&tz, &berlin));
    CHECK(neo_tz_parse(&tz, "GMT0BST,M3.5.0/1,M10.5.0") && tz_eq(&tz, &london));
    CHECK(neo_tz_parse(&tz, "EET-2EEST,M3.5.0/3,M10.5.0/4") && tz_eq(&tz, &hel));
    CHECK(neo_tz_parse(&tz, "IST-5:30") && tz.std_offset == 330 && tz.dst_offset == 330);
    CHECK(neo_tz_parse(&tz, "<+0545>-5:45") && tz.std_offset == 345);
    CHECK(neo_tz_parse(&tz, "EST5EDT,M3.2.0,M11.1.0") && tz.std_offset == -300 && tz.dst_offset == -240);
    /* ungueltig / nicht unterstuetzt: tz bleibt unveraendert */
    tz = berlin;
    CHECK(!neo_tz_parse(&tz, "CET"));
    CHECK(!neo_tz_parse(&tz, "CET-1CEST"));
    CHECK(!neo_tz_parse(&tz, "CET-1CEST,J60,J300"));
    CHECK(!neo_tz_parse(&tz, "CET-1CEST,M13.5.0,M10.5.0"));
    CHECK(!neo_tz_parse(&tz, "CET-1CEST,M3.5.0,M10.5.0/3x"));
    CHECK(!neo_tz_parse(&tz, "Europe/Berlin"));
    CHECK(!neo_tz_parse(&tz, NULL));
    CHECK(tz_eq(&tz, &berlin));
}

#if defined(__unix__) || defined(__APPLE__)
/* Vergleich mit der glibc-Zeitzonenlogik ueber 1971..2105 */
static void test_time_vs_libc(void)
{
    static const char *zones[] = {
        "CET-1CEST,M3.5.0,M10.5.0/3",              /* Berlin              */
        "GMT0BST,M3.5.0/1,M10.5.0",                /* London              */
        "EST5EDT,M3.2.0,M11.1.0",                  /* New York            */
        "AEST-10AEDT,M10.1.0,M4.1.0/3",            /* Sydney (Suedhalbk.) */
        "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0",    /* Lord Howe: 30 min   */
        "<-04>4<-03>,M9.1.6/24,M4.1.6/24",         /* Chile: Umschalten 24:00 */
        "<-02>2<-01>,M3.5.0/-1,M10.5.0/0",         /* Groenland: negative Zeit */
        "IST-5:30",                                /* Indien, keine Sommerzeit */
    };
    unsigned z, bad = 0;
    for (z = 0; z < sizeof(zones) / sizeof(zones[0]); z++) {
        neo_tz_t tz;
        uint32_t t;
        unsigned n = 0, zbad = 0;
        CHECK(neo_tz_parse(&tz, zones[z]));
        setenv("TZ", zones[z], 1);
        tzset();
        for (t = 31536000UL; t < 4290000000UL; t += 2999u, n++) {   /* 2999 s: wandert durch alle Uhrzeiten */
            time_t tt = (time_t)t;
            struct tm ref;
            neo_datetime_t dt;
            int16_t off;
            uint8_t dst, wd;
            uint32_t loc = neo_tz_utc_to_local(&tz, t, &off, &dst);
            localtime_r(&tt, &ref);
            wd = neo_time_from_unix(loc, &dt);
            if (dt.year != ref.tm_year + 1900 || dt.month != ref.tm_mon + 1 || dt.day != ref.tm_mday ||
                dt.hour != ref.tm_hour || dt.minute != ref.tm_min || dt.second != ref.tm_sec ||
                wd != ref.tm_wday || dst != (ref.tm_isdst > 0)) {
                if (zbad++ < 3)
                    printf("  %s: t=%u -> %04u-%02u-%02u %02u:%02u dst=%u, libc %04d-%02d-%02d %02d:%02d dst=%d\n",
                           zones[z], t, dt.year, dt.month, dt.day, dt.hour, dt.minute, dst,
                           ref.tm_year + 1900, ref.tm_mon + 1, ref.tm_mday, ref.tm_hour, ref.tm_min, ref.tm_isdst);
            }
        }
        /* jede Umschaltung minutengenau: 2 h um alle Wechsel herum, Minute fuer Minute */
        {
            int16_t prev_off = 0;
            uint8_t first = 1;
            for (t = 946684800UL; t < 2524608000UL; t += 1800u) {     /* 2000..2050 in 30-min-Schritten */
                int16_t off;
                neo_tz_utc_to_local(&tz, t, &off, NULL);
                if (!first && off != prev_off) {
                    uint32_t u;
                    for (u = t - 3600u; u < t + 3600u; u += 60u, n++) {
                        time_t tt = (time_t)u;
                        struct tm ref;
                        neo_datetime_t dt;
                        localtime_r(&tt, &ref);
                        neo_time_from_unix(neo_tz_utc_to_local(&tz, u, NULL, NULL), &dt);
                        if (dt.hour != ref.tm_hour || dt.minute != ref.tm_min || dt.day != ref.tm_mday) zbad++;
                    }
                }
                prev_off = off; first = 0;
            }
        }
        if (zbad) printf("  %s: %u von %u falsch\n", zones[z], zbad, n);
        bad += zbad;
    }
    unsetenv("TZ");
    CHECK_EQ(bad, 0);
}
#endif

static void bench(void)
{
    neo_gps_t g;
    const char *s = "$GNRMC,083559.00,A,4717.11437,N,00833.91522,E,0.004,77.52,091202,,,A*49\r\n"
                    "$GNGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*45\r\n"
                    "$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74\r\n";
    size_t len = strlen(s), i, total = 0;
    unsigned it;
    clock_t t0;
    neo_gps_init(&g, NULL, NULL);
    t0 = clock();
    for (it = 0; it < 200000; it++) { for (i = 0; i < len; i++) neo_gps_feed(&g, (uint8_t)s[i]); total += len; }
    printf("Benchmark: %.1f MB/s NMEA, sizeof(neo_gps_t) = %u Byte\n",
           (double)total / ((double)(clock() - t0) / CLOCKS_PER_SEC) / 1e6, (unsigned)sizeof(neo_gps_t));
}

int main(void)
{
    test_rmc();
    test_gga_gsa_gsv_vtg();
    test_errors_and_resync();
    test_ubx_pvt();
    test_ubx_neo6();
    test_model_detect();
    test_tx_and_ack();
    test_mixed_stream();
    test_util();
    test_filter_off();
    test_filter_jump();
    test_filter_reanchor();
    test_filter_quality();
    test_filter_tunnel();
    test_filter_smoothing();
    test_filter_nmea();
    test_time_berlin();
    test_time_parse();
#if defined(__unix__) || defined(__APPLE__)
    test_time_vs_libc();
#endif
    printf("%d/%d Checks bestanden\n", checks - fails, checks);
    if (getenv("NEO_BENCH")) bench();
    return fails ? 1 : 0;
}
