# Konsumenten-Wächter des gemeinsamen Update-Ablaufs (Vertrag V2 §8, K2).
#
# Prüfkriterium für „identisch“ (Vertrag §2): In der App steht keine Entscheidung des
# Update-Ablaufs mehr — nur Verdrahtung, und die Oberfläche bindet nur. Dieser
# Wächter liest QTmux' eigenen Code (src/, qml/; NICHT third_party/, dort liegt der
# Ablauf selbst) und schlägt an bei:
#   1. einem Aufruf des Kerns an der Verdrahtung vorbei (`UpdateChecker`),
#   2. einem eigenen Einstellungsschlüssel unter `update/` (die Schlüssel gehören
#      dem Hub: appupdate::settingskey), auch als QML `Settings { category: "update" }`,
#   3. einem Versionsvergleich (`QVersionNumber`),
#   4. QML, das am Zustand des Ablaufs entscheidet (`flow.state`), statt an `view`
#      und `hint` zu binden.
# Kommentare zählen nicht (eine Zeile, die das Verbot ERKLÄRT, ist kein Verstoß).
#
# Aufruf: cmake -DSRC_DIRS="src;qml" -DROOT=<repo> -P CheckUpdateConsumer.cmake
# Plattformneutral (kein grep), damit er auch unter Windows in der CI läuft.
#
# Gegenprobe (-DEXPECT_ALL_RULES=ON, Test test_update_consumer_guard_bites): Über
# einem Satz absichtlich falscher Dateien muss JEDE Regel mindestens einmal
# anschlagen; dann endet der Lauf grün, sonst rot. MIN_FILES senkt dafür die
# Positivkontrolle (Vorgabe 20).

cmake_minimum_required(VERSION 3.24)

set(_problems "")
set(_files_seen 0)
set(_hit_checker 0)
set(_hit_key 0)
set(_hit_version 0)
set(_hit_state 0)
if(NOT DEFINED MIN_FILES)
    set(MIN_FILES 20)
endif()

foreach(_dir IN LISTS SRC_DIRS)
    file(GLOB_RECURSE _files
        "${ROOT}/${_dir}/*.cpp" "${ROOT}/${_dir}/*.h" "${ROOT}/${_dir}/*.qml")
    foreach(_file IN LISTS _files)
        math(EXPR _files_seen "${_files_seen} + 1")
        # Ganze Datei lesen und selbst in Zeilen teilen: file(STRINGS) zerlegte
        # jede Zeile zusätzlich an „;“ (CMake-Listentrenner), und die Zeilennummern
        # im Befund stimmten dann nicht mehr.
        file(READ "${_file}" _text)
        string(REPLACE ";" "<SEMI>" _text "${_text}")
        # Eckige Klammern ebenso: In einer CMake-Liste trennt „;“ zwischen „[“ und
        # „]“ nicht, eine offene Klammer verschmölze also Zeilen.
        string(REPLACE "[" "<LB>" _text "${_text}")
        string(REPLACE "]" "<RB>" _text "${_text}")
        string(REPLACE "\n" ";" _lines "${_text}")
        set(_n 0)
        foreach(_line IN LISTS _lines)
            math(EXPR _n "${_n} + 1")
            # Kommentar abschneiden (// … und Zeilen, die mit * beginnen) und
            # #include-Zeilen überspringen.
            string(REGEX REPLACE "//.*$" "" _code "${_line}")
            string(STRIP "${_code}" _code)
            if(_code STREQUAL "" OR _code MATCHES "^\\*" OR _code MATCHES "^#include")
                continue()
            endif()
            file(RELATIVE_PATH _rel "${ROOT}" "${_file}")
            if(_code MATCHES "UpdateChecker")
                list(APPEND _problems "${_rel}:${_n}: Aufruf des Kerns an der Verdrahtung vorbei (UpdateChecker)")
                math(EXPR _hit_checker "${_hit_checker} + 1")
            endif()
            if(_code MATCHES "\"update/"
               OR (_file MATCHES "\\.qml$" AND _code MATCHES "category *: *\"update\""))
                list(APPEND _problems "${_rel}:${_n}: eigener update/-Schluessel (gehoert dem Hub)")
                math(EXPR _hit_key "${_hit_key} + 1")
            endif()
            if(_code MATCHES "QVersionNumber")
                list(APPEND _problems "${_rel}:${_n}: eigener Versionsvergleich")
                math(EXPR _hit_version "${_hit_version} + 1")
            endif()
            if(_file MATCHES "\\.qml$" AND _code MATCHES "flow\\.state")
                list(APPEND _problems "${_rel}:${_n}: QML entscheidet am Zustand (flow.state) statt an view/hint")
                math(EXPR _hit_state "${_hit_state} + 1")
            endif()
        endforeach()
    endforeach()
endforeach()

# Positivkontrolle: Ein Wächter, der keine Datei gelesen hat, beweist nichts.
if(_files_seen LESS MIN_FILES)
    message(FATAL_ERROR "CheckUpdateConsumer: nur ${_files_seen} Dateien gelesen — falscher ROOT/SRC_DIRS?")
endif()

if(EXPECT_ALL_RULES)
    if(_hit_checker GREATER 0 AND _hit_key GREATER 1 AND _hit_version GREATER 0
       AND _hit_state GREATER 0)
        message(STATUS "CheckUpdateConsumer (Gegenprobe): jede Regel schlug an "
                       "(Kern ${_hit_checker}, Schluessel ${_hit_key}, Version ${_hit_version}, "
                       "Zustand ${_hit_state}).")
        return()
    endif()
    message(FATAL_ERROR "CheckUpdateConsumer (Gegenprobe): nicht jede Regel schlug an "
                        "(Kern ${_hit_checker}, Schluessel ${_hit_key} von 2, Version ${_hit_version}, "
                        "Zustand ${_hit_state}).")
endif()

if(_problems)
    list(JOIN _problems "\n  " _msg)
    message(FATAL_ERROR "Update-Entscheidung im App-Code (Vertrag V2 §2):\n  ${_msg}")
endif()
message(STATUS "CheckUpdateConsumer: ${_files_seen} Dateien, keine Update-Entscheidung im App-Code.")
