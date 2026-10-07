#!/usr/bin/env bash
# Signiert ein fertig deploytes .app-Bundle (macdeployqt-Ausgabe) mit der
# Code-Signing-Identität der Familie — innen nach außen, ohne --deep.
#
# Warum überhaupt: Eine Ad-hoc-Signatur hat als Designated Requirement ihren
# CDHash, und der ändert sich mit JEDEM Build. tccd verwirft danach alle
# Datenschutz-Erlaubnisse („Failed to match existing code requirement") und
# fragt neu — bei QTmux hielt ein unbeantworteter Downloads-Dialog eine
# Agent-Session 4 h 48 min an (2026-10-07). Mit der Familien-Identität lautet
# die Requirement `identifier "<id>" and certificate root = H"<sha1>"` (bei
# einem selbst ausgestellten Zertifikat ist Root = Leaf) und überlebt Updates.
#
# Hub liefert MECHANIK (dieses Skript), Produkt liefert BEDEUTUNG (welches
# Bundle, welche Entitlements). Doku: docs/codesign.md.
#
# Ablauf:
#   1. Identität suchen: eigener Schlüsselbund unter ~/.keys/codesign/
#      (nie der Login-Schlüsselbund), entsperrt per Passwortdatei.
#      Fehlt etwas → Rückfall auf ad-hoc mit LAUTER Warnung (CI und fremde
#      Maschinen bauen weiter). FAMILY_CODESIGN=require macht daraus einen
#      Abbruch, FAMILY_CODESIGN=adhoc erzwingt ad-hoc.
#   2. Innen nach außen signieren: jede lose Mach-O-Datei (dylibs, Plugins,
#      Hilfsprogramme) und jedes eingebettete Bundle (.framework, .appex,
#      .app, .plugin, .bundle, .xpc), tiefste Pfade zuerst; zuletzt das
#      äußere Bundle. --deep wäre kein Ersatz: es signiert verschachtelte
#      Bundles OHNE ihre Entitlements (EmbyStudio-Erweiterung verlöre die
#      Sandbox) und verdeckt, was eigentlich signiert wurde.
#   3. Prüfen: codesign --verify --deep --strict, und die Requirement muss zur
#      Betriebsart passen (Identität → certificate-Klausel, ad-hoc → cdhash).
#
# Keine Hardened Runtime: Sie ist nur für die Notarisierung nötig, und ihre
# Library-Validation verlangt eine Team-ID, die ein selbst ausgestelltes
# Zertifikat nicht hat. TCC braucht sie nicht. Verhalten der Apps bleibt wie
# bei der bisherigen Ad-hoc-Signatur.
#
# ⚠️ Keine GUI-Dialoge: entsperrt wird mit dem Passwort aus der Datei (über
# stdin an `security -i`, nie als Argument), die
# Partitionsliste des Schlüssels erlaubt codesign (gesetzt beim Anlegen,
# codesign-identity-create.sh). Jeder security/codesign-Aufruf läuft mit
# hartem Zeitlimit; ein Zeitablauf ist ein ABBRUCH, keine Entwarnung.
# Der Schlüsselbund wird danach bewusst NICHT wieder gesperrt: ein paralleler
# Bau, dem er unter den Füßen gesperrt wird, ist genau die Dialog-Falle.
#
# Aufruf:
#   sign-bundle.sh [--entitlements <pfad-im-bundle>=<plist>]... \
#                  [--app-entitlements <plist>] <Bundle.app>
#   z. B. --entitlements Contents/PlugIns/EmbyThumbnail.appex=quicklook/x.entitlements
#
# Umgebung:
#   FAMILY_CODESIGN           auto (Vorgabe) | require | adhoc
#   FAMILY_CODESIGN_DIR       ~/.keys/codesign
#   FAMILY_CODESIGN_KEYCHAIN  $FAMILY_CODESIGN_DIR/family-codesign.keychain-db
#   FAMILY_CODESIGN_PWFILE    $FAMILY_CODESIGN_DIR/keychain.pw
#   FAMILY_CODESIGN_IDENTITY  SHA-1 oder CN (Vorgabe: erste Code-Signing-
#                             Identität im Schlüsselbund)
#
# Letzte Ausgabezeile (maschinenlesbar):
#   SIGNATUR: identity <sha1>   bzw.   SIGNATUR: adhoc
set -euo pipefail

MODE="${FAMILY_CODESIGN:-auto}"
DIR="${FAMILY_CODESIGN_DIR:-$HOME/.keys/codesign}"
KC="${FAMILY_CODESIGN_KEYCHAIN:-$DIR/family-codesign.keychain-db}"
PWFILE="${FAMILY_CODESIGN_PWFILE:-$DIR/keychain.pw}"
WANT_ID="${FAMILY_CODESIGN_IDENTITY:-}"

ENT_PATHS=()   # bash 3.2 (macOS /bin/bash) kennt keine assoziativen Arrays
ENT_FILES=()
APP_ENT=""
APP=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --entitlements)
            [[ "${2:-}" == *=* ]] || { echo "FEHLER: --entitlements <pfad>=<plist>" >&2; exit 2; }
            ENT_PATHS+=("${2%%=*}"); ENT_FILES+=("${2#*=}"); shift 2 ;;
        --app-entitlements) APP_ENT="${2:-}"; shift 2 ;;
        -*) echo "FEHLER: unbekannte Option $1" >&2; exit 2 ;;
        *)  [[ -z "$APP" ]] || { echo "FEHLER: nur EIN Bundle je Aufruf" >&2; exit 2; }
            APP="$1"; shift ;;
    esac
done
[[ -n "$APP" && -d "$APP" ]] || { echo "FEHLER: Bundle fehlt: '${APP}'" >&2; exit 2; }
APP="$(cd "$APP" && pwd)"
for f in ${ENT_FILES[@]+"${ENT_FILES[@]}"} ${APP_ENT:+"$APP_ENT"}; do
    [[ -f "$f" ]] || { echo "FEHLER: Entitlements-Datei fehlt: $f" >&2; exit 2; }
done
case "$MODE" in auto|require|adhoc) ;; *)
    echo "FEHLER: FAMILY_CODESIGN='$MODE' (erlaubt: auto|require|adhoc)" >&2; exit 2 ;;
esac

# Zeitlimit ohne coreutils-timeout (fehlt auf macOS). SIGALRM → Exit 142.
t() { local s="$1"; shift; perl -e 'alarm shift; exec @ARGV' "$s" "$@"; }
timeout_abort() {
    echo "ABBRUCH: Zeitlimit bei '$1' — wartet ein unsichtbarer Dialog?" >&2
    echo "  Das Bundle ist NICHT (vollständig) signiert. Kein Rückfall, keine Entwarnung." >&2
    exit 3
}

fallback() {
    local why="$1"
    if [[ "$MODE" = require ]]; then
        echo "FEHLER: FAMILY_CODESIGN=require, aber $why" >&2
        exit 1
    fi
    {
        echo "########################################################################"
        echo "##  WARNUNG: AD-HOC-SIGNATUR — KEINE FAMILIEN-IDENTITÄT               ##"
        echo "########################################################################"
        echo "Grund: $why"
        echo "Folge: Die Designated Requirement ist der CDHash dieses Builds. macOS"
        echo "verwirft beim Anwender nach dem Update ALLE Datenschutz-Erlaubnisse"
        echo "(Downloads, Dokumente, …) und fragt neu. Für ein Release untauglich —"
        echo "auf der Release-Maschine mit Schlüsselbund wiederholen"
        echo "(oder FAMILY_CODESIGN=require setzen, damit es abbricht)."
        echo "########################################################################"
    } >&2
    SIGN_ID="-"
    KC_ARGS=()
}

SIGN_ID=""
KC_ARGS=()
SIGN_CN=""
if [[ "$MODE" = adhoc ]]; then
    echo "    FAMILY_CODESIGN=adhoc: bewusst ad-hoc signiert (kein Release-Artefakt)." >&2
    SIGN_ID="-"
elif [[ ! -f "$KC" ]]; then
    fallback "Schlüsselbund fehlt ($KC)"
elif [[ ! -r "$PWFILE" ]]; then
    fallback "Passwortdatei fehlt oder ist nicht lesbar ($PWFILE)"
else
    # Das Passwort geht über stdin an `security -i`, NIE als Argument: argv ist
    # für jeden lokalen Prozess per `ps` lesbar. printf ist ein Shell-Builtin,
    # erzeugt also selbst keinen Prozess mit dem Passwort in der Kommandozeile.
    # `security -i` endet bei falschem Passwort mit Exit ≠ 0 (gemessen: 51).
    pw="$(cat "$PWFILE")"
    case "$pw" in *'"'*|*'\'*|*$'\n'*)
        echo "FEHLER: Passwortdatei enthält \" oder \\ oder Zeilenumbruch — für security -i ungeeignet." >&2
        exit 1 ;;
    esac
    set +e
    printf 'unlock-keychain -p "%s" "%s"\n' "$pw" "$KC" | t 20 security -i >/dev/null; rc=$?
    set -e
    unset pw
    [[ "$rc" = 142 ]] && timeout_abort "security unlock-keychain"
    if [[ "$rc" != 0 ]]; then
        fallback "Schlüsselbund ließ sich nicht entsperren (Exit $rc)"
    else
        set +e
        ids="$(t 20 security find-identity -p codesigning "$KC")"; rc=$?
        set -e
        [[ "$rc" = 142 ]] && timeout_abort "security find-identity"
        # Zeilen der Form:  1) <SHA1> "<CN>" (CSSMERR_TP_NOT_TRUSTED)
        # „not trusted" ist bei einem selbst ausgestellten Zertifikat der
        # Normalfall und für codesign wie für TCC ohne Belang.
        line="$(printf '%s\n' "$ids" | sed -n '/Matching identities/,/identities found/p' \
                | grep -E '^ *[0-9]+\) [0-9A-F]{40} "' \
                | { if [[ -n "$WANT_ID" ]]; then grep -F -- "$WANT_ID"; else cat; fi; } \
                | head -1 || true)"
        if [[ -z "$line" ]]; then
            fallback "keine Code-Signing-Identität${WANT_ID:+ '$WANT_ID'} in $KC"
        else
            SIGN_ID="$(printf '%s' "$line" | sed -E 's/^ *[0-9]+\) ([0-9A-F]{40}) .*/\1/')"
            SIGN_CN="$(printf '%s' "$line" | sed -E 's/^[^"]*"([^"]*)".*/\1/')"
            KC_ARGS=(--keychain "$KC")
        fi
    fi
fi

if [[ "$SIGN_ID" = "-" ]]; then
    echo "    Signatur: ad-hoc"
else
    echo "    Signatur: $SIGN_CN ($SIGN_ID)"
fi

is_macho() {
    case "$(head -c4 "$1" 2>/dev/null | xxd -p)" in
        feedface|feedfacf|cefaedfe|cffaedfe|cafebabe|bebafeca) return 0 ;;
    esac
    return 1
}

# Haupt-Executable des innersten umschließenden Bundles? Dann signiert es
# der Bundle-Lauf mit, als lose Datei wäre es doppelt (und für Frameworks
# falsch — die Signatur gehört an Versions/<X>, nicht an die Datei allein).
is_bundle_main() {
    local f="$1" b="" p="$1" exe name
    while [[ "$p" != "$APP" && "$p" != "/" ]]; do
        p="$(dirname "$p")"
        case "$p" in *.app|*.appex|*.framework|*.plugin|*.bundle|*.xpc) b="$p"; break ;; esac
    done
    [[ -z "$b" || "$b" = "$APP" ]] && b="$APP"
    case "$b" in
        *.framework)
            name="$(basename "$b" .framework)"
            [[ "$f" == "$b"/Versions/*/"$name" && "$(basename "$f")" = "$name" ]] && return 0 ;;
        *)
            exe="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$b/Contents/Info.plist" 2>/dev/null || true)"
            [[ -n "$exe" && "$f" = "$b/Contents/MacOS/$exe" ]] && return 0 ;;
    esac
    return 1
}

entitlements_for() {
    local rel="${1#$APP/}" i
    for i in ${ENT_PATHS[@]+"${!ENT_PATHS[@]}"}; do
        [[ "${ENT_PATHS[$i]%/}" = "$rel" ]] && { printf '%s' "${ENT_FILES[$i]}"; return 0; }
    done
    return 0
}

sign_one() {
    local path="$1" ent="$2" rc
    local args=(--force --sign "$SIGN_ID" --timestamp=none)
    [[ ${#KC_ARGS[@]} -gt 0 ]] && args+=("${KC_ARGS[@]}")
    [[ -n "$ent" ]] && args+=(--entitlements "$ent")
    set +e
    t 120 codesign "${args[@]}" "$path" 2>"$WORK/cs.err"; rc=$?
    set -e
    [[ "$rc" = 142 ]] && timeout_abort "codesign ${path#$APP/}"
    if [[ "$rc" != 0 ]]; then
        echo "FEHLER: codesign ${path#$APP/} (Exit $rc):" >&2
        cat "$WORK/cs.err" >&2
        exit 1
    fi
}

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Arbeitsliste: Tiefe<TAB>Pfad. Tiefste zuerst — der Inhalt eines Bundles
# liegt immer tiefer als das Bundle selbst, also ist er vorher signiert.
: > "$WORK/items"
while IFS= read -r -d '' f; do
    is_macho "$f" || continue
    is_bundle_main "$f" && continue
    printf '%s\t%s\n' "$(tr -cd / <<<"$f" | wc -c | tr -d ' ')" "$f" >> "$WORK/items"
done < <(find "$APP" -type f -print0)
while IFS= read -r -d '' b; do
    [[ "$b" = "$APP" ]] && continue
    printf '%s\t%s\n' "$(tr -cd / <<<"$b" | wc -c | tr -d ' ')" "$b" >> "$WORK/items"
done < <(find "$APP" -type d \( -name '*.app' -o -name '*.appex' -o -name '*.framework' \
                              -o -name '*.plugin' -o -name '*.bundle' -o -name '*.xpc' \) -print0)

n=0
while IFS=$'\t' read -r _ p; do
    sign_one "$p" "$(entitlements_for "$p")"
    n=$((n + 1))
done < <(sort -t$'\t' -k1,1nr -k2,2 "$WORK/items")
sign_one "$APP" "$APP_ENT"
echo "    $n innere Objekte + Bundle signiert (innen → außen)"

# Riegel: Entitlements, die übergeben wurden, müssen auch angekommen sein.
for i in ${ENT_PATHS[@]+"${!ENT_PATHS[@]}"}; do
    target="$APP/${ENT_PATHS[$i]%/}"
    [[ -e "$target" ]] || { echo "FEHLER: --entitlements-Ziel fehlt im Bundle: ${ENT_PATHS[$i]}" >&2; exit 1; }
    got="$(codesign -d --entitlements :- "$target" 2>/dev/null || true)"
    [[ -n "$got" ]] || { echo "FEHLER: ${ENT_PATHS[$i]} trägt keine Entitlements." >&2; exit 1; }
done

set +e
t 300 codesign --verify --deep --strict "$APP" 2>"$WORK/verify.err"; rc=$?
set -e
[[ "$rc" = 142 ]] && timeout_abort "codesign --verify"
[[ "$rc" = 0 ]] || { echo "FEHLER: codesign --verify --deep --strict:" >&2; cat "$WORK/verify.err" >&2; exit 1; }

# Ad-hoc druckt die (implizite) Requirement als Kommentar: „# designated => cdhash …".
DR="$(codesign -d -r- "$APP" 2>&1 | sed -n 's/^#* *designated => //p')"
echo "    Designated Requirement: $DR"
if [[ "$SIGN_ID" = "-" ]]; then
    [[ "$DR" == *cdhash* ]] || { echo "FEHLER: ad-hoc, aber Requirement ohne cdhash?" >&2; exit 1; }
    echo "SIGNATUR: adhoc"
else
    sha_lc="$(printf '%s' "$SIGN_ID" | tr 'A-F' 'a-f')"
    [[ "$DR" == *"certificate "*"H\"$sha_lc\""* ]] \
        || { echo "FEHLER: Requirement nennt das Zertifikat $SIGN_ID nicht." >&2; exit 1; }
    echo "SIGNATUR: identity $SIGN_ID"
fi
