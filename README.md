# iot-lib-public

Wiederverwendbare IoT-Libraries, die bedenkenlos öffentlich sein dürfen. Jede Library wird von GitHub Actions geprüft, für alle unterstützten Ziele gebaut und als versioniertes Release veröffentlicht. Geräteprojekte binden diese Releases ein, statt Quellcode zu kopieren.

| Library | Sprache | Wofür | Releases |
|---|---|---|---|
| [`c/neo_gps`](c/neo_gps) | C99 | u-blox NEO-6M/7M/M8N: NMEA + UBX, Sprungfilter, Ortszeit | `c/neo_gps/v*` |

## Aufbau

```
<sprache>/<libname>/      Quellcode, Tests, Beispiele, README, CHANGELOG
.github/workflows/        ein Workflow pro Library (<sprache>-<libname>.yml) + privcheck.yml
tools/session-setup.sh    Toolchains aus r4d1um/setup (nur was fehlt)
tools/privcheck/          Check auf private Inhalte (Pflicht in jedem Lauf)
```

## Versionen und Releases

- SemVer; Schnittstellen werden nur erweitert, nie umbenannt.
- Tag und Release heißen `<sprache>/<libname>/v<version>` und entstehen automatisch, sobald die Version der Library auf `main` erhöht wurde.
- Jedes Release enthält die fertigen Pakete pro Ziel und `SHA256SUMS`. Welche Ziele und Toolchains es gibt, steht in der README der Library.

## Regeln

- Hier liegt nichts Privates: keine Zugangsdaten, keine Hostnamen, IPs, Topics oder Namen aus einer echten Installation, auch nicht in Tests, Beispielen oder Logs. Beispielwerte sind neutral (`192.0.2.x`, `example.com`). `tools/privcheck` prüft das bei jedem Push.
- Libraries hier hängen nie von `iot-lib-internal` ab.
- Lizenz: MIT, soweit in der Library nicht anders angegeben.
