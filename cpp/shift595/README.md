# Shift595

74HC595 shift register chains (1 to 32 chips): set single outputs, whole bytes or LED bar
graphs, and `update()` shifts them out only when something changed.

- **Core** `shift595.c` (C99, no malloc, no float, no I/O), precompiled per target, tested
  against a fake 74HC595 chain that clocks bits like the chip.
- **Arduino adapter** `Shift595` (source): `shiftOut` on any 3 pins or **hardware SPI**
  (MOSI/SCK + latch), **brightness** via PWM on OE.

## Platforms

| Target | Folder in the Arduino zip | Compiler |
|---|---|---|
| ESP8266 (core 3.1.x) | `src/esp8266/` | xtensa-lx106-elf GCC 10.3 (r4d1um/setup) |
| ESP32 (core 2.0.x and 3.x) | `src/esp32/` | xtensa-esp32-elf GCC 8.4 (r4d1um/setup) |
| Uno / Nano / Pro Mini | `src/atmega328p/` | avr-gcc 7.3 (r4d1um/setup) |
| Mega 2560 | `src/atmega2560/` | avr-gcc 7.3 (r4d1um/setup) |
| Linux x86_64 (tests, PC tools) | `.tar.gz` | gcc |

Other targets: build `src/shift595.c` from source and pass a bus with your GPIO/SPI functions.

## Wiring (SN74HC595N, DIP-16)

| Pin | Name | Connect to |
|---|---|---|
| 14 | SER | data pin (or MOSI); on later chips: QH' (9) of the chip before |
| 11 | SRCLK | clock pin (or SCK), all chips |
| 12 | RCLK | latch pin, all chips |
| 13 | OE | GND, or a PWM pin for brightness (active low) |
| 10 | SRCLR | VCC |
| 16 / 8 | VCC / GND | 2-6 V / GND, 100 nF across them at each chip |
| 15, 1-7 | QA, QB-QH | outputs (max. 6 mA per pin, 70 mA per chip - use LED resistors) |

Pin numbers in the library: `p` = output Q(p % 8) of chip `p / 8`, chip 0 next to the
microcontroller, QA = 0 ... QH = 7.

## Use it (Arduino)

Download `Shift595-<v>-arduino.zip` from the release `cpp/shift595/v<v>` and install it as a library.

```cpp
#include <Shift595.h>

Shift595 leds(5, 6, 7);          // data, clock, latch (+ registers, OE pin)

void setup() { leds.begin(); }

void loop() {
  leds.bar(0, 8, rssi + 100, 70);   // 8-LED bar, or SHIFT595_DOT for a single dot
  leds.update();                    // writes only if the bar changed
}
```

Hardware SPI: `Shift595 chain(Shift595::Spi{10}, 3, 9);` (latch 10, 3 chips, OE on 9).

## Use it (plain C)

```c
#include "shift595.h"

static void shift(void *ctx, uint8_t b) { /* 8 clocks, MSB first */ }
static void latch(void *ctx) { /* RCLK high, low */ }

uint8_t mem[2];
shift595 sr;
shift595_bus bus = { shift, latch, NULL };
shift595_init(&sr, &bus, 2, mem);
shift595_set(&sr, 9, 1);
shift595_update(&sr);
```

## API

| Function | What it does |
|---|---|
| `shift595_init(s, bus, regs, mem)` | 1..32 registers, `mem` = `regs` bytes; all off, first update writes |
| `shift595_set / get / toggle(s, pin, ...)` | one output; pins past the chain are ignored / read 0 |
| `shift595_set_byte / get_byte(s, reg, ...)` | a whole register, bit i = Q(i) |
| `shift595_clear(s)` | all off |
| `shift595_bar(s, first, count, value, max, mode)` | `SHIFT595_BAR` lights round(value/max*count), `SHIFT595_DOT` only the last |
| `shift595_update(s)` | shift out + latch if changed; 1 if written |
| `shift595_write(s)` | shift out + latch now |

The 74HC595 also drives an HD44780 LCD on 3 pins: see `cpp/char-lcd` (`CharLcd::Shift595Pins`).

## Build

```sh
make test            # host tests (ASan/UBSan) against the fake chain
make package         # .a per target, Arduino zip, tarballs, SHA256SUMS in build/pkg/
make firmware-check  # Arduino Uno firmware against the zip (arduino-cli + AVR core)
```
