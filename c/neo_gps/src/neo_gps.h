/*
 * neo_gps - kleine, schnelle C-Library fuer u-blox NEO-6M / NEO-7M / NEO-M8N
 *
 *  - reines C99, keine dynamische Speicherverwaltung, kein printf/scanf
 *  - Byte-weiser Streaming-Parser fuer NMEA *und* UBX (gemischt im selben Strom)
 *  - nur Ganzzahl-Arithmetik (Fixed-Point) im Parser -> schnell auch auf AVR/Cortex-M0
 *  - hardware-unabhaengig: du lieferst die empfangenen Bytes und eine Sende-Funktion
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NEO_GPS_H
#define NEO_GPS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEO_GPS_VERSION "1.2.0"

/* ------------------------------------------------------------------------- */
/* Compile-Time-Konfiguration (vor dem #include oder per -D ueberschreiben)   */
/* ------------------------------------------------------------------------- */

#ifndef NEO_GPS_ENABLE_NMEA
#define NEO_GPS_ENABLE_NMEA 1        /* NMEA-Parser (RMC, GGA, GSA, VTG, GSV) */
#endif
#ifndef NEO_GPS_ENABLE_UBX
#define NEO_GPS_ENABLE_UBX 1         /* UBX-Parser + Konfigurationsbefehle   */
#endif
#ifndef NEO_GPS_UBX_MAX_PAYLOAD
#define NEO_GPS_UBX_MAX_PAYLOAD 92   /* NAV-PVT (M8) = 92 Byte; laengere Nachrichten werden
                                        abgeschnitten gespeichert, aber voll geprueft */
#endif
#ifndef NEO_GPS_UBX_LEN_LIMIT
#define NEO_GPS_UBX_LEN_LIMIT 1024   /* groessere Laengenangaben gelten als Datenmuell */
#endif
#ifndef NEO_GPS_ENABLE_FILTER
#define NEO_GPS_ENABLE_FILTER 1      /* Sprung-/Qualitaetsfilter fuer die Position */
#endif
#ifndef NEO_GPS_NMEA_FIELD_MAX
#define NEO_GPS_NMEA_FIELD_MAX 15    /* max. Zeichen pro NMEA-Feld, das ausgewertet wird */
#endif

/* ------------------------------------------------------------------------- */
/* Typen                                                                     */
/* ------------------------------------------------------------------------- */

/* Fix-Typ, identisch zu u-blox gpsFix/fixType */
typedef enum {
    NEO_FIX_NONE      = 0,
    NEO_FIX_DR        = 1,  /* nur Dead Reckoning */
    NEO_FIX_2D        = 2,
    NEO_FIX_3D        = 3,
    NEO_FIX_GNSS_DR   = 4,
    NEO_FIX_TIME_ONLY = 5
} neo_fix_t;

typedef enum {
    NEO_MODEL_UNKNOWN = 0,
    NEO_MODEL_NEO6,         /* u-blox 6  (NEO-6M-0-001)  - kein NAV-PVT, max 5 Hz  */
    NEO_MODEL_NEO7,         /* u-blox 7  (NEO-7M-0-000)  - NAV-PVT (84 B), max 10 Hz */
    NEO_MODEL_M8            /* u-blox M8 (NEO-M8N-0-01)  - NAV-PVT (92 B), max 10 Hz */
} neo_model_t;

/* Rueckgabe von neo_gps_feed(): welche Nachricht gerade fertig wurde */
typedef enum {
    NEO_EV_NONE = 0,
    NEO_EV_NMEA_RMC,
    NEO_EV_NMEA_GGA,
    NEO_EV_NMEA_GSA,
    NEO_EV_NMEA_VTG,
    NEO_EV_NMEA_GSV,
    NEO_EV_UBX_NAV_PVT,
    NEO_EV_UBX_NAV_POSLLH,
    NEO_EV_UBX_NAV_VELNED,
    NEO_EV_UBX_NAV_TIMEUTC,
    NEO_EV_UBX_NAV_SOL,
    NEO_EV_UBX_NAV_DOP,
    NEO_EV_UBX_ACK,
    NEO_EV_UBX_NAK,
    NEO_EV_UBX_MON_VER,
    NEO_EV_UBX_OTHER,       /* gueltige UBX-Nachricht ohne eingebauten Handler */
    NEO_EV_CHECKSUM_ERROR
} neo_event_t;

/* Bits in data.updated - was hat sich seit dem letzten neo_gps_updates() geaendert */
#define NEO_UPD_POSITION  0x0001u
#define NEO_UPD_ALTITUDE  0x0002u
#define NEO_UPD_TIME      0x0004u
#define NEO_UPD_DATE      0x0008u
#define NEO_UPD_SPEED     0x0010u
#define NEO_UPD_COURSE    0x0020u
#define NEO_UPD_FIX       0x0040u
#define NEO_UPD_DOP       0x0080u
#define NEO_UPD_SATS      0x0100u
#define NEO_UPD_ACCURACY  0x0200u   /* nur UBX */
#define NEO_UPD_VELNED    0x0400u   /* nur UBX */
#define NEO_UPD_REJECTED  0x0800u   /* Filter hat eine Position verworfen (siehe reject_reason) */

/* Grund fuer die zuletzt verworfene Position (data.reject_reason) */
typedef enum {
    NEO_REJECT_NONE = 0,
    NEO_REJECT_FIX,         /* Fix-Typ zu schlecht            */
    NEO_REJECT_SATS,        /* zu wenige Satelliten           */
    NEO_REJECT_HDOP,        /* HDOP zu gross                  */
    NEO_REJECT_ACCURACY,    /* h_acc zu gross (nur UBX)       */
    NEO_REJECT_JUMP         /* unplausibler Positionssprung   */
} neo_reject_t;

typedef struct {
    uint16_t year;          /* z.B. 2026 */
    uint8_t  month, day;
    uint8_t  hour, minute, second;
    uint16_t ms;
} neo_datetime_t;

/* Alle Werte als Ganzzahlen - verlustfrei und ohne Float-Kosten */
typedef struct {
    int32_t  lat;           /* Breite  in 1e-7 Grad  (481173230 = 48.1173230 N) */
    int32_t  lon;           /* Laenge  in 1e-7 Grad  (negativ = West)          */
    int32_t  alt_msl;       /* Hoehe ueber Meeresspiegel in mm                  */
    int32_t  geoid_sep;     /* Ellipsoid - MSL in mm                            */
    uint32_t speed;         /* Geschwindigkeit ueber Grund in mm/s              */
    int32_t  course;        /* Kurs ueber Grund in 1e-5 Grad (0..36000000)       */
    int32_t  vel_n, vel_e, vel_d; /* NED-Geschwindigkeit mm/s (nur UBX)          */
    uint32_t h_acc, v_acc;  /* Genauigkeitsschaetzung in mm (nur UBX)           */
    uint32_t s_acc;         /* Geschwindigkeitsgenauigkeit mm/s (nur UBX)       */
    uint32_t itow;          /* GPS Time of Week in ms (nur UBX)                 */
    neo_datetime_t time;    /* UTC                                              */
    uint16_t hdop, vdop, pdop; /* in 0.01 (101 = 1.01)                          */
    uint8_t  fix;           /* neo_fix_t                                        */
    uint8_t  valid;         /* 1 = Position gueltig                             */
    uint8_t  time_valid;    /* 1 = Uhrzeit gueltig                              */
    uint8_t  date_valid;    /* 1 = Datum gueltig                                */
    uint8_t  num_sv;        /* Satelliten in der Loesung                        */
    uint8_t  sats_in_view;  /* sichtbare Satelliten (aus GSV)                   */
    uint8_t  quality;       /* GGA-Qualitaet: 0 kein Fix, 1 GPS, 2 DGPS, 6 DR    */
    uint16_t updated;       /* NEO_UPD_* Bits                                   */
#if NEO_GPS_ENABLE_FILTER
    /* Bei aktivem Filter sind lat/lon die gefilterte Position, hier die Rohwerte: */
    int32_t  raw_lat, raw_lon;
    uint16_t rejected;      /* Anzahl verworfener Positionen (laeuft ueber)    */
    uint8_t  reject_reason; /* neo_reject_t der zuletzt verworfenen Position   */
#endif
} neo_gps_data_t;

#if NEO_GPS_ENABLE_FILTER
/* Filter-Einstellungen. Jeder Grenzwert = 0 schaltet die jeweilige Pruefung ab. */
typedef struct {
    uint8_t  min_fix;       /* z.B. NEO_FIX_3D (wird ignoriert, wenn kein GSA/UBX-Fixtyp kommt) */
    uint8_t  min_sv;        /* min. Satelliten in der Loesung                   */
    uint16_t max_hdop;      /* in 0.01 (400 = 4.0)                               */
    uint32_t max_h_acc;     /* mm, nur UBX                                       */
    uint32_t max_accel;     /* mm/s^2 - erlaubte Beschleunigung fuer Sprungtest; 0 = kein Sprungtest */
    uint32_t margin;        /* mm  - feste Toleranz im Sprungtest (+ 2 x h_acc)   */
    uint8_t  reanchor;      /* nach so vielen Spruengen in Folge neue Position uebernehmen */
    uint8_t  smooth_shift;  /* Glaettung im Stand: Neuanteil 1/2^n (2 = 25 %), 0 = aus */
    uint16_t static_speed;  /* mm/s - darunter gilt "Stillstand" (Glaettung aktiv) */
} neo_filter_cfg_t;

typedef enum {
    NEO_FILTER_OFF = 0,
    NEO_FILTER_PEDESTRIAN,
    NEO_FILTER_AUTOMOTIVE,
    NEO_FILTER_AIRBORNE,    /* Drohne, Flugzeug                           */
    NEO_FILTER_STATIONARY   /* fest montiert: starke Glaettung, kaum Bewegung */
} neo_filter_preset_t;
#endif

/* Sende-Callback: schreibt len Bytes an die UART des Moduls (RX-Pin des Moduls) */
typedef void (*neo_write_fn)(void *user, const uint8_t *data, uint16_t len);

/* Optionaler Callback fuer jede gueltige UBX-Nachricht (Erweiterungen ohne die Library
 * anzufassen). len = gespeicherte Laenge (max. NEO_GPS_UBX_MAX_PAYLOAD). */
typedef void (*neo_ubx_handler_fn)(void *user, uint8_t cls, uint8_t id,
                                   const uint8_t *payload, uint16_t len);

typedef enum { NEO_ACK_NONE = 0, NEO_ACK_PENDING, NEO_ACK_OK, NEO_ACK_NAK } neo_ack_t;

#define NEO_GPS_TIME_UNKNOWN 0xFFFFFFFFUL   /* intern */

struct neo_gps;
typedef uint8_t (*neo_pos_filter_fn)(struct neo_gps *gps, int32_t lat, int32_t lon,
                                     uint32_t t_ms, uint32_t period_ms);

/* Gesamter Zustand - kein malloc, einfach als globale/statische Variable anlegen. */
typedef struct neo_gps {
    neo_gps_data_t data;

    /* Statistik */
    uint32_t msgs_ok;
    uint32_t msgs_err;

    /* --- intern --- */
    neo_write_fn       write;
    neo_ubx_handler_fn ubx_handler;
    void              *user;
    uint8_t  model;          /* neo_model_t */
    uint8_t  state;
    uint8_t  ack_state, ack_cls, ack_id;
#if NEO_GPS_ENABLE_NMEA
    uint8_t  n_chk, n_rx_chk, n_field, n_flen, n_type, n_talker, n_len;
    char     n_buf[NEO_GPS_NMEA_FIELD_MAX + 1];
    uint8_t  gsv_view[4];
    struct {
        uint32_t time_raw, date_raw, speed_raw;
        int32_t  lat, lon, alt, sep, course;
        uint16_t hdop, pdop, vdop, have;
        uint8_t  num_sv, quality, nav_mode, status, mode, in_view;
    } n;
#endif
#if NEO_GPS_ENABLE_FILTER
    neo_pos_filter_fn f_fn;          /* NULL = Filter aus */
    neo_filter_cfg_t f_cfg;
    int32_t  f_lat, f_lon;          /* Referenz = letzte akzeptierte Position */
    uint32_t f_t, f_speed;          /* deren Zeit (ms) und Geschwindigkeit    */
    uint32_t f_epoch;               /* Zeit der zuletzt bewerteten Epoche     */
    uint8_t  f_have, f_epoch_ok, f_epoch_set, f_jumps;
#endif
#if NEO_GPS_ENABLE_UBX
    uint8_t  u_cls, u_id, u_cka, u_ckb;
    uint16_t u_len, u_idx;
    uint8_t  u_buf[NEO_GPS_UBX_MAX_PAYLOAD];
#endif
} neo_gps_t;

/* ------------------------------------------------------------------------- */
/* Kern-API                                                                  */
/* ------------------------------------------------------------------------- */

/* Initialisiert die Struktur. write darf NULL sein (dann nur Empfang). */
void neo_gps_init(neo_gps_t *gps, neo_write_fn write, void *user);

/* Ein empfangenes Byte verarbeiten. Gibt das Event zurueck, wenn eine Nachricht
 * vollstaendig und gueltig empfangen wurde, sonst NEO_EV_NONE.
 * Kann direkt aus einer UART-ISR aufgerufen werden (konstante, sehr kurze Laufzeit). */
neo_event_t neo_gps_feed(neo_gps_t *gps, uint8_t byte);

/* Mehrere Bytes verarbeiten. Gibt die Anzahl gueltiger Nachrichten zurueck. */
uint16_t neo_gps_feed_buf(neo_gps_t *gps, const uint8_t *buf, uint16_t len);

/* Liefert die Update-Bits seit dem letzten Aufruf und setzt sie zurueck. */
uint16_t neo_gps_updates(neo_gps_t *gps);

#if NEO_GPS_ENABLE_FILTER
/* Positionsfilter gegen GPS-Spruenge (standardmaessig aus).
 * Pruefungen: Fix-Typ, Satelliten, HDOP, h_acc und ein Sprungtest, der die Distanz zur
 * letzten guten Position mit der (Doppler-)Geschwindigkeit des Moduls vergleicht.
 * Im Stillstand wird zusaetzlich geglaettet. Rein ganzzahlig, laeuft nur 1x pro Epoche.
 * Ergebnis: data.lat/lon = gefiltert, data.raw_lat/raw_lon = roh. */
void neo_gps_filter_preset(neo_gps_t *gps, neo_filter_preset_t preset);
void neo_gps_filter_config(neo_gps_t *gps, const neo_filter_cfg_t *cfg);
void neo_gps_filter_reset(neo_gps_t *gps);   /* Referenz vergessen (z.B. nach Ortswechsel) */
#endif

/* Bequemer Zugriff */
static inline const neo_gps_data_t *neo_gps_data(const neo_gps_t *gps) { return &gps->data; }
static inline int neo_gps_has_fix(const neo_gps_t *gps) { return gps->data.valid != 0; }
static inline neo_model_t neo_gps_model(const neo_gps_t *gps) { return (neo_model_t)gps->model; }
static inline void neo_gps_set_model(neo_gps_t *gps, neo_model_t m) { gps->model = (uint8_t)m; }
static inline void neo_gps_set_ubx_handler(neo_gps_t *gps, neo_ubx_handler_fn fn) { gps->ubx_handler = fn; }

/* Float-Komfort (Achtung: auf AVR ist double nur 32 bit -> ~1 m Aufloesung) */
static inline double neo_gps_lat_deg(const neo_gps_t *gps) { return (double)gps->data.lat * 1e-7; }
static inline double neo_gps_lon_deg(const neo_gps_t *gps) { return (double)gps->data.lon * 1e-7; }
static inline float  neo_gps_alt_m(const neo_gps_t *gps)   { return (float)gps->data.alt_msl * 1e-3f; }
static inline float  neo_gps_speed_kmh(const neo_gps_t *gps) { return (float)gps->data.speed * 0.0036f; }
static inline float  neo_gps_course_deg(const neo_gps_t *gps) { return (float)gps->data.course * 1e-5f; }

/* ------------------------------------------------------------------------- */
/* UBX-Konfiguration (alle Befehle funktionieren auf NEO-6M, NEO-7M, NEO-M8N) */
/* Alle Funktionen senden sofort und blockieren nicht. Bestaetigung per       */
/* neo_gps_ack_status().                                                      */
/* ------------------------------------------------------------------------- */
#if NEO_GPS_ENABLE_UBX

#define NEO_PROTO_UBX   0x01u
#define NEO_PROTO_NMEA  0x02u

typedef enum {
    NEO_DYN_PORTABLE   = 0,
    NEO_DYN_STATIONARY = 2,
    NEO_DYN_PEDESTRIAN = 3,
    NEO_DYN_AUTOMOTIVE = 4,
    NEO_DYN_SEA        = 5,
    NEO_DYN_AIR_1G     = 6,
    NEO_DYN_AIR_2G     = 7,
    NEO_DYN_AIR_4G     = 8
} neo_dyn_model_t;

typedef enum {
    NEO_OUT_NMEA_DEFAULT = 0, /* Werkszustand: GGA GLL GSA GSV RMC VTG          */
    NEO_OUT_NMEA_MINIMAL,     /* nur RMC + GGA + GSA  (spart ~70 % Datenrate)   */
    NEO_OUT_UBX               /* NMEA aus, binaer UBX (effizientester Modus)    */
} neo_output_t;

typedef enum { NEO_START_HOT = 0, NEO_START_WARM, NEO_START_COLD } neo_start_t;

/* Beliebige UBX-Nachricht senden */
void neo_gps_ubx_send(neo_gps_t *gps, uint8_t cls, uint8_t id,
                      const uint8_t *payload, uint16_t len);
/* Poll-Anfrage (leere Payload) */
static inline void neo_gps_ubx_poll(neo_gps_t *gps, uint8_t cls, uint8_t id)
{ neo_gps_ubx_send(gps, cls, id, NULL, 0); }

/* MON-VER abfragen -> setzt Modell automatisch (NEO_EV_UBX_MON_VER) */
void neo_gps_detect_model(neo_gps_t *gps);

/* UART1 des Moduls: Baudrate + Ausgabeprotokolle (NEO_PROTO_*).
 * Danach die eigene UART auf die neue Baudrate umstellen! */
void neo_gps_cfg_port(neo_gps_t *gps, uint32_t baud, uint8_t out_proto);

/* Messrate in ms (1000 = 1 Hz, 200 = 5 Hz, 100 = 10 Hz). NEO-6: max 5 Hz. */
void neo_gps_cfg_rate(neo_gps_t *gps, uint16_t meas_ms);

/* Ausgaberate einer Nachricht (0 = aus, 1 = jede Messung, n = jede n-te) */
void neo_gps_cfg_msg(neo_gps_t *gps, uint8_t cls, uint8_t id, uint8_t rate);

/* Vorgefertigte Ausgabeprofile (mehrere CFG-MSG hintereinander).
 * Fuer NEO_OUT_UBX muss das Modell bekannt sein (neo_gps_detect_model oder _set_model):
 * NEO-6 bekommt POSLLH+VELNED+TIMEUTC+SOL+DOP, NEO-7/M8 bekommt NAV-PVT+DOP. */
void neo_gps_cfg_output(neo_gps_t *gps, neo_output_t out);

/* Dynamikmodell (CFG-NAV5) */
void neo_gps_cfg_dynmodel(neo_gps_t *gps, neo_dyn_model_t model);

/* Energiesparmodus (CFG-RXM): 0 = volle Leistung, 1 = Power Save */
void neo_gps_cfg_power_save(neo_gps_t *gps, uint8_t enable);

/* Aktuelle Konfiguration dauerhaft speichern (BBR/Flash/EEPROM) */
void neo_gps_cfg_save(neo_gps_t *gps);

/* Werkseinstellungen laden + speichern */
void neo_gps_cfg_factory_reset(neo_gps_t *gps);

/* GNSS-Neustart (Hot/Warm/Cold) */
void neo_gps_reset(neo_gps_t *gps, neo_start_t type);

/* Status der letzten gesendeten CFG-Nachricht */
neo_ack_t neo_gps_ack_status(const neo_gps_t *gps);

#endif /* NEO_GPS_ENABLE_UBX */

#ifdef __cplusplus
}
#endif

#endif /* NEO_GPS_H */
