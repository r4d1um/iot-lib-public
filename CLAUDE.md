# Rules for this repo (public)

First step in every session: `tools/session-setup.sh` (installs only what is missing; toolchains from r4d1um/setup).

Everything in this repo is written in English: code, comments, docs, CHANGELOGs, workflows, commit messages, PRs,
issues, test and CLI output. (UTF-8 sample strings such as "Grüße" in char-lcd are test data for umlaut mapping.)

## Layout

```
cpp/stamp-display/         Arduino library StampDisplay (core .a + adapter source, tools/mk/arduino-lib.mk)
cpp/shift595/              Arduino library Shift595 for 74HC595 chains (C99 core .a + adapter source, fake 595 in tests/)
cpp/char-lcd/              Arduino library CharLcd for HD44780 (C99 core .a + adapter source, fake HD44780 in tests/)
.github/workflows/         <language>-<libname>.yml per library, privcheck.yml for the whole repo
tools/session-setup.sh     toolchains (public ones only, from r4d1um/setup)
tools/privcheck/           Go check for private content
tools/mk/arduino-lib.mk    shared Makefile for Arduino libraries (test, dist, package)
go.mod                     module for tools/ only (Go libraries get their own go.mod)
```

`neo_gps` (formerly `c/neo_gps/`) lives in its own repo, r4d1um/neo-gps.

## Rules

- New library: `<language>/<libname>/` with README (purpose, platforms, how to include it, minimal example), CHANGELOG, tests, `examples/`.
- One workflow per library with `paths:` on the library, `timeout-minutes`, `concurrency`, toolchain cache, actions on Node 24.
- Releases: the workflow creates the tag `<language>/<libname>/v<version>` when the version was bumped. Sessions cannot push tags.
- Compiled libraries: packages per target + source package + `SHA256SUMS`; targets and toolchains in the README table.
- Nothing private, no dependency on `iot-lib-internal`. `go run ./tools/privcheck .` must be clean.
- New toolchains go to r4d1um/setup once and are then fetched here via `tools/session-setup.sh`. The CI of this repo cannot read private toolchains (setup-private).
- Arduino libraries: core without Arduino headers in `CORE_SRC` (precompiled for esp8266/esp32/linux-x86_64), adapter as source. Objects are named `*.cpp.o` (the ESP8266 linker only puts those into flash). After every change, build a real firmware against the zip (arduino-cli + core from setup/setup-private).

## Decisions

- 2026-11-11: `neo_gps` taken over from `r4d1um/lib` without Git history, because the old commits contain private author data.
- 2026-11-11: binaries as release assets instead of committed in `dist/`, so every version can be referenced exactly and the repo stays small.
- 2026-11-11: build and release in one job (same artifacts, no double per-job minute rounding).
- 2026-11-11: Arduino libraries as precompiled core + adapter source: finished C++ code with Arduino headers only fits one core version (cloud ESP32 3.x, netbook 2.0.17); the core without Arduino fits all of them. ESP32 core built with GCC 8.4 (core 2.0.17), also links with 3.x.
- 2026-11-11: core built with RTTI: sketches with `-frtti` need the typeinfo, sketches without RTTI ignore it.
- 2026-11-18: `shift595` builds AVR in addition via `override TARGETS`, like `char-lcd`, and checks a real Uno firmware with `firmware-check`.
- 2026-11-18: `char-lcd` builds AVR in addition (atmega328p, atmega2560) via `override TARGETS` in its own Makefile, without changing `tools/mk/arduino-lib.mk`; `firmware-check` builds a real Uno firmware against the zip (the ESP cores are in setup-private, which CI here cannot read).
- 2026-11-18: the `char-lcd` screen buffer gets its memory from the caller (`HD44780_SCREEN_MEM(cols, rows)`), so a 16x2 on the Uno does not reserve 40x4 buffers.
- 2026-11-18: `char-lcd` on the 74HC595 uses the same bit layout as the PCF8574 backpack (one encoding for both); before every RS change one byte with E low, so t_AS is met.
- 2026-11-21: `neo_gps` moved to its own repo r4d1um/neo-gps (releases `v*` from 1.2.1 on); `c/neo_gps` and its workflow removed here, the old `c/neo_gps/v*` releases stay.
- 2026-11-21: whole repo translated to English (no German on GitHub). The ARM/i686 toolchains were only needed by `neo_gps` and are no longer in the session-setup defaults.
