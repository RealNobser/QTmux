# Wächter für den Teil des Baus, den das Selbst-Ersetzen am Installer und am Bundle
# braucht (Vertrag V2 §6, K2). Dass die Hilfsprogramme im gebauten Programm liegen,
# prüft test_update_helper_embedded (Byte-Vergleich, appupdate_embed_install_helpers);
# hier steht nur, was das MSI-Skript dafür tun muss (Punkt 4).
#
#   1. installer/QTmux.wxs legt den Installationsort in „Apps und Features“ ab:
#      <SetProperty Id="ARPINSTALLLOCATION" Value="[INSTALLFOLDER]"
#      After="CostFinalize"/>, ohne Sequence (sonst liefe es unter /passive nicht)
#      und ohne Condition; der Ordner heißt INSTALLFOLDER, und die Nutzlast liegt
#      darin (ComponentGroup … Directory="INSTALLFOLDER").
#   2. Nur macOS: Die Info.plist des GEBAUTEN Bundles trägt CFBundleVersion und
#      CFBundleShortVersionString = VERSION (vorher stand als Kurzversion „1.9“).
#   3. Die Vorgabe-Version von installer/build-dmg.sh ist VERSION: Das DMG trägt
#      sonst einen anderen Namen als das Bundle seine Version.
#   4. installer/build-msi.ps1 nimmt update-helper.exe aus dem Bauordner ins
#      Staging, bricht ab, wenn er fehlt, und prüft ihn danach in der File-Tabelle
#      des fertigen MSI (Kommentarzeilen zählen nicht).
#
# Aufruf: cmake -DWXS=<QTmux.wxs> -DVERSION=<x.y.z> [-DPLIST=<Info.plist>]
#               [-DDMG_SCRIPT=<build-dmg.sh>] [-DMSI_SCRIPT=<build-msi.ps1>]
#               -P CheckSelfUpdateBuild.cmake
# Gegenprobe (-DEXPECT_PROBLEMS=<n>, Test test_installer_self_update_contract_bites):
# Über absichtlich falschen Dateien muss der Wächter genau n Befunde liefern; dann
# endet der Lauf grün.

cmake_minimum_required(VERSION 3.24)

set(_problems "")

if(NOT EXISTS "${WXS}")
    message(FATAL_ERROR "CheckSelfUpdateBuild: WXS fehlt: ${WXS}")
endif()
file(READ "${WXS}" _wxs)
# Kommentare entfernen, damit eine Zeile, die die Regel nur ERKLÄRT, nicht zählt.
string(REGEX REPLACE "<!--([^-]|-[^-])*-->" "" _wxs "${_wxs}")

if(_wxs MATCHES "<SetProperty[^>]*Id=\"ARPINSTALLLOCATION\"[^>]*>")
    set(_sp "${CMAKE_MATCH_0}")
    if(NOT _sp MATCHES "Value=\"\\[INSTALLFOLDER\\]\"")
        list(APPEND _problems "QTmux.wxs: ARPINSTALLLOCATION nicht = [INSTALLFOLDER]")
    endif()
    if(NOT _sp MATCHES "After=\"CostFinalize\"")
        list(APPEND _problems "QTmux.wxs: ARPINSTALLLOCATION nicht After=\"CostFinalize\"")
    endif()
    if(_sp MATCHES "[ \t\r\n]Sequence=")
        list(APPEND _problems "QTmux.wxs: ARPINSTALLLOCATION mit Sequence (liefe nicht in jeder Sequenz)")
    endif()
    if(_sp MATCHES "[ \t\r\n]Condition=")
        list(APPEND _problems "QTmux.wxs: ARPINSTALLLOCATION mit Condition")
    endif()
else()
    list(APPEND _problems "QTmux.wxs: kein <SetProperty Id=\"ARPINSTALLLOCATION\" …/>")
endif()
if(NOT _wxs MATCHES "<Directory[ \t\r\n]+Id=\"INSTALLFOLDER\"")
    list(APPEND _problems "QTmux.wxs: kein <Directory Id=\"INSTALLFOLDER\" …>")
endif()
if(NOT _wxs MATCHES "<ComponentGroup[^>]*Directory=\"INSTALLFOLDER\"")
    list(APPEND _problems "QTmux.wxs: keine Nutzlast in INSTALLFOLDER (ComponentGroup … Directory=\"INSTALLFOLDER\")")
endif()

if(DEFINED PLIST)
    if(NOT EXISTS "${PLIST}")
        message(FATAL_ERROR "CheckSelfUpdateBuild: Info.plist fehlt: ${PLIST}")
    endif()
    file(READ "${PLIST}" _plist)
    foreach(_key CFBundleVersion CFBundleShortVersionString)
        if(_plist MATCHES "<key>${_key}</key>[ \t\r\n]*<string>([^<]*)</string>")
            if(NOT CMAKE_MATCH_1 STREQUAL "${VERSION}")
                list(APPEND _problems "Info.plist: ${_key} = \"${CMAKE_MATCH_1}\", erwartet \"${VERSION}\"")
            endif()
        else()
            list(APPEND _problems "Info.plist: ${_key} fehlt")
        endif()
    endforeach()
endif()

if(DEFINED DMG_SCRIPT)
    file(READ "${DMG_SCRIPT}" _dmg)
    if(_dmg MATCHES "VERSION=\"\\$\\{1:-([^}\"]*)\\}\"")
        if(NOT CMAKE_MATCH_1 STREQUAL "${VERSION}")
            list(APPEND _problems "build-dmg.sh: Vorgabe-Version \"${CMAKE_MATCH_1}\", erwartet \"${VERSION}\"")
        endif()
    else()
        list(APPEND _problems "build-dmg.sh: keine Zeile VERSION=\"\${1:-…}\"")
    endif()
endif()

if(DEFINED MSI_SCRIPT)
    if(NOT EXISTS "${MSI_SCRIPT}")
        message(FATAL_ERROR "CheckSelfUpdateBuild: MSI-Skript fehlt: ${MSI_SCRIPT}")
    endif()
    # Ohne Kommentarzeilen: Eine Zeile, die die Regel nur beschreibt, darf nicht
    # zählen. Am Stück gelesen, nicht als CMake-Liste: PowerShell-Zeilen tragen
    # eckige Klammern, und eine Liste verschmilzt dann Zeilen still miteinander.
    file(READ "${MSI_SCRIPT}" _msi)
    # Erst die Blockkommentare <# … #> (PowerShell), dann die Zeilen ab „#“.
    while(TRUE)
        string(FIND "${_msi}" "<#" _b)
        if(_b EQUAL -1)
            break()
        endif()
        string(SUBSTRING "${_msi}" ${_b} -1 _rest)
        string(FIND "${_rest}" "#>" _e)
        string(SUBSTRING "${_msi}" 0 ${_b} _head)
        if(_e EQUAL -1)
            set(_msi "${_head}")   # offener Block: der Rest ist Kommentar
            break()
        endif()
        math(EXPR _after "${_b} + ${_e} + 2")
        string(SUBSTRING "${_msi}" ${_after} -1 _tail)
        set(_msi "${_head}${_tail}")
    endwhile()
    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" _msi "${_msi}")
    if(NOT _msi MATCHES "Join-Path \\$build \"update-helper\\.exe\"")
        list(APPEND _problems "build-msi.ps1: holt update-helper.exe nicht aus dem Bauordner")
    endif()
    if(NOT _msi MATCHES "throw \"update-helper\\.exe fehlt")
        list(APPEND _problems "build-msi.ps1: bricht nicht ab, wenn update-helper.exe fehlt")
    endif()
    if(NOT _msi MATCHES "Copy-Item \\$helper \\$stage")
        list(APPEND _problems "build-msi.ps1: legt update-helper.exe nicht ins Staging")
    endif()
    if(NOT _msi MATCHES "@\\(\"qtmux\\.exe\", \"update-helper\\.exe\"\\)")
        list(APPEND _problems "build-msi.ps1: prüft update-helper.exe nicht in der File-Tabelle des MSI")
    endif()
endif()

list(LENGTH _problems _count)
if(DEFINED EXPECT_PROBLEMS)
    if(_count EQUAL EXPECT_PROBLEMS)
        message(STATUS "CheckSelfUpdateBuild (Gegenprobe): ${_count} Befunde wie erwartet.")
        return()
    endif()
    list(JOIN _problems "\n  " _msg)
    message(FATAL_ERROR "CheckSelfUpdateBuild (Gegenprobe): ${_count} statt ${EXPECT_PROBLEMS} Befunde:\n  ${_msg}")
endif()
if(_problems)
    list(JOIN _problems "\n  " _msg)
    message(FATAL_ERROR "Bau fürs Selbst-Ersetzen unvollständig:\n  ${_msg}")
endif()
message(STATUS "CheckSelfUpdateBuild: Installationsort, Bundle-Versionen, DMG-Vorgabe und "
               "Helfer im MSI-Skript in Ordnung.")
