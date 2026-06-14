cmake_minimum_required(VERSION 3.24)

project(QTmux
    VERSION 1.0.2
    DESCRIPTION "Cross-platform multi-AI-agent terminal manager"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Position-Independent-Code für ALLE Targets. Nötig, weil die statische qtmux_core
# (und ggf. die vendored vterm-Lib) in das per qt_add_plugin gebaute, GETEILTE
# Plugin (.so) gelinkt wird: eine Shared Library verlangt PIC. GCC baut statische
# Libs sonst ohne -fPIC -> Linkfehler "relocation R_X86_64_… can not be used when
# making a shared object; recompile with -fPIC". Auf Windows (keine PIC-Semantik)
# und macOS (clang erzeugt ohnehin PIC) ist die Option ein No-op.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(QTMUX_BUILD_TESTS "Build unit tests" ON)

# Qt6 ------------------------------------------------------------------------
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 SerialPort Network Svg LinguistTools ShaderTools)
if(QTMUX_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.6)

# libvterm (VT-/ANSI-Parser) -------------------------------------------------
# Vendored unter third_party/libvterm (BSD). libvterm ist aus vcpkg verschwunden;
# das Einbinden in den Baum hält macOS/Windows/Linux identisch und abhängigkeitsfrei.
# Definiert das Target vterm::vterm.
add_subdirectory(third_party/libvterm)

# QML-Dateien liegen im Unterverzeichnis qml/ — Policy explizit setzen.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# Kern-Bibliothek (plattformneutral, testbar) --------------------------------
add_library(qtmux_core STATIC
    src/core/Pty.h
    src/core/UnixPty.cpp
    src/core/WindowsPty.cpp
    src/core/ProcessInfo.h
    src/core/ProcessInfo.cpp
    src/core/ITerminalBackend.h
    src/core/PtyBackend.h
    src/core/PtyBackend.cpp
    src/core/VtScreen.h
    src/core/VtScreen.cpp
    src/core/ColorScheme.h
    src/core/ColorScheme.cpp
    src/core/ConnectionProfile.h
    src/core/ConnectionProfile.cpp
    src/core/HotkeyRegistry.h
    src/core/HotkeyRegistry.cpp
    src/core/SecretsVault.h
    src/core/SecretsVault.cpp
    src/core/GlobalHotkey.h
    src/core/GlobalHotkey.cpp
    src/core/Session.h
    src/core/Session.cpp
    src/core/AgentRegistry.h
    src/core/AgentRegistry.cpp
    src/core/ShellRegistry.h
    src/core/ShellRegistry.cpp
    src/core/SerialBackend.h
    src/core/SerialBackend.cpp
    src/core/SshBackend.h
    src/core/SshBackend.cpp
    src/plugins/QTmuxPlugin.h
    src/plugins/PluginHost.h
    src/plugins/PluginHost.cpp
)
target_include_directories(qtmux_core PUBLIC src/core src/plugins)
target_link_libraries(qtmux_core PUBLIC Qt6::Core Qt6::SerialPort PRIVATE vterm::vterm)

# Windows: ConPTY (kernel32, default) + TCP-Tabelle/Sockets für ProcessInfo.
if(WIN32)
    target_link_libraries(qtmux_core PRIVATE iphlpapi ws2_32)
endif()

# macOS: Carbon für den globalen Quake-Hotkey (RegisterEventHotKey).
if(APPLE)
    target_link_libraries(qtmux_core PRIVATE "-framework Carbon")
endif()

# Anwendungs-Target ----------------------------------------------------------
# App-Icon je Plattform: macOS bekommt das .icns ins Bundle (s. set_source_files_
# properties + MACOSX_BUNDLE_ICON_FILE weiter unten), Windows kompiliert das .rc
# (verweist auf qtmux.ico) in die .exe. Linux nutzt das PNG via .desktop (Installer).
set(qtmux_platform_icon "")
if(APPLE)
    set(qtmux_macos_icon "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon/qtmux.icns")
    set(qtmux_platform_icon "${qtmux_macos_icon}")
elseif(WIN32)
    set(qtmux_platform_icon "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon/qtmux.rc")
endif()

qt_add_executable(qtmux
    src/app/main.cpp
    ${qtmux_platform_icon}
)

qt_add_qml_module(qtmux
    URI QTmux
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/SplitNode.qml
    SOURCES
        src/terminal/TerminalItem.h
        src/terminal/TerminalItem.cpp
        src/terminal/GlyphAtlas.h
        src/terminal/GlyphAtlas.cpp
        src/viewmodels/SessionModel.h
        src/viewmodels/SessionModel.cpp
        src/viewmodels/SftpClient.h
        src/viewmodels/SftpClient.cpp
        src/viewmodels/Theme.h
        src/viewmodels/Theme.cpp
        src/viewmodels/AppController.h
        src/viewmodels/AppController.cpp
        src/server/McpServer.h
        src/server/McpServer.cpp
)
target_include_directories(qtmux PRIVATE src/terminal src/viewmodels src/server)
target_link_libraries(qtmux PRIVATE Qt6::Network Qt6::Svg)

# GPU-Glyph-Atlas (QTMUX-6): RHI-Shader für das Glyph-Material in .qsb kompilieren
# und unter qrc:/shaders/ einbetten. BATCHABLE, damit das Material in der
# Scene-Graph-Batch-Pipeline mitläuft. Liegt unter :/shaders/glyph.{vert,frag}.qsb.
qt_add_shaders(qtmux "terminal_shaders"
    BATCHABLE
    PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/terminal/shaders"
    FILES
        src/terminal/shaders/glyph.vert
        src/terminal/shaders/glyph.frag
)

# Phosphor-Icons (SVG) unter qrc:/icons/<name>.svg einbetten. Sie werden über
# das Qt-icon-System (icon.source + icon.color) in Toolbar UND Menüs genutzt;
# in nativen macOS-Menüs landet das getönte Icon als auto-angepasstes Template.
qt_add_resources(qtmux "icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons"
    FILES
        resources/icons/plus.svg
        resources/icons/x.svg
        resources/icons/terminal-window.svg
        resources/icons/plugs.svg
        resources/icons/usb.svg
        resources/icons/sun.svg
        resources/icons/moon.svg
        resources/icons/gear.svg
        resources/icons/info.svg
        resources/icons/broadcast.svg
        resources/icons/translate.svg
        resources/icons/caret-down.svg
        resources/icons/trash.svg
        resources/icons/robot.svg
        resources/icons/command.svg
        resources/icons/copy.svg
        resources/icons/clipboard.svg
        resources/icons/split-h.svg
        resources/icons/split-v.svg
        resources/icons/broadcast-input.svg
        resources/icons/bookmark.svg
        resources/icons/key.svg
        resources/icons/eye.svg
)

target_link_libraries(qtmux PRIVATE
    qtmux_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

# Übersetzungen (i18n) — .qm werden unter :/i18n/ eingebettet ----------------
qt_add_translations(qtmux
    TS_FILES
        i18n/qtmux_de.ts
        i18n/qtmux_en.ts
    RESOURCE_PREFIX "/i18n"
)

# Deutsch ist die QUELLSPRACHE: nach jedem lupdate-Lauf neue Einträge in
# qtmux_de.ts automatisch als finished (Übersetzung = Quelltext) markieren —
# sonst warnt lrelease bei jedem Build über unfinished/ignorierte Strings.
# qt_add_translations legt die lupdate-Targets DEFERRED (am Ende des Directory-
# Scopes) an — der Hook muss daher ebenfalls deferred laufen, sonst existiert
# das Target noch nicht. QTmux_lupdate ist das echte lupdate-Target;
# update_translations ist nur ein phony-Aggregator darüber.
function(qtmux_hook_finish_source_ts)
    if(TARGET ${PROJECT_NAME}_lupdate)
        add_custom_command(TARGET ${PROJECT_NAME}_lupdate POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                    -DTS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/i18n/qtmux_de.ts
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FinishSourceLanguageTs.cmake
            COMMENT "i18n: Quellsprache (de) als finished markieren"
            VERBATIM)
    else()
        message(WARNING "Target ${PROJECT_NAME}_lupdate fehlt — qtmux_de.ts wird nach lupdate nicht auto-finalisiert.")
    endif()
endfunction()
cmake_language(DEFER CALL qtmux_hook_finish_source_ts)

# macOS App-Bundle
set_target_properties(qtmux PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "QTmux"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.qtmux.app"
    MACOSX_BUNDLE_ICON_FILE "qtmux.icns"
)
# Das .icns landet in QTmux.app/Contents/Resources; MACOSX_BUNDLE_ICON_FILE oben
# trägt nur den Dateinamen in die Info.plist ein.
if(APPLE)
    set_source_files_properties("${qtmux_macos_icon}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

# Windows: GUI-Subsystem (kein Konsolenfenster). WICHTIG nicht nur kosmetisch —
# eine Konsolen-Subsystem-App (CUI) erbt/erhält eine Konsole, an die sich die per
# ConPTY gestarteten Kindprozesse hängen, statt an unsere Pseudo-Konsole. Als
# GUI-App (WIN32) gibt es keine Konsole zum Erben → ConPTY-Anbindung greift.
set_target_properties(qtmux PROPERTIES WIN32_EXECUTABLE TRUE)

# Windows: Qt-Laufzeit (DLLs, QML-Module, Plugins) neben die .exe deployen, damit
# sie ohne manuell gesetzten PATH startet. windeployqt kommt aus dem Qt-Paket.
if(WIN32)
    if(NOT TARGET Qt6::windeployqt)
        message(WARNING "Qt6::windeployqt nicht gefunden — qtmux.exe braucht Qt-bin im PATH.")
    else()
        add_custom_command(TARGET qtmux POST_BUILD
            COMMAND Qt6::windeployqt
                    --no-translations --no-system-d3d-compiler --no-opengl-sw
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    "$<TARGET_FILE:qtmux>"
            COMMENT "windeployqt: Qt-Laufzeit neben qtmux.exe legen"
            VERBATIM)
    endif()
endif()

# Plugins (QTMUX-8) -----------------------------------------------------------
# Demo-Plugin als Beweis der Plugin-Kette + Kopiervorlage; landet in <build>/plugins.
add_subdirectory(plugins/echo)

# Tests ----------------------------------------------------------------------
if(QTMUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
