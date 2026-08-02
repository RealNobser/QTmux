# Vendiert: `MacPCANUpdater` — der geteilte Desktop-Update-Kern (`appupdate`)

`update/` ist eine **byte-identische Kopie** von `MacPCAN/src/update/`. Kanonische
Quelle ist MacPCAN; QTmux teilt mit MacPCAN/RAFTNG **keine** Submodule, deshalb der
Vendoring-Weg — dasselbe Muster wie bei `third_party/libvterm`.

## Gepinnter Stand

| | |
|---|---|
| Repository | `MacPCAN` (Worker-Checkout `/Users/nobser/Projects/_ClaudeWorkspace/MacPCAN`) |
| Commit | `59a9e3531ca4d97848e7bb0913bbcb7d26d97b4f` |
| Datum | 2026-08-02 |
| Betreff | `fix(update): clear the in-flight marker before invoking a callback` |

Enthält den **Produktions-Public-Key** (`update/UpdateKeys.hpp`, Ed25519, 32 Byte,
Owner-Schlüssel vom 2026-08-02) — kein Platzhalter mehr.

## ⚠️ Einbahnstraße

Dateien unter `update/` werden **nie lokal editiert**. Jede Änderung gehört nach
`MacPCAN/src/update/` und kommt von dort zurück:

```bash
tools/check-updater-sync.sh            # SHA-256-Abgleich, Exit 1 bei Drift
tools/check-updater-sync.sh --update   # Änderungen von MacPCAN übernehmen
```

Danach den Commit-Block oben nachziehen. Ohne MacPCAN-Checkout beendet sich das
Skript mit Exit 0 und einer Meldung (Build-Maschinen und CI haben ihn nicht — ein
Test, der dort rot würde, meldete ein Umgebungsproblem als Regression).

QTmux-spezifisches liegt **außerhalb** dieses Verzeichnisses:
[`src/viewmodels/UpdateViewModel.{h,cpp}`](../../src/viewmodels/UpdateViewModel.h)
(QSettings-Persistenz, Q_PROPERTYs) und [`qml/dialogs/UpdateDialog.qml`](../../qml/dialogs/UpdateDialog.qml).

## Warum ein `update/`-Unterverzeichnis

Der Kern inkludiert sich selbst mit dem Präfix `update/`
(`#include "update/UpdateChecker.hpp"`) — der Include-Pfad ist bei MacPCAN `src/`.
Byte-identisch heißt: auch diese Zeilen bleiben unangetastet. Deshalb liegt die
Kopie in einem Verzeichnis **namens `update`**, und `qtmux_updater` exportiert
`third_party/updater` als Include-Pfad. Ein flaches Vendieren nach
`third_party/updater/*.hpp` würde jede `#include`-Zeile ändern und die
Byte-Identität — und damit den Sync-Wächter — aufgeben.

## Baustein

CMake-Target **`qtmux_updater`** (STATIC, `Qt6::Core` + `Qt6::Network`), definiert in
der Haupt-`CMakeLists.txt`. Bewusst **nicht** Teil von `qtmux_core`: der Kern bleibt
Qt6::Core-only (Projektregel, `CLAUDE.md`), `UpdateChecker` braucht aber
`QNetworkAccessManager`.

`update/ed25519/` ist **Monocypher 4.0.2** (C, dual BSD-2-Clause/CC0) — Herkunft und
die Falle „`crypto_ed25519_check` statt `crypto_eddsa_check`" stehen in
[`update/ed25519/README.md`](update/ed25519/README.md).
