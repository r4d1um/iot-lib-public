/*
 * Linux / Raspberry Pi / USB-Serial-Adapter
 *
 *   make linux
 *   ./build/neo_gps_linux /dev/ttyUSB0 9600          (NMEA, Werkszustand)
 *   ./build/neo_gps_linux /dev/ttyUSB0 9600 ubx      (auf 115200 Baud + UBX umstellen)
 */
#include "neo_gps.h"
#include "neo_gps_util.h"
#include "neo_gps_time.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int fd = -1;

static void uart_write(void *user, const uint8_t *data, uint16_t len)
{
    (void)user;
    if (write(fd, data, len) < 0) perror("write");
    tcdrain(fd);
}

static speed_t to_speed(long baud)
{
    switch (baud) {
    case 4800: return B4800;   case 9600: return B9600;     case 19200: return B19200;
    case 38400: return B38400; case 57600: return B57600;   case 115200: return B115200;
    case 230400: return B230400;
    default: return B9600;
    }
}

static int uart_open(const char *dev, long baud)
{
    struct termios t;
    int f = open(dev, O_RDWR | O_NOCTTY);
    if (f < 0) { perror(dev); exit(1); }
    tcgetattr(f, &t);
    cfmakeraw(&t);
    cfsetispeed(&t, to_speed(baud));
    cfsetospeed(&t, to_speed(baud));
    t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    tcsetattr(f, TCSANOW, &t);
    return f;
}

static void uart_set_baud(long baud)
{
    struct termios t;
    tcgetattr(fd, &t);
    cfsetispeed(&t, to_speed(baud));
    cfsetospeed(&t, to_speed(baud));
    tcsetattr(fd, TCSANOW, &t);
}

static neo_gps_t gps;

/* Wartet (max. ~2 s) auf ein bestimmtes Event */
static int wait_for(neo_event_t want)
{
    uint8_t buf[256];
    int tries = 0;
    while (tries++ < 200) {
        ssize_t n = read(fd, buf, sizeof(buf));
        for (ssize_t i = 0; i < n; i++) {
            neo_event_t e = neo_gps_feed(&gps, buf[i]);
            if (e == want || (want == NEO_EV_UBX_ACK && e == NEO_EV_UBX_NAK)) return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *dev = argc > 1 ? argv[1] : "/dev/ttyUSB0";
    long baud = argc > 2 ? strtol(argv[2], NULL, 10) : 9600;
    int ubx = argc > 3 && strcmp(argv[3], "ubx") == 0;
    uint8_t buf[256];

    fd = uart_open(dev, baud);
    neo_gps_init(&gps, uart_write, NULL);
    neo_gps_filter_preset(&gps, NEO_FILTER_PEDESTRIAN);

    neo_gps_detect_model(&gps);
    if (wait_for(NEO_EV_UBX_MON_VER)) {
        static const char *names[] = { "unbekannt", "NEO-6M", "NEO-7M", "NEO-M8N" };
        printf("Modul erkannt: %s\n", names[neo_gps_model(&gps)]);
    }

    if (ubx) {
        neo_gps_cfg_port(&gps, 115200, NEO_PROTO_UBX);
        usleep(100000);
        uart_set_baud(115200);
        neo_gps_cfg_output(&gps, NEO_OUT_UBX);
        neo_gps_cfg_rate(&gps, neo_gps_model(&gps) == NEO_MODEL_NEO6 ? 200 : 100);
        printf("Rate: %s\n", wait_for(NEO_EV_UBX_ACK) && neo_gps_ack_status(&gps) == NEO_ACK_OK ? "OK" : "kein ACK");
    }

    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) continue;
        neo_gps_feed_buf(&gps, buf, (uint16_t)n);

        uint16_t upd = neo_gps_updates(&gps);
        if (upd & NEO_UPD_POSITION) {
            const neo_gps_data_t *d = neo_gps_data(&gps);
            static const neo_tz_t berlin = NEO_TZ_BERLIN;
            neo_local_time_t lt;
            if (neo_gps_local_time(d, &berlin, &lt))
                printf("Berlin %02u.%02u.%04u %02u:%02u:%02u %s | ", lt.t.day, lt.t.month, lt.t.year,
                       lt.t.hour, lt.t.minute, lt.t.second, lt.is_dst ? "MESZ" : "MEZ");
            printf("%04u-%02u-%02u %02u:%02u:%02u.%03u  fix=%u sv=%2u  %.7f %.7f  %.1f m  %.1f km/h  %.1f deg  hdop=%.2f  (unix %u)\n",
                   d->time.year, d->time.month, d->time.day, d->time.hour, d->time.minute,
                   d->time.second, d->time.ms, d->fix, d->num_sv,
                   neo_gps_lat_deg(&gps), neo_gps_lon_deg(&gps), neo_gps_alt_m(&gps),
                   neo_gps_speed_kmh(&gps), neo_gps_course_deg(&gps), d->hdop / 100.0,
                   neo_gps_unix_time(d));
        } else if (upd & NEO_UPD_REJECTED) {
            static const char *why[] = { "", "Fix", "Satelliten", "HDOP", "Genauigkeit", "Sprung" };
            printf("verworfen (%s): roh %.7f %.7f\n", why[gps.data.reject_reason],
                   gps.data.raw_lat * 1e-7, gps.data.raw_lon * 1e-7);
        } else if ((upd & NEO_UPD_FIX) && !neo_gps_has_fix(&gps)) {
            printf("warte auf Fix ... (%u Satelliten sichtbar)\n", gps.data.sats_in_view);
        }
    }
}
