# qml/ — Oberfläche

> Schicht-Datei: Claude Code lädt sie, wenn in `qml/` gearbeitet wird. Die Absätze stammen
> wörtlich aus „Design 1a/2a (GUI-Umbau)“ der Wurzel-`CLAUDE.md` (ausgelagert 2026-11-19); dort
> bleiben Herkunft des Designs und der offene Owner-Durchklick.

**Randbedingungen (dauerhaft):** Qt Quick Controls **Basic**, keine neuen Effekte,
Chrome-Farben nur über `Theme.*` — seit QTMUX-142 auch die Statusfarben (`Theme.ok/warn/danger/muted`;
der Wächter `test_statuscolors` lehnt Farbliterale in `qml/` ab), jede neue Zeichenkette in `qsTr`
+ beiden `.ts`.
**Bewusste Abweichungen von der Anweisung** (Begründungen in der Feature-Referenz): keine
macOS-Menü-Rollen (QtQuick.Controls kennt sie nicht) · Umbenennen/Gruppe wirken aufs
**Window** (QTMUX-83) · Export ist eine **Allowlist**, nicht „alles außer dem Vault" ·
listenartige Prefs-Seiten bleiben strukturell wie sie waren.

