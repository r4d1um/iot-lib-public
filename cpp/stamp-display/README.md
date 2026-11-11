# StampDisplay

Treiber und Grafik für das 1,77"-TFT-Modul mit ST7735S (160 × 128, RGB565, SPI), dazu Panels für 1-Bit-Displays (SSD1306-Layout) und für Bilder im Speicher. Text in festen 6×8-Feldern überschreibt seinen alten Inhalt, deshalb bleiben beim Aktualisieren keine Pixelreste stehen.

## Aufbau

| Teil | Dateien | Ausgeliefert als |
|---|---|---|
| Kern ohne Arduino: `Gfx`, `Panel`, `BandPanel`, `MonoPanel`, `FramebufferPanel` (nur PC), ST7735S-Protokoll `St7735sPanel` + Schnittstelle `St7735sBus` | `Gfx.cpp`, `St7735sPanel.cpp`, Header | vorkompiliert: `src/<mcu>/libStampDisplay.a` |
| Arduino-Adapter: `St7735s` (SPI, CS, DC, RESET, Hintergrundlicht) | `St7735s.cpp/.h` | Quelle, wird mit dem Board-Core übersetzt |

Der Adapter ist dünn und hängt von der Core-Version ab. Der Kern hängt von keiner Core-Version ab, deshalb passt dieselbe `.a` zu ESP8266-Core 3.1.2 und zu ESP32-Core 2.0.17 und 3.x.

## Plattformen

| Ziel | `.a` gebaut mit (aus [r4d1um/setup](https://github.com/r4d1um/setup) v1.3.0) | Flags | getestet mit |
|---|---|---|---|
| `esp8266` | xtensa-lx106-elf GCC 10.3 (Compiler von Core 3.1.2) | `-Os -mlongcalls -mtext-section-literals -falign-functions=4` | Core 3.1.2, NodeMCU v2 / D1 mini |
| `esp32` | xtensa-esp32-elf GCC 8.4 (Compiler von Core 2.0.17) | `-Os -mlongcalls -fstrict-volatile-bitfields` | Core 2.0.17 und 3.x |
| `linux-x86_64` | gcc des Runners | `-O2 -fPIC` | PC-Tests, Screenshot-Renderer |

Alle Ziele: `-std=gnu++17 -fno-exceptions -ffunction-sections -fdata-sections`, mit RTTI. Andere Arduino-Architekturen (AVR, RP2040) werden noch nicht unterstützt (`architectures=esp8266,esp32`).

## Einbinden

Jede Version ist ein Release `cpp/stamp-display/v<version>` mit:

- `StampDisplay-<v>-arduino.zip`: Arduino-Library (`precompiled=true`) für arduino-cli, die Arduino IDE und den Netbook-Builder
- `StampDisplay-<v>-<ziel>.tar.gz`: `include/` + `lib/libStampDisplay.a` für Builds ohne Arduino
- `SHA256SUMS`

```sh
V=1.1.0
base=https://github.com/r4d1um/iot-lib-public/releases/download/cpp/stamp-display/v$V
curl -fsSLO $base/StampDisplay-$V-arduino.zip
curl -fsSL $base/SHA256SUMS | grep arduino.zip | sha256sum -c
unzip -q StampDisplay-$V-arduino.zip -d libraries/     # -> libraries/StampDisplay/
arduino-cli compile --fqbn esp8266:esp8266:d1_mini --libraries libraries MeinSketch
```

Mit der Arduino IDE: *Sketch → Bibliothek einbinden → .ZIP-Bibliothek hinzufügen*.

## Minimalbeispiel

```cpp
#include <StampDisplay.h>
using namespace stampdisplay;

St7735sConfig cfg() { St7735sConfig c{}; c.pinCs = 15; c.pinDc = 4; c.pinRst = 5; c.rotation = 1; return c; }
St7735s panel(cfg());
Gfx gfx(panel);

void setup() {
  panel.begin();                                   // Reset + Init, ca. 1 s
  gfx.fillScreen(color::BLACK);
  gfx.drawText(4, 4, "Hallo", color::WHITE, color::BLACK);
}
void loop() {}
```

Vollständig in [`examples/minimal`](examples/minimal/minimal.ino).

## API

### `St7735sConfig`

| Feld | Bedeutung |
|---|---|
| `pinCs`, `pinDc` | Chip Select, Data/Command (am Modul „RS“), Pflicht |
| `pinRst` | Reset („RES“), `-1` wenn am Board-Reset |
| `pinSck`, `pinMosi` | nur ESP32, `-1`: Standard-SPI-Pins. ESP8266 nutzt fest GPIO 14/13 |
| `pinBacklight` | „LEDA“, `-1` wenn fest an 3,3 V |
| `rotation` | 0/2 hochkant 128 × 160, 1/3 quer 160 × 128 |
| `bgr`, `invert` | setzen, wenn Rot/Blau vertauscht bzw. das Bild negativ ist |
| `xOffset`, `yOffset` | Versatz von 1–2 px bei manchen Panels |
| `spiHz` | SPI-Takt, Standard 20 MHz |

### `St7735s` (Arduino)

- `St7735s(const St7735sConfig&, SPIClass& spi = SPI)`
- `void begin()`: stellt Pins und SPI ein, schaltet das Hintergrundlicht an und macht dann Reset und Init. Blockiert etwa 1 s, wegen der Wartezeiten des Controllers.
- `void setRotation(uint8_t)`: 0–3, größere Werte zählen modulo 4. Setzt `width()`/`height()` neu.
- `void setBacklight(bool)`: wirkungslos, wenn `pinBacklight == -1`.
- `void sleep(bool)`: Schlafmodus an/aus, wartet die nötigen 120 ms.

### `St7735sPanel` / `St7735sBus` (ohne Arduino)

Für eigene Hardware-Anbindungen (anderes SPI-API, Tests): `St7735sBus` implementieren (`beginWrite/endWrite`, `dc`, `write`, `hasReset/reset`, `delayMs`) und `St7735sPanel(bus, config).initPanel()` aufrufen. `write()` muss beliebige Längen annehmen. Puffer der Library sind 4-Byte-ausgerichtet, die Argumente der Init-Sequenz nicht (der ESP8266-Adapter kopiert sie deshalb).

### `Gfx`

Alle Koordinaten werden am Panel abgeschnitten. Werte außerhalb des Panels sind deshalb kein Fehler.

- `fillScreen`, `fillRect`, `drawHLine`, `drawVLine`, `drawRect`
- `drawChar(x, y, c, fg, bg)`: eine 6×8-Zelle. Zeichen außerhalb von ASCII 0x20–0x7E werden als `?` gezeichnet.
- `drawText(...)`: gibt das x hinter dem Text zurück.
- `drawField(x, y, text, chars, fg, bg)` / `drawFieldRight(...)`: genau `chars` Zellen breit, zu langer Text wird abgeschnitten, kürzerer aufgefüllt.
- `drawTextScaled(x, y, text, scale, fg, bg)`: `scale` 1–4.
- `textWidth(text, scale)`, `drawPixels(x, y, w, h, pixels)` (RGB565, Zeile für Zeile).

### Panels

- `BandPanel<W, MAX_H>`: ein Streifen im RAM. Hinein zeichnen und dann mit `Gfx::drawPixels` am Stück senden, so flackert nichts.
- `MonoPanel<W, H>`: 1-Bit-Puffer im SSD1306-Seitenlayout. Ein Pixel leuchtet ab etwa 25 % Helligkeit. `buffer()`, `dirty()`/`clean()`.
- `FramebufferPanel(w, h)` (nur ohne Arduino): RGB565 im Speicher, `writePpm(path)` für Screenshots.

## Bauen und testen

```sh
../../tools/session-setup.sh          # Cross-Compiler aus r4d1um/setup
make test                              # PC-Tests mit Sanitizern gegen die Host-.a
make package                           # alle Ziele + Release-Pakete in build/pkg/
```

Die Tests prüfen die Bytes, die das Display bekäme (Init-Sequenz, Rotation, Fenster, Pixel), über einen aufzeichnenden Fake-Bus.

## Lizenz

MIT ([LICENSE](LICENSE)). Der 5×7-Font stammt aus Adafruit-GFX (BSD, [LICENSE-font.txt](LICENSE-font.txt)).
