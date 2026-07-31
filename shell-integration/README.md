# QTmux Shell-Integration

Optionale Skripte, mit denen Shells und KI-Agenten QTmux Ereignisse melden:
Befehls-Lebenszyklus (Sidebar-Status), Notifications und **Agenten-Ereignisse** für die
Inter-Agenten-Benachrichtigung (ein Agent wird benachrichtigt, wenn ein Agent in einer
anderen Session **fertig** ist oder eine **Frage** hat).

## Dateien

| Datei | Plattform | Zweck |
|---|---|---|
| `qtmux.bash` / `qtmux.zsh` | macOS/Linux | OSC-133-Prompt-Marker + `qtmux-notify` + `qtmux-event` (interaktive Shell) |
| `qtmux.ps1` | Windows (pwsh & PS 5.1) | dito für PowerShell (Prompt-Marker D/A + `qtmux-notify` + `qtmux-event`) |
| `qtmux-event.cmd` | Windows (cmd) | Einzelaufruf `qtmux-event done\|question\|error\|info "Text"` (OSC) |
| `qtmux-emit.sh` | macOS/Linux | Ereignis **aus einem Hook** via MCP `post_event` (HTTP) — siehe unten |
| `qtmux-emit.ps1` / `qtmux-emit.cmd` | Windows | dito für Windows |
| `qtmux-wait.sh` | macOS/Linux | **Warten** auf ein Ereignis; blockiert und endet beim ersten Treffer — siehe unten |
| `qtmux-wait.ps1` / `qtmux-wait.cmd` | Windows | dito für Windows |

## Woher bekomme ich diese Dateien?

**Als Anwender einer Installation** (DMG, MSI, portables ZIP, AppImage) stecken sie **im
Programm** — hol sie dir an einen stabilen Ort:

```bash
qtmux --install-shell-integration            # Standardziel (s. Tabelle)
qtmux --install-shell-integration /mein/ort  # eigenes Verzeichnis
```

Der Befehl schreibt alle Dateien, setzt unter Unix das Ausführbar-Bit und nennt anschließend
den Pfad **samt fertiger Hook-Zeile zum Kopieren**. Er öffnet kein Fenster. Ein zweiter Aufruf
ist gefahrlos: inhaltsgleiche Dateien werden nicht angefasst, veränderte auf die zur laufenden
Version passende Fassung zurückgesetzt.

| Plattform | Standardziel |
|---|---|
| macOS | `~/Library/Application Support/QTmux/shell-integration` |
| Linux | `~/.local/share/QTmux/shell-integration` |
| Windows | `%LOCALAPPDATA%\QTmux\shell-integration` |

> **Warum nicht einfach im Installationsverzeichnis?** Weil das AppImage keines hat: Es wird
> bei jedem Start unter einem anderen `/tmp/.mount_XXXXXX` gemountet — ein Hook-Eintrag, der
> dorthin zeigt, überlebt keinen Neustart. Der Weg über das Programm gilt deshalb auf allen
> drei Plattformen gleich.

> **Windows: Die Ausgabe kommt asynchron.** QTmux ist ein GUI-Programm (zwingend, sonst
> erben die Terminal-Sitzungen die Konsole) — die Shell wartet deshalb nicht auf das
> Prozessende, und der Prompt ist sofort zurück. Die Dateien werden trotzdem vollständig
> geschrieben, die Ausgabe erscheint Sekundenbruchteile später. Wer sie sicher einfangen
> will, nimmt `cmd`: `cmd /c "qtmux.exe --install-shell-integration > ausgabe.txt 2>&1"`.
> PowerShells `>` schließt seine Datei zu früh und bleibt leer — die Skripte landen aber
> auch dann korrekt am Ziel.

**Als Entwickler** liegen dieselben Dateien hier im Repo; sie sind zugleich die Quelle der
eingebetteten Ressource, es gibt also keine zweite Fassung, die veralten könnte.

Danach: die zur Shell passende Datei im Profil sourcen (`~/.bashrc`, `~/.zshrc`, `$PROFILE`),
bzw. die `.cmd` in ein PATH-Verzeichnis legen.

## Zwei Wege, ein Ereignis zu melden

1. **OSC (`qtmux-event`)** — schreibt eine Terminal-Steuersequenz
   (`OSC 777 ; qtmux-event ; <kind> ; <text>`). Ideal für die **interaktive Shell** und
   für Tools, die in ihr eigenes TTY schreiben.
2. **MCP (`post_event`)** — HTTP-Aufruf an den eingebetteten MCP-Server
   (`127.0.0.1:7345`). Der Weg für **alles Out-of-band**.

### ⚠️ Für KI-Agenten-Hooks immer MCP (`post_event`) nutzen

Der **stdout eines Hooks** (z. B. Claude Codes Stop-/Notification-Hook) wird vom Agenten
**gekapselt** und erreicht das Terminal **nicht** — eine OSC-Ausgabe aus einem Hook kommt
also nicht bei QTmux an. Hooks müssen das Ereignis daher über `post_event` melden. Dafür
gibt es `qtmux-emit.ps1`/`.cmd` (proxy-unabhängig, liest `$QTMUX_SESSION_ID`):

Beispiel — Claude Code, `~/.claude/settings.json` (oder projektlokal `.claude/settings.json`):

```json
{
  "hooks": {
    "Stop": [
      { "hooks": [ { "type": "command",
        "command": "powershell -NoProfile -ExecutionPolicy Bypass -File C:/pfad/zu/qtmux/shell-integration/qtmux-emit.ps1 done \"Aufgabe erledigt\"" } ] }
    ],
    "Notification": [
      { "hooks": [ { "type": "command",
        "command": "powershell -NoProfile -ExecutionPolicy Bypass -File C:/pfad/zu/qtmux/shell-integration/qtmux-emit.ps1 question \"Brauche eine Entscheidung\"" } ] }
    ]
  }
}
```

macOS/Linux analog mit `qtmux-emit.sh done "Aufgabe erledigt"`. **Nimm in beiden Fällen
das Skript, nicht einen `curl`-Einzeiler im Hook**: Der bräuchte eine dreifach
verschachtelte Maskierung (JSON in JSON in der Shell), und geht die schief, feuert der
Hook **still** nicht — für den Empfänger ununterscheidbar von „gerade passiert nichts".

## ⚠️ Empfangen: ein arbeitender Agent hört nicht zu

`wait_for_events` ist ein **Abholen** — es wirkt nur, *während* der Empfänger darin
wartet. Ein KI-Agent tut das praktisch nie: Solange er arbeitet, sitzt er in keinem
Werkzeugaufruf, und ein MCP-Server kann in einen laufenden Zug nicht hineinreichen. Ein
eingerichteter Stop-Hook auf der Senderseite ist deshalb nur die **halbe** Bedingung.

Dafür gibt es `qtmux-wait.*`: Es wartet **stellvertretend in einem Hintergrundprozess**
und **endet**, sobald ein Ereignis vorliegt — und das Ende eines Hintergrundbefehls ist
die eine Stelle, an der die Agenten-Umgebung einen arbeitenden Agenten weckt.

```bash
qtmux-wait.sh --sessions 2,3 --kinds done,question &   # im Hintergrund starten!
```

Ausgabe bei Treffer: `QTMUX EVENT seq=<n>` plus das Ereignis als JSON (enthält
`sourceSessionId`, um dort per `send_text`/`read_screen`/`focus_session` weiterzuarbeiten).
Ohne Treffer endet es nach `--max-wait` (Vorgabe ~50 min) mit `QTMUX TIMEOUT seq=<n>`.
Das `seq=` ist der Cursor für den nächsten Wächter (`--after <n>`) — so bleibt zwischen
zwei Wartephasen keine Lücke. Einzelheiten und der saubere Einstiegs-Cursor:
[`docs/MCP.md`](../docs/MCP.md).
