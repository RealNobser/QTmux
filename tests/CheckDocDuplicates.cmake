# Wächter gegen zerrissene/doppelte Doku-Abschnitte (QTMUX-34).
#
# Hintergrund: Beim Kompaktieren einer langen Markdown-Datei kann ein Block
# EINGEFÜGT statt ERSETZT werden. Zurück bleiben zwei gleichnamige Überschriften
# mit widersprüchlichem Inhalt — im Nachbarprojekt RAFTNG genau so passiert, und
# beim Lesen fällt es kaum auf, weil beide Fassungen plausibel klingen.
# Doppelte Überschriften sind das verräterische Zeichen; genau darauf prüfen wir.
#
# Aufruf: cmake -DDOC_FILES="a.md;b.md" -P CheckDocDuplicates.cmake
# Plattformneutral (kein sh/grep), damit der Test auch unter Windows in der CI läuft.

# PFLICHT im Skriptmodus (cmake -P): Ohne cmake_minimum_required stehen die Policies
# auf OLD, und dann ist IN_LIST unten kein Operator, sondern ein unbekanntes Argument
# (CMP0057) — der Test bricht ab. Fällt lokal leicht durch, wenn die eigene
# CMake-Version neuer ist als die des CI-Runners.
cmake_minimum_required(VERSION 3.24)

set(_problems "")

foreach(_file IN LISTS DOC_FILES)
    if(NOT EXISTS "${_file}")
        continue()   # optionale Datei fehlt -> nichts zu prüfen
    endif()

    # ENCODING UTF-8 ist PFLICHT: ohne das verschluckt CMake bei Zeilen mit
    # Mehrbyte-Zeichen (im README die Flaggen-Emoji der Sprachhälften) den Anfang
    # der Zeile — die '##'-Marke ginge verloren, der Überschriften-Pfad verrutschte
    # und der Wächter meldete Duplikate, die keine sind.
    #
    # ⚠️ REGEX ist ebenso PFLICHT, und zwar aus einem teurer erkauften Grund
    # (2026-08-06 gemessen, nachdem ein Gegentest NICHT fiel): Liest man die Datei
    # VOLLSTÄNDIG ein — egal ob mit file(STRINGS) oder file(READ) + Splitten —, dann
    # zerlegen einzelne Prosa-Zeilen die CMake-Liste und ALLES DAHINTER geht verloren.
    # Schuld sind Zeilen mit Backslash-Sequenzen (`\033[2J`) und Variablen-Referenzen
    # (`${command:cmake.activeBuildPresetName}`) im Fließtext. Wirkung war verheerend
    # und völlig unsichtbar: Von der CLAUDE.md kamen nur 141 von 779 Zeilen an und
    # damit 7 von 27 Überschriften, von docs/feature-referenz.md 62 von 1044 (4 von 13).
    # Der Wächter meldete seit QTMUX-34 grün, ohne die Datei je vollständig zu sehen —
    # README.md und docs/MCP.md waren zufällig unauffällig, deshalb fiel es nie auf.
    #
    # Mit dem REGEX-Filter werden die Problemzeilen gar nicht erst angefasst: gelesen
    # werden nur Überschriften und Code-Fences, und genau die sind harmlos. Gegenprobe
    # nach jeder Änderung hier: Erkennungsrate je Datei gegen `grep -c '^#\+ '` messen —
    # sie muss 100 % sein, sonst prüft der Wächter wieder ins Leere.
    file(STRINGS "${_file}" _lines ENCODING UTF-8 REGEX "^([ \t]*```|#+[ \t])")
    set(_seen "")
    set(_stack "")     # aktueller Überschriften-Pfad, Index 0 = Ebene 1
    set(_in_code FALSE)

    foreach(_line IN LISTS _lines)
        # Code-Blöcke überspringen: dort sind '#'-Zeilen Kommentare, keine Überschriften.
        if(_line MATCHES "^[ \t]*```")
            if(_in_code)
                set(_in_code FALSE)
            else()
                set(_in_code TRUE)
            endif()
            continue()
        endif()
        if(_in_code)
            continue()
        endif()

        if(_line MATCHES "^(#+)[ \t]+(.+)$")
            string(LENGTH "${CMAKE_MATCH_1}" _level)
            set(_heading "${CMAKE_MATCH_2}")
            string(STRIP "${_heading}" _heading)

            # Verglichen wird der PFAD, nicht der bloße Text: Das README ist
            # zweisprachig, dort steht '### Roadmap' völlig zu Recht einmal unter
            # der deutschen und einmal unter der englischen Hälfte. Erst zweimal
            # derselbe Titel unter DERSELBEN übergeordneten Überschrift ist der
            # Fehler, den wir suchen.
            # Stack auf die Ebene der ÜBERGEORDNETEN Überschrift zurückschneiden.
            math(EXPR _parent_len "${_level} - 1")
            list(LENGTH _stack _have)
            while(_have GREATER _parent_len)
                list(POP_BACK _stack)
                list(LENGTH _stack _have)
            endwhile()

            string(JOIN " > " _path ${_stack} "${_heading}")
            # list(FIND) statt IN_LIST: funktioniert unabhängig von Policy CMP0057,
            # die im Skriptmodus je nach CMake-Version des Runners nicht gesetzt ist.
            list(FIND _seen "${_path}" _dup_idx)
            if(NOT _dup_idx EQUAL -1)
                list(APPEND _problems "${_file}: doppelte Überschrift '${_path}'")
            else()
                list(APPEND _seen "${_path}")
            endif()
            list(APPEND _stack "${_heading}")
        endif()
    endforeach()
endforeach()

if(_problems)
    message("Doppelte Überschriften gefunden — beim Kompaktieren wurde vermutlich")
    message("ein Block eingefügt statt ersetzt. VOR dem Löschen prüfen, welche Zeilen")
    message("NUR in der Kopie stehen: dort steckt erfahrungsgemäß noch Gültiges.")
    foreach(_p IN LISTS _problems)
        message("  - ${_p}")
    endforeach()
    message(FATAL_ERROR "Doku-Wächter: ${_problems}")
endif()
