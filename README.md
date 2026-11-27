# iot-lib-public

Reusable IoT libraries that are fine to be public. GitHub Actions checks every library, builds it for all supported targets and publishes it as a versioned release. Device projects include these releases instead of copying source code.

| Library | Language | Purpose | Releases |
|---|---|---|---|
| [`cpp/stamp-display`](cpp/stamp-display) | C++17, Arduino | ST7735S TFT, Gfx with 6×8 text, 1-bit and in-memory panels | `cpp/stamp-display/v*` |
| [`cpp/shift595`](cpp/shift595) | C99 core + Arduino | 74HC595 chains (1–32): pins, bytes, LED bars, writes only on change; shiftOut or SPI, brightness via OE | `cpp/shift595/v*` |
| [`cpp/char-lcd`](cpp/char-lcd) | C99 core + Arduino | HD44780 character LCDs (16x2 … 40x4) via PCF8574 I2C, one 74HC595 (3 pins) or 4-bit parallel, UTF-8, screen buffer | `cpp/char-lcd/v*` |
| [`cpp/remote-rx`](cpp/remote-rx) | C99 core + Arduino | IR remotes via VS1838B/KY-022/TSOP: NEC, Samsung, Sony, RC5, RC6, hash for unknown protocols, hold/toggle | `cpp/remote-rx/v*` |

**Moved:** the u-blox NEO GPS library `neo_gps` now lives in its own repo, [r4d1um/neo-gps](https://github.com/r4d1um/neo-gps) (releases `v*` from 1.2.1 on). The old releases `c/neo_gps/v*` stay here so existing references keep working.

## Layout

```
<language>/<libname>/     source, tests, examples, README, CHANGELOG
.github/workflows/        one workflow per library (<language>-<libname>.yml) + privcheck.yml
tools/toolchains.sh       pinned ESP cross compilers from their upstream releases (only what is missing)
tools/privcheck/          check for private content (mandatory on every run)
tools/mk/arduino-lib.mk   shared build for Arduino libraries (core as .a, adapter as source)
```

## Versions and releases

- SemVer; interfaces are only extended, never renamed.
- Tags and releases are named `<language>/<libname>/v<version>` and are created automatically once the library's version is bumped on `main`.
- Every release contains the finished packages per target and `SHA256SUMS`. The targets and toolchains are listed in each library's README.

## Arduino libraries

Every Arduino library consists of an Arduino-free core and a thin adapter. The core is precompiled per target (`src/esp8266/lib<Name>.a`, `src/esp32/lib<Name>.a`, `precompiled=true`). The adapter (SPI, Wire, Client …) ships as source so it matches the installed core version. The release asset `<Name>-<v>-arduino.zip` works directly with `arduino-cli --libraries` or the Arduino IDE.

## Rules

- Nothing private goes here: no credentials, no hostnames, IPs, topics or names from a real installation, not even in tests, examples or logs. Example values are neutral (`192.0.2.x`, `example.com`). `tools/privcheck` checks this on every push.
- Libraries here never depend on `iot-lib-internal`.
- License: MIT unless stated otherwise in the library.
