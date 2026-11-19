# i18n/ — Übersetzungen

> Schicht-Datei: Claude Code lädt sie, wenn in `i18n/` gearbeitet wird. Ergänzung zur i18n-Regel
> in der Wurzel-`CLAUDE.md` (Abschnitt „Konventionen“); der Absatz stammt wörtlich von dort
> (ausgelagert 2026-11-19).

  🔑 **`SOURCE_TARGETS` ist Pflicht, nicht Kosmetik (2026-07-31).** Ohne die Angabe scannt
  lupdate **alle** Targets — auch `tests/`. `tst_i18n.cpp` fragt für QTMUX-117 gezielt
  Qt-eigene Texte ab (Kontext **`QPlatformTheme`**: „Cancel"/„OK"/„Save"/„Close"), und
  lupdate trug sie in unsere `.ts` ein; weil Deutsch Quellsprache ist, finalisierte der Hook
  sie prompt als `Cancel` → **„Cancel"**. Damit lag in `qtmux_de` ein **konkurrierender**
  Eintrag zu dem aus `qtbase_de` (dort korrekt „Abbrechen") — also eine latente Regression
  von QTMUX-117. Unsichtbar blieb sie nur, weil Qt die Translator in **umgekehrter
  Installationsreihenfolge** befragt und `qtbase` zuletzt installiert wird: Ein Vertauschen
  der zwei `swapTranslator`-Zeilen in [main.cpp](src/app/main.cpp) hätte QTMUX-117 still
  zurückgedreht. **Merke:** Ein Test, der Fremdtexte abfragt, wird sonst zur
  Übersetzungsquelle. Kontrollgröße nach der Eingrenzung: **kein `QPlatformTheme`-Kontext**
  in den `.ts` (eine absolute Quelltext-Zahl veraltet mit jedem Feature und taugt nicht als
  Anker).
