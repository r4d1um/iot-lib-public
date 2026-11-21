# StampDisplay

Driver and graphics for the 1.77" TFT module with ST7735S (160 × 128, RGB565, SPI), plus panels for 1-bit displays (SSD1306 layout) and for images in memory. Text in fixed 6×8 cells overwrites its old content, so updates leave no stray pixels behind.

## Structure

| Part | Files | Shipped as |
|---|---|---|
| Arduino-free core: `Gfx`, `Panel`, `BandPanel`, `MonoPanel`, `FramebufferPanel` (PC only), ST7735S protocol `St7735sPanel` + interface `St7735sBus` | `Gfx.cpp`, `St7735sPanel.cpp`, headers | precompiled: `src/<mcu>/libStampDisplay.a` |
| Arduino adapter: `St7735s` (SPI, CS, DC, RESET, backlight) | `St7735s.cpp/.h` | source, compiled with the board core |

The adapter is thin and depends on the core version. The core depends on no core version, so the same `.a` works with ESP8266 core 3.1.2 and with ESP32 core 2.0.17 and 3.x.

## Platforms

| Target | `.a` built with (pinned in [tools/toolchains.sh](../../tools/toolchains.sh)) | Flags | Tested with |
|---|---|---|---|
| `esp8266` | xtensa-lx106-elf GCC 10.3 (compiler of core 3.1.2) | `-Os -mlongcalls -mtext-section-literals -falign-functions=4` | core 3.1.2, NodeMCU v2 / D1 mini |
| `esp32` | xtensa-esp32-elf GCC 8.4 (compiler of core 2.0.17) | `-Os -mlongcalls -fstrict-volatile-bitfields` | core 2.0.17 and 3.x |
| `linux-x86_64` | the runner's gcc | `-O2 -fPIC` | PC tests, screenshot renderer |

All targets: `-std=gnu++17 -fno-exceptions -ffunction-sections -fdata-sections`, with RTTI. Other Arduino architectures (AVR, RP2040) are not supported yet (`architectures=esp8266,esp32`).

## Including it

Every version is a release `cpp/stamp-display/v<version>` with:

- `StampDisplay-<v>-arduino.zip`: Arduino library (`precompiled=true`) for arduino-cli, the Arduino IDE and other Arduino builders
- `StampDisplay-<v>-<target>.tar.gz`: `include/` + `lib/libStampDisplay.a` for builds without Arduino
- `SHA256SUMS`

```sh
V=1.1.1
base=https://github.com/r4d1um/iot-lib-public/releases/download/cpp/stamp-display/v$V
curl -fsSLO $base/StampDisplay-$V-arduino.zip
curl -fsSL $base/SHA256SUMS | grep arduino.zip | sha256sum -c
unzip -q StampDisplay-$V-arduino.zip -d libraries/     # -> libraries/StampDisplay/
arduino-cli compile --fqbn esp8266:esp8266:d1_mini --libraries libraries MySketch
```

With the Arduino IDE: *Sketch → Include Library → Add .ZIP Library…*.

## Minimal example

```cpp
#include <StampDisplay.h>
using namespace stampdisplay;

St7735sConfig cfg() { St7735sConfig c{}; c.pinCs = 15; c.pinDc = 4; c.pinRst = 5; c.rotation = 1; return c; }
St7735s panel(cfg());
Gfx gfx(panel);

void setup() {
  panel.begin();                                   // reset + init, about 1 s
  gfx.fillScreen(color::BLACK);
  gfx.drawText(4, 4, "Hello", color::WHITE, color::BLACK);
}
void loop() {}
```

Complete version in [`examples/minimal`](examples/minimal/minimal.ino).

## API

### `St7735sConfig`

| Field | Meaning |
|---|---|
| `pinCs`, `pinDc` | chip select, data/command ("RS" on the module), required |
| `pinRst` | reset ("RES"), `-1` if tied to the board reset |
| `pinSck`, `pinMosi` | ESP32 only, `-1`: default SPI pins. ESP8266 always uses GPIO 14/13 |
| `pinBacklight` | "LEDA", `-1` if wired permanently to 3.3 V |
| `rotation` | 0/2 portrait 128 × 160, 1/3 landscape 160 × 128 |
| `bgr`, `invert` | set if red/blue are swapped or the image is negative |
| `xOffset`, `yOffset` | 1–2 px offset on some panels |
| `spiHz` | SPI clock, default 20 MHz |

### `St7735s` (Arduino)

- `St7735s(const St7735sConfig&, SPIClass& spi = SPI)`
- `void begin()`: sets up pins and SPI, switches the backlight on, then resets and initialises the controller. Blocks for about 1 s because of the controller's wait times.
- `void setRotation(uint8_t)`: 0–3, larger values wrap modulo 4. Updates `width()`/`height()`.
- `void setBacklight(bool)`: no effect if `pinBacklight == -1`.
- `void sleep(bool)`: sleep mode on/off, waits the required 120 ms.

### `St7735sPanel` / `St7735sBus` (without Arduino)

For your own hardware bindings (another SPI API, tests): implement `St7735sBus` (`beginWrite/endWrite`, `dc`, `write`, `hasReset/reset`, `delayMs`) and call `St7735sPanel(bus, config).initPanel()`. `write()` must accept any length. The library's buffers are 4-byte aligned, the init sequence arguments are not (so the ESP8266 adapter copies them).

### `Gfx`

All coordinates are clipped to the panel, so values outside the panel are not an error.

- `fillScreen`, `fillRect`, `drawHLine`, `drawVLine`, `drawRect`
- `drawChar(x, y, c, fg, bg)`: one 6×8 cell. Characters outside ASCII 0x20–0x7E are drawn as `?`.
- `drawText(...)`: returns the x after the text.
- `drawField(x, y, text, chars, fg, bg)` / `drawFieldRight(...)`: exactly `chars` cells wide; longer text is cut off, shorter text is padded.
- `drawTextScaled(x, y, text, scale, fg, bg)`: `scale` 1–4.
- `textWidth(text, scale)`, `drawPixels(x, y, w, h, pixels)` (RGB565, row by row).

### Panels

- `BandPanel<W, MAX_H>`: a strip in RAM. Draw into it, then send it in one go with `Gfx::drawPixels` — no flicker.
- `MonoPanel<W, H>`: 1-bit buffer in SSD1306 page layout. A pixel lights up from about 25 % brightness. `buffer()`, `dirty()`/`clean()`.
- `FramebufferPanel(w, h)` (without Arduino only): RGB565 in memory, `writePpm(path)` for screenshots.

## Build and test

```sh
export PATH="$(../../tools/toolchains.sh | paste -sd:):$PATH"   # ESP cross compilers
make test                              # PC tests with sanitizers against the host .a
make package                           # all targets + release packages in build/pkg/
```

The tests check the bytes the display would receive (init sequence, rotation, window, pixels) through a recording fake bus.

## License

MIT ([LICENSE](LICENSE)). The 5×7 font comes from Adafruit-GFX (BSD, [LICENSE-font.txt](LICENSE-font.txt)).
