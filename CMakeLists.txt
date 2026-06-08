cmake_minimum_required(VERSION 3.24)

project(QTmux
    VERSION 0.1.0
    DESCRIPTION "Cross-platform multi-AI-agent terminal manager"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(QTMUX_BUILD_TESTS "Build unit tests" ON)

# Qt6 ------------------------------------------------------------------------
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 SerialPort Network Svg LinguistTools)
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
)
target_include_directories(qtmux_core PUBLIC src/core)
target_link_libraries(qtmux_core PUBLIC Qt6::Core Qt6::SerialPort PRIVATE vterm::vterm)

# Windows: ConPTY (kernel32, default) + TCP-Tabelle/Sockets für ProcessInfo.
if(WIN32)
    target_link_libraries(qtmux_core PRIVATE iphlpapi ws2_32)
endif()

# Anwendungs-Target ----------------------------------------------------------
qt_add_executable(qtmux
    src/app/main.cpp
)

qt_add_qml_module(qtmux
    URI QTmux
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/terminal/TerminalItem.h
        src/terminal/TerminalItem.cpp
        src/viewmodels/SessionModel.h
        src/viewmodels/SessionModel.cpp
        src/viewmodels/Theme.h
        src/viewmodels/Theme.cpp
        src/viewmodels/AppController.h
        src/viewmodels/AppController.cpp
        src/server/McpServer.h
        src/server/McpServer.cpp
)
target_include_directories(qtmux PRIVATE src/terminal src/viewmodels src/server)
target_link_libraries(qtmux PRIVATE Qt6::Network Qt6::Svg)

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

# macOS App-Bundle
set_target_properties(qtmux PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "QTmux"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.qtmux.app"
)

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

# Tests ----------------------------------------------------------------------
if(QTMUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
