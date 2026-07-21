# --- Reine Logik-Tests (laufen direkt, keine echten Prozesse) ---------------
qt_add_executable(test_vtscreen tst_vtscreen.cpp)
target_link_libraries(test_vtscreen PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_vtscreen COMMAND test_vtscreen)

qt_add_executable(test_linkdetector tst_linkdetector.cpp)
target_link_libraries(test_linkdetector PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_linkdetector COMMAND test_linkdetector)

qt_add_executable(test_agent tst_agent.cpp)
target_link_libraries(test_agent PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_agent COMMAND test_agent)

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

# --- PTY-/ConPTY-E2E-Tests (starten echte Shells) ---------------------------
qt_add_executable(test_pty tst_pty.cpp)
target_link_libraries(test_pty PRIVATE qtmux_core Qt6::Test)

qt_add_executable(test_session tst_session.cpp)
target_link_libraries(test_session PRIVATE qtmux_core Qt6::Test)

if(WIN32)
    # Zweiteilige Windows-Lösung gegen Konsolen-Vererbung (sonst hängen sich die
    # ConPTY-Kindshells an eine geerbte Konsole statt an die Pseudo-Konsole):
    #  1) GUI-Subsystem  -> keine automatisch allozierte Konsole.
    #  2) run_detached.ps1 -> Start losgelöst von ctest, damit auch dessen Konsole
    #     nicht geerbt wird. Exit-Code wird durchgereicht.
    set_target_properties(test_pty test_session PROPERTIES WIN32_EXECUTABLE TRUE)
    foreach(t test_pty test_session)
        add_test(NAME ${t} COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/run_detached.ps1" "$<TARGET_FILE:${t}>")
    endforeach()
else()
    add_test(NAME test_pty COMMAND test_pty)
    add_test(NAME test_session COMMAND test_session)
endif()

# --- Doku-Wächter (QTMUX-34) ------------------------------------------------
# Findet doppelte Überschriften, wie sie entstehen, wenn beim Kompaktieren einer
# langen Markdown-Datei ein Block eingefügt statt ersetzt wird. Reines CMake-Skript,
# damit der Test auf allen drei Plattformen ohne sh/grep läuft.
add_test(NAME test_doc_duplicates
    COMMAND ${CMAKE_COMMAND}
        "-DDOC_FILES=${CMAKE_SOURCE_DIR}/CLAUDE.md;${CMAKE_SOURCE_DIR}/README.md;${CMAKE_SOURCE_DIR}/docs/MCP.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckDocDuplicates.cmake")
