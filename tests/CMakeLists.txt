# --- Reine Logik-Tests (laufen direkt, keine echten Prozesse) ---------------
qt_add_executable(test_vtscreen tst_vtscreen.cpp)
target_link_libraries(test_vtscreen PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_vtscreen COMMAND test_vtscreen)

qt_add_executable(test_linkdetector tst_linkdetector.cpp)
target_link_libraries(test_linkdetector PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_linkdetector COMMAND test_linkdetector)

qt_add_executable(test_keyencoding tst_keyencoding.cpp)
target_link_libraries(test_keyencoding PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_keyencoding COMMAND test_keyencoding)

qt_add_executable(test_terminalgrid tst_terminalgrid.cpp)
target_link_libraries(test_terminalgrid PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_terminalgrid COMMAND test_terminalgrid)

qt_add_executable(test_terminalsearch tst_terminalsearch.cpp)
target_link_libraries(test_terminalsearch PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_terminalsearch COMMAND test_terminalsearch)

qt_add_executable(test_agent tst_agent.cpp)
target_link_libraries(test_agent PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_agent COMMAND test_agent)

# QTMUX-117: Qt-eigene Uebersetzungen (Standardknoepfe). Der Test bekommt DIESELBE
# Dateiliste eingebettet wie die App (`_qtmux_qtbase_qm` aus der Haupt-CMakeLists,
# die hier im Subdirectory-Scope sichtbar ist) und prueft damit exakt die Kette aus
# main.cpp: aus :/i18n laden -> installieren -> QPlatformTheme-Text.
# Ohne gefundene .qm wird der Test gar nicht erst angelegt — dann meldet bereits die
# Warnung beim Konfigurieren den Grund, statt dass ein roter Test ein
# Umgebungsproblem als Regression ausgibt.
if(_qtmux_qtbase_qm)
    qt_add_executable(test_i18n tst_i18n.cpp)
    target_link_libraries(test_i18n PRIVATE Qt6::Test)
    qt_add_resources(test_i18n "test_qt_translations"
        PREFIX "/i18n"
        BASE "${QT_TRANSLATIONS_DIR}"
        FILES ${_qtmux_qtbase_qm}
    )
    add_test(NAME test_i18n COMMAND test_i18n)
endif()

# QTMUX-38: Shell-Helfer aus der Ressource an einen stabilen Ort schreiben. Der Test
# prueft zugleich, dass die Ressource aus der STATISCHEN qtmux_core im Programm landet —
# ein Linker darf Objektdateien verwerfen, auf die niemand verweist.
qt_add_executable(test_shellintegration tst_shellintegration.cpp)
target_link_libraries(test_shellintegration PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_shellintegration COMMAND test_shellintegration)

qt_add_executable(test_profiles tst_profiles.cpp)
target_link_libraries(test_profiles PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_profiles COMMAND test_profiles)

qt_add_executable(test_hotkeys tst_hotkeys.cpp)
target_link_libraries(test_hotkeys PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_hotkeys COMMAND test_hotkeys)

qt_add_executable(test_agenteventhub tst_agenteventhub.cpp)
target_link_libraries(test_agenteventhub PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_agenteventhub COMMAND test_agenteventhub)

qt_add_executable(test_vault tst_vault.cpp)
target_link_libraries(test_vault PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_vault COMMAND test_vault)

# MacPCAN-Plugin (QTMUX-9): Frame-Format/Parser (CanText.h + vendorierter CanFrame).
# Bewusst PCBUSB- und hardwarefrei → läuft auf allen Plattformen, auch ohne das Plugin.
qt_add_executable(test_macpcan tst_macpcan.cpp)
target_include_directories(test_macpcan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../plugins/macpcan
    ${CMAKE_CURRENT_SOURCE_DIR}/../plugins/macpcan/vendor)
target_link_libraries(test_macpcan PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_macpcan COMMAND test_macpcan)

# SftpClient liegt im QML-Modul; fürs Parsing (statisch, serverlos) die .cpp direkt
# mitkompilieren (Qt6::Qml wegen qqmlintegration.h / QML_ELEMENT).
qt_add_executable(test_sftp tst_sftp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/SftpClient.cpp)
target_include_directories(test_sftp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels)
target_link_libraries(test_sftp PRIVATE qtmux_core Qt6::Test Qt6::Qml)
add_test(NAME test_sftp COMMAND test_sftp)

# Plugin-System (QTMUX-8): lädt das echte Echo-Demo-Plugin aus <build>/plugins —
# der Pfad kommt per Test-Env QTMUX_PLUGIN_DIR, das Plugin muss vorher gebaut sein.
qt_add_executable(test_plugins tst_plugins.cpp)
target_link_libraries(test_plugins PRIVATE qtmux_core Qt6::Test)
add_dependencies(test_plugins qtmux_echo_plugin)
add_test(NAME test_plugins COMMAND test_plugins)
set_tests_properties(test_plugins PROPERTIES
    ENVIRONMENT "QTMUX_PLUGIN_DIR=${CMAKE_BINARY_DIR}/plugins")

# Per-Window-Layouts (QTMUX-83, Modul A): Window/WindowModel liegen wie SessionModel
# im QML-Modul → die .cpp direkt mitkompilieren (Qt6::Qml wegen QML_ELEMENT). Reine
# Datenschicht ohne Prozesse, deshalb in der Logik-Kategorie.
qt_add_executable(test_windowmodel tst_windowmodel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/WindowModel.cpp)
target_include_directories(test_windowmodel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels)
target_link_libraries(test_windowmodel PRIVATE qtmux_core Qt6::Test Qt6::Qml)
add_test(NAME test_windowmodel COMMAND test_windowmodel)

# Einstellungs-Export/Import/Reset (Design 1a, Stufe 6): SettingsIo liegt wie
# SftpClient im QML-Modul → .cpp direkt mitkompilieren. Reine QSettings-/JSON-Logik,
# läuft ohne GUI und ohne Prozesse.
qt_add_executable(test_settingsio tst_settingsio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/SettingsIo.cpp)
target_include_directories(test_settingsio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels)
target_link_libraries(test_settingsio PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_settingsio COMMAND test_settingsio)

# --- PTY-/ConPTY-E2E-Tests (starten echte Shells) ---------------------------
qt_add_executable(test_pty tst_pty.cpp)
target_link_libraries(test_pty PRIVATE qtmux_core Qt6::Test)

# Sitzungsgruppen (QTMUX-42): prüft die Blockbildung im SessionModel. Das Model
# liegt im QML-Modul (nicht in qtmux_core) → .cpp direkt mitkompilieren, wie bei
# test_sftp. Legt echte Shell-Sessions an, deshalb in der E2E-Kategorie.
qt_add_executable(test_sessiongroups tst_sessiongroups.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/SessionModel.cpp)
target_include_directories(test_sessiongroups PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugins)
target_link_libraries(test_sessiongroups PRIVATE qtmux_core Qt6::Test Qt6::Qml)

qt_add_executable(test_session tst_session.cpp)
target_link_libraries(test_session PRIVATE qtmux_core Qt6::Test)

if(WIN32)
    # Zweiteilige Windows-Lösung gegen Konsolen-Vererbung (sonst hängen sich die
    # ConPTY-Kindshells an eine geerbte Konsole statt an die Pseudo-Konsole):
    #  1) GUI-Subsystem  -> keine automatisch allozierte Konsole.
    #  2) run_detached.ps1 -> Start losgelöst von ctest, damit auch dessen Konsole
    #     nicht geerbt wird. Exit-Code wird durchgereicht.
    set_target_properties(test_pty test_session test_sessiongroups
                          PROPERTIES WIN32_EXECUTABLE TRUE)
    foreach(t test_pty test_session test_sessiongroups)
        add_test(NAME ${t} COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_detached.ps1" "$<TARGET_FILE:${t}>")
    endforeach()
else()
    add_test(NAME test_pty COMMAND test_pty)
    add_test(NAME test_session COMMAND test_session)
    add_test(NAME test_sessiongroups COMMAND test_sessiongroups)
endif()

# --- Doku-Wächter (QTMUX-34) ------------------------------------------------
# Findet doppelte Überschriften, wie sie entstehen, wenn beim Kompaktieren einer
# langen Markdown-Datei ein Block eingefügt statt ersetzt wird. Reines CMake-Skript,
# damit der Test auf allen drei Plattformen ohne sh/grep läuft.
add_test(NAME test_doc_duplicates
    COMMAND ${CMAKE_COMMAND}
        "-DDOC_FILES=${CMAKE_SOURCE_DIR}/CLAUDE.md;${CMAKE_SOURCE_DIR}/README.md;${CMAKE_SOURCE_DIR}/docs/MCP.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckDocDuplicates.cmake")
