# QTmux Shell-Integration für zsh (OSC 133 / FinalTerm-Prompt-Marker).
#
# Damit erkennt QTmux den Befehls-Lebenszyklus: laufend / fertig / Exit-Code,
# und meldet "Aufmerksamkeit", wenn ein Befehl in einer nicht-fokussierten
# Session fertig wird.
#
# Installation: in ~/.zshrc ergänzen:
#     source /pfad/zu/qtmux/shell-integration/qtmux.zsh

_qtmux_precmd() {
    local exit=$?
    printf '\e]133;D;%s\a' "$exit"   # vorheriger Befehl beendet (mit Exit-Code)
    printf '\e]133;A\a'              # neue Prompt beginnt
}
_qtmux_preexec() {
    printf '\e]133;C\a'              # Befehl startet
}

autoload -Uz add-zsh-hook
add-zsh-hook precmd  _qtmux_precmd
add-zsh-hook preexec _qtmux_preexec

# Notification aus Skripten/Agenten senden:  qtmux-notify "Text"
qtmux-notify() { printf '\e]9;%s\a' "$*"; }

# Strukturiertes Agenten-Ereignis für die Inter-Agenten-Benachrichtigung senden:
#     qtmux-event done|question|error|info "Text"
# Gedacht für Agenten-Hooks (z. B. Claude Codes Stop-Hook -> 'qtmux-event done "…"',
# Notification-Hook -> 'qtmux-event question "…"'). Ein abonnierender Agent in einer
# anderen Session wird per MCP (wait_for_events) benachrichtigt und erhält diese
# Session-ID, um hier weiterzuarbeiten. ';' im Text wird durch ',' ersetzt (Trenner).
qtmux-event() {
    local kind="$1"; shift
    local text="$*"
    printf '\e]777;qtmux-event;%s;%s\a' "$kind" "${text//;/,}"
}
