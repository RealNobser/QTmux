# QTmux Shell-Integration für zsh (OSC 133 / FinalTerm-Prompt-Marker).
#
# Damit erkennt QTmux den Befehls-Lebenszyklus: laufend / fertig / Exit-Code,
# und meldet "Aufmerksamkeit", wenn ein Befehl in einer nicht-fokussierten
# Session fertig wird.
#
# Installation: in ~/.zshrc ergänzen:
#     source /pfad/zu/qtmux/shell-integration/qtmux.zsh

# Prozent-Kodierung für die OSC-7-URL. Läuft in einer Subshell, damit LC_ALL=C lokal
# bleibt: nur so werden Nicht-ASCII-Zeichen BYTEWEISE kodiert ("ä" -> %C3%A4). Ohne
# externe Werkzeuge, damit die Integration auch in kargen Umgebungen trägt.
_qtmux_urlencode() (
    LC_ALL=C
    str="$1"
    while [ -n "$str" ]; do
        safe="${str%%[!a-zA-Z0-9/:_.~-]*}"   # längstes unbedenkliches Stück
        printf '%s' "$safe"
        str="${str#"$safe"}"
        if [ -n "$str" ]; then
            # Erstes Byte des Rests kodieren. Die Maske ist Pflicht: manche Shells
            # (bash) liefern für Bytes >= 0x80 eine NEGATIVE Zahl (signed char) und
            # schrieben sonst "%FFFFFFFFFFFFFFC3" statt "%C3" — bei jedem Umlaut.
            byte=$(printf '%d' "'$str")
            printf '%%%02X' "$((byte & 0xFF))"
            str="${str#?}"
        fi
    done
)

# OSC 7: die Shell meldet ihr Arbeitsverzeichnis selbst (QTMUX-108). Der einzige Weg,
# der auch über ssh und in Containern stimmt — dort kennt QTmux den Ort sonst nicht.
_qtmux_cwd() {
    printf '\e]7;file://%s%s\a' \
        "${HOST:-${HOSTNAME:-$(hostname 2>/dev/null)}}" "$(_qtmux_urlencode "$PWD")"
}

_qtmux_precmd() {
    local exit=$?
    printf '\e]133;D;%s\a' "$exit"   # vorheriger Befehl beendet (mit Exit-Code)
    printf '\e]133;A\a'              # neue Prompt beginnt
    _qtmux_cwd                       # Arbeitsverzeichnis melden (OSC 7)
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
