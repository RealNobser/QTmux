# src/server/ — MCP-Server

> Schicht-Datei: Claude Code lädt sie, wenn in `src/server/` gearbeitet wird. Ergänzung zur
> Regel „Fremde Pfade NIE mit `readAll()` lesen“ in der Wurzel-`CLAUDE.md` (Abschnitt
> „Konventionen“); die Absätze stammen wörtlich von dort (ausgelagert 2026-11-19).

  🔑 **Der MCP-Server ist frei von dieser Bauart** (2026-08-06 geprüft): Kein Werkzeug nimmt
  einen Pfad zum **Lesen**. `cwd` wird nur als Arbeitsverzeichnis gesetzt, `program`
  ausgeführt, `identity` unverändert an `ssh -i` durchgereicht. Gehärtet sind die vier
  Stellen, die einen fremden Pfad wirklich öffnen: `SettingsIo`-Import, Farbschema-Import,
  die beiden Scrollback-Dumps und der Vergleich in `ShellIntegration`.
  📋 Nachprüfen mit [tools/fuzz_mcp.py](tools/fuzz_mcp.py) — 51 Fälle, prüft **nach jeder
  Anfrage**, ob der Prozess noch lebt (sonst meldet so ein Lauf nur „es hat nicht geknallt",
  statt zu sagen, welcher Aufruf getötet hätte). ⚠️ Nur gegen eine **eigene** Instanz auf
  eigenem Port — QTmux hält Terminal-Sessions.
  ⚠️ **Nebenbefund für Diagnosen:** Trotz 30 GB gab es **keinen** jetsam-Eintrag im
  macOS-Log. Wer einen Prozesstod mit „kein OOM laut Log" ausschließt, schließt zu früh aus.
