# Vendored libvterm 0.3.3 (BSD-Lizenz, siehe LICENSE).
#
# Warum vendored statt vcpkg/brew? libvterm wurde aus vcpkg entfernt (kein Port,
# keine Version mehr), und ein eigener, generierter Tabellen-Satz (.inc) liegt
# bereits bei — d.h. KEIN perl zur Build-Zeit nötig. So bauen macOS/Windows/Linux
# identisch und ohne externe libvterm-Abhängigkeit.
#
# Quelle: https://github.com/neovim/libvterm (gepflegter Mirror, MSVC-tauglich).

add_library(vterm STATIC
    src/encoding.c
    src/keyboard.c
    src/mouse.c
    src/parser.c
    src/pen.c
    src/screen.c
    src/state.c
    src/unicode.c
    src/vterm.c
)

# C11: libvterm nutzt C99/C11-Konstrukte (designated initializers, compound
# literals, stdbool). MSVC braucht dafür mindestens /std:c11.
set_target_properties(vterm PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    # Reines C-Target: Qt-Autogen (global an) hier abschalten.
    AUTOMOC OFF
    AUTORCC OFF
    AUTOUIC OFF
)

# Öffentliche Header in include/, interne Header + .inc-Tabellen in src/.
target_include_directories(vterm
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(MSVC)
    # snprintf & Co. lösen sonst die "unsicheren CRT"-Warnungen aus.
    target_compile_definitions(vterm PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Alias passend zum bisherigen Konsumenten (target_link_libraries vterm::vterm).
add_library(vterm::vterm ALIAS vterm)
