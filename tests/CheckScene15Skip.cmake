# Rücknahme-Wächter für das Aussetzen von Szene 15 (tests/Scene15Probe.h).
#
# Das Aussetzen ist ein Übergang, bis der Update-Kern den Proxy der Szene nicht mehr
# „localhost“ nennt. Dieser Wächter wird rot, sobald der vendierte Szenen-Baustein kein
# QStringLiteral("localhost") mehr setzt, das Aussetzen aber noch existiert: Dann ist
# der Grund weg, und der Skip verdeckte nur noch.
#
# Aufruf: cmake -DSHOWCASE=<UpdateShowcase.cpp> -DPROBE=<Scene15Probe.h> -P CheckScene15Skip.cmake
# Gegenprobe (test_scene15_skip_retired_bites): SHOWCASE zeigt auf eine Datei ohne
# „localhost“; der Lauf muss die Meldung SKIP-RUECKNAHME-FAELLIG bringen.

cmake_minimum_required(VERSION 3.24)

foreach(_v SHOWCASE PROBE)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "CheckScene15Skip: -D${_v}=… fehlt")
    endif()
endforeach()
# Positivkontrolle: Ein Wächter, der den Baustein nicht gelesen hat, beweist nichts.
if(NOT EXISTS "${SHOWCASE}")
    message(FATAL_ERROR "CheckScene15Skip: ${SHOWCASE} fehlt (falscher Pfad?)")
endif()
if(NOT EXISTS "${PROBE}")
    message(STATUS "CheckScene15Skip: kein Aussetzen mehr vorhanden, nichts zu prüfen.")
    return()
endif()

file(READ "${SHOWCASE}" _showcase)
string(FIND "${_showcase}" "QStringLiteral(\"localhost\")" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR "SKIP-RUECKNAHME-FAELLIG: ${SHOWCASE} setzt kein \"localhost\" mehr, "
                        "das Aussetzen von Szene 15 steht aber noch (${PROBE}). Zurückbauen: "
                        "tests/Scene15Probe.h, die Tests test_updatepictures_szene15 und "
                        "test_updatedialogkeys_szene15 samt QTMUX_SCENE15_OWN_TEST.")
endif()
message(STATUS "CheckScene15Skip: Szenen-Baustein setzt noch \"localhost\", Aussetzen begründet.")
