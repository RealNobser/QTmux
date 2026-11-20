# appupdate_add_catalogs(<target>) — embeds the hub's update texts
# (src/update/i18n/appupdate_<lang>.ts → .qm) as resources of <target> under
# :/appupdate/i18n/, where installUpdateTranslator() looks for them.
#
# CANONICAL: QTmux vendors src/update/ byte-identically, this file included,
# so the hub and QTmux embed the catalogues the SAME way:
#
#   hub     src/CMakeLists → appupdate_add_catalogs(MacPCANUpdater)
#           (a consumer via add_subdirectory — RAFTNG, EmbyStudio with
#           MACPCAN_NO_CAN — gets the resource through the static lib, no
#           step of its own; measured 2026-11-19)
#   QTmux   include(third_party/updater/update/UpdaterCatalogs.cmake)
#           appupdate_add_catalogs(qtmux_updater)
#
# Only lrelease runs here, never lupdate: a consumer must not rewrite the
# vendored .ts files (QTmux' sync guard would turn red). The hub refreshes
# them with its own target `appupdate_lupdate` (top-level CMakeLists).
#
# Measured 2026-11-19 (probe): without this call the catalogue is
# simply missing — installUpdateTranslator() returns false and the texts stay
# English, with no build error. That is why the hub tests assert the load.

set(APPUPDATE_CATALOG_DIR "${CMAKE_CURRENT_LIST_DIR}/i18n" CACHE INTERNAL
    "Directory of the hub update catalogues")
set(APPUPDATE_CATALOG_LANGUAGES de sv CACHE INTERNAL
    "Languages with a hub update catalogue (source language: English)")

function(appupdate_add_catalogs target)
    if(NOT COMMAND qt_add_lrelease)
        find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
    endif()
    set(_ts "")
    foreach(_lang IN LISTS APPUPDATE_CATALOG_LANGUAGES)
        list(APPEND _ts "${APPUPDATE_CATALOG_DIR}/appupdate_${_lang}.ts")
    endforeach()
    set(_out "${CMAKE_CURRENT_BINARY_DIR}/appupdate_i18n")
    set_source_files_properties(${_ts} PROPERTIES OUTPUT_LOCATION "${_out}")
    qt_add_lrelease(${target} TS_FILES ${_ts} QM_FILES_OUTPUT_VARIABLE _qms)
    qt_add_resources(${target} appupdate_catalogs
        PREFIX "/appupdate/i18n"
        BASE "${_out}"
        FILES ${_qms})
endfunction()
