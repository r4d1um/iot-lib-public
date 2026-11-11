/*
 * neo_gps_util - optionale Helfer (Entfernung, Peilung). Braucht math.h / -lm.
 * Wird nur gelinkt, wenn du die Funktionen auch benutzt.
 * SPDX-License-Identifier: MIT
 */
#ifndef NEO_GPS_UTIL_H
#define NEO_GPS_UTIL_H

#include "neo_gps.h"
#include "neo_gps_time.h"   /* neo_gps_unix_time() liegt jetzt dort */

#ifdef __cplusplus
extern "C" {
#endif

/* Entfernung in Metern zwischen zwei Punkten (1e-7 Grad), Haversine */
float neo_gps_distance_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);

/* Schnelle Naeherung (equirectangular) - fuer Strecken < ~20 km, Fehler < 0.1 % */
float neo_gps_distance_fast_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);

/* Anfangspeilung von Punkt 1 nach Punkt 2 in Grad (0..360) */
float neo_gps_bearing_deg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);

#ifdef __cplusplus
}
#endif

#endif /* NEO_GPS_UTIL_H */
