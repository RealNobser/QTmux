# QTmux — Architektur-Landkarte (Vollanalyse 2026-08-01)

> **Wann lesen:** bevor du an `qml/Main.qml` strukturell arbeitest oder einen Refactoring-
> Vorschlag machst. Die Analyse war **nicht beauftragt** — sie ist Befund, kein Auftrag.
> Ausgelagert am 2026-08-07 aus der `CLAUDE.md` (dort stand sie in jeder Session im Kontext,
> gebraucht wird sie nur bei Umbauten).

## Befund

Die **C++-Seite ist sauber**: Gui-freier Core bestätigt, keine Include-Zyklen, keine
Lifetime-Probleme.

Die Schulden sitzen in **`qml/Main.qml`** — ~4.700 LOC, 136 Funktionen: Split-Baum,
Layout-Persistenz, Aggregationslogik als **ungetestetes JS**, zwei divergierende
Layout-Serialisierer.

## Abbaupfad (in dieser Reihenfolge)

1. Sidebar (~735 LOC) und Inline-Dialoge (~945 LOC) in eigene QML-Dateien ziehen.
2. Layout-Baum und Persistenz als **testbare C++-Klasse** in `core`.
3. Damit entfällt die QML-Brücke — **13 der 39 MCP-Tools brauchen heute die geladene UI**,
   weshalb der `McpServer` keinen einzigen Test hat. **25 Tools wären schon jetzt testbar.**

## Kleinere Punkte

- `Session` ist ein God-Object (~40 Member). Extraktionskandidaten: `AgentDetection`,
  `LoginAutomation`, `CwdTracker`.
- 22 attached `ToolTip` über [IconToolButton.qml](../qml/Ui/IconToolButton.qml) statt
  `AppToolTip`.
- Statusfarben-Literale ~10× dupliziert — Kandidat für ein `StatusColors`-Singleton.
- `SessionModel::sessionById` fehlt `Q_INVOKABLE`.

## Bereits umgesetzt

`Theme.accentText` statt hartem Weiß (6 Stellen).
