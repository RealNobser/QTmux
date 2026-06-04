cmake_minimum_required(VERSION 3.24)

project(QTmux
    VERSION 0.1.0
    DESCRIPTION "Cross-platform multi-AI-agent terminal manager"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(QTMUX_BUILD_TESTS "Build unit tests" ON)

# Qt6 ------------------------------------------------------------------------
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
if(QTMUX_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(REQUIRES 6.6)

# libvterm (VT-/ANSI-Parser) -------------------------------------------------
# Bevorzugt vcpkg/CMake-Config, sonst pkg-config, sonst manuelle Suche (Homebrew).
find_path(VTERM_INCLUDE_DIR vterm.h)
find_library(VTERM_LIBRARY NAMES vterm libvterm)
if(NOT VTERM_INCLUDE_DIR OR NOT VTERM_LIBRARY)
    message(FATAL_ERROR
        "libvterm nicht gefunden. macOS: 'brew install libvterm'; "
        "Windows/Linux: über vcpkg (siehe vcpkg.json).")
endif()
add_library(vterm::vterm UNKNOWN IMPORTED)
set_target_properties(vterm::vterm PROPERTIES
    IMPORTED_LOCATION "${VTERM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${VTERM_INCLUDE_DIR}")

# QML-Dateien liegen im Unterverzeichnis qml/ — Policy explizit setzen.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# Kern-Bibliothek (plattformneutral, testbar) --------------------------------
add_library(qtmux_core STATIC
    src/core/Pty.h
    src/core/UnixPty.cpp
    src/core/WindowsPty.cpp
    src/core/ITerminalBackend.h
    src/core/PtyBackend.h
    src/core/PtyBackend.cpp
    src/core/VtScreen.h
    src/core/VtScreen.cpp
)
target_include_directories(qtmux_core PUBLIC src/core)
target_link_libraries(qtmux_core PUBLIC Qt6::Core PRIVATE vterm::vterm)

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
)
target_include_directories(qtmux PRIVATE src/terminal)

target_link_libraries(qtmux PRIVATE
    qtmux_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

# macOS App-Bundle
set_target_properties(qtmux PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "QTmux"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.qtmux.app"
)

# Tests ----------------------------------------------------------------------
if(QTMUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
