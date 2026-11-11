# neo_gps

Schlanke, schnelle C-Library für die u-blox-GPS-Module

| Modul | Chip | Max. Rate | UBX-Navigation |
|---|---|---|---|
| GY-GPSV3-NEO | **NEO-M8N-0-01** (u-blox M8) | 10 Hz | NAV-PVT (92 Byte) |
| GY-GPSV3-NEO / HW-448 | **NEO-7M-0-000** (u-blox 7) | 10 Hz | NAV-PVT (84 Byte) |
| GY-NEO6MV2 u. ä. | **NEO-6M-0-001** (u-blox 6) | 5 Hz | POSLLH + VELNED + TIMEUTC + SOL |

Alle drei haben dieselben Anschlüsse (VCC, RX, TX, GND) und starten mit **9600 Baud, NMEA**.

## Warum diese Library

- **Zwei Dateien** (`neo_gps.c` + `neo_gps.h`), reines C99, kein `malloc`, kein `printf`/`scanf`.
- **Streaming-Parser**: jedes Byte wird sofort verarbeitet, kein Zeilenpuffer. NMEA und UBX dürfen im selben Datenstrom gemischt sein.
- **Kein Float im Parser**: Koordinaten in 1e-7 Grad, Höhe in mm, Geschwindigkeit in mm/s, verlustfrei und schnell auch auf AVR und Cortex-M0.
- **Werte werden erst bei korrekter Prüfsumme übernommen**, kaputte oder abgeschnittene Sätze ändern nichts.
- **Optionaler Sprungfilter** gegen Multipath-Ausreißer und Wandern im Stand, ebenfalls rein ganzzahlig.
- **Ortszeit mit Zeitzone und Sommerzeit** (z. B. Berlin MEZ/MESZ), ohne Zeitzonen-Datenbank.
- **Hardware-unabhängig**: du gibst Bytes rein und eine Sende-Funktion mit. Läuft auf Arduino, ESP32, STM32, RP2040, Linux …
- **UBX-Konfiguration** eingebaut: Baudrate, Messrate, Ausgabeprofile, Dynamikmodell, Power-Save, Speichern, Reset, automatische Modellerkennung.

| | ATmega328P | Cortex-M0 | Cortex-M4 |
|---|---|---|---|
| Flash (komplett) | 6,9 KB | 4,6 KB | 4,4 KB |
| Flash (nur NMEA, `NEO_GPS_ENABLE_UBX=0`) | 3,5 KB | – | – |
| + Sprungfilter (nur wenn benutzt) | +3,7 KB | +1,3 KB | +1,1 KB |
| + Ortszeit (nur wenn benutzt; + TZ-String-Parser) | +2,3 KB (+1,2 KB) | – | +0,8 KB (+0,6 KB) |
| RAM (`neo_gps_t`) | 324 Byte | 340 Byte | 340 Byte |

Auf einem PC parst sie rund 190 MB/s NMEA; das Modul liefert bei 115200 Baud höchstens 11,5 KB/s.

## Verkabelung

```
Modul TX  ->  MCU RX
Modul RX  ->  MCU TX     (bei 5-V-MCUs Spannungsteiler auf 3,3 V empfohlen)
VCC       ->  3,3 V oder 5 V (Boards haben einen Regler)
GND       ->  GND
```

## Schnellstart

```c
#include "neo_gps.h"

static neo_gps_t gps;

static void uart_tx(void *user, const uint8_t *data, uint16_t len)
{
    my_uart_write(data, len);          /* deine UART-Funktion */
}

int main(void)
{
    neo_gps_init(&gps, uart_tx, NULL); /* uart_tx darf NULL sein = nur Empfang */

    for (;;) {
        while (my_uart_available())
            neo_gps_feed(&gps, my_uart_read());

        if (neo_gps_updates(&gps) & NEO_UPD_POSITION) {
            const neo_gps_data_t *d = neo_gps_data(&gps);
            /* d->lat, d->lon     1e-7 Grad   (481173230 = 48.1173230°)
               d->alt_msl         mm
               d->speed           mm/s
               d->course          1e-5 Grad
               d->time.hour ...   UTC                                      */
        }
    }
}
```

Statt Polling kannst du auch auf den Rückgabewert von `neo_gps_feed()` reagieren (`NEO_EV_NMEA_RMC`, `NEO_EV_UBX_NAV_PVT`, …).

## Einbinden

Die Library liegt öffentlich in [`r4d1um/iot-lib-public`](https://github.com/r4d1um/iot-lib-public) unter `c/neo_gps`. GitHub Actions testet sie, baut alle Ziele einmal und veröffentlicht jede Version als Release **`c/neo_gps/v<version>`**. Projekte holen sich das fertige Paket und kopieren keinen Quellcode.

### Fertige Binaries (bevorzugt)

Pro Ziel gibt es `neo_gps-<version>-<ziel>.tar.gz` mit `lib/libneo_gps.a` und `include/`. Prüfsummen stehen in `SHA256SUMS` am selben Release.

| Ziel | Toolchain (aus [r4d1um/setup](https://github.com/r4d1um/setup) v1.2.0) | Flags | Für |
|---|---|---|---|
| `linux-x86_64` | gcc des Runners (Ubuntu 24.04) | `-O2 -fPIC` | PC |
| `linux-i686` | i686-linux-gnu 12.5.0, glibc 2.36 | `-O2 -fPIC` | 32-Bit-PC, Netbook |
| `linux-aarch64` | aarch64-linux-gnu 12.5.0, glibc 2.36 | `-O2 -fPIC` | Pi 3/4/5 mit 64-Bit-OS |
| `linux-armv6hf` | arm-linux-gnueabihf 12.5.0, glibc 2.36 | `-march=armv6+fp -mfpu=vfp -mfloat-abi=hard -marm` | alle Pi mit 32-Bit-OS (auch Pi 1 / Zero) |
| `avr-atmega328p` | avr-gcc 7.3.0 | `-Os -mmcu=atmega328p` | Arduino Uno/Nano/Pro Mini |
| `cortex-m0` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m0 -mthumb` | STM32F0 |
| `cortex-m0plus` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m0plus -mthumb` | RP2040 (Pico / Pico W) |
| `cortex-m3` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m3 -mthumb` | STM32F1 |
| `cortex-m4f` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16` | STM32F4 |
| `cortex-m33` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16` | RP2350 (Pico 2) |

Alle MCU-Ziele mit `-ffunction-sections -fdata-sections`; linke mit `-Wl,--gc-sections`, dann holt der Linker nur, was du benutzt. Die Float-ABI muss zu deinem Projekt passen (`cortex-m4f`/`m33` sind hard-float).

```sh
V=1.2.0 T=cortex-m0plus
curl -fsSLO https://github.com/r4d1um/iot-lib-public/releases/download/c/neo_gps/v$V/neo_gps-$V-$T.tar.gz
curl -fsSL  https://github.com/r4d1um/iot-lib-public/releases/download/c/neo_gps/v$V/SHA256SUMS | grep "$T.tar.gz" | sha256sum -c
tar -xzf neo_gps-$V-$T.tar.gz -C third_party/
# CMake: target_include_directories(app PRIVATE third_party/neo_gps-$V-$T/include)
#        target_link_libraries(app ${CMAKE_CURRENT_SOURCE_DIR}/third_party/neo_gps-$V-$T/lib/libneo_gps.a m)
# gcc:   -Ithird_party/neo_gps-$V-$T/include third_party/neo_gps-$V-$T/lib/libneo_gps.a -lm
```

### Quellpaket (für ESP32/ESP8266, PlatformIO, Arduino, ESP-IDF)

Für Xtensa (ESP32/ESP8266) gibt es noch kein Binary; dort und wenn du Compile-Schalter wie `NEO_GPS_ENABLE_UBX=0` änderst, nimmst du `neo_gps-<version>-src.tar.gz` vom selben Release:

| Umgebung | So geht's |
|---|---|
| PlatformIO | `lib_deps = https://github.com/r4d1um/iot-lib-public/releases/download/c/neo_gps/v1.2.0/neo_gps-1.2.0-src.tar.gz` |
| Arduino IDE / arduino-cli | Archiv nach `~/Arduino/libraries/neo_gps` entpacken |
| ESP-IDF | Archiv nach `components/neo_gps` entpacken (`CMakeLists.txt` erkennt IDF) |
| CMake (Pico-SDK, Zephyr, STM32CubeIDE) | entpacken, `add_subdirectory(neo_gps-1.2.0-src)` + `target_link_libraries(app neo_gps)` |

Version immer fest pinnen. `neo_gps_util.c/.h` (Entfernung, Peilung) braucht `math.h`; `neo_gps_time.c/.h` (Ortszeit, Unix-Zeit) kommt ohne aus.

### Selbst bauen

`../../tools/session-setup.sh` installiert die Cross-Toolchains. Danach `make test`, `make dist` (fehlende Compiler werden übersprungen, `DIST_STRICT=1` macht daraus einen Fehler), `make dist-check` linkt Tests und eine Firmware gegen die fertigen `.a`, `make package` erzeugt die Release-Pakete in `build/pkg/`.

## Empfohlene Konfiguration

**Einfach (NMEA, reicht für fast alles):**

```c
neo_gps_cfg_output(&gps, NEO_OUT_NMEA_MINIMAL);   /* nur RMC + GGA + GSA */
neo_gps_cfg_dynmodel(&gps, NEO_DYN_PEDESTRIAN);   /* oder AUTOMOTIVE, AIR_1G ... */
```

**Maximal effizient (UBX binär, 115200 Baud, 10 Hz):**

```c
neo_gps_detect_model(&gps);                        /* MON-VER, Antwort -> NEO_EV_UBX_MON_VER */
/* ... auf das Event warten ... */
neo_gps_cfg_port(&gps, 115200, NEO_PROTO_UBX);     /* danach eigene UART auf 115200 */
neo_gps_cfg_output(&gps, NEO_OUT_UBX);             /* wählt passende Nachrichten fürs Modell */
neo_gps_cfg_rate(&gps, 100);                       /* 10 Hz (NEO-6M: 200 = 5 Hz) */
neo_gps_cfg_save(&gps);                            /* optional dauerhaft speichern */
```

Im UBX-Modus bekommst du zusätzlich Genauigkeitsschätzungen (`h_acc`, `v_acc`, `s_acc` in mm bzw. mm/s), die NED-Geschwindigkeit (`vel_n/e/d`) und die GPS-Zeit (`itow`).

Hinweis zu `cfg_save`: Die NEO-6M/7M-Boards speichern im batteriegepufferten RAM bzw. EEPROM. Ist die Knopfzelle leer, startet das Modul wieder mit Werkseinstellungen.

## Sprungfilter (GPS-Jumps)

Einzelne Positionen springen gerne mal 20–200 m daneben, besonders zwischen Häusern (Multipath), unter Bäumen oder direkt nach dem Fix. Der Filter ist standardmäßig aus und wird mit einer Zeile aktiviert:

```c
neo_gps_filter_preset(&gps, NEO_FILTER_PEDESTRIAN);   /* AUTOMOTIVE, AIRBORNE, STATIONARY, OFF */
neo_gps_cfg_dynmodel(&gps, NEO_DYN_PEDESTRIAN);       /* passend dazu im Modul (empfohlen)  */
```

Danach sind `data.lat`/`data.lon` die gefilterte Position, die Rohwerte stehen in `data.raw_lat`/`data.raw_lon`. Eine verworfene Position setzt `NEO_UPD_REJECTED` statt `NEO_UPD_POSITION`; `data.reject_reason` sagt warum, `data.rejected` zählt mit. Bei verworfener Position wird auch die Höhe nicht übernommen.

So arbeitet er, einmal pro Messung:

1. **Qualität**: gültiger Fix, Fix-Typ, Satellitenzahl, HDOP und (im UBX-Modus) die Genauigkeitsschätzung `h_acc` des Moduls.
2. **Sprungtest**: Die Strecke seit der letzten guten Position wird mit dem verglichen, was physikalisch möglich ist: 1,5 × Geschwindigkeit × Zeit + max. Beschleunigung × Zeit² + Toleranz + 2 × `h_acc`. Die Geschwindigkeit misst das Modul per Doppler, unabhängig von der Position, deshalb fällt ein Sprung auch während der Fahrt auf. Nach einer längeren Lücke (Tunnel) wächst die erlaubte Strecke automatisch mit.
3. **Neu verankern**: Kommen mehrmals hintereinander „Sprünge“ zur selben Stelle, war die alte Position falsch. Der Filter übernimmt dann die neue, statt ewig festzuhängen.
4. **Glätten im Stand**: Unterhalb der Stillstandsgeschwindigkeit wird die Position exponentiell geglättet, das typische Wandern um ein paar Meter verschwindet. In Bewegung wird nicht geglättet, es entsteht also keine Verzögerung.

| Preset | min. Satelliten | max. HDOP | max. `h_acc` | Beschleunigung | Toleranz | neu verankern | Glättung im Stand |
|---|---|---|---|---|---|---|---|
| `PEDESTRIAN` | 5 | 4,0 | 25 m | 3 m/s² | 10 m | nach 5 | 1/4, unter 0,3 m/s |
| `AUTOMOTIVE` | 5 | 5,0 | 30 m | 8 m/s² | 15 m | nach 5 | 1/4, unter 0,5 m/s |
| `AIRBORNE` | 6 | 3,0 | 20 m | 20 m/s² | 10 m | nach 3 | 1/2, unter 0,2 m/s |
| `STATIONARY` | 6 | 3,0 | 15 m | 1 m/s² | 5 m | nach 10 | 1/16, unter 1 m/s |

Alle Presets verlangen einen 3D-Fix. Eigene Werte gehen mit `neo_gps_filter_config()`; jeder Grenzwert `0` schaltet die jeweilige Prüfung ab:

```c
neo_filter_cfg_t f = {
    .min_fix = NEO_FIX_3D, .min_sv = 6, .max_hdop = 250, .max_h_acc = 8000,
    .max_accel = 4000, .margin = 5000, .reanchor = 5,
    .smooth_shift = 3, .static_speed = 300,
};
neo_gps_filter_config(&gps, &f);
neo_gps_filter_reset(&gps);   /* Referenz vergessen, z.B. nach Transport im ausgeschalteten Zustand */
```

Der Filter steckt in `neo_gps_filter.c` und wird nur gelinkt, wenn du `neo_gps_filter_*` aufrufst. Mit `NEO_GPS_ENABLE_FILTER=0` entfallen auch die Felder im RAM.

## Ortszeit (Zeitzone + Sommerzeit)

Das GPS liefert UTC. `neo_gps_time.h` rechnet daraus die Ortszeit, Sommerzeit inklusive:

```c
#include "neo_gps_time.h"

static const neo_tz_t berlin = NEO_TZ_BERLIN;      /* MEZ / MESZ */

neo_local_time_t lt;
if (neo_gps_local_time(neo_gps_data(&gps), &berlin, &lt)) {
    /* lt.t.year, .month, .day, .hour, .minute, .second, .ms   -> Ortszeit
       lt.weekday  0 = Sonntag
       lt.is_dst   1 = Sommerzeit (MESZ)
       lt.offset_min  60 bzw. 120                                         */
}
```

Für Berlin gilt die EU-Regel: Sommerzeit vom letzten Sonntag im März 02:00 MEZ bis zum letzten Sonntag im Oktober 03:00 MESZ. Fertig dabei sind `NEO_TZ_BERLIN` (= `NEO_TZ_CET`, gilt für ganz Mitteleuropa), `NEO_TZ_LONDON`, `NEO_TZ_HELSINKI` und `NEO_TZ_UTC`.

Jede andere Zeitzone geht über den üblichen POSIX-TZ-String, denselben wie bei Linux oder dem ESP32:

```c
neo_tz_t tz;
neo_tz_parse(&tz, "EST5EDT,M3.2.0,M11.1.0");        /* New York */
neo_tz_parse(&tz, "AEST-10AEDT,M10.1.0,M4.1.0/3");  /* Sydney (Sommerzeit über Neujahr) */
neo_tz_parse(&tz, "IST-5:30");                      /* Indien, keine Sommerzeit */
```

Unterstützt werden Regeln im Format `Mm.w.d[/Uhrzeit]`, also fast alle Zonen. Namen aus der Zeitzonen-Datenbank wie `Europe/Berlin` gehen nicht; den passenden POSIX-String findest du zu jeder Zone in `/usr/share/zoneinfo` (letzte Zeile der Datei) oder in den gängigen Tabellen für ESP32/`setenv("TZ", …)`.

Gerechnet wird rein ganzzahlig und ohne libc-`time.h`, gültig für 1970–2105. Die Tests vergleichen die Umrechnung für acht Zonen, darunter Halbstunden-Sommerzeit, Umschalten um „24:00“ und negative Umschaltzeiten, mit der glibc über 1971–2105, minutengenau um jede Umschaltung.

Bausteine für eigene Zwecke: `neo_gps_unix_time()`, `neo_tz_utc_to_local()`, `neo_time_from_unix()`, `neo_time_to_unix()`.

Hinweis: Direkt nach einem Kaltstart kennt das Modul die aktuellen Schaltsekunden noch nicht und nimmt den Wert aus seiner Firmware. Bei älteren Firmwares (vor allem NEO-6M) kann die UTC-Zeit dann einige Sekunden danebenliegen, bis nach spätestens rund 12,5 Minuten die UTC-Parameter empfangen sind.

## API-Übersicht

### Kern

| Funktion | |
|---|---|
| `neo_gps_init(gps, write, user)` | Initialisieren |
| `neo_gps_feed(gps, byte)` | Ein Byte verarbeiten → `neo_event_t` (ISR-tauglich) |
| `neo_gps_feed_buf(gps, buf, len)` | Puffer verarbeiten → Anzahl gültiger Nachrichten |
| `neo_gps_updates(gps)` | `NEO_UPD_*`-Bits seit letztem Aufruf (setzt zurück) |
| `neo_gps_data(gps)` | Zeiger auf `neo_gps_data_t` |
| `neo_gps_has_fix(gps)` | Position gültig? |
| `neo_gps_lat_deg/lon_deg/alt_m/speed_kmh/course_deg` | Float-Komfortfunktionen |
| `neo_gps_set_ubx_handler(gps, fn)` | Callback für jede UBX-Nachricht (eigene Erweiterungen) |
| `neo_gps_filter_preset(gps, preset)` | Sprungfilter mit Preset an/aus |
| `neo_gps_filter_config(gps, &cfg)` | Sprungfilter mit eigenen Werten |
| `neo_gps_filter_reset(gps)` | Filter-Referenz vergessen |
| `neo_gps_local_time(data, &tz, &lt)` | Ortszeit mit Sommerzeit (`neo_gps_time.h`) |
| `neo_tz_parse(&tz, "CET-1CEST,…")` | Zeitzone aus POSIX-TZ-String |

### Konfiguration (UBX)

| Funktion | UBX | |
|---|---|---|
| `neo_gps_detect_model` | MON-VER | erkennt NEO-6 / NEO-7 / M8 |
| `neo_gps_cfg_port(baud, proto)` | CFG-PRT | Baudrate + Ausgabeprotokolle |
| `neo_gps_cfg_rate(ms)` | CFG-RATE | Messintervall |
| `neo_gps_cfg_msg(cls, id, rate)` | CFG-MSG | einzelne Nachricht an/aus |
| `neo_gps_cfg_output(profil)` | CFG-MSG | `NMEA_DEFAULT`, `NMEA_MINIMAL`, `UBX` |
| `neo_gps_cfg_dynmodel(modell)` | CFG-NAV5 | Portable, Stationary, Pedestrian, Automotive, Sea, Air |
| `neo_gps_cfg_power_save(on)` | CFG-RXM | Energiesparmodus |
| `neo_gps_cfg_save()` | CFG-CFG | Konfiguration speichern |
| `neo_gps_cfg_factory_reset()` | CFG-CFG | Werkseinstellungen |
| `neo_gps_reset(HOT/WARM/COLD)` | CFG-RST | GNSS-Neustart |
| `neo_gps_ack_status()` | ACK | `PENDING`, `OK`, `NAK` der letzten CFG-Nachricht |
| `neo_gps_ubx_send / _ubx_poll` | – | beliebige UBX-Nachricht |

Alle Befehle sind nicht blockierend. Die Bestätigung kommt als `NEO_EV_UBX_ACK`/`NEO_EV_UBX_NAK` über `neo_gps_feed()`.

### Ausgewertete Nachrichten

- **NMEA** (alle Talker: GP, GN, GL, GA, GB): RMC, GGA, GSA, VTG, GSV (Anzahl sichtbarer Satelliten)
- **UBX**: NAV-PVT, NAV-POSLLH, NAV-VELNED, NAV-TIMEUTC, NAV-SOL, NAV-DOP, ACK-ACK/NAK, MON-VER

### Compile-Time-Optionen

| Makro | Standard | |
|---|---|---|
| `NEO_GPS_ENABLE_NMEA` | 1 | NMEA-Parser |
| `NEO_GPS_ENABLE_UBX` | 1 | UBX-Parser + Konfiguration |
| `NEO_GPS_ENABLE_FILTER` | 1 | Sprungfilter (Felder + `neo_gps_filter.c`) |
| `NEO_GPS_UBX_MAX_PAYLOAD` | 92 | Puffer für UBX-Payload |
| `NEO_GPS_NMEA_FIELD_MAX` | 15 | max. Zeichen pro NMEA-Feld |

## Beispiele

- `examples/arduino/NeoGpsBasic`: NMEA, läuft auch auf dem Uno (SoftwareSerial)
- `examples/arduino/NeoGpsUbx10Hz`: UBX-Binärmodus mit 115200 Baud und 5–10 Hz
- `examples/stm32_hal`: Interrupt + Ringpuffer
- `examples/linux`: USB-Seriell-Adapter oder Raspberry Pi: `make linux && ./build/neo_gps_linux /dev/ttyUSB0 9600`
- `examples/simulation`: Gelände-Simulation mit 4 Sensoren (Fläche und Steigung, 100 × 24 h durch Parser und Sprungfilter): `make sim` schreibt `build/sim.json`

## Tests

```sh
make test     # 266 Checks: NMEA, UBX, Fehler, Resync, Senden, Modellerkennung, Filter, Zeitzonen
make bench    # Durchsatz
```

Die CI baut zusätzlich mit Sanitizern sowie für AVR und Cortex-M0 mit `-Werror`.

## Lizenz

MIT
