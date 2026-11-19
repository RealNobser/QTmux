# src/core/ — Kern: ConPTY und libvterm

> Schicht-Datei: Claude Code lädt sie, wenn in `src/core/` gearbeitet wird. **Vor einem
> libvterm-Update** (Arbeit in `third_party/libvterm/`, wo keine `CLAUDE.md` liegen darf) liest
> man sie von dort aus selbst. Die Absätze stammen wörtlich aus der Wurzel-`CLAUDE.md`
> (ausgelagert 2026-11-19). Dort bleiben die Regel „`qtmux_core` bleibt Gui-frei“, der erste
> libvterm-Patch mit dem Suchbefehl für alle `QTMUX:`-Stellen und die Messung des
> Arbeitsverzeichnisses unter Windows (`Pty::currentWorkingDirectory()`, OSC 7).

## ConPTY-Lektionen (teuer erkauft)

- qtmux MUSS **`WIN32_EXECUTABLE`** (GUI-Subsystem) sein — eine Konsolen-App vererbt ihre
  Konsole an die ConPTY-Kindshells → Terminal stumm.
- conhost gibt **kein Pipe-EOF** beim Shell-Selbst-Exit → **Waiter-Thread**
  (`WaitForSingleObject` auf das Prozess-Handle) erkennt das Prozessende.
- terminate-Reihenfolge: `ClosePseudoConsole` **vor** `TerminateProcess` (+ `CancelSynchronousIo`).
- PTY-Tests sind `WIN32_EXECUTABLE` und laufen via `tests/run_detached.ps1` losgelöst
  (sonst erben sie die ctest-Konsole); ohne Qt-bin im PATH → Exit `0xc0000135`.
- VS-Code-Debugger: `launch.json` braucht **`"console": "externalTerminal"`**
  (internalConsole leitet Std-Handles um → ConPTY-Kindshells hängen).
- **PS-5.1-Umlaut-Mojibake** („für"→„fÃ¼r"): conhost re-kodiert Kind-Ausgabe von CP1252 nach
  UTF-8, **bevor** die Bytes QTmux erreichen (per rohen UTF-8-Bytes bewiesen). Keine
  Shell-Einstellung hilft (chcp/OutputEncoding alles getestet); Abhilfe nur PowerShell 7 oder
  die System-Option „UTF-8 weltweit". Kein QTmux-Bug → bewusst **kein** Dekodier-Hack.
- Debug-Qt asserted „QSGGeometryNode is missing geometry": jedem GeometryNode beim Anlegen
  sofort eine (leere) `QSGGeometry` setzen (Release-Qt prüft das nicht).

## libvterm: Patches 2–5

Fortsetzung der Patch-Liste aus der Wurzel-`CLAUDE.md` (Abschnitt „libvterm“, dort Patch 1 und
der Suchbefehl):

> **Patch 2 — Emoji-Presentation (Unicode TR#51), `state.c` (QTMUX-97):** Basiszeichen mit
> Text-Default + **VS-16 (U+FE0F)** → Breite **2** (VS-15/U+FE0E umgekehrt → 1). libvterm 0.3.3
> führt U+FE00–FE0F in seiner `combining`-Tabelle (Breite 0) und kennt die Emoji-Form nicht;
> `fullwidth.inc` deckt nur Zeichen mit Emoji-**Default** ab (✅ ❌ 😀 sind drin, **U+26A0 nicht**).
> Betroffen ist also genau die VS-16-Klasse: ⚠️ ©️ ®️ ❤️ ‼️ ✔️ … Test
> `tst_vtscreen::emojiPresentationWidth`.
>
> **Patch 3 — DECSET 1007 (Alternate Scroll), `state.c`/`vterm.h`/`vterm.c` (QTMUX-128):**
> Modus 1007 lief in den `default:`-Zweig von `set_dec_mode` (nur `DEBUG_LOG`) und war damit
> unsichtbar; der **CSI-Fallback sieht ihn nie**, weil `CSI ? Ps h` als Sequenz erkannt ist —
> ein Abfangen im Frontend ist also prinzipiell unmöglich. Neu: `case 1007` →
> `settermprop_bool(VTERM_PROP_ALTSCROLL)`, Prop am **Enum-Ende** (ABI, wie
> `VTERM_ATTR_FAINT`), Typ in `vterm_get_prop_type`, in `vterm_state_set_termprop` nur
> **durchgereicht** (wie TITLE) — der Modus wird nicht im State gehalten, allein das
> Frontend entscheidet. `VtScreen::altScroll()`; Tests `tst_vtscreen::altScrollModeTracked`
> + `altScrollRule` (Gegentest ohne den Patch: genau die zwei parse-abhängigen Fälle fallen,
> die reine Regel bleibt grün).
>
> **Patch 4 — Reflow-Absicherung, `screen.c` `resize_buffer` (Clink-Auftrag 2026-11-03):**
> libvterms Reflow ist in 0.3.3 nicht absturzfest — ein Zufallstester unter ASan scheiterte
> mit dem unveränderten Code in **jedem** von 1500 Läufen (`abort()` „screen_resize failed to
> update cursor position“ bzw. Lesen vor dem Puffer). Übernommen aus Neovims libvterm-Fassung
> (`src/nvim/vterm/screen.c`): Reflow nur im Primärpuffer, `old_row > 0`, `<= old_row_end`,
> Grenzprüfung bei der Cursor-Spalte; zusätzlich ersetzt eine Begrenzung das `abort()`
> (in 3000 Läufen nie erreicht, reines Netz). Danach 3000/3000 sauber. Relevant, seit QTmux
> den Reflow für ConPTY-Sessions einschaltet (`VtScreen::setReflow`). Tests
> `tst_conptyreplay::reflow*`; Mutationsproben `old_row > 0` und `<=` je rot, die zwei
> übrigen Neovim-Stellen zeigten keine messbare Wirkung. ⚠️ Grenze (auch bei Neovim): Wird
> eine **einzelne** logische Zeile nach dem Umbruch höher als der ganze Schirm, landet nur
> ihre erste alte Zeile im Scrollback.
>
> **Patch 5 — Hyperlink-Kennung (OSC 8), `vterm.h`/`pen.c`/`screen.c`/`state.c`/`vterm.c`
> (Auftrag Links/Umbruch 2026-11-13):** libvterm 0.3.3 kennt keine Hyperlinks, OSC 8 läuft in
> den OSC-Fallback. Dort parst `VtScreen` es und setzt per `vterm_state_set_pen_uri` eine
> Kennung in den Stift; libvterm trägt sie durch die Zellen (`VTermPen::uri`, `ScreenPen::uri`,
> `VTermScreenCell::uri`, `VTERM_ATTR_URI` am Enum-**Ende**) und damit durch Scrollen, Löschen
> und Scrollback. Das Ziel selbst kennt libvterm nicht. Der Stift behält die Kennung bei SGR 0
> und bei DECRC (OSC 8 ist kein SGR-Attribut), ein Reset setzt sie auf 0, gelöschte Zellen
> tragen keine. Tests `tst_vtscreen::hyperlink*`; Mutationsproben je Stelle rot.
