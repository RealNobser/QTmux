# --- Reine Logik-Tests (laufen direkt, keine echten Prozesse) ---------------
qt_add_executable(test_vtscreen tst_vtscreen.cpp)
target_link_libraries(test_vtscreen PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_vtscreen COMMAND test_vtscreen)

qt_add_executable(test_agent tst_agent.cpp)
target_link_libraries(test_agent PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_agent COMMAND test_agent)

qt_add_executable(test_profiles tst_profiles.cpp)
target_link_libraries(test_profiles PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_profiles COMMAND test_profiles)

qt_add_executable(test_hotkeys tst_hotkeys.cpp)
target_link_libraries(test_hotkeys PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_hotkeys COMMAND test_hotkeys)

qt_add_executable(test_vault tst_vault.cpp)
target_link_libraries(test_vault PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_vault COMMAND test_vault)

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
