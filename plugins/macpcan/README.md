# MacPCAN — CAN-Bus als QTmux-Terminal-Backend (nur macOS)

Plugin nach dem QTmux-SDK (`src/plugins/QTmuxPlugin.h`, IID `com.qtmux.PluginInterface/1.0`),
entstanden mit QTMUX-8/9. Diese Datei ist die Einzelquelle für die Plugin-Details; die
Projekt-`CLAUDE.md` verweist nur hierher.

## Aufbau

- Zwei Backend-Typen: **`pcan`** (echte Hardware) und **`pcan-mock`** (Demo, ohne Hardware
  vorführbar). Bewusst **getrennt** — es gibt keinen stillen Fallback auf Mock, sonst hält man
  eine funktionierende Demo für eine funktionierende Messung.
- Vendorte, Qt-freie Schicht unter `vendor/` (Namespace `mac_pcan`).
- **PCBUSB liegt nicht im Repo.** CMake findet die Bibliothek über `QTMUX_PCBUSB_DIR`; fehlt
  sie, wird das Plugin **still übersprungen** (der Build bleibt grün). dylib + Lizenz werden
  ins App-Bundle kopiert, rpath `@loader_path/../Frameworks`.

## Terminal-UX

Ausgabe im candump-Stil; `<hexid> b0 b1 …` sendet einen Frame; Befehle `baud <rate>`, `help`,
`clear`, `quit`.

## Fallen

- ⚠️ **Nur ein Handle pro PCAN-Kanal.** Eine wiederhergestellte Session belegt den Kanal und
  blockiert damit jede weitere.
- ⚠️ PCBUSB meldet einen Kanal **ohne** angeschlossene Hardware optimistisch als „verbunden";
  auffällig wird das erst daran, dass RX leer bleibt.

## Offen (v1)

CAN-FD · ID-Filter · Konfigurationsdialog · DBC-Decoding.
