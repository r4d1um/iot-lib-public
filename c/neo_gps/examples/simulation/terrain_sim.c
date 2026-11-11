/*
 * Gelaende-Simulation: Flaeche und Steigung eines Grundstuecks per GPS.
 *
 * Vier virtuelle NEO-M8N stehen an den Ecken eines geneigten Grundstuecks und
 * erzeugen 24 h lang UBX NAV-PVT-Frames (1 Hz) mit einem Fehlermodell. Die Frames
 * laufen Byte fuer Byte durch neo_gps_feed() inklusive Sprungfilter. Verglichen
 * werden drei Vorgehensweisen bei gleicher Gesamt-Messdauer T:
 *
 *   seq   Ein Geraet, Ecken nacheinander: jede Ecke T/4 lang, 60 s zum Weitertragen
 *   snap  Vier Sensoren gleichzeitig, nur ein einzelner Moment (Epoche T)
 *   sync  Vier Sensoren gleichzeitig, ueber 0..T gemittelt
 *
 * Gemeinsame Fehler (Atmosphaere, Bahnen) verschieben alle gleichzeitigen Messungen
 * gleich und heben sich in Flaeche und Steigung auf. Bei "seq" stammen die Ecken aus
 * verschiedenen Zeiten, dort wirken sie voll.
 *
 *   make sim                 -> build/sim.json (100 Laeufe)
 *   build/terrain_sim 20     -> 20 Laeufe
 *
 * Das Fehlermodell besteht aus plausiblen Annahmen fuer einen NEO-M8N im Freien,
 * nicht aus Messwerten.
 * SPDX-License-Identifier: MIT
 */
#include "neo_gps.h"
#include "neo_gps_time.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS        4
#define DURATION  86400          /* s, 1 Hz */
#define MAXCONV   200
#define CLOUD_DT  60
#define WALK_S    60             /* Umtragen des Einzelgeraets zur naechsten Ecke */
#define PI        3.14159265358979323846
#define R_EARTH   6371008.8
#define LAT0      52.5200
#define LON0      13.4050
#define START_UTC 1793430000UL   /* 2026-10-31 07:00:00 UTC = 08:00 MEZ */

/* ---------------- Wahrheit: Grundstueck ---------------------------------- */
static const double TE[NS] = { 0.0, 32.0, 29.0, -2.0 };   /* Ost  [m] */
static const double TN[NS] = { 0.0,  3.0, 24.0, 21.0 };   /* Nord [m] */
#define H0      48.0             /* Hoehe am Punkt A [m]               */
#define GRADE   0.08             /* 8 % Gefaelle                        */
#define ASPECT  150.0            /* Richtung bergab, Grad ab Nord (SSO) */

static double true_h(double e, double n)
{
    double a = ASPECT * PI / 180.0;
    return H0 - GRADE * (e * sin(a) + n * cos(a));
}

/* ---------------- Fehlermodell (Annahmen) -------------------------------- */
#define COM_TAU   1800.0         /* gemeinsam fuer alle Sensoren         */
#define COM_SH    1.5            /*   m je Achse horizontal              */
#define COM_SV    3.5            /*   m vertikal                         */
#define MP_TAU    600.0          /* je Sensor: Multipath, langsam        */
static const double MP_SH[NS]  = { 0.5, 0.6, 1.2, 0.5 };    /* C steht nah am Haus */
static const double BIAS_H[NS] = { 0.10, 0.12, 0.35, 0.10 }; /* Rest-Bias, mittelt sich nicht weg */
static const double BIAS_V[NS] = { 0.20, 0.25, 0.70, 0.20 };
#define WN_H      0.25           /* weisses Rauschen */
#define WN_V      0.50
#define JUMP_RATE (1.0 / 1800.0) /* Ausreisser pro Sensor und Sekunde (~48 pro Tag) */

static uint64_t rng;
static double urand(void)
{
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return (double)(rng >> 11) * (1.0 / 9007199254740992.0);
}
static double grand(void)
{
    double u1 = urand(), u2 = urand();
    if (u1 < 1e-300) u1 = 1e-300;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
}
static double gm(double x, double sigma, double tau)   /* Gauss-Markov 1. Ordnung */
{
    double phi = exp(-1.0 / tau);
    return x * phi + sigma * sqrt(1.0 - phi * phi) * grand();
}

/* ---------------- UBX NAV-PVT erzeugen ----------------------------------- */
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static size_t make_pvt(uint8_t *f, uint32_t unix_t, double e, double n, double h,
                       uint32_t hacc, uint32_t vacc, uint32_t speed, uint8_t sv)
{
    uint8_t *p = f + 6, a = 0, b = 0;
    neo_datetime_t dt;
    size_t i;
    double lat = LAT0 + n / R_EARTH * 180.0 / PI;
    double lon = LON0 + e / (R_EARTH * cos(LAT0 * PI / 180.0)) * 180.0 / PI;
    neo_time_from_unix(unix_t, &dt);
    memset(f, 0, 100);
    f[0] = 0xB5; f[1] = 0x62; f[2] = 0x01; f[3] = 0x07; f[4] = 92; f[5] = 0;
    put32(p, (uint32_t)((unix_t - 315964800UL + 18u) % 604800UL) * 1000u);   /* iTOW */
    put16(p + 4, dt.year); p[6] = dt.month; p[7] = dt.day;
    p[8] = dt.hour; p[9] = dt.minute; p[10] = dt.second;
    p[11] = 0x07; p[20] = 3; p[21] = 0x01; p[23] = sv;
    put32(p + 24, (uint32_t)(int32_t)lround(lon * 1e7));
    put32(p + 28, (uint32_t)(int32_t)lround(lat * 1e7));
    put32(p + 32, (uint32_t)(int32_t)lround((h + 40.0) * 1000.0));
    put32(p + 36, (uint32_t)(int32_t)lround(h * 1000.0));
    put32(p + 40, hacc); put32(p + 44, vacc);
    put32(p + 60, speed);
    put16(p + 76, 140);
    for (i = 2; i < 98; i++) { a = (uint8_t)(a + f[i]); b = (uint8_t)(b + a); }
    f[98] = a; f[99] = b;
    return 100;
}

/* ---------------- Auswertung --------------------------------------------- */
typedef struct { double e[NS], n[NS], h[NS]; } geo_t;
typedef struct { double area, surface, slope_pct, slope_deg, aspect; } result_t;

static result_t evaluate(const geo_t *g)
{
    result_t r;
    double s = 0, m[3][4] = { { 0 } }, b, c;
    int i, j, k;
    for (i = 0; i < NS; i++) {                     /* Gauss'sche Trapezformel */
        j = (i + 1) % NS;
        s += g->e[i] * g->n[j] - g->e[j] * g->n[i];
    }
    r.area = fabs(s) * 0.5;
    for (i = 0; i < NS; i++) {                     /* Ausgleichsebene h = a + b*E + c*N */
        double row[3] = { 1.0, g->e[i], g->n[i] };
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 3; k++) m[j][k] += row[j] * row[k];
            m[j][3] += row[j] * g->h[i];
        }
    }
    for (i = 0; i < 3; i++)
        for (j = i + 1; j < 3; j++) {
            double f = m[j][i] / m[i][i];
            for (k = i; k < 4; k++) m[j][k] -= f * m[i][k];
        }
    c = m[2][3] / m[2][2];
    b = (m[1][3] - m[1][2] * c) / m[1][1];
    r.slope_pct = 100.0 * sqrt(b * b + c * c);
    r.slope_deg = atan(sqrt(b * b + c * c)) * 180.0 / PI;
    r.aspect    = fmod(atan2(-b, -c) * 180.0 / PI + 360.0, 360.0);
    r.surface   = r.area / cos(r.slope_deg * PI / 180.0);
    return r;
}

/* ---------------- ein 24-h-Lauf ------------------------------------------ */
static double pos[NS][DURATION][3];          /* akzeptierte bzw. zuletzt gueltige Position */
static uint8_t okf[NS][DURATION];
static double pre[NS][DURATION + 1][3];      /* Praefixsummen ueber akzeptierte Epochen   */
static uint32_t prc[NS][DURATION + 1];

typedef struct { long events, jump_epochs, passed, rejected; } fstat_t;

static void run(uint64_t seed, fstat_t *fs, char *start_s, char *end_s)
{
    static neo_gps_t gps[NS];
    neo_filter_cfg_t fc;
    double com[3], mp[NS][3], bias[NS][3], jd[NS][3], last[NS][3] = { { 0 } };
    int jump_left[NS] = { 0 }, k;
    uint32_t t;

    rng = seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;
    for (k = 0; k < 20; k++) urand();

    /* Filter: Qualitaet + Sprungtest, keine Glaettung (gemittelt wird in der Auswertung) */
    memset(&fc, 0, sizeof(fc));
    fc.min_fix = NEO_FIX_3D; fc.min_sv = 6; fc.max_hdop = 300; fc.max_h_acc = 10000;
    fc.max_accel = 300; fc.margin = 4000; fc.reanchor = 30;

    com[0] = COM_SH * grand(); com[1] = COM_SH * grand(); com[2] = COM_SV * grand();
    for (k = 0; k < NS; k++) {
        double dir = urand() * 2 * PI;
        neo_gps_init(&gps[k], NULL, NULL);
        neo_gps_filter_config(&gps[k], &fc);
        mp[k][0] = MP_SH[k] * grand(); mp[k][1] = MP_SH[k] * grand(); mp[k][2] = 2 * MP_SH[k] * grand();
        bias[k][0] = BIAS_H[k] * sin(dir); bias[k][1] = BIAS_H[k] * cos(dir);
        bias[k][2] = BIAS_V[k] * (urand() < 0.5 ? -1 : 1);
        jd[k][0] = jd[k][1] = jd[k][2] = 0;
        fs[k].events = fs[k].jump_epochs = fs[k].passed = 0;
    }

    for (t = 0; t < DURATION; t++) {
        uint32_t ut = START_UTC + t;
        com[0] = gm(com[0], COM_SH, COM_TAU);
        com[1] = gm(com[1], COM_SH, COM_TAU);
        com[2] = gm(com[2], COM_SV, COM_TAU);

        for (k = 0; k < NS; k++) {
            uint8_t frame[100];
            size_t n, b;
            double e, nn, h, je = 0, jn = 0, jh = 0;
            int in_jump;

            mp[k][0] = gm(mp[k][0], MP_SH[k], MP_TAU);
            mp[k][1] = gm(mp[k][1], MP_SH[k], MP_TAU);
            mp[k][2] = gm(mp[k][2], 2 * MP_SH[k], MP_TAU);

            if (!jump_left[k] && urand() < JUMP_RATE) {       /* neuer Ausreisser, 1..4 s */
                double mag = 15 + urand() * 65, dir = urand() * 2 * PI;
                jump_left[k] = 1 + (int)(urand() * 4);
                jd[k][0] = mag * sin(dir); jd[k][1] = mag * cos(dir); jd[k][2] = (urand() - 0.5) * 60;
                fs[k].events++;
            }
            in_jump = jump_left[k] > 0;
            if (in_jump) { je = jd[k][0]; jn = jd[k][1]; jh = jd[k][2]; jump_left[k]--; fs[k].jump_epochs++; }

            e  = TE[k] + com[0] + mp[k][0] + bias[k][0] + WN_H * grand() + je;
            nn = TN[k] + com[1] + mp[k][1] + bias[k][1] + WN_H * grand() + jn;
            h  = true_h(TE[k], TN[k]) + com[2] + mp[k][2] + bias[k][2] + WN_V * grand() + jh;

            n = make_pvt(frame, ut, e, nn, h,
                         (uint32_t)(2200 + 400 * fabs(grand())), (uint32_t)(4000 + 800 * fabs(grand())),
                         (uint32_t)(30 * fabs(grand())), (uint8_t)(10 + (int)(urand() * 4)));
            neo_gps_updates(&gps[k]);
            for (b = 0; b < n; b++) neo_gps_feed(&gps[k], frame[b]);   /* echte Library */

            okf[k][t] = (neo_gps_updates(&gps[k]) & NEO_UPD_POSITION) != 0;
            if (in_jump && okf[k][t]) fs[k].passed++;
            if (okf[k][t]) {
                const neo_gps_data_t *d = neo_gps_data(&gps[k]);
                last[k][1] = (d->lat * 1e-7 - LAT0) * PI / 180.0 * R_EARTH;
                last[k][0] = (d->lon * 1e-7 - LON0) * PI / 180.0 * R_EARTH * cos(LAT0 * PI / 180.0);
                last[k][2] = d->alt_msl * 1e-3;
            }
            pos[k][t][0] = last[k][0]; pos[k][t][1] = last[k][1]; pos[k][t][2] = last[k][2];
        }
        if (start_s && (t == 0 || t == DURATION - 1)) {           /* Ortszeit mit der Library */
            static const neo_tz_t berlin = NEO_TZ_BERLIN;
            neo_local_time_t lt;
            if (neo_gps_local_time(neo_gps_data(&gps[0]), &berlin, &lt))
                snprintf(t == 0 ? start_s : end_s, 64, "%02u.%02u.%04u %02u:%02u %s",
                         lt.t.day, lt.t.month, lt.t.year, lt.t.hour, lt.t.minute,
                         lt.is_dst ? "MESZ" : "MEZ");
        }
    }
    for (k = 0; k < NS; k++) {
        int a;
        fs[k].rejected = gps[k].data.rejected;
        for (a = 0; a < 3; a++) pre[k][0][a] = 0;
        prc[k][0] = 0;
        for (t = 0; t < DURATION; t++) {
            for (a = 0; a < 3; a++) pre[k][t + 1][a] = pre[k][t][a] + (okf[k][t] ? pos[k][t][a] : 0);
            prc[k][t + 1] = prc[k][t] + okf[k][t];
        }
    }
}

/* Mittel von Sensor k ueber [a, b) */
static void wavg(int k, uint32_t a, uint32_t b, double out[3])
{
    uint32_t c = prc[k][b] - prc[k][a];
    int i;
    for (i = 0; i < 3; i++)
        out[i] = c ? (pre[k][b][i] - pre[k][a][i]) / c : pos[k][b ? b - 1 : 0][i];
}

static void methods(uint32_t T, geo_t *seq, geo_t *snap, geo_t *sync)
{
    int k;
    for (k = 0; k < NS; k++) {
        double v[3];
        /* ein Geraet: jede Ecke T/4 messen, dazwischen WALK_S Sekunden zum Umtragen */
        uint32_t q = T / NS ? T / NS : 1;
        uint32_t a = (uint32_t)k * (q + WALK_S), b = a + q;
        if (b > DURATION) { b = DURATION; a = b - q; }
        wavg(k, a, b, v);         seq->e[k] = v[0];  seq->n[k] = v[1];  seq->h[k] = v[2];
        snap->e[k] = pos[k][T - 1][0]; snap->n[k] = pos[k][T - 1][1]; snap->h[k] = pos[k][T - 1][2];
        wavg(k, 0, T, v);         sync->e[k] = v[0]; sync->n[k] = v[1]; sync->h[k] = v[2];
    }
}

/* ---------------- JSON-Helfer -------------------------------------------- */
static void j_geo(const geo_t *g)
{
    int i;
    printf("[");
    for (i = 0; i < NS; i++) printf("%s[%.3f,%.3f,%.3f]", i ? "," : "", g->e[i], g->n[i], g->h[i]);
    printf("]");
}
static void j_res(const result_t *r)
{
    printf("{\"area\":%.2f,\"surface\":%.2f,\"slope_pct\":%.3f,\"slope_deg\":%.3f,\"aspect\":%.1f}",
           r->area, r->surface, r->slope_pct, r->slope_deg, r->aspect);
}
static int cmpd(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static double pct(double *v, int n, double p)
{
    qsort(v, (size_t)n, sizeof(double), cmpd);
    return v[(int)floor(p * (n - 1) + 0.5)];
}
static double ang_diff(double a, double b)
{
    double d = fmod(fabs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

int main(int argc, char **argv)
{
    int runs = argc > 1 ? atoi(argv[1]) : 100;
    static uint32_t T[MAXCONV];
    static result_t rs[3][MAXCONV][1000];         /* [Methode][T][Lauf] */
    static double final_err[1000], sorted[1000];
    static fstat_t fs[NS], fsum[NS];
    char start_s[64] = "", end_s[64] = "";
    geo_t truth;
    result_t rt;
    int nt = 0, i, r, k, m;
    double lg;

    if (runs < 1) runs = 1;
    if (runs > 1000) runs = 1000;

    /* Messdauern 4 s .. 24 h, logarithmisch, ganzzahlig, ohne Duplikate */
    for (lg = log10(4.0); nt < MAXCONV; lg += (log10((double)DURATION) - log10(4.0)) / 119.0) {
        uint32_t v = (uint32_t)lround(pow(10.0, lg));
        if (v > DURATION) v = DURATION;
        if (!nt || v > T[nt - 1]) T[nt++] = v;
        if (v == DURATION) break;
    }

    for (k = 0; k < NS; k++) { truth.e[k] = TE[k]; truth.n[k] = TN[k]; truth.h[k] = true_h(TE[k], TN[k]); }
    rt = evaluate(&truth);
    memset(fsum, 0, sizeof(fsum));

    for (r = 0; r < runs; r++) {
        run((uint64_t)r + 1, fs, start_s, end_s);
        for (k = 0; k < NS; k++) {
            fsum[k].events += fs[k].events; fsum[k].jump_epochs += fs[k].jump_epochs;
            fsum[k].passed += fs[k].passed; fsum[k].rejected += fs[k].rejected;
        }
        for (i = 0; i < nt; i++) {
            geo_t g[3];
            methods(T[i], &g[0], &g[1], &g[2]);
            for (m = 0; m < 3; m++) {
                rs[m][i][r] = evaluate(&g[m]);
            }
        }
        final_err[r] = fabs(rs[2][nt - 1][r].slope_pct - rt.slope_pct);
        fprintf(stderr, "\rLauf %d/%d", r + 1, runs);
    }
    fprintf(stderr, "\n");

    printf("{\"runs\":%d,\"start\":\"%s\",\"end\":\"%s\",\"duration\":%d,", runs, start_s, end_s, DURATION);
    printf("\"truth\":{\"corners\":"); j_geo(&truth); printf(",\"res\":"); j_res(&rt); printf("},");
    printf("\"model\":{\"com_h\":%.1f,\"com_v\":%.1f,\"com_tau\":%.0f,\"mp_h\":[%.1f,%.1f,%.1f,%.1f],\"mp_tau\":%.0f,"
           "\"bias_h\":[%.2f,%.2f,%.2f,%.2f],\"bias_v\":[%.2f,%.2f,%.2f,%.2f],\"wn_h\":%.2f,\"wn_v\":%.2f,\"jumps_per_day\":%.0f,\"walk_s\":%d},",
           COM_SH, COM_SV, COM_TAU, MP_SH[0], MP_SH[1], MP_SH[2], MP_SH[3], MP_TAU,
           BIAS_H[0], BIAS_H[1], BIAS_H[2], BIAS_H[3], BIAS_V[0], BIAS_V[1], BIAS_V[2], BIAS_V[3],
           WN_H, WN_V, JUMP_RATE * 86400, WALK_S);
    printf("\"filter\":[");
    for (k = 0; k < NS; k++)
        printf("%s{\"events\":%ld,\"jump_epochs\":%ld,\"passed\":%ld,\"rejected\":%ld}", k ? "," : "",
               fsum[k].events, fsum[k].jump_epochs, fsum[k].passed, fsum[k].rejected);
    printf("],\"T\":[");
    for (i = 0; i < nt; i++) printf("%s%u", i ? "," : "", T[i]);
    printf("],");

    /* Fehlerstatistik ueber alle Laeufe: Median und 90-%-Quantil des Betrags */
    {
        static const char *mn[3] = { "seq", "snap", "sync" };
        static const char *q[6] = { "area50", "area90", "slope50", "slope90", "aspect50", "aspect90" };
        printf("\"stats\":{");
        for (m = 0; m < 3; m++) {
            int qq;
            printf("%s\"%s\":{", m ? "," : "", mn[m]);
            for (qq = 0; qq < 6; qq++) {
                printf("%s\"%s\":[", qq ? "," : "", q[qq]);
                for (i = 0; i < nt; i++) {
                    static double v[1000];
                    for (r = 0; r < runs; r++) {
                        const result_t *x = &rs[m][i][r];
                        v[r] = qq < 2 ? fabs(x->area - rt.area)
                             : qq < 4 ? fabs(x->slope_pct - rt.slope_pct)
                                      : ang_diff(x->aspect, rt.aspect);
                    }
                    printf("%s%.3f", i ? "," : "", pct(v, runs, (qq % 2) ? 0.9 : 0.5));
                }
                printf("]");
            }
            printf("}");
        }
        printf("},");

        /* Beispiel-Laeufe: typisch (Median) und ungünstig (90 %) nach Steigungsfehler
         * der Methode "sync" nach 24 h - deterministisch neu gerechnet */
        printf("\"examples\":[");
        {
            static const double want[2] = { 0.5, 0.9 };
            static const char *label[2] = { "typisch", "ungünstig" };
            int e;
            memcpy(sorted, final_err, sizeof(double) * (size_t)runs);
            for (e = 0; e < 2; e++) {
                double target = pct(sorted, runs, want[e]);
                int best = 0;
                uint32_t t;
                for (r = 0; r < runs; r++) if (final_err[r] == target) { best = r; break; }
                run((uint64_t)best + 1, fs, NULL, NULL);
                printf("%s{\"label\":\"%s\",\"run\":%d,\"quantile\":%.2f,\"cloud\":[", e ? "," : "",
                       label[e], best + 1, want[e]);
                for (k = 0; k < NS; k++) {
                    printf("%s[", k ? "," : "");
                    for (t = 0; t < DURATION; t += CLOUD_DT)
                        printf("%s[%.2f,%.2f,%.2f]", t ? "," : "", pos[k][t][0], pos[k][t][1], pos[k][t][2]);
                    printf("]");
                }
                printf("],\"dcloud\":[");            /* gleiche Epoche, relativ zu A */
                for (k = 0; k < NS; k++) {
                    printf("%s[", k ? "," : "");
                    for (t = 0; t < DURATION; t += CLOUD_DT)
                        printf("%s[%.2f,%.2f,%.2f]", t ? "," : "",
                               TE[0] + pos[k][t][0] - pos[0][t][0], TN[0] + pos[k][t][1] - pos[0][t][1],
                               true_h(TE[0], TN[0]) + pos[k][t][2] - pos[0][t][2]);
                    printf("]");
                }
                printf("]");
                for (m = 0; m < 3; m++) {
                    printf(",\"%s\":[", mn[m]);
                    for (i = 0; i < nt; i++) {
                        geo_t g[3];
                        result_t x;
                        methods(T[i], &g[0], &g[1], &g[2]);
                        x = evaluate(&g[m]);
                        printf("%s{\"g\":", i ? "," : ""); j_geo(&g[m]);
                        printf(",\"r\":"); j_res(&x); printf("}");
                    }
                    printf("]");
                }
                printf("}");
            }
        }
        printf("]}\n");
    }
    return 0;
}
