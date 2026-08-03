# Workorder: Online-Update-Epic — Anteil QTmux

> Auftraggeber: Orchestrator-Session (QTmux #23, `_ClaudeWorkspace`), 2026-08-02.
> Masterplan: `/Users/nobser/Projects/_ClaudeWorkspace/online-update-masterplan.md` (maßgeblich bei Widersprüchen).
> Jira: **QTMUX-125** existiert (aktualisieren, nicht neu anlegen), dual. Autarke Auftragsgrundlage.

## Rolle im Epic

QTmux ist reiner Desktop-Update-Konsument. Es teilt KEINEN Code über Submodule mit MacPCAN/RAFTNG → der Update-Core wird **byte-identisch vendiert** (libvterm-Muster). Kein CAN, keine Firmware.

## Arbeitspakete (E im Masterplan)

1. **Vendoring**: `third_party/updater/` = byte-identische Kopie von `../MacPCAN/src/update/` (inkl. `ed25519/`); neues CMake-Target `qtmux_updater` (STATIC, Qt6::Core+Network) — **NICHT** in `qtmux_core` (bleibt Qt6::Core-only, CLAUDE.md-Regel). `tools/check-updater-sync.sh` (SHA-256-Abgleich gegen MacPCAN) + `third_party/updater/UPSTREAM.md` (gepinnter MacPCAN-Commit).
2. **ViewModel**: `src/viewmodels/UpdateViewModel.{h,cpp}` (Q_PROPERTYs busy/updateAvailable/remoteVersion/notes[DE/EN nach `ui/language`]/downloadProgress/state/lastError; Q_INVOKABLEs checkNow/download/skipVersion/launchInstaller). Registrierung als **Context-Property** in `src/app/main.cpp` (CLAUDE.md-Regel: kein `qmlRegisterSingletonInstance` in URI „QTmux"). Startup-Hook dort (Throttle via `UpdatePolicy`).
3. **QML**: `qml/dialogs/UpdateDialog.qml` (Notes, Progress, Buttons, SmartScreen/Gatekeeper-Hinweis); Hilfe-Menü-Action vor `actAbout` (`qml/Main.qml` ~2952) + Command-Palette-Eintrag; Checkbox in `qml/prefs/CatAllgemein.qml`.
4. **Settings**: Keys `update/*` in `src/viewmodels/SettingsIo.cpp` (~Zeile 96) + `tests/tst_settingsio.cpp` erweitern; i18n DE-Quellsprache + `qtmux_en.ts` + `tst_i18n` grün; neuer `tests/tst_updater.cpp` gegen file://-Fixtures.
5. **Opportunistisch**: generiertes `qtmux_version.h` aus `PROJECT_VERSION` (`cmake/Version.h.in`) → ersetzt Hardcodes in `src/app/main.cpp:256` + `src/server/McpServer.cpp:413`; übrige Bump-Stellen bleiben (CLAUDE.md:337).

## Abhängigkeiten (WICHTIG)

| Richtung | Partner | Gegenstand |
|---|---|---|
| ⬅ EINGEHEND (BLOCKER) | **MacPCAN** (`../MacPCAN`, Session #12) | Der zu vendierende `src/update/`-Core muss existieren + gepusht sein. **E erst starten, wenn MacPCANUpdater fertig ist.** Danach byte-identisch kopieren; bei künftigen Core-Änderungen re-vendieren (Sync-Skript) |
| ⬅ EINGEHEND | **Owner** | Ed25519-Produktions-Public-Key (kommt über MacPCANs `UpdateKeys.hpp`) — bis dahin Test-Key |
| ➡ AUSGEHEND | — | Einbahnstraße: `third_party/updater/` NIE lokal editieren (sonst Drift); QTmux-spezifische Logik gehört ins ViewModel/QML |

## Nachgemeldete Anforderungen (eingetragen, NICHT begonnen)

### Proxy-Unterstützung (Owner, 2026-08-03)

**Anforderung:** „Im Firmenumfeld müssen auch Netzwerk-Proxies unterstützt werden, was eine
Konfiguration erfordert."

**Proxy-Unterstützung: appupdate-Proxy-Konfiguration nachvendieren, sobald MacPCAN sie
liefert** (SHA-256-Abgleich via `check-updater-sync.sh`, `UPSTREAM.md` nachziehen).
**QTmux-Anteil:** Settings-Keys `update/proxy*` in `SettingsIo.cpp` + `tst_settingsio`,
Proxy-Abschnitt in `qml/prefs/CatAllgemein.qml`, Auth-Abfrage im `UpdateDialog.qml`,
`UpdateViewModel`-Properties, i18n DE→EN + `tst_i18n`.

⛔ **Keine eigene Proxy-Implementierung anfangen.** Der Mechanismus wird kanonisch in der
Shared-Lib **MacPCANUpdater (`appupdate`)** gebaut — MacPCAN ist der Hub und liefert das
Konzept. Drei Apps mit drei verschiedenen Proxy-Wegen sind genau das, was damit vermieden
wird.

**Warum diese Schnittlinie:** Die Lib bleibt **GUI- und QSettings-frei** (bestehender
Vertrag). Dialog und Persistenz gehören deshalb in die App, das **Netzwerkverhalten** in die
Lib.

🔑 **QTmux-Besonderheit gegenüber RAFTNG (dort Submodul): wir erben über VENDORING.** Beim
Nachziehen gilt deshalb zusätzlich — `third_party/updater/` bleibt **byte-identisch** zur
MacPCAN-Quelle (nie lokal editieren, Einbahnstraße s. o.), **`tools/check-updater-sync.sh`
muss danach wieder grün sein**, und der gepinnte MacPCAN-Commit in
`third_party/updater/UPSTREAM.md` wird nachgezogen. Ein Proxy-Fix gehört also nach MacPCAN,
nicht hierher.

**Nicht betroffen:** Die **CI ändert sich nicht** (github-hosted; QTmux ist als einziges
Repo der Familie public). Der Update-Zyklus ist mit **v1.8.0 live verifiziert** — Proxy ist
eine **Erweiterung, kein Defekt**.

**Reihenfolge:** eingehender Blocker wie bei Paket 1 — erst wenn MacPCAN die
Proxy-Konfiguration gepusht hat, wird re-vendiert; der QTmux-Anteil hängt an den dann
vorhandenen Lib-Schnittstellen.

## Infrastruktur-Fakten

- Update-Basis-URL: **`https://nobser.de/updates/qtmux/manifest.json`** (statisch, T-Online). Assets liegen zusätzlich auf GitHub Releases (`RealNobser/QTmux`) — Manifest kann auf beides zeigen (Owner-Entscheidung bei Umsetzung; Default T-Online-Webspace).
- Owner-Entscheidungen: Download + geführte Installation (kein Silent-Self-Replace); Check beim Start max. 1×/Tag + manuell, abschaltbar; Downgrade zulassen mit Warnung.
- QTmux-Installer existieren alle (`installer/build-dmg.sh`, `build-msi.ps1`, `build-appimage.sh`) → alle drei OS-Keys von Anfang an belegbar.
- Diese Session (QTmux-Worker) ODER die Orchestrator-Session setzt E um — vor Beginn `list_sessions`/Arbeitsverzeichnis prüfen, um Doppelarbeit zu vermeiden.
