/* SPDX-License-Identifier: MIT */
#include "neo_gps_util.h"
#include <math.h>

#define DEG2RAD_E7 (3.14159265358979323846 / 180.0 * 1e-7)
#define EARTH_R    6371008.8   /* mittlerer Erdradius in m */

float neo_gps_distance_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2)
{
    double p1 = (double)lat1 * DEG2RAD_E7, p2 = (double)lat2 * DEG2RAD_E7;
    double dp = (double)(lat2 - lat1) * DEG2RAD_E7;
    double dl = (double)((int64_t)lon2 - lon1) * DEG2RAD_E7;
    double a, s1, s2;
    if (dl > 3.14159265358979323846)  dl -= 2.0 * 3.14159265358979323846;
    if (dl < -3.14159265358979323846) dl += 2.0 * 3.14159265358979323846;
    s1 = sin(dp * 0.5);
    s2 = sin(dl * 0.5);
    a = s1 * s1 + cos(p1) * cos(p2) * s2 * s2;
    return (float)(2.0 * EARTH_R * atan2(sqrt(a), sqrt(1.0 - a)));
}

float neo_gps_distance_fast_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2)
{
    float x = (float)((float)((int64_t)lon2 - lon1) * DEG2RAD_E7) *
              cosf((float)(((double)lat1 + (double)lat2) * 0.5 * DEG2RAD_E7));
    float y = (float)((float)((int64_t)lat2 - lat1) * DEG2RAD_E7);
    return sqrtf(x * x + y * y) * (float)EARTH_R;
}

float neo_gps_bearing_deg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2)
{
    double p1 = (double)lat1 * DEG2RAD_E7, p2 = (double)lat2 * DEG2RAD_E7;
    double dl = (double)((int64_t)lon2 - lon1) * DEG2RAD_E7;
    double y = sin(dl) * cos(p2);
    double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    double b = atan2(y, x) * (180.0 / 3.14159265358979323846);
    return (float)(b < 0.0 ? b + 360.0 : b);
}
