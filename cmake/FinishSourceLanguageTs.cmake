# Finalisiert die .ts-Datei der QUELLSPRACHE (Deutsch) nach einem lupdate-Lauf.
#
# Hintergrund: lupdate legt neue Strings ohne Übersetzung ("unfinished"/leer) an.
# Für die Quellsprache gilt aber immer Übersetzung == Quelltext — ohne diesen
# Schritt warnt lrelease bei jedem Build ("unfinished" / "ignored untranslated").
#
# Aufruf (passiert automatisch nach dem Target update_translations):
#   cmake -DTS_FILE=<pfad/qtmux_de.ts> -P cmake/FinishSourceLanguageTs.cmake
if(NOT DEFINED TS_FILE)
    message(FATAL_ERROR "TS_FILE nicht gesetzt")
endif()
file(READ "${TS_FILE}" content)

# 1) Leere unfinished-Einträge: Quelltext als Übersetzung übernehmen.
string(REGEX REPLACE
    "<source>([^<]*)</source>(\r?\n[ \t]*)<translation type=\"unfinished\"></translation>"
    "<source>\\1</source>\\2<translation>\\1</translation>"
    content "${content}")

# 2) Nicht-leere unfinished-Einträge: nur den Marker entfernen.
#    ([^<]+) lässt leere Rest- und numerus-Einträge bewusst stehen (deren
#    Plural-Formen brauchen Handarbeit; lieber Warnung als leere "Übersetzung").
string(REGEX REPLACE
    "<translation type=\"unfinished\">([^<]+)</translation>"
    "<translation>\\1</translation>"
    content "${content}")

file(WRITE "${TS_FILE}" "${content}")
message(STATUS "Quellsprache finalisiert: ${TS_FILE}")
