# MacPCAN-Plugin (QTMUX-9) — erstes echtes QTmux-Plugin: CAN-Bus als Terminal-Backend.
#
# Nur macOS. Die proprietäre (aber redistributierbare) PCBUSB-Bibliothek (UV Software)
# wird NICHT ins Repo gelegt — sie wird zur Build-Zeit gesucht (analog Clink-Erkennung).
# Fehlt sie, wird das Plugin übersprungen (kein Build-Fehler). Suchreihenfolge für das
# PCBUSB-Verzeichnis (muss include/PCBUSB.h + lib/libPCBUSB.dylib enthalten):
#   1. -DQTMUX_PCBUSB_DIR=… (bzw. Umgebungsvariable QTMUX_PCBUSB_DIR)
#   2. repo-lokal third_party/PCBUSB (git-ignoriert)
#   3. Nachbar-Checkout ../MacPCAN/third_party/PCBUSB

if(NOT APPLE)
    return()
endif()

set(_pcbusb_candidates
    "${QTMUX_PCBUSB_DIR}"
    "$ENV{QTMUX_PCBUSB_DIR}"
    "${CMAKE_SOURCE_DIR}/third_party/PCBUSB"
    "$ENV{HOME}/Projects/GitHub/MacPCAN/third_party/PCBUSB"
)
set(_pcbusb_dir "")
foreach(_cand IN LISTS _pcbusb_candidates)
    if(_cand AND EXISTS "${_cand}/include/PCBUSB.h" AND EXISTS "${_cand}/lib/libPCBUSB.dylib")
        set(_pcbusb_dir "${_cand}")
        break()
    endif()
endforeach()

if(NOT _pcbusb_dir)
    message(STATUS "MacPCAN-Plugin: PCBUSB nicht gefunden — übersprungen "
                   "(setze -DQTMUX_PCBUSB_DIR=<Pfad zu .../PCBUSB>, um es zu bauen).")
    return()
endif()
message(STATUS "MacPCAN-Plugin: PCBUSB gefunden in ${_pcbusb_dir}")

qt_add_plugin(qtmux_macpcan_plugin
    MacPcanPlugin.cpp
    vendor/core/CanService.cpp
    vendor/drivers/PcanDevice.cpp
    vendor/drivers/MockDevice.cpp
)
target_compile_features(qtmux_macpcan_plugin PRIVATE cxx_std_20)
target_include_directories(qtmux_macpcan_plugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"          # CanText.h + QTmuxPlugin.h (via qtmux_core)
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor"   # core/… und drivers/… Includes
    "${_pcbusb_dir}/include"               # PCBUSB.h
)
# qtmux_core liefert die SDK-Header (src/plugins) und das Meta-Objekt von
# ITerminalBackend (statisch dazugelinkt); PCBUSB die CAN-Hardware-Anbindung.
target_link_libraries(qtmux_macpcan_plugin PRIVATE
    qtmux_core Qt6::Core "${_pcbusb_dir}/lib/libPCBUSB.dylib")

# @rpath-Auflösung von libPCBUSB.0.13.dylib (install_name = @rpath/…): sowohl neben
# dem Plugin (Build-Baum, <build>/plugins) als auch im App-Bundle (Contents/PlugIns →
# ../Frameworks).
set_target_properties(qtmux_macpcan_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    BUILD_RPATH   "@loader_path;@loader_path/../Frameworks"
    INSTALL_RPATH "@loader_path;@loader_path/../Frameworks"
)

set(_pcbusb_dylib "${_pcbusb_dir}/lib/libPCBUSB.0.13.dylib")

# Für den Build-Baum: dylib neben das Plugin legen (@loader_path).
add_custom_command(TARGET qtmux_macpcan_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_pcbusb_dylib}" "${CMAKE_BINARY_DIR}/plugins/"
    COMMENT "PCBUSB-dylib neben das MacPCAN-Plugin kopieren (Build-Baum)"
    VERBATIM)

# Fürs App-Bundle: Plugin nach Contents/PlugIns, dylib nach Contents/Frameworks.
add_custom_command(TARGET qtmux_macpcan_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:qtmux>/PlugIns"
            "$<TARGET_BUNDLE_CONTENT_DIR:qtmux>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:qtmux_macpcan_plugin>"
            "$<TARGET_BUNDLE_CONTENT_DIR:qtmux>/PlugIns/$<TARGET_FILE_NAME:qtmux_macpcan_plugin>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_pcbusb_dylib}"
            "$<TARGET_BUNDLE_CONTENT_DIR:qtmux>/Frameworks/libPCBUSB.0.13.dylib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "$<TARGET_BUNDLE_CONTENT_DIR:qtmux>/Resources/PCBUSB-LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/COPYRIGHT"
            "$<TARGET_BUNDLE_CONTENT_DIR:qtmux>/Resources/PCBUSB-COPYRIGHT.txt"
    COMMENT "MacPCAN-Plugin + PCBUSB-dylib + Lizenz ins qtmux.app-Bundle kopieren"
    VERBATIM)
