/*
 * neo_gps - schnellster Modus: 115200 Baud, nur UBX-Binaerdaten, 5-10 Hz.
 * Fuer Boards mit Hardware-Serial1 (ESP32, Mega, RP2040, STM32duino ...).
 *
 * NEO-6M: max. 5 Hz, NEO-7M / NEO-M8N: 10 Hz.
 */
#include <neo_gps.h>

#if defined(ARDUINO_ARCH_ESP32)
  #define GPS_BEGIN(b) Serial1.begin(b, SERIAL_8N1, 16, 17)
#else
  #define GPS_BEGIN(b) Serial1.begin(b)
#endif

static neo_gps_t gps;

static void gps_write(void *user, const uint8_t *data, uint16_t len)
{
  (void)user;
  Serial1.write(data, len);
  Serial1.flush();
}

/* Bytes verarbeiten, bis ein Event kommt oder Timeout */
static bool wait_event(neo_event_t want, uint32_t timeout_ms)
{
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    while (Serial1.available()) {
      neo_event_t e = neo_gps_feed(&gps, (uint8_t)Serial1.read());
      if (e == want) return true;
    }
  }
  return false;
}

void setup()
{
  Serial.begin(115200);
  neo_gps_init(&gps, gps_write, NULL);

  // Modul kann schon auf 115200 stehen (cfg gespeichert) - beide Raten probieren
  const uint32_t rates[] = { 9600, 115200 };
  for (uint8_t i = 0; i < 2 && neo_gps_model(&gps) == NEO_MODEL_UNKNOWN; i++) {
    GPS_BEGIN(rates[i]);
    neo_gps_detect_model(&gps);
    wait_event(NEO_EV_UBX_MON_VER, 1500);
  }
  Serial.print(F("Modell: "));
  Serial.println(neo_gps_model(&gps));   // 1=NEO-6, 2=NEO-7, 3=M8

  neo_gps_cfg_port(&gps, 115200, NEO_PROTO_UBX);
  delay(50);
  GPS_BEGIN(115200);

  neo_gps_cfg_output(&gps, NEO_OUT_UBX);
  neo_gps_cfg_rate(&gps, neo_gps_model(&gps) == NEO_MODEL_NEO6 ? 200 : 100);
  wait_event(NEO_EV_UBX_ACK, 500);
  Serial.println(neo_gps_ack_status(&gps) == NEO_ACK_OK ? F("Rate OK") : F("Rate: kein ACK"));

  neo_gps_cfg_dynmodel(&gps, NEO_DYN_AUTOMOTIVE);
  neo_gps_filter_preset(&gps, NEO_FILTER_AUTOMOTIVE);   // Sprungfilter (nutzt h_acc aus UBX)
  // neo_gps_cfg_save(&gps);   // dauerhaft speichern (wenn gewuenscht)
}

void loop()
{
  while (Serial1.available())
    neo_gps_feed(&gps, (uint8_t)Serial1.read());

  if (neo_gps_updates(&gps) & NEO_UPD_POSITION) {
    const neo_gps_data_t *d = neo_gps_data(&gps);
    Serial.print(d->time.second); Serial.print('.'); Serial.print(d->time.ms);
    Serial.print(F("  ")); Serial.print(neo_gps_lat_deg(&gps), 7);
    Serial.print(F(" "));  Serial.print(neo_gps_lon_deg(&gps), 7);
    Serial.print(F("  +/-")); Serial.print(d->h_acc / 1000.0f, 1);
    Serial.print(F(" m  ")); Serial.print(neo_gps_speed_kmh(&gps), 1);
    Serial.println(F(" km/h"));
  }
}
