# Changelog StampDisplay

Versions follow SemVer. Interfaces are only extended, never renamed. Releases:
`cpp/stamp-display/v<version>` in [r4d1um/iot-lib-public](https://github.com/r4d1um/iot-lib-public/releases).

## 1.1.1

- 2026-11-21: README and CHANGELOG translated to English. No code changes.

## 1.1.0

- 2026-11-11: taken over from `r4d1um/parking-lot-display` (`libraries/StampDisplay`, 1.0.0).
- Split up: Gfx, panels and the ST7735S protocol (`St7735sPanel`, `St7735sBus`) are an
  Arduino-free core, precompiled for ESP8266 and ESP32 (`src/<mcu>/libStampDisplay.a`). Only the
  SPI adapter `St7735s` (Arduino) ships as source and is compiled with the board core.
- `St7735s` has the same API as in 1.0.0 (`St7735sConfig`, `begin`, `setRotation`, `setBacklight`, `sleep`).

## 1.0.0

- ST7735S driver, Gfx (rectangles, lines, 6x8 text, fields), BandPanel, MonoPanel, FramebufferPanel.
