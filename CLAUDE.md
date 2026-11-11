# Regeln für dieses Repo (öffentlich)

Erster Schritt in jeder Session: `tools/session-setup.sh` (installiert nur, was fehlt; Toolchains aus r4d1um/setup).

## Struktur

```
c/neo_gps/                 C99-Library für u-blox NEO (Makefile: test, dist, dist-check, package)
.github/workflows/         <sprache>-<libname>.yml pro Library, privcheck.yml für das ganze Repo
tools/session-setup.sh     Toolchains (nur öffentliche aus r4d1um/setup)
tools/privcheck/           Go-Check auf private Inhalte
go.mod                     Modul nur für tools/ (Go-Libraries bekommen ihr eigenes go.mod)
```

## Regeln

- Neue Library: `<sprache>/<libname>/` mit README (Zweck, Plattformen, Einbindung, Minimalbeispiel), CHANGELOG, Tests, `examples/`.
- Pro Library ein Workflow mit `paths:` auf die Library, `timeout-minutes`, `concurrency`, Toolchain-Cache, Actions auf Node 24.
- Releases: Tag `<sprache>/<libname>/v<version>` erzeugt der Workflow, wenn die Version erhöht wurde. Sessions können keine Tags pushen.
- Kompilierte Libraries: Pakete pro Ziel + Quellpaket + `SHA256SUMS`; Ziele und Toolchains in der README-Tabelle.
- Nichts Privates, keine Abhängigkeit zu `iot-lib-internal`. `go run ./tools/privcheck .` muss sauber sein.
- Neue Toolchains kommen einmal nach r4d1um/setup und werden dann hier per `tools/session-setup.sh` geholt. Private Toolchains (setup-private) kann das CI dieses Repos nicht lesen.

## Entscheidungen

- 2026-11-11: `neo_gps` aus `r4d1um/lib` übernommen, ohne Git-Historie, weil die alten Commits private Autorendaten enthalten.
- 2026-11-11: Binaries als Release-Assets statt committet in `dist/`, damit jede Version fest referenzierbar ist und das Repo klein bleibt.
- 2026-11-11: Build und Release in einem Job (gleiche Artefakte, keine doppelte Minute-Rundung).
