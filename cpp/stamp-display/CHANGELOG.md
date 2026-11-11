# Changelog StampDisplay

Versionen nach SemVer. Schnittstellen werden nur erweitert, nie umbenannt. Releases:
`cpp/stamp-display/v<version>` in [r4d1um/iot-lib-public](https://github.com/r4d1um/iot-lib-public/releases).

## 1.1.0

- 2026-11-11: aus `r4d1um/parking-lot-display` (`libraries/StampDisplay`, 1.0.0) übernommen.
- Neu aufgeteilt: Gfx, Panels und das ST7735S-Protokoll (`St7735sPanel`, `St7735sBus`) sind ein
  Arduino-freier Kern, vorkompiliert für ESP8266 und ESP32 (`src/<mcu>/libStampDisplay.a`). Nur der
  SPI-Adapter `St7735s` (Arduino) kommt als Quelle mit und wird mit dem Board-Core übersetzt.
- `St7735s` hat dieselbe API wie in 1.0.0 (`St7735sConfig`, `begin`, `setRotation`, `setBacklight`, `sleep`).

## 1.0.0

- ST7735S-Treiber, Gfx (Rechtecke, Linien, 6x8-Text, Felder), BandPanel, MonoPanel, FramebufferPanel.
