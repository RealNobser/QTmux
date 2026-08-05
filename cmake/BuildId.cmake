# Erzeugt `qtmux_buildid.h` mit der workspace-weiten Build-ID
# `<version>+<git-short-hash>[-dirty]` (Owner-Vorgabe 2026-08-05).
#
# ⚠️ VORLÄUFIG: Der kanonische CMake-Baustein entsteht in MacPCAN und wird hier
# später vendiert. Diese Fassung existiert, damit die Anzeige-Kette (Fenstertitel,
# MCP) schon steht; beim Nachvendieren wird nur die QUELLE getauscht, nicht die
# Anzeige. Format und Fallverhalten sind bereits die festgelegten.
#
# 🔑 WARUM DIESES SKRIPT ZUR BUILD-ZEIT LÄUFT und nicht einfach `configure_file`
# in der CMakeLists steht: `configure_file` wird zur CONFIGURE-Zeit ausgewertet.
# Ein Commit ändert keine CMake-Datei, löst also kein Re-Configure aus — der
# eingebettete Hash bliebe auf dem Stand des letzten Konfigurierens stehen. Die
# Anwendung behauptete dann einen Stand, der nicht ihrer ist, und das ist
# schlimmer als gar keine Angabe: Genau das soll die Build-ID ja verhindern.
#
# Aufruf (aus einem Target, das bei JEDEM Build läuft):
#   cmake -DQTMUX_SRC=<repo> -DQTMUX_OUT=<header> -DQTMUX_VERSION=<x.y.z>
#         -P cmake/BuildId.cmake

cmake_minimum_required(VERSION 3.24)

set(_hash "unknown")
set(_dirty "")

find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${QTMUX_SRC}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${QTMUX_SRC}"
        OUTPUT_VARIABLE _out RESULT_VARIABLE _rc
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_rc EQUAL 0 AND NOT _out STREQUAL "")
        set(_hash "${_out}")

        # 🔑 `-dirty` NUR aus verfolgten Dateien (`--untracked-files=no`).
        # Sonst macht jede herumliegende unversionierte Datei jeden Build
        # „dirty" — hier etwa das generierte `.qmlls.ini` oder ein `build/` im
        # Baum — und die Markierung verliert genau die Bedeutung, für die sie da
        # ist: einen Bastelstand von einem Stand zu trennen, den es im Repo gibt.
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
            WORKING_DIRECTORY "${QTMUX_SRC}"
            OUTPUT_VARIABLE _st RESULT_VARIABLE _strc
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_strc EQUAL 0 AND NOT _st STREQUAL "")
            set(_dirty "-dirty")
        endif()
    endif()
endif()
# Ohne Git-Umgebung (Tarball, flacher CI-Checkout) bleibt es bei „unknown" —
# der Build wird NICHT abgebrochen. Eine fehlende Herkunftsangabe ist ein
# Schönheitsfehler, ein gebrochener Build wäre einer.

set(QTMUX_BUILD_ID "${QTMUX_VERSION}+${_hash}${_dirty}")
set(QTMUX_GIT_HASH "${_hash}")
if(_dirty STREQUAL "")
    set(QTMUX_GIT_DIRTY "false")
else()
    set(QTMUX_GIT_DIRTY "true")
endif()

# 🔑 Erst in eine temporäre Datei, dann nur bei INHALTSÄNDERUNG kopieren. Ohne
# das bekäme der Header bei jedem Build einen neuen Zeitstempel und zöge einen
# vollständigen Rebuild nach sich — bei jedem einzelnen Bauen.
configure_file("${QTMUX_SRC}/cmake/BuildId.h.in" "${QTMUX_OUT}.tmp" @ONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${QTMUX_OUT}.tmp" "${QTMUX_OUT}")
file(REMOVE "${QTMUX_OUT}.tmp")
