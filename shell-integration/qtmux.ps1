# QTmux Shell-Integration für PowerShell (Windows: pwsh & Windows PowerShell 5.1).
#
# Bietet — analog zu qtmux.bash/qtmux.zsh —:
#   - OSC-133-Prompt-Marker (Befehl fertig/Exit-Code -> Sidebar-Status),
#   - qtmux-notify  "Text"                       (Desktop-/App-Notification, OSC 9),
#   - qtmux-event   done|question|error|info "Text"  (Agenten-Ereignis, OSC 777).
#
# Installation: in das PowerShell-Profil ($PROFILE) aufnehmen:
#     . "C:\pfad\zu\qtmux\shell-integration\qtmux.ps1"
#
# Hinweis: ESC/BEL und ASCII passieren ConPTY unverändert (die bekannte PS-5.1-Umlaut-
# Verzerrung betrifft nur Nicht-ASCII-Text, nicht die OSC-Steuersequenzen).
#
# WICHTIG für KI-Agenten-HOOKS (z. B. Claude Codes Stop-/Notification-Hook): Der STDOUT
# eines Hooks wird vom Agenten GEKAPSELT und erreicht das Terminal NICHT — eine OSC-
# Ausgabe aus einem Hook kommt also nicht bei QTmux an. Hooks daher über das MCP-Tool
# 'post_event' melden (HTTP an 127.0.0.1, siehe shell-integration/qtmux-emit.ps1 und
# docs/MCP.md). Die OSC-Funktionen hier sind für die INTERAKTIVE Shell gedacht.

# Schreibt eine OSC-Sequenz (ESC ] <body> BEL) direkt auf das Konsolen-Handle.
function global:__qtmux_osc([string]$body) {
    $e = [char]27; $bel = [char]7
    [Console]::Write($e + ']' + $body + $bel)
}

# OSC-133-Prompt-Integration: bestehende prompt-Funktion umschließen (Befehlsende + neue
# Prompt). Der Befehls-START (OSC 133;C) bräuchte einen preexec-Hook (PSReadLine) und ist
# hier bewusst ausgelassen — Befehlsende/Exit-Code genügen für den Sidebar-Status.
if (-not (Test-Path variable:global:__qtmux_prevPrompt)) {
    $global:__qtmux_prevPrompt = $function:prompt
}
# OSC 7: das Arbeitsverzeichnis selbst melden (QTMUX-108).
# 🔑 Auf Windows ist das der EINZIGE Weg: 'Set-Location' ist ein PowerShell-*Provider*-
# Begriff und ruft kein SetCurrentDirectory — das Win32-Arbeitsverzeichnis des Prozesses
# bleibt dort stehen, wo die Shell gestartet ist. Jedes Werkzeug, das es ausliest (auch
# QTmux), sieht deshalb dauerhaft das Startverzeichnis.
function global:__qtmux_cwd {
    $loc = Get-Location
    # Nur echte Dateisystem-Pfade melden — in 'HKLM:\...' gibt es kein Verzeichnis.
    if ($loc.Provider.Name -ne 'FileSystem') { return }
    $p = $loc.ProviderPath -replace '\\', '/'
    # Segmentweise kodieren, damit die '/'-Trenner erhalten bleiben; der Doppelpunkt des
    # Laufwerks wird dabei zu %3A und von QTmux zurückgewandelt.
    $enc = ($p -split '/' | ForEach-Object { [Uri]::EscapeDataString($_) }) -join '/'
    if (-not $enc.StartsWith('/')) { $enc = '/' + $enc }   # Windows: /C%3A/Users/...
    __qtmux_osc "7;file://$([Environment]::MachineName)$enc"
}

function global:prompt {
    $ec = if ($?) { 0 } else { 1 }
    if ($null -ne $LASTEXITCODE) { $ec = $LASTEXITCODE }
    __qtmux_osc "133;D;$ec"   # vorheriger Befehl beendet (mit Exit-Code)
    __qtmux_osc "133;A"       # neue Prompt beginnt
    __qtmux_cwd               # Arbeitsverzeichnis melden (OSC 7)
    if ($global:__qtmux_prevPrompt) { & $global:__qtmux_prevPrompt }
    else { "PS $($executionContext.SessionState.Path.CurrentLocation)$('>' * ($nestedPromptLevel + 1)) " }
}

# Notification aus Skripten/Agenten:  qtmux-notify "Text"
function global:qtmux-notify {
    [CmdletBinding()] param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Text)
    __qtmux_osc "9;$([string]::Join(' ', $Text))"
}

# Agenten-Ereignis (Inter-Agenten-Benachrichtigung) für die interaktive Shell:
#     qtmux-event done|question|error|info "Text"
# ';' im Text wird durch ',' ersetzt (Feldtrenner der OSC-Sequenz).
function global:qtmux-event {
    [CmdletBinding()] param(
        [Parameter(Mandatory=$true, Position=0)][string]$Kind,
        [Parameter(ValueFromRemainingArguments=$true)][string[]]$Text)
    $t = ([string]::Join(' ', $Text)) -replace ';', ','
    __qtmux_osc "777;qtmux-event;$Kind;$t"
}
