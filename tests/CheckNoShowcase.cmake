# Wächter: Die ausgelieferte App enthält den Szenen-Baustein NICHT.
#
# Der Szenen-Baustein des Hubs (appupdate::UpdateShowcase: Wegwerf-Schlüssel,
# Server auf 127.0.0.1, ein Installationsweg, der nie etwas startet) und der
# Schalter --update-picture gehören nur ins Bildziel qtmux-bilder. Gemessen wird
# am GEBAUTEN Programm, nicht an der CMake-Zeile: Rutscht der Baustein über den
# Glob, einen Include oder eine Lib in die App, steht sein Name im Binary (moc legt
# den Klassennamen als Zeichenkette ab, ein Schalter steht als Literal darin).
#
#   APP       das Programm der App (Pflicht): 0 Treffer für beide Suchtexte; als
#             Positivkontrolle muss "--selftest-update" darin stehen, sonst hat das
#             Lesen nichts gemessen.
#   PROBE     eine feste Prüfdatei, die beide Suchtexte trägt (Pflicht): Hier MÜSSEN
#             beide gefunden werden. Fällt das Suchwerkzeug aus (file(STRINGS)
#             liefert nichts), wird der Lauf rot statt „0 Treffer“.
#   PICTURES  das Programm qtmux-bilder (nur, wenn es gebaut wird): beide Suchtexte
#             müssen darin stehen — dieselbe Probe am echten Gegenstück.
#
# Aufruf: cmake -DAPP=<binary> -DPROBE=<datei> [-DPICTURES=<binary>] -P CheckNoShowcase.cmake

cmake_minimum_required(VERSION 3.24)

set(_needles "UpdateShowcase" "--update-picture")
set(_control "--selftest-update")

# Zählt, in wie vielen Zeichenketten von `file` der Text `needle` vorkommt.
function(_count_in file needle out)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "CheckNoShowcase: Datei fehlt: ${file}")
    endif()
    # Die Suchtexte tragen nur Buchstaben und "-": als Regex wörtlich.
    file(STRINGS "${file}" _hits REGEX "${needle}")
    list(LENGTH _hits _n)
    set(${out} ${_n} PARENT_SCOPE)
endfunction()

foreach(_v APP PROBE)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "CheckNoShowcase: -D${_v}=… fehlt")
    endif()
endforeach()

set(_problems "")

foreach(_n IN LISTS _needles)
    _count_in("${PROBE}" "${_n}" _c)
    if(_c EQUAL 0)
        list(APPEND _problems "Prüfdatei: \"${_n}\" nicht gefunden — das Suchwerkzeug misst nichts")
    endif()
endforeach()

_count_in("${APP}" "${_control}" _c)
if(_c EQUAL 0)
    list(APPEND _problems "App: Positivkontrolle \"${_control}\" fehlt — falsches Programm oder Lesen gescheitert")
endif()
foreach(_n IN LISTS _needles)
    _count_in("${APP}" "${_n}" _c)
    if(NOT _c EQUAL 0)
        list(APPEND _problems "App: \"${_n}\" steht ${_c}-mal im Programm (${APP})")
    endif()
endforeach()

if(DEFINED PICTURES)
    foreach(_n IN LISTS _needles)
        _count_in("${PICTURES}" "${_n}" _c)
        if(_c EQUAL 0)
            list(APPEND _problems "Bildziel: \"${_n}\" fehlt in ${PICTURES} — die Gegenprobe greift nicht")
        endif()
    endforeach()
    set(_pic "Gegenprobe am Bildziel und an der Prüfdatei")
else()
    set(_pic "Bildziel nicht gebaut, Gegenprobe an der Prüfdatei")
endif()

if(_problems)
    list(JOIN _problems "\n  " _msg)
    message(FATAL_ERROR "CheckNoShowcase:\n  ${_msg}")
endif()
message(STATUS "CheckNoShowcase: App ohne Szenen-Baustein und ohne --update-picture (${_pic}).")
