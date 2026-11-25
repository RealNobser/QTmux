# Prüft, dass der eingebettete Helfer des Selbst-Ersetzens (macOS update-swap,
# Windows update-helper.exe) die Build-ID von QTmux trägt und nicht „unknown“
# (Hub 0.8.1, MAC-132; appupdate_embed_install_helpers(... BUILD_ID_HEADER ...)).
#
#   cmake -DHELPER=<Helfer> -DHEADER=<qtmux_buildid.h> -DPREFIX=<"update-swap build "> -P CheckHelperBuildId.cmake
#
# Erwartet wird genau die Zeichenkette "<PREFIX><QTMUX_BUILD_ID>" aus dem Kopf,
# den das Ziel qtmux_buildid bei jedem Bau nachführt. Fehlende Datei oder ein
# Kopf ohne Makro ist ein Werkzeugfehler (rot), nie eine Entwarnung.
foreach(_v HELPER HEADER PREFIX)
    if(NOT DEFINED ${_v} OR "${${_v}}" STREQUAL "")
        message(FATAL_ERROR "HELPER-BUILD-ID ERROR: -D${_v} fehlt")
    endif()
endforeach()
foreach(_f "${HELPER}" "${HEADER}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "HELPER-BUILD-ID ERROR: Datei fehlt: ${_f}")
    endif()
endforeach()

file(STRINGS "${HEADER}" _def REGEX "^#define QTMUX_BUILD_ID[ \t]+\"[^\"]+\"")
if(NOT _def MATCHES "\"([^\"]+)\"")
    message(FATAL_ERROR "HELPER-BUILD-ID ERROR: kein QTMUX_BUILD_ID in ${HEADER}")
endif()
set(_id "${CMAKE_MATCH_1}")

file(STRINGS "${HELPER}" _hits REGEX "${PREFIX}")
set(_want "${PREFIX}${_id}")
foreach(_h IN LISTS _hits)
    string(FIND "${_h}" "${_want}" _pos)
    if(_pos GREATER_EQUAL 0)
        message(STATUS "HELPER-BUILD-ID OK: ${_want}")
        return()
    endif()
endforeach()
message(FATAL_ERROR "HELPER-BUILD-ID FAIL: ${HELPER} trägt nicht \"${_want}\" (gefunden: ${_hits})")
