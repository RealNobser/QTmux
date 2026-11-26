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

## Zwei Protokollfassungen und MCP-Tasks (QTMUX-162, 2026-11-26)

Mechanik in [docs/feature-referenz.md](../../docs/feature-referenz.md), Abschnitt
„MCP-Server“, Vertrag nach außen in [docs/MCP.md](../../docs/MCP.md). Hier die Fallen:

- 🔴 **Den alten Weg nicht „mitmodernisieren“.** Claude Code erkennt einen alten Server
  daran, dass `server/discover` mit HTTP 200 und `-32601` antwortet, und fällt dann auf
  `initialize` zurück (Mitschnitt 2026-11-26). Wer auf dem alten Weg unbekannte Methoden auf
  404 umstellt oder `resultType` einfügt, ändert, was alte Clients sehen. Wächter:
  `legacyInitializeIsByteIdentical`, `legacyToolsListOnlyJustifiedChanges` in
  `tests/tst_mcphttp.cpp` gegen `tests/data/mcp/*-61ddc5a.json`. Eine neue Zeile in der
  alten Werkzeugliste gehört ausdrücklich in die Liste der begründeten Zusätze dort.
- 🔴 **Nach `sendHttpJson` keine Referenz in `m_buffers` mehr benutzen** (Begründung im
  Kommentar am Anfang von `onReadyRead`); Eintrag per Schlüssel entfernen.
- ⚠️ **Ein Messaufbau, der den Prozessbaum bricht, misst die Besitzprüfung nicht.** QTmux
  ordnet den Anrufer über den Prozess am Client-Port zu. Ein Mitschnitt-Proxy davor muss
  darum **in** der Sitzung als Kind ihrer Shell laufen (`proxy … & claude …`). Mit
  `( proxy & )` hängt er an launchd, und jeder Task endet mit „Keine eigene Session“ — das
  sah am 2026-11-26 zuerst wie ein Fehler des Servers aus.
- ⚠️ **Tests mit `QLocalServer` scheitern an einem langen `TMPDIR`** (`QLocalServer::listen:
  Name error`, der Unix-Socket-Pfad ist auf gut 100 Zeichen begrenzt). Betroffen:
  `test_mcpmessage`, `test_agentinbox`; mit 138 Zeichen rot an altem und neuem Stand, mit
  kurzem `TMPDIR` grün. Kein Befund am Code.
- 🔑 **Wann ein Task „lebt“, ist gemessen, nicht gewählt:** Claude Code folgt
  `pollIntervalMs` aus jeder Antwort, auch während langer Züge und `/compact`. Wer den Takt
  ändert, ändert damit die Lebendschwelle (3 Takte) und die Frist für den Rückfall nicht
  abgeholter Posteingänge mit.
- 🔑 **Jede Anfrage bestimmt ihren Aufrufer selbst** (`m_callerSessionId`), auch ein
  Stapel des alten Wegs. Ein veralteter Wert gab vorher einem Stapel den Absender und die
  `message_status`-Auskunft der vorigen Anfrage.
- ⚠️ **Zahlen aus JSON nie ungeprüft in `quint64` wandeln** (`seqArgument`): Ein `double`
  außerhalb des Wertebereichs ist undefiniertes Verhalten; auf arm64 kam 1,84·10^19 heraus.
- ⚠️ **Tests mit „nackter Shell“** brauchen einen echten Prozess ohne Kinder
  (`addBareShellSession` in `tst_mcphttp.cpp` startet dafür `/bin/sleep`); eine
  Mitschnitt-Sitzung mit PID −1 gilt nie als nackt.
- 🔑 **Ein Nachrichtenbeleg trägt den Text nur, solange er ihn braucht** (im Puffer bzw. bis
  der Client abholt). Vorher blieben je abgelehnter 3,9-MB-Nachricht rund 16 MB eine
  Stunde liegen (gemessen: +321 MB RSS nach 20 Aufrufen).
