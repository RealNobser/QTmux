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
| `qtmux-emit.ps1` / `qtmux-emit.cmd` | Windows | Ereignis **aus einem Hook** via MCP `post_event` (HTTP) — siehe unten |

Installation (interaktiv): die jeweilige Datei im Shell-Profil sourcen
(`~/.bashrc`, `~/.zshrc`, `$PROFILE`), bzw. die `.cmd` in ein PATH-Verzeichnis legen.

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

macOS/Linux analog mit einem kleinen `curl`-Aufruf auf `post_event` (siehe `docs/MCP.md`).

Der benachrichtigte Agent (selbst MCP-Client) ruft `subscribe_events` und dann in einer
Schleife `wait_for_events` auf und erhält `sourceSessionId` der meldenden Session, um dort
per `send_text`/`read_screen`/`focus_session` weiterzuarbeiten.
