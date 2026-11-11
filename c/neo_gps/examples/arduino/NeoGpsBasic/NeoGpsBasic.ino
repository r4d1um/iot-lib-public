/*
 * neo_gps - Arduino-Beispiel (ESP32, Mega, Uno mit SoftwareSerial, RP2040 ...)
 *
 * Verkabelung:  Modul TX -> MCU RX,  Modul RX -> MCU TX,  VCC -> 3.3V/5V,  GND -> GND
 * Werkszustand aller drei Module: 9600 Baud, NMEA.
 */
#include <neo_gps.h>
#include <neo_gps_time.h>

#if defined(ARDUINO_ARCH_ESP32)
  #define GPS_SERIAL Serial1
  #define GPS_BEGIN(b) Serial1.begin(b, SERIAL_8N1, 16 /*RX*/, 17 /*TX*/)
#elif defined(HAVE_HWSERIAL1)
  #define GPS_SERIAL Serial1
  #define GPS_BEGIN(b) Serial1.begin(b)
#else
  #include <SoftwareSerial.h>
  SoftwareSerial gpsSerial(4 /*RX*/, 3 /*TX*/);
  #define GPS_SERIAL gpsSerial
  #define GPS_BEGIN(b) gpsSerial.begin(b)
#endif

static neo_gps_t gps;
static const neo_tz_t berlin = NEO_TZ_BERLIN;   // MEZ/MESZ, Sommerzeit automatisch

static void gps_write(void *user, const uint8_t *data, uint16_t len)
{
  (void)user;
  GPS_SERIAL.write(data, len);
}

void setup()
{
  Serial.begin(115200);
  GPS_BEGIN(9600);
  neo_gps_init(&gps, gps_write, NULL);

  neo_gps_detect_model(&gps);                    // Antwort kommt asynchron in loop()
  neo_gps_cfg_output(&gps, NEO_OUT_NMEA_MINIMAL); // nur RMC/GGA/GSA -> weniger CPU-Last
  neo_gps_cfg_dynmodel(&gps, NEO_DYN_PEDESTRIAN);
  neo_gps_filter_preset(&gps, NEO_FILTER_PEDESTRIAN); // GPS-Spruenge verwerfen, im Stand glaetten
}

void loop()
{
  while (GPS_SERIAL.available()) {
    neo_event_t ev = neo_gps_feed(&gps, (uint8_t)GPS_SERIAL.read());
    if (ev == NEO_EV_UBX_MON_VER) {
      static const char *names[] = { "unbekannt", "NEO-6M", "NEO-7M", "NEO-M8N" };
      Serial.print(F("Modul: "));
      Serial.println(names[neo_gps_model(&gps)]);
    }
  }

  uint16_t upd = neo_gps_updates(&gps);
  if (upd & NEO_UPD_POSITION) {
    const neo_gps_data_t *d = neo_gps_data(&gps);
    neo_local_time_t lt;
    char buf[96];
    if (neo_gps_local_time(d, &berlin, &lt)) {
      snprintf(buf, sizeof(buf), "%02u.%02u.%04u %02u:%02u:%02u %s  ",
               lt.t.day, lt.t.month, lt.t.year, lt.t.hour, lt.t.minute, lt.t.second,
               lt.is_dst ? "MESZ" : "MEZ");
      Serial.print(buf);
    }
    // Ganzzahl-Ausgabe: exakt auch auf AVR (dort ist double nur 32 bit)
    snprintf(buf, sizeof(buf), "%s%ld.%07ld %s%ld.%07ld  sv=%u",
             d->lat < 0 ? "-" : "", labs(d->lat) / 10000000L, labs(d->lat) % 10000000L,
             d->lon < 0 ? "-" : "", labs(d->lon) / 10000000L, labs(d->lon) % 10000000L,
             d->num_sv);
    Serial.print(buf);
    Serial.print(F("  Hoehe ")); Serial.print(neo_gps_alt_m(&gps), 1);
    Serial.print(F(" m  ")); Serial.print(neo_gps_speed_kmh(&gps), 1);
    Serial.println(F(" km/h"));
  } else if (upd & NEO_UPD_REJECTED) {
    static const char *why[] = { "", "Fix", "Satelliten", "HDOP", "Genauigkeit", "Sprung" };
    Serial.print(F("Position verworfen: "));
    Serial.println(why[gps.data.reject_reason]);
  } else if ((upd & NEO_UPD_FIX) && !neo_gps_has_fix(&gps)) {
    Serial.println(F("Warte auf Fix ..."));
  }
}
