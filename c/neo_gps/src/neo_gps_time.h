/*
 * neo_gps_time - Ortszeit aus der GPS-Zeit (UTC) mit Zeitzone und Sommerzeit.
 *
 *  - rein ganzzahlig, kein math.h, kein libc-time.h, keine Zeitzonen-Datenbank
 *  - Regeln wie im POSIX-TZ-String (z.B. "CET-1CEST,M3.5.0,M10.5.0/3" fuer Berlin),
 *    damit funktioniert praktisch jede Zeitzone der Welt
 *  - wird nur gelinkt, wenn du die Funktionen auch benutzt
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NEO_GPS_TIME_H
#define NEO_GPS_TIME_H

#include "neo_gps.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Zeitzone mit Sommerzeitregel "Monat, Woche, Wochentag" (POSIX "Mm.w.d/time") */
typedef struct {
    int16_t std_offset;     /* Minuten oestlich von UTC in Normalzeit (Berlin: +60)   */
    int16_t dst_offset;     /* Minuten in Sommerzeit (Berlin: +120); = std_offset: keine */
    uint8_t start_month;    /* Sommerzeitbeginn: Monat 1..12                          */
    uint8_t start_week;     /*   Woche 1..4, 5 = letzte                               */
    uint8_t start_wday;     /*   Wochentag 0 = Sonntag                                */
    int16_t start_min;      /*   Uhrzeit in Minuten, lokale Normalzeit (Berlin 02:00) */
    uint8_t end_month;      /* Sommerzeitende                                         */
    uint8_t end_week;
    uint8_t end_wday;
    int16_t end_min;        /*   Uhrzeit in Minuten, lokale Sommerzeit (Berlin 03:00) */
} neo_tz_t;

/* Fertige Zeitzonen (als Initialisierer: static const neo_tz_t tz = NEO_TZ_BERLIN;) */
#define NEO_TZ_UTC       {   0,   0, 0, 0, 0,   0,  0, 0, 0,   0 }
#define NEO_TZ_BERLIN    {  60, 120, 3, 5, 0, 120, 10, 5, 0, 180 }  /* CET-1CEST,M3.5.0,M10.5.0/3 */
#define NEO_TZ_CET       NEO_TZ_BERLIN                              /* auch Wien, Zuerich, Paris ... */
#define NEO_TZ_LONDON    {   0,  60, 3, 5, 0,  60, 10, 5, 0, 120 }  /* GMT0BST,M3.5.0/1,M10.5.0   */
#define NEO_TZ_HELSINKI  { 120, 180, 3, 5, 0, 180, 10, 5, 0, 240 }  /* EET-2EEST,M3.5.0/3,M10.5.0/4 */

/* Ortszeit */
typedef struct {
    neo_datetime_t t;       /* lokales Datum + Uhrzeit (ms aus der GPS-Zeit uebernommen) */
    uint8_t  weekday;       /* 0 = Sonntag ... 6 = Samstag                           */
    uint8_t  is_dst;        /* 1 = Sommerzeit                                        */
    int16_t  offset_min;    /* aktuelle Abweichung von UTC in Minuten (Berlin 60/120) */
} neo_local_time_t;

/* GPS-Zeit (UTC) in Ortszeit umrechnen. Rueckgabe 0, wenn Datum/Uhrzeit (noch)
 * nicht gueltig sind - dann ist *out unveraendert. */
int neo_gps_local_time(const neo_gps_data_t *d, const neo_tz_t *tz, neo_local_time_t *out);

/* POSIX-TZ-String einlesen, z.B. "CET-1CEST,M3.5.0,M10.5.0/3", "EST5EDT,M3.2.0,M11.1.0",
 * "AEST-10AEDT,M10.1.0,M4.1.0/3", "<+03>-3". Nur Regeln im Format Mm.w.d.
 * Rueckgabe 1 = ok, 0 = Format nicht unterstuetzt (tz bleibt unveraendert). */
int neo_tz_parse(neo_tz_t *tz, const char *posix);

/* --- Bausteine ---------------------------------------------------------- */

/* UTC-Datum/Zeit -> Unix-Zeit (Sekunden seit 1970). 0 wenn ungueltig. */
uint32_t neo_gps_unix_time(const neo_gps_data_t *d);

/* Unix-Zeit (UTC) -> lokale Unix-Zeit (Sekunden), optional mit Offset/Sommerzeit-Flag */
uint32_t neo_tz_utc_to_local(const neo_tz_t *tz, uint32_t utc, int16_t *offset_min, uint8_t *is_dst);

/* Sekunden seit 1970 -> Datum/Uhrzeit (ms = 0), Rueckgabe Wochentag (0 = Sonntag) */
uint8_t neo_time_from_unix(uint32_t t, neo_datetime_t *out);

/* Datum -> Unix-Zeit 00:00:00 dieses Tages */
uint32_t neo_time_to_unix(uint16_t year, uint8_t month, uint8_t day);

#ifdef __cplusplus
}
#endif

#endif /* NEO_GPS_TIME_H */
