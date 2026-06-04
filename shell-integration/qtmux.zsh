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
