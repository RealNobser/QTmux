# selfinstall-helpers.cmake — the install helpers of the self-installation
# and the one call that puts them into an app.
#
#   include(<this update folder>/selfinstall-helpers.cmake)
#   appupdate_embed_install_helpers(<app target> [CHECK_TEST <test name>]
#       [BUILD_ID_HEADER <header> BUILD_ID_MACRO <macro> [BUILD_ID_DEPENDS <target>]])
#
# CANONICAL: consumers that vendor this folder include this file themselves;
# a consumer that adds the hub with add_subdirectory (also EXCLUDE_FROM_ALL)
# only calls the function, the hub has already included it.
#
# Including defines the helper programs (once per build):
#   macOS    update-swap   the swap program (renamex_np RENAME_SWAP: the name
#                          of the app is never free). libc, Security and
#                          CoreFoundation; needs the C language enabled.
#   Windows  update-helper the update helper: waits for the app, hashes the
#                          MSI again, runs msiexec unattended with the install
#                          folder, writes the result file, starts the app
#                          again unelevated.
#   Linux    nothing: replacing an AppImage needs no helper.
#
# The function makes the helper part of the app:
#   - the app target depends on the helper, so a project that adds this folder's
#     project with EXCLUDE_FROM_ALL still builds it (without the call nothing
#     builds it there). The copy below names the helper and already pulls it
#     in; the explicit dependency says so on its own;
#   - after every build the helper is copied to where the app looks for it:
#     macOS <app>.app/Contents/Helpers/update-swap, Windows update-helper.exe
#     next to the app's exe (the MSI must carry it there as well);
#   - CHECK_TEST <name> adds a test that compares the embedded copy with the
#     built helper byte for byte (it fails when the copy is missing). Testing
#     must be enabled for the calling directory by the end of it
#     (enable_testing() there, later in the same file, or in a parent BEFORE
#     add_subdirectory); otherwise CMake drops the test without a word, and
#     the configure step stops instead (checked at the end of the directory).
#     Where both are the same file (Windows, when the app and the helper are
#     built into one folder) it only proves the helper is there;
#   - BUILD_ID_HEADER/BUILD_ID_MACRO give the helper the APP's build id: the
#     header (the one the app's own build-id step writes, with
#     `#define <macro> "<version>+<hash>[-dirty]"`) is included first into the
#     helper's source (a forced include), and APPUPDATE_HELPER_BUILD_ID is
#     defined as <macro>; BUILD_ID_DEPENDS names the target that refreshes the
#     header, so the helper is rebuilt with every new commit. The id belongs to
#     the app's tree, not to the folder this file lies in (a submodule has its
#     own hash). One helper per build: a second app with another header stops
#     the configure step. Without the options the helper says "unknown";
#   - anything missing stops the configure step with FATAL_ERROR.
# Without the helper the app still runs, but offers only "install by hand"
# (HelperUnavailable) — easy to miss, because on a developer build another
# reason usually comes first. That is why this is a build fact, not a habit.
#
# The app never runs the helper from where it is embedded: it checks a
# private copy (macOS: against its own identity, Windows: in a folder only
# the user can write) and only that copy runs. On macOS the release signing
# gives update-swap the identifier "<bundle id>.update-swap"; the app checks
# exactly that.

include_guard(GLOBAL)

# Only needed while the targets are defined below (a plain variable: a cache
# entry would be overwritten silently by a second copy of this folder).
set(APPUPDATE_HELPERS_DIR "${CMAKE_CURRENT_LIST_DIR}")
# The sources include "update/…" — the root is the folder above this one.
get_filename_component(_appupdate_helpers_root "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)

if(APPLE AND NOT TARGET update-swap)
    get_property(_appupdate_langs GLOBAL PROPERTY ENABLED_LANGUAGES)
    if(NOT "C" IN_LIST _appupdate_langs)
        message(FATAL_ERROR
            "selfinstall-helpers.cmake: update-swap is written in C. Enable the C "
            "language (project(... LANGUAGES C CXX) or enable_language(C)) before "
            "this include.")
    endif()
    add_executable(update-swap "${APPUPDATE_HELPERS_DIR}/swaptool/update_swap.c")
    set_target_properties(update-swap PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    # --check-running asks the Security framework about the running code.
    target_link_libraries(update-swap PRIVATE "-framework Security" "-framework CoreFoundation")
endif()

if(WIN32 AND NOT TARGET update-helper)
    # No Qt (its logic, UpdateHelperCore, is part of the updater library and
    # tested on every system), GUI subsystem (no console window), static
    # runtime (it runs from a copy outside the install folder, next to nothing).
    add_executable(update-helper WIN32
        "${APPUPDATE_HELPERS_DIR}/winhelper/UpdateHelperMain.cpp"
        "${APPUPDATE_HELPERS_DIR}/winhelper/UpdateHelperCore.cpp"
        "${APPUPDATE_HELPERS_DIR}/winhelper/UpdateHelperCore.hpp"
    )
    target_include_directories(update-helper PRIVATE "${_appupdate_helpers_root}")
    target_compile_definitions(update-helper PRIVATE UNICODE _UNICODE)
    target_link_libraries(update-helper PRIVATE bcrypt)
    # DLLs that the loader resolves before main (bcrypt, shell32) come only
    # from System32, never from the folder of the copy (0x800 =
    # LOAD_LIBRARY_SEARCH_SYSTEM32); later loads: SetDefaultDllDirectories.
    if(MSVC)
        target_link_options(update-helper PRIVATE "/DEPENDENTLOADFLAG:0x800")
    endif()
    set_target_properties(update-helper PROPERTIES
        AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Runs at the end of the directory that asked for a CHECK_TEST (deferred):
# only then is it known whether enable_testing() came later in that file.
function(_appupdate_require_testing app test)
    if(NOT CMAKE_TESTING_ENABLED)
        message(FATAL_ERROR
            "appupdate_embed_install_helpers(${app} CHECK_TEST ${test}): testing is not enabled for "
            "${CMAKE_CURRENT_SOURCE_DIR}, so CMake would drop the test silently. Call enable_testing() "
            "in this directory (or in a parent before add_subdirectory), or leave CHECK_TEST out.")
    endif()
endfunction()

function(appupdate_embed_install_helpers app)
    cmake_parse_arguments(PARSE_ARGV 1 _ai "" "CHECK_TEST;BUILD_ID_HEADER;BUILD_ID_MACRO;BUILD_ID_DEPENDS" "")
    if(_ai_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "appupdate_embed_install_helpers(${app}): unknown arguments '${_ai_UNPARSED_ARGUMENTS}'")
    endif()
    if(NOT TARGET ${app})
        message(FATAL_ERROR
            "appupdate_embed_install_helpers: '${app}' is not a target (call it after the app is defined)")
    endif()

    if(APPLE)
        get_target_property(_bundle ${app} MACOSX_BUNDLE)
        if(NOT _bundle)
            message(FATAL_ERROR
                "appupdate_embed_install_helpers(${app}): the app is no bundle (MACOSX_BUNDLE); "
                "update-swap belongs to Contents/Helpers/")
        endif()
        if(NOT TARGET update-swap)
            message(FATAL_ERROR
                "appupdate_embed_install_helpers(${app}): the target update-swap is missing; "
                "include selfinstall-helpers.cmake first")
        endif()
        set(_helper update-swap)
        set(_dest "$<TARGET_BUNDLE_CONTENT_DIR:${app}>/Helpers/update-swap")
        add_dependencies(${app} update-swap)
        # A dependency on the FILE, not only on the order: without it a build
        # in which only update-swap changed does not link the app again, the
        # POST_BUILD copy below does not run, and the bundle keeps the old
        # helper (measured 2026-11-24; the hub's CHECK_TEST caught it, a
        # consumer without one would ship it). LINK_DEPENDS makes the app link
        # again whenever the helper is newer — rare, and then necessary.
        set_property(TARGET ${app} APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:update-swap>")
        add_custom_command(TARGET ${app} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${app}>/Helpers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:update-swap>" "${_dest}"
            COMMENT "Embedding update-swap into ${app}.app/Contents/Helpers/"
            VERBATIM)
    elseif(WIN32)
        if(NOT TARGET update-helper)
            message(FATAL_ERROR
                "appupdate_embed_install_helpers(${app}): the target update-helper is missing; "
                "include selfinstall-helpers.cmake first")
        endif()
        set(_helper update-helper)
        set(_dest "$<TARGET_FILE_DIR:${app}>/update-helper.exe")
        add_dependencies(${app} update-helper)
        # Where both land in the same output folder this copies nothing.
        add_custom_command(TARGET ${app} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:update-helper>" "${_dest}"
            COMMENT "Placing update-helper.exe next to ${app}"
            VERBATIM)
    else()
        return()
    endif()

    if(_ai_BUILD_ID_HEADER OR _ai_BUILD_ID_MACRO OR _ai_BUILD_ID_DEPENDS)
        if(NOT _ai_BUILD_ID_HEADER OR NOT _ai_BUILD_ID_MACRO)
            message(FATAL_ERROR
                "appupdate_embed_install_helpers(${app}): BUILD_ID_HEADER and BUILD_ID_MACRO go together")
        endif()
        if(NOT IS_ABSOLUTE "${_ai_BUILD_ID_HEADER}" OR NOT EXISTS "${_ai_BUILD_ID_HEADER}")
            message(FATAL_ERROR
                "appupdate_embed_install_helpers(${app}): BUILD_ID_HEADER '${_ai_BUILD_ID_HEADER}' is no "
                "existing absolute path (the build-id step must write it at configure time)")
        endif()
        if(NOT _ai_BUILD_ID_MACRO MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
            message(FATAL_ERROR
                "appupdate_embed_install_helpers(${app}): BUILD_ID_MACRO '${_ai_BUILD_ID_MACRO}' is no macro name")
        endif()
        get_property(_prev GLOBAL PROPERTY APPUPDATE_HELPER_BUILD_ID_HEADER)
        if(_prev AND NOT _prev STREQUAL _ai_BUILD_ID_HEADER)
            message(FATAL_ERROR
                "appupdate_embed_install_helpers(${app}): the helper ${_helper} already carries the build id of "
                "'${_prev}'; one helper per build cannot carry two")
        endif()
        if(NOT _prev)
            set_property(GLOBAL PROPERTY APPUPDATE_HELPER_BUILD_ID_HEADER "${_ai_BUILD_ID_HEADER}")
            if(MSVC)
                target_compile_options(${_helper} PRIVATE "/FI${_ai_BUILD_ID_HEADER}")
            else()
                target_compile_options(${_helper} PRIVATE "SHELL:-include \"${_ai_BUILD_ID_HEADER}\"")
            endif()
            target_compile_definitions(${_helper} PRIVATE "APPUPDATE_HELPER_BUILD_ID=${_ai_BUILD_ID_MACRO}")
            if(_ai_BUILD_ID_DEPENDS)
                if(NOT TARGET ${_ai_BUILD_ID_DEPENDS})
                    message(FATAL_ERROR
                        "appupdate_embed_install_helpers(${app}): BUILD_ID_DEPENDS '${_ai_BUILD_ID_DEPENDS}' is not a target")
                endif()
                add_dependencies(${_helper} ${_ai_BUILD_ID_DEPENDS})
            endif()
        endif()
    endif()

    if(_ai_CHECK_TEST)
        # The values are baked in now: a deferred call evaluates its
        # arguments only when it runs, where these variables do not exist.
        cmake_language(EVAL CODE
            "cmake_language(DEFER CALL _appupdate_require_testing [==[${app}]==] [==[${_ai_CHECK_TEST}]==])")
        add_test(NAME ${_ai_CHECK_TEST}
            COMMAND ${CMAKE_COMMAND} -E compare_files "${_dest}" "$<TARGET_FILE:${_helper}>")
        set_tests_properties(${_ai_CHECK_TEST} PROPERTIES TIMEOUT 30)
    endif()
endfunction()
