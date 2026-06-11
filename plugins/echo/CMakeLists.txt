# Echo-Demo-Plugin (QTMUX-8): beweist die Plugin-Kette und dient als Vorlage.
# Landet in <build>/plugins — der PluginHost findet es dort über QTMUX_PLUGIN_DIR
# (Tests) bzw. neben der App nach einem Deploy.
qt_add_plugin(qtmux_echo_plugin
    EchoPlugin.cpp
)
# qtmux_core liefert die SDK-Header (src/plugins via PUBLIC-Include) und das
# Meta-Objekt von ITerminalBackend (statisch dazugelinkt).
target_link_libraries(qtmux_echo_plugin PRIVATE qtmux_core Qt6::Core)
set_target_properties(qtmux_echo_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# macOS: zusätzlich ins App-Bundle (Contents/PlugIns) kopieren — dort findet es der
# PluginHost beim normalen App-Start (Suchpfad 3). Auf Windows/Linux liegt
# <build>/plugins neben der exe und wird über Suchpfad 2 gefunden.
if(APPLE)
    add_custom_command(TARGET qtmux_echo_plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:qtmux_echo_plugin>"
                "$<TARGET_BUNDLE_CONTENT_DIR:qtmux>/PlugIns/$<TARGET_FILE_NAME:qtmux_echo_plugin>"
        COMMENT "Echo-Plugin ins qtmux.app-Bundle (PlugIns) kopieren"
        VERBATIM)
endif()
