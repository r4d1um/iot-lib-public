# Changelog neo_gps

Versionen nach SemVer. Schnittstellen werden nur erweitert, nie umbenannt.
Jede Version erscheint als Release `c/neo_gps/v<version>` in
[r4d1um/iot-lib-public](https://github.com/r4d1um/iot-lib-public/releases).

## 1.2.0

- Ortszeit mit Zeitzone und Sommerzeit (`neo_gps_time.c/.h`), Unix-Zeit.
- Beispiel: Gelände-Simulation für Fläche und Steigung mit vier Empfängern.
- 2026-11-11: nach `iot-lib-public/c/neo_gps` umgezogen (vorher `r4d1um/lib`,
  davor `r4d1um/gps-library-neo`). Grund: öffentliche, versionierte Releases
  statt committeter Binaries. Die Historie wurde nicht übernommen, weil sie
  private Autorendaten enthält. Code und API sind unverändert.

## 1.1.0

- Sprungfilter gegen GPS-Jumps (`neo_gps_filter.c`).

## 1.0.0

- Erste Version: Streaming-Parser für NMEA und UBX ohne malloc und ohne Float,
  UBX-Konfiguration, Modellerkennung für NEO-6M, NEO-7M und NEO-M8N.
