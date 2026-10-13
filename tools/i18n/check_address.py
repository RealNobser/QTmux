#!/usr/bin/env python3
"""German address guard (ctest I18n.GermanAddressesWithSie).

The German interface addresses the user with "Sie" (decision 2026-10-13),
or without any address where German reads better that way. In QTmux German is
the SOURCE language, so the texts live in two places and both are checked:

  catalogue  every translation of i18n/qtmux_de.ts -- singular and every
             plural form (numerusform), live and vanished/obsolete entries
  sources    every string literal under qml/, src/ and plugins/ (vendored
             code excluded) that contains a space -- this also covers texts
             that never pass through lupdate, e.g. the MCP tool descriptions,
             and a new qsTr() text before anyone ran update_translations.
             Literals joined by "+" or by C++ adjacency count as one text.

A text addresses the user with "du" when it contains a

  pronoun     du, dich, dir, dein*, euch, euer/eure*        (any case)
  verb        a 2nd-person-singular verb form from VERB_2SG ("klickst",
              "hast", "musst") -- only forms that are NOT also 3rd person
              ("lässt", "passt", "liest" are; with "du" the pronoun catches)
  imperative  a du-imperative from IMPERATIVE anywhere ("Lege", "Nutze",
              "beende"), or from IMPERATIVE_AT_START only at the start of a
              sentence and before an article/adverb -- those words are nouns
              too ("Stelle", "Teile", "Halte")

"Sie" is never counted, so "Sie werden nicht gespeichert" (3rd person) cannot
be a false hit. Mnemonics ("Du&plizieren") and HTML tags are removed first.
A 1st-person progress line ("Lade herunter: %1 …", "Warte auf …") is not an
imperative.

Every run first checks itself against SELF_CHECK (texts that must pass and
texts that must be caught) and against minimum sizes of what it read; a
mismatch is an ERROR -- a rule that went blind, or a scan that read nothing,
never reads as a clean result.

Output contract (the ctest probes match on it):
  "I18N-GUARD OK: German address: ..."     exit 0
  "I18N-GUARD FAIL: German address: ..."   exit 1, one line per finding
  "I18N-GUARD ERROR: German address: ..."  exit 3, the TOOL failed
argparse errors exit 2.

--probe CLASS adds one du-text of that class (the positive probes; their
ctests expect the FAIL line naming "probe"). --probe source feeds a two-line
qsTr() snippet through the literal extraction, so the source half is probed
end to end. --list prints every finding with its class and a count per class,
exit 0 (inventory, never a verdict).

Same rules and output contract as the address guards of the sibling products.
"""
from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_TS = ROOT / "i18n" / "qtmux_de.ts"
SOURCE_DIRS = ("qml", "src", "plugins")
SOURCE_SUFFIXES = {".qml", ".js", ".cpp", ".h"}
SOURCE_EXCLUDE = {"vendor", "third_party"}
# Positive controls: fewer than this means the scan read the wrong tree.
MIN_TRANSLATIONS = 100
MIN_SOURCE_FILES = 50
MIN_SOURCE_TEXTS = 500

PRONOUN = re.compile(r"\b(?:du|dich|dir|dein|deine|deinem|deinen|deiner|deines|"
                     r"euch|euer|eure|eurem|euren|eurer|eures)\b", re.I)

# 2nd person singular, unambiguous (never equal to the 3rd person form).
VERB_2SG = {
    "hast", "bist", "kannst", "willst", "musst", "möchtest", "sollst", "darfst",
    "wirst", "weißt", "klickst", "wählst", "siehst", "findest", "brauchst",
    "ziehst", "gibst", "nimmst", "öffnest", "speicherst", "legst", "lädst",
    "arbeitest", "sagst", "übernimmst", "anhängst", "einträgst", "blätterst",
    "startest", "drückst", "tippst", "suchst", "änderst", "markierst",
    "bekommst", "hältst", "stellst", "schreibst", "fügst", "entfernst",
    "erreichst", "zoomst", "entscheidest", "meldest", "beendest",
}

# du-imperatives that are neither nouns nor (without "ich") anything else.
IMPERATIVE = {
    "aktiviere", "beende", "beginne", "benutze", "blättere", "drück", "drücke",
    "entferne", "frag", "füge", "geh", "gehe", "gib", "hilf", "hol", "hole",
    "installiere", "klicke", "kopiere", "lade", "lass", "leg", "lege", "lies",
    "markiere", "melde", "nimm", "nutze", "öffne", "probier", "probiere", "prüfe",
    "richte", "schalte", "schau", "schick", "schicke", "schließe", "schreib",
    "schreibe", "setze", "sieh", "speichere", "starte", "tippe", "trage",
    "übernimm", "verwende", "vergiss", "versuche", "wähle", "warte", "wechsle",
    "wirf", "zieh", "ziehe",
}
# Elliptic 1st person in progress lines ("[Ich] lade … herunter…", "Warte auf
# Bestätigung"): not an address when the line ends in "…" or the word is
# followed by "auf" / "herunter".
PROGRESS = {"lade", "warte", "versuche"}
# Also nouns ("die Stelle", "Teile", "Halte"): only at the start of a sentence
# and before an article/adverb ("Stelle im Fenster … ein").
IMPERATIVE_AT_START = {"stelle", "teile", "halte"}
AFTER_START = re.compile(r"\s+(?:im|in|hier|dort|die|den|das|dem|dieselbe|denselben|"
                         r"dasselbe|ein|eine|einen|sie|ihn|es|alle|zuerst|jetzt)\b", re.I)
SENTENCE_START = re.compile(r"(?:^|[.!?:;·—–\n]\s*|[„“\"(]\s*)$")
WORD = re.compile(r"[A-Za-zÄÖÜäöüß]+")

# One string literal, and a chain of literals joined by "+" or adjacency.
LITERAL = r'"(?:[^"\\\n]|\\.)*"'
LITERAL_RE = re.compile(LITERAL)
CHAIN_RE = re.compile(LITERAL + r'(?:\s*\+?\s*' + LITERAL + r')*')

SELF_CHECK_PASS = [
    "Sie werden nicht gespeichert und erscheinen in keiner Einstellungsdatei.",
    "Bitte prüfen Sie, ob Sie die Datei lesen dürfen.",
    "Du&plizieren", "Besitze ich", "kostet aber einen Klick",
    "Klick: einschalten · Rechtsklick: Einstellungen",
    "Er lässt sich rückgängig machen; das passt.", "Liest alle Kataloge neu ein.",
    "Legen Sie ein Master-Passwort fest:", "Nutzen Sie close_pane/close_session.",
    "Wählen Sie oben einen anderen Endpunkt.", "Suche läuft…",
    "Lade herunter: %1 …", "Verbinde mit %1 …", "Warte auf Bestätigung am Gerät",
]
SELF_CHECK_FAIL = [
    ("Beste Farben aus deinem Besitz", "pronoun"),
    ("Wartet ein Agent auf eine Antwort von dir, darf er schlafen.", "pronoun"),
    ("Gedruckt wird erst, wenn Sie klickst.", "verb"),
    ("Hast Ja gesagt?", "verb"),
    ("Noch kein Vault vorhanden. Lege ein Master-Passwort fest:", "imperative"),
    ("Nutzen Sie close_pane oder beende die App in der Oberfläche.", "imperative"),
    ("Stelle im Fenster dieselbe Schrift ein.", "imperative"),
    ("Die Zeiten sind geschätzt. Warte, bis sie steht.", "imperative"),
]
# (text, expected joined literal) for the source half of the self-check.
SELF_CHECK_SOURCE = [
    ('qsTr("Erste Zeile, "\n    + "zweite Zeile.")', "Erste Zeile, zweite Zeile."),
    ('tool("x",\n  "Teil eins "\n  "Teil zwei")', "Teil eins Teil zwei"),
]
PROBES = {
    "pronoun": "probe: Nichts ändert sich, bis du übernimmst.",
    "verb": "probe: Gedruckt wird erst, wenn Sie dort klickst.",
    "imperative": "probe: Richte in den Einstellungen einen Anbieter ein.",
}
SOURCE_PROBE = ('description: qsTr("Noch keine Sitzung. Lege mit „Neu …“ "\n'
                '                  + "eine an.")')


class ToolError(Exception):
    pass


def clean(text: str) -> str:
    text = text.replace("&&", "\x00")
    text = re.sub(r"&(?=\w)", "", text)
    text = text.replace("\x00", "&")
    return re.sub(r"<[^>]+>", " ", text)


def classify(text: str) -> list[tuple[str, str]]:
    """Every du-address in one text as (class, word)."""
    t = clean(text)
    hits: list[tuple[str, str]] = []
    for m in PRONOUN.finditer(t):
        hits.append(("pronoun", m.group(0)))
    for m in WORD.finditer(t):
        w = m.group(0)
        lw = w.lower()
        before = t[:m.start()]
        if lw in VERB_2SG:
            hits.append(("verb", w))
        elif lw in IMPERATIVE:
            if re.search(r"\bich\s+$", before, re.I):
                continue                       # "ich lade" = 1st person
            if lw in PROGRESS and (t.rstrip().endswith("…")
                                   or re.match(r"\s+(?:auf|herunter)\b", t[m.end():])):
                continue                       # progress line, not an address
            hits.append(("imperative", w))
        elif lw in IMPERATIVE_AT_START and SENTENCE_START.search(before) \
                and AFTER_START.match(t, m.end()):
            hits.append(("imperative", w))
    return hits


def translations(ts: Path) -> list[tuple[str, str]]:
    """(where, text) for every German translation of the catalogue."""
    if not ts.is_file():
        raise ToolError(f"{ts} is not a file")
    try:
        root = ET.parse(ts).getroot()
    except ET.ParseError as e:
        raise ToolError(f"{ts}: {e}") from e
    out = []
    for ctx in root.iter("context"):
        name = ctx.findtext("name") or "?"
        for msg in ctx.iter("message"):
            tr = msg.find("translation")
            if tr is None:
                continue
            forms = [f.text or "" for f in tr.iter("numerusform")] or [tr.text or ""]
            out.extend((f"{ts.name} [{name}]", f) for f in forms)
    if len(out) < MIN_TRANSLATIONS:
        raise ToolError(f"{ts}: only {len(out)} translations read -- not a catalogue")
    return out


def literal_texts(code: str) -> list[tuple[int, str]]:
    """(line, text) for every literal chain in one source file."""
    out = []
    for m in CHAIN_RE.finditer(code):
        parts = LITERAL_RE.findall(m.group(0))
        text = "".join(p[1:-1] for p in parts)
        out.append((code.count("\n", 0, m.start()) + 1, text))
    return out


def source_texts(root: Path) -> list[tuple[str, str]]:
    """(where, text) for every prose literal under SOURCE_DIRS."""
    files = 0
    out = []
    for d in SOURCE_DIRS:
        base = root / d
        if not base.is_dir():
            raise ToolError(f"{base} is not a directory")
        for p in sorted(base.rglob("*")):
            if p.suffix not in SOURCE_SUFFIXES or not p.is_file():
                continue
            if SOURCE_EXCLUDE & set(p.relative_to(root).parts):
                continue
            try:
                code = p.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError) as e:
                raise ToolError(f"{p}: {e}") from e
            files += 1
            rel = p.relative_to(root).as_posix()
            # Only prose: a literal without a space is a key, path or id
            # (":/shaders/glyph.frag.qsb" would otherwise read as "frag").
            out.extend((f"{rel}:{line}", t) for line, t in literal_texts(code) if " " in t)
    if files < MIN_SOURCE_FILES or len(out) < MIN_SOURCE_TEXTS:
        raise ToolError(f"sources: only {files} files / {len(out)} texts read -- wrong tree?")
    return out


def self_check() -> None:
    for text in SELF_CHECK_PASS:
        if classify(text):
            raise ToolError(f"self-check: {text!r} must pass, got {classify(text)}")
    for text, cls in SELF_CHECK_FAIL:
        if cls not in {c for c, _ in classify(text)}:
            raise ToolError(f"self-check: {text!r} must be caught as {cls}, got {classify(text)}")
    for code, want in SELF_CHECK_SOURCE:
        got = [t for _, t in literal_texts(code)]
        if want not in got:
            raise ToolError(f"self-check: literal extraction of {code!r} gave {got}, want {want!r}")


def main() -> int:
    # Findings quote German text („…“, —); a legacy console code page must not
    # turn them into a crash that no probe pattern matches.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    ap = argparse.ArgumentParser(allow_abbrev=False, description=__doc__.splitlines()[0])
    ap.add_argument("--ts", type=Path, default=DEFAULT_TS, help="German .ts file")
    ap.add_argument("--root", type=Path, default=ROOT, help="source tree to scan")
    ap.add_argument("--probe", choices=sorted(PROBES) + ["source"],
                    help="inject one du-text (positive probe)")
    ap.add_argument("--list", action="store_true", help="inventory: list every finding, exit 0")
    args = ap.parse_args()
    try:
        self_check()
        items = translations(args.ts)
        n_catalogue = len(items)
        items += source_texts(args.root)
        if args.probe in PROBES:
            items.append(("probe", PROBES[args.probe]))
        elif args.probe == "source":
            items.extend(("probe", t) for _, t in literal_texts(SOURCE_PROBE))
        findings = [(where, text, classify(text)) for where, text in items]
        findings = [f for f in findings if f[2]]
    except ToolError as e:
        print(f"I18N-GUARD ERROR: German address: {e}")
        return 3
    if args.list:
        counts = {"pronoun": 0, "verb": 0, "imperative": 0}
        for where, text, hits in findings:
            for cls in {c for c, _ in hits}:
                counts[cls] += 1
            print(f"{where} {','.join(sorted({c for c, _ in hits}))}: {text}")
        print(f"catalogue={n_catalogue} sources={len(items) - n_catalogue} "
              f"with-du={len(findings)} " + " ".join(f"{k}={v}" for k, v in counts.items()))
        return 0
    if findings:
        for where, text, hits in findings:
            words = ", ".join(f"{c} '{w}'" for c, w in hits)
            print(f"I18N-GUARD FAIL: German address: {where} {text!r} ({words})")
        return 1
    print(f"I18N-GUARD OK: German address: {args.ts.name}: {n_catalogue} translations, "
          f"sources: {len(items) - n_catalogue} texts, none with du")
    return 0


if __name__ == "__main__":
    sys.exit(main())
