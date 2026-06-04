qt_add_executable(test_pty tst_pty.cpp)
target_link_libraries(test_pty PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_pty COMMAND test_pty)

qt_add_executable(test_vtscreen tst_vtscreen.cpp)
target_link_libraries(test_vtscreen PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_vtscreen COMMAND test_vtscreen)

qt_add_executable(test_session tst_session.cpp)
target_link_libraries(test_session PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_session COMMAND test_session)
