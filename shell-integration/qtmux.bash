# QTmux Shell-Integration für bash (OSC 133 / FinalTerm-Prompt-Marker).
#
# Damit erkennt QTmux den Befehls-Lebenszyklus: laufend / fertig / Exit-Code,
# und meldet "Aufmerksamkeit", wenn ein Befehl in einer nicht-fokussierten
# Session fertig wird.
#
# Installation: in ~/.bashrc ergänzen:
#     source /pfad/zu/qtmux/shell-integration/qtmux.bash

_qtmux_prompt() {
    local exit=$?
    printf '\e]133;D;%s\a' "$exit"   # vorheriger Befehl beendet (mit Exit-Code)
    printf '\e]133;A\a'              # neue Prompt beginnt
}
# precmd-Äquivalent über PROMPT_COMMAND (bestehendes erhalten):
PROMPT_COMMAND="_qtmux_prompt${PROMPT_COMMAND:+; $PROMPT_COMMAND}"

# Befehlsstart über DEBUG-Trap (feuert vor jedem Kommando):
trap '_qtmux_in_cmd=1; printf "\e]133;C\a"' DEBUG

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
