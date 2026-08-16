# Vendiert: MacPCAN-CAN-Schicht (`vendor/`)

`vendor/` ist eine **byte-identische Auswahl** aus `MacPCAN/src/` — die Qt-freie
CAN-Schicht (Namespace `mac_pcan`), die das QTmux-CAN-Plugin als Backend nutzt.
Kanonische Quelle ist MacPCAN; dasselbe Einbahnstraßen-Muster wie bei
`third_party/updater/` und `third_party/libvterm/`.

## Gepinnter Stand

| | |
|---|---|
| Repository | `MacPCAN` (Worker-Checkout `/Users/nobser/Projects/_ClaudeWorkspace/MacPCAN`) |
| Commit | `2c6c15e40707ba100f33e95133c1adfbbe7a4bb3` |
| Datum | 2026-08-16 |

Mit diesem Stand (Welle 0.1, 2026-08-12) wurden **7 von 8 Dateien** nachgeführt —
das Verzeichnis war seit dem Anlegen auf dem Hub-Stand M9 (`a3448a7`) eingefroren,
weil der Sync-Wächter es nicht kannte. Herübergekommen sind ausschließlich
**additive** Hub-Erweiterungen, in QTmux bewusst **nicht verdrahtet** (enger
Auftrags-Scope, nur der Sync):

- `decodePayloadBytes()` als freie Funktion in `CanFrame.hpp` (CAN-FD-DLC-Tabelle
  an einer Stelle, RAF-137),
- `DeviceInfo::deviceId` (adapter-gespeicherte Kennung, überlebt Umstecken),
- `ICanDevice::BusStatus` + `busStatusLabel()` (Bus-Gesundheit; Definition liegt
  in der mitvendierten `CanService.cpp`),
- Thread-Safety in `PcanDevice` (`std::atomic` für channel/open/isFd,
  Mutex-geschütztes `lastError_`).

Nachzug 2026-08-13: `ICanDevice.hpp` trägt seit Hub-Commit `7356c45` einen
**Threading-Vertrag als Kommentar** (read() und write() laufen auf verschiedenen
Threads; veränderlicher Zustand in Implementierungen muss synchronisiert werden) —
reine Doku, keine API-Änderung.

Nachzug 2026-08-16 (Vorarbeit V2 des OTA-v4-Epics): 5 Dateien nachgeführt
(`core/CanService.{cpp,hpp}`, `core/ICanDevice.hpp`, `drivers/PcanDevice.{cpp,hpp}`),
Ursache Hub-Commits `667c180` und `50c9578`: **Empfangsfehler-Meldeweg**
(`ICanDevice::errorCount()`, `CanService::deviceErrors()/deviceError()` — vorher
verwarf der Worker `read() == false` wortlos) und **RX-Supervisor**
(`receiveStuck()`/`recoveries()` + selbsttätiger Kanal-Neustart, wenn der
Fehlerzähler bei gleichzeitiger Frame-Stille steigt). Wieder rein **additiv**,
in QTmux weiterhin **nicht verdrahtet**.

## Umfang des Kontrakts

Vendiert ist **nur die Auswahl unter `vendor/`** (derzeit `core/CanFrame.hpp`,
`core/CanService.{hpp,cpp}`, `core/ICanDevice.hpp`, `drivers/MockDevice.{hpp,cpp}`,
`drivers/PcanDevice.{hpp,cpp}`) — **nicht** der ganze Hub-`src/`-Baum (gui/,
specs/, TraceBuffer, SocketCanDevice … gehen QTmux nichts an). Jede vendierte
Datei muss byte-identisch zu `MacPCAN/src/<pfad>` sein.

Der Wächter [`tools/check-updater-sync.sh`](../../tools/check-updater-sync.sh)
prüft seit 2026-08-12 **auch dieses Verzeichnis** (SHA-256 je Datei + Fremd-
Include-Prüfung: jeder `#include "…"`-Pfad muss innerhalb von `vendor/` liegen,
sonst ist es eine Kontrakt-Erweiterung, die benannt gehört). `--update` zieht
Hub-Änderungen herüber; danach den Pin oben nachziehen.

## ⚠️ Einbahnstraße

Dateien unter `vendor/` werden **nie lokal editiert**. Änderungen gehören nach
`MacPCAN/src/` und kommen von dort per `tools/check-updater-sync.sh --update`
zurück. QTmux-spezifisches liegt außerhalb: `MacPcanPlugin.cpp` (Qt-Adapter,
`ITerminalBackend`-Anbindung) und `CanText.h`.
