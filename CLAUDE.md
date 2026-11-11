# Regeln für dieses Repo (öffentlich)

Erster Schritt in jeder Session: `tools/session-setup.sh` (installiert nur, was fehlt; Toolchains aus r4d1um/setup).

## Struktur

```
c/neo_gps/                 C99-Library für u-blox NEO (Makefile: test, dist, dist-check, package)
cpp/stamp-display/         Arduino-Library StampDisplay (Kern .a + Adapter-Quelle, tools/mk/arduino-lib.mk)
.github/workflows/         <sprache>-<libname>.yml pro Library, privcheck.yml für das ganze Repo
tools/session-setup.sh     Toolchains (nur öffentliche aus r4d1um/setup)
tools/privcheck/           Go-Check auf private Inhalte
tools/mk/arduino-lib.mk    gemeinsames Makefile für Arduino-Libraries (test, dist, package)
go.mod                     Modul nur für tools/ (Go-Libraries bekommen ihr eigenes go.mod)
```

## Regeln

- Neue Library: `<sprache>/<libname>/` mit README (Zweck, Plattformen, Einbindung, Minimalbeispiel), CHANGELOG, Tests, `examples/`.
- Pro Library ein Workflow mit `paths:` auf die Library, `timeout-minutes`, `concurrency`, Toolchain-Cache, Actions auf Node 24.
- Releases: Tag `<sprache>/<libname>/v<version>` erzeugt der Workflow, wenn die Version erhöht wurde. Sessions können keine Tags pushen.
- Kompilierte Libraries: Pakete pro Ziel + Quellpaket + `SHA256SUMS`; Ziele und Toolchains in der README-Tabelle.
- Nichts Privates, keine Abhängigkeit zu `iot-lib-internal`. `go run ./tools/privcheck .` muss sauber sein.
- Neue Toolchains kommen einmal nach r4d1um/setup und werden dann hier per `tools/session-setup.sh` geholt. Private Toolchains (setup-private) kann das CI dieses Repos nicht lesen.

- Arduino-Libraries: Kern ohne Arduino-Header in `CORE_SRC` (vorkompiliert für esp8266/esp32/linux-x86_64), Adapter als Quelle. Objekte heißen `*.cpp.o` (ESP8266-Linker legt nur die in den Flash). Nach jeder Änderung eine echte Firmware gegen das Zip bauen (arduino-cli + Core aus setup/setup-private).

## Entscheidungen

- 2026-11-11: `neo_gps` aus `r4d1um/lib` übernommen, ohne Git-Historie, weil die alten Commits private Autorendaten enthalten.
- 2026-11-11: Binaries als Release-Assets statt committet in `dist/`, damit jede Version fest referenzierbar ist und das Repo klein bleibt.
- 2026-11-11: Build und Release in einem Job (gleiche Artefakte, keine doppelte Minute-Rundung).
- 2026-11-11: Arduino-Libraries als vorkompilierter Kern + Adapter-Quelle: fertiger C++-Code mit Arduino-Headern passt nur zu einer Core-Version (Cloud ESP32 3.x, Netbook 2.0.17); der Kern ohne Arduino passt zu allen. ESP32-Kern mit GCC 8.4 (Core 2.0.17), linkt auch mit 3.x.
- 2026-11-11: Kern mit RTTI gebaut: Sketches mit `-frtti` brauchen die typeinfo, Sketches ohne RTTI ignorieren sie.
