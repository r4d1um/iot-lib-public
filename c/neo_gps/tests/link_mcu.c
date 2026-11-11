/* Linktest fuer die fertigen dist/<ziel>/libneo_gps.a: ruft aus jeder Quelle
 * etwas auf, damit fehlende oder falsch gebaute Objekte beim Linken auffallen. */
#include "neo_gps.h"
#include "neo_gps_util.h"
#include "neo_gps_time.h"

static neo_gps_t gps;
static void tx(void *user, const uint8_t *d, uint16_t n) { (void)user; (void)d; (void)n; }

int main(void)
{
    static const char s[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    neo_tz_t tz;
    neo_local_time_t lt;
    neo_gps_init(&gps, tx, 0);
    neo_gps_filter_preset(&gps, NEO_FILTER_PEDESTRIAN);
    neo_gps_feed_buf(&gps, (const uint8_t *)s, (uint16_t)(sizeof s - 1));
    neo_tz_parse(&tz, "CET-1CEST,M3.5.0,M10.5.0/3");
    neo_gps_local_time(neo_gps_data(&gps), &tz, &lt);
    return (int)(neo_gps_distance_m(0, 0, 1, 1) > 0.0) + (int)neo_gps_updates(&gps);
}
