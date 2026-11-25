cmake_minimum_required(VERSION 3.24)

# macOS: die niedrigste Systemversion, auf der QTmux läuft — die EINE Stelle für
# den Wert (Familienvorgabe 2026-11-24: 13.0; Muster MacPCAN docs/SHARED.md,
# Konsumenten-Checkliste Punkt 10). Muss VOR project() stehen: Dort legt CMake
# CMAKE_OSX_DEPLOYMENT_TARGET als leeren Cache-Eintrag an, und ein späteres
# `set(... CACHE ...)` ohne FORCE wirkt nicht. Ohne diese Zeile trug jedes Release
# still das SDK des Bau-Macs als Untergrenze (1.10.0: macOS 27.2), und Qts
# Info.plist-Vorlage schrieb ein LEERES LSMinimumSystemVersion. Ein -D auf der
# Kommandozeile gewinnt weiter; ein bestehender Bauordner mit leerem Eintrag
# bekommt den Wert (FORCE nur bei leerem Eintrag). Die Info.plist liest dieselbe
# Variable (Qts Vorlage macos/Info.plist.app.in). Wächter: ctest
# test_macos_min_version (App, Info.plist, update-swap); am fertigen DMG prüft
# installer/build-dmg.sh mit tools/ci/dmg_minos.py. Nur arm64 (Owner 2026-11-24):
# CMAKE_OSX_ARCHITECTURES bleibt unbelegt, gebaut wird für den Bau-Mac.
# ⚠️ Mit 13.0 übersetzt `std::to_chars` mit Gleitkomma nicht (erst ab 13.3), auch
# nicht über std::format oder Testausgaben von chrono-Typen.
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
        "Lowest macOS QTmux runs on (Qt 6.10 itself needs 13.0)" FORCE)
endif()

project(QTmux
    VERSION 1.10.0
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
    src/core/LinkDetector.h
    src/core/LinkDetector.cpp
    src/core/ScreenLinks.h
    src/core/ScreenLinks.cpp
    src/core/TerminalSelection.h
    src/core/TerminalSelection.cpp
    src/core/FileManagerReveal.h
    src/core/FileManagerReveal.cpp
    src/core/ExecutableTarget.h
    src/core/ExecutableTarget.cpp
    src/core/KeyEncoding.h
    src/core/KeyEncoding.cpp
    src/core/TerminalGrid.h
    src/core/TerminalGrid.cpp
    src/core/GitInfo.h
    src/core/GitInfo.cpp
    src/core/RestoreMode.h
    src/core/AltScroll.h
    src/core/ShellIntegration.h
    src/core/ShellIntegration.cpp
    src/core/SleepInhibitor.h
    src/core/SleepInhibitor.cpp
    src/core/TerminalSearch.h
    src/core/TerminalSearch.cpp
    src/core/ProjectCommands.h
    src/core/ProjectCommands.cpp
    src/core/ColorScheme.h
    src/core/ColorScheme.cpp
    src/core/ConnectionProfile.h
    src/core/ConnectionProfile.cpp
    src/core/HotkeyRegistry.h
    src/core/HotkeyRegistry.cpp
    src/core/AgentEventHub.h
    src/core/AgentEventHub.cpp
    src/core/AgentInbox.h
    src/core/AgentInbox.cpp
    src/core/OwnerDraft.h
    src/core/OwnerDraft.cpp
    src/core/PromptQueue.h
    src/core/PromptQueue.cpp
    src/core/StallDetector.h
    src/core/StallDetector.cpp
    src/core/SafeFileRead.h
    src/core/SafeFileRead.cpp
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

# Shell-Helfer als Ressource (QTMUX-38) -------------------------------------
# Die Skripte lagen bisher NUR im Repo — wer QTmux aus DMG/MSI/ZIP/AppImage bezieht,
# hatte sie nicht, obwohl genau er derjenige ist, der einen Stop-Hook einrichtet.
# Mitpaketieren allein löst das nicht: Ein AppImage wird bei jedem Start unter einem
# anderen `/tmp/.mount_XXXXXX` gemountet, ein dorthin zeigender Hook-Eintrag überlebt
# also keinen Neustart. Deshalb stecken sie im Binary und werden per
# `qtmux --install-shell-integration` an einen stabilen Ort geschrieben. Nebeneffekt:
# die installierte Fassung passt zwangsläufig zur laufenden Version.
qt_add_resources(qtmux_core "shell_integration"
    PREFIX "/shell-integration"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/shell-integration"
    FILES
        shell-integration/qtmux.bash
        shell-integration/qtmux.zsh
        shell-integration/qtmux.ps1
        shell-integration/qtmux-event.cmd
        shell-integration/qtmux-emit.sh
        shell-integration/qtmux-emit.ps1
        shell-integration/qtmux-emit.cmd
        shell-integration/qtmux-wait.sh
        shell-integration/qtmux-wait.ps1
        shell-integration/qtmux-wait.cmd
        shell-integration/qtmux-inbox.sh
        shell-integration/README.md
)

# Windows: ConPTY (kernel32, default) + TCP-Tabelle/Sockets für ProcessInfo.
if(WIN32)
    target_link_libraries(qtmux_core PRIVATE iphlpapi ws2_32)
endif()

# macOS: Carbon für den globalen Quake-Hotkey (RegisterEventHotKey),
# IOKit für die Ruhezustands-Sperre (IOPMAssertion, QTMUX-89).
if(APPLE)
    target_link_libraries(qtmux_core PRIVATE "-framework Carbon" "-framework IOKit")
endif()

# Versions-Header (generiert) ------------------------------------------------
# EINE Quelle für die zur Laufzeit angezeigte Version: `project(... VERSION ...)`.
# Ersetzt die Hardcodes in main.cpp (setApplicationVersion) und McpServer.cpp
# (serverInfo) — beide liefen zwangsläufig irgendwann auseinander, und die
# MCP-Antwort ist genau das, woran ein Agent die Version abliest.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/qtmux_version.h" @ONLY)

# Build-ID `<version>+<git-short-hash>[-dirty]` (Owner-Vorgabe, workspace-weit).
# ⚠️ VORLÄUFIG: Der kanonische Baustein entsteht in MacPCAN und wird später
# vendiert; beim Tausch ändert sich nur die Quelle, nicht die Anzeige.
#
# 🔑 Bewusst ein eigenes Target statt eines zweiten `configure_file`: Letzteres
# liefe zur CONFIGURE-Zeit, und ein Commit ändert keine CMake-Datei — der Hash
# bliebe stehen und die Anwendung behauptete einen fremden Stand. Das Target
# läuft bei JEDEM Build; das Skript schreibt den Header nur bei Inhaltsänderung
# (sonst zöge der neue Zeitstempel jedes Mal einen Rebuild nach sich).
set(qtmux_buildid_header "${CMAKE_CURRENT_BINARY_DIR}/generated/qtmux_buildid.h")
add_custom_target(qtmux_buildid ALL
    BYPRODUCTS "${qtmux_buildid_header}"
    COMMAND "${CMAKE_COMMAND}"
            -DQTMUX_SRC=${CMAKE_CURRENT_SOURCE_DIR}
            -DQTMUX_OUT=${qtmux_buildid_header}
            -DQTMUX_VERSION=${PROJECT_VERSION}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildId.cmake"
    COMMENT "Build-ID aus Git ermitteln"
    VERBATIM)
# Einmal beim Konfigurieren erzeugen, damit der Header schon beim ERSTEN
# Compile-Lauf existiert (das Target läuft sonst evtl. zu spät).
execute_process(COMMAND "${CMAKE_COMMAND}"
    -DQTMUX_SRC=${CMAKE_CURRENT_SOURCE_DIR}
    -DQTMUX_OUT=${qtmux_buildid_header}
    -DQTMUX_VERSION=${PROJECT_VERSION}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildId.cmake")

# Update-Kern (QTMUX-125) -----------------------------------------------------
# `third_party/updater/update/` ist eine BYTE-IDENTISCHE Kopie von
# MacPCANs `src/update/` (Namespace `appupdate`) — QTmux teilt mit MacPCAN/RAFTNG
# keine Submodule, also derselbe Vendoring-Weg wie bei libvterm. Herkunft,
# gepinnter Commit und die Einbahnstraßen-Regel: third_party/updater/UPSTREAM.md,
# Abgleich per tools/check-updater-sync.sh.
#
# 🔑 Eigenes Target, NICHT Teil von qtmux_core: der Kern bleibt Qt6::Core-only
# (Projektregel), `UpdateChecker` braucht aber QNetworkAccessManager.
# 🔑 Der Include-Pfad zeigt auf `third_party/updater`, weil sich der Kern selbst
# mit dem Präfix `update/` inkludiert — das gehört zur Byte-Identität.
#
# 🔑 Die Dateiliste wird GEGLOBBT, und das ist hier bewusst richtig (2026-08-05,
# Owner-Auftrag „ein Wächter, den man von Hand nachziehen muss, fällt irgendwann
# still aus"). Vorher stand hier eine Aufzählung; sie war die einzige Stelle, die
# beim Nachvendieren von Hand gepflegt werden musste. Eine neu vendierte Datei war
# damit sync-grün (tools/check-updater-sync.sh vergleicht BEIDE Bäume und sieht
# neue Dateien selbst) und trotzdem nicht übersetzt — genau die stille Lücke, die
# bei der kommenden Proxy-Erweiterung aufgeschlagen wäre.
#
# ⚠️ Der übliche Einwand gegen GLOB — „er verdeckt Drift" — trägt für DIESEN Baum
# nicht: Sein Inhalt wird nicht hier verantwortet, sondern per SHA-256 gegen
# MacPCAN geprüft. Die explizite Liste machte also keine zusätzliche Drift
# sichtbar, sie war reine Redundanz zu einem stärkeren Wächter. Für eigenen Code
# gilt die Regel unverändert: dort wird aufgezählt.
# CONFIGURE_DEPENDS lässt CMake bei jedem Build neu globben, sodass eine neue
# Datei auch ohne manuelles Re-Configure ankommt.
file(GLOB qtmux_updater_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/ed25519/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/ed25519/*.c"
    # Vertrag V2: Die Logik des Windows-Helfers gehört auf JEDEM System zur Lib
    # (so testet der Hub sie überall). Nur diese zwei Dateien, nicht das ganze
    # Verzeichnis: UpdateHelperMain.cpp ist das eigene Programm update-helper.exe
    # und hat ein eigenes main(). swaptool/ ist ebenfalls ein eigenes Programm.
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/winhelper/UpdateHelperCore.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/winhelper/UpdateHelperCore.cpp"
)
# Der Szenen-Baustein (UpdateShowcase: ein Ablauf mit Wegwerf-Schlüssel und
# Server auf 127.0.0.1, nur für Bildläufe) gehört NICHT in den Kern, den die
# ausgelieferte App linkt. Er bekommt eine eigene Lib, die nur das Bildziel
# nimmt; der Hub baut ihn genauso getrennt.
list(FILTER qtmux_updater_sources EXCLUDE REGEX "/UpdateShowcase\\.(hpp|cpp)$")
# Ein leerer oder halber Glob darf NICHT in einem unverständlichen Linkfehler
# enden: fehlt der vendierte Baum (unvollständiger Checkout), sagt es das hier.
foreach(qtmux_updater_required
        UpdateManifest.cpp UpdateChecker.cpp InstallerLauncher.cpp Ed25519Verify.cpp
        UpdateFlow.cpp UpdateSettings.cpp UpdateTexts.cpp SelfInstall.cpp
        SelfInstallWin.cpp UpdateShowcase.cpp selfinstall-helpers.cmake
        winhelper/UpdateHelperCore.cpp UpdaterCatalogs.cmake)
    set(qtmux_updater_probe
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/${qtmux_updater_required}")
    if(NOT EXISTS "${qtmux_updater_probe}")
        message(FATAL_ERROR
            "qtmux_updater: ${qtmux_updater_required} fehlt in third_party/updater/update/. "
            "Vendierter Update-Kern unvollständig — tools/check-updater-sync.sh --update ausführen.")
    endif()
endforeach()
add_library(qtmux_updater STATIC ${qtmux_updater_sources})
target_include_directories(qtmux_updater PUBLIC third_party/updater)
target_link_libraries(qtmux_updater PUBLIC Qt6::Core Qt6::Network)
if(APPLE)
    # SelfInstallMac.cpp prüft die Signatur des neuen Bundles gegen die
    # Designated Requirement der laufenden App (Vertrag V2 §6).
    target_link_libraries(qtmux_updater PUBLIC
        "-framework Security" "-framework CoreFoundation")
endif()
if(WIN32)
    # SelfInstallWin.cpp fragt Windows Installer nach dem installierten Produkt
    # (MsiEnumRelatedProducts, MsiGetProductInfo).
    target_link_libraries(qtmux_updater PUBLIC msi)
endif()
# Die Update-Texte bringt der Hub als eigene Kataloge mit (Vertrag V2 §4.5).
# Dieselbe Funktion ruft der Hub auf seiner Lib auf. Hier läuft nur lrelease, nie
# lupdate: Die vendierten .ts müssen byte-gleich bleiben. qtmux_updater steht
# deshalb auch NICHT in SOURCE_TARGETS von qt_add_translations (unten).
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/UpdaterCatalogs.cmake")
appupdate_add_catalogs(qtmux_updater)

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
    # 🔑 Die .ico ist für CMake/Ninja KEINE Abhängigkeit des .rc: der RC-Scan erfasst nur
    # #include, nicht die Dateien hinter ICON-Anweisungen (ninja -t deps: "#deps 0").
    # Ohne diese Zeile bettet ein inkrementeller Build ein geändertes Icon nie ein —
    # 1.9.4/1.9.5 gingen so mit dem alten Icon raus (QTMUX-137).
    set_source_files_properties("${qtmux_platform_icon}" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/appicon/qtmux.ico")
endif()

# 🔑 Zwei Programme, EIN Satz Objekte: Alles außer main.cpp (QML-Modul, Rendering,
# Viewmodels, MCP-Server, Update-Verdrahtung) steht in der statischen Bibliothek
# qtmux_app. Die App `qtmux` linkt sie, das Bildziel `qtmux-bilder` (unten, Schalter
# QTMUX_UPDATE_PICTURES) ebenso — so übersetzt der Bau nichts doppelt außer main.cpp.
# Das QML-Modul „QTmux“ wird dadurch ein statisches Plugin: main.cpp holt es mit
# Q_IMPORT_QML_PLUGIN(QTmuxPlugin) herein.
qt_add_library(qtmux_app STATIC
    src/app/UpdateQuit.h
    src/app/UpdateQuit.cpp
    src/app/UpdateSelfTest.h
    src/app/UpdateSelfTest.cpp
)

# --- Editor-Unterstützung für QML (qmlls) ------------------------------------
# Ohne Importpfade findet der QML-Language-Server weder `QtQuick` noch unser eigenes
# Modul `QTmux` — jede Typreferenz wird dann zur Fehlermeldung. Gemessen: 1092 Meldungen
# über 28 Dateien, davon nur 372 echt. Qt schreibt die nötige `.qmlls.ini` (buildDir +
# Importpfade) selbst; sie enthält absolute Pfade und ist darum git-ignoriert.
set(QT_QML_GENERATE_QMLLS_INI ON)
# 🔑 `no-cmake-calls=true` ist hier Pflicht, nicht Geschmack: qmlls ruft sonst von sich aus
# CMake, um das Modul nachzubauen. Auf der Windows-Maschine hätte das zwei bekannte Folgen
# (QTMUX-79) — es liefe ohne die VS-2022-Umgebung (fehlendes INCLUDE/LIB, verdorbener
# Cache), und es baute in genau das Verzeichnis, aus dem die produktive Instanz läuft
# (LNK1168, s. Windows-Kasten in CLAUDE.md). Der Editor soll lesen, nicht bauen.
set(QT_QML_GENERATE_QMLLS_INI_NO_CMAKE_CALLS ON)

qt_add_qml_module(qtmux_app
    URI QTmux
    CLASS_NAME QTmuxPlugin
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/QTmux"
    QML_FILES
        qml/Main.qml
        qml/SplitNode.qml
        qml/PrefsWindow.qml
        qml/dialogs/UpdateDialog.qml
        qml/dialogs/ProxyAuthDialog.qml
        qml/dialogs/ExecOpenDialog.qml
        qml/dialogs/UpdateNotes.qml
        qml/Ui/UpdateIcon.qml
        qml/Ui/UpdateHint.qml
        qml/Ui/UpdateButton.qml
        qml/Ui/CommitOnClose.qml
        qml/Ui/IconToolButton.qml
        qml/Ui/ShortcutMenuItem.qml
        qml/Ui/AppMenuItem.qml
        qml/Ui/AppPopupBg.qml
        qml/Ui/ThemedMenu.qml
        qml/Ui/AppComboBox.qml
        qml/Ui/AppDialog.qml
        qml/Ui/AppToolTip.qml
        qml/Ui/SectionLabel.qml
        qml/Ui/PrefRow.qml
        qml/Ui/AppSwitch.qml
        qml/Ui/PrefGroup.qml
        qml/Ui/SegmentedControl.qml
        qml/prefs/CatPage.qml
        qml/prefs/PrefAnchor.qml
        qml/prefs/CatAllgemein.qml
        qml/prefs/CatUpdates.qml
        qml/prefs/CatErscheinungsbild.qml
        qml/prefs/CatTerminal.qml
        qml/prefs/CatEingabe.qml
        qml/prefs/CatAgenten.qml
        qml/prefs/CatHotkeys.qml
        qml/prefs/CatVerbindungen.qml
        qml/prefs/CatVault.qml
        qml/prefs/CatErweiterungen.qml
    SOURCES
        src/terminal/TerminalItem.h
        src/terminal/TerminalItem.cpp
        src/terminal/GlyphAtlas.h
        src/terminal/GlyphAtlas.cpp
        src/viewmodels/SessionModel.h
        src/viewmodels/SessionModel.cpp
        src/viewmodels/Window.h
        src/viewmodels/Window.cpp
        src/viewmodels/WindowModel.h
        src/viewmodels/WindowModel.cpp
        src/viewmodels/SftpClient.h
        src/viewmodels/SftpClient.cpp
        src/viewmodels/Theme.h
        src/viewmodels/Theme.cpp
        src/viewmodels/AppController.h
        src/viewmodels/AppController.cpp
        src/viewmodels/SettingsIo.h
        src/viewmodels/SettingsIo.cpp
        src/viewmodels/UpdateBridge.h
        src/viewmodels/UpdateBridge.cpp
        src/server/McpServer.h
        src/server/McpServer.cpp
        src/server/McpAccess.h
        src/server/McpAccess.cpp
        src/server/AgentInboxClient.h
        src/server/AgentInboxClient.cpp
)
target_include_directories(qtmux_app PUBLIC src/app src/terminal src/viewmodels src/server
                                           "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(qtmux_app PUBLIC Qt6::Network Qt6::Svg qtmux_updater)
# Ohne das kann der Header beim ersten Übersetzungslauf noch fehlen.
add_dependencies(qtmux_app qtmux_buildid)

# GPU-Glyph-Atlas (QTMUX-6): RHI-Shader für das Glyph-Material in .qsb kompilieren
# und unter qrc:/shaders/ einbetten. BATCHABLE, damit das Material in der
# Scene-Graph-Batch-Pipeline mitläuft. Liegt unter :/shaders/glyph.{vert,frag}.qsb.
qt_add_shaders(qtmux_app "terminal_shaders"
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
# Die Liste steht in EINER Variablen, weil tests/ (test_icons) dieselben Dateien
# einbettet und gegen alle Icon-Verweise im QML/C++ prüft — eine zweite Liste dort
# würde genau die Drift verdecken, die der Test finden soll (QTMUX-141).
set(QTMUX_ICON_FILES
        # Online-Update, Vertrag V2 (K2): die Icons, die der Hub-Ablauf nennt
        # (UpdateFlow::iconNames(), dazu lock-key und caret-right der Oberfläche),
        # Phosphor „regular“ aus derselben Quelle wie der Rest (s. NOTICE).
        resources/icons/arrow-circle-up.svg
        resources/icons/check-circle.svg
        resources/icons/circle-notch.svg
        resources/icons/download-simple.svg
        resources/icons/folder-open.svg
        resources/icons/package.svg
        resources/icons/power.svg
        resources/icons/shield-check.svg
        resources/icons/warning.svg
        resources/icons/warning-circle.svg
        resources/icons/wifi-slash.svg
        resources/icons/lock-key.svg
        resources/icons/caret-right.svg
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
        resources/icons/folder.svg
)
list(TRANSFORM QTMUX_ICON_FILES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
qt_add_resources(qtmux_app "icons"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons"
    FILES ${QTMUX_ICON_FILES}
)

target_link_libraries(qtmux_app PUBLIC
    qtmux_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

# Die App: main.cpp und das Plattform-Icon, alles andere aus qtmux_app. Das
# statische QML-Plugin (qtmux_appplugin) muss ausdrücklich mitgelinkt werden.
qt_add_executable(qtmux
    src/app/main.cpp
    ${qtmux_platform_icon}
)
target_link_libraries(qtmux PRIVATE qtmux_app qtmux_appplugin)

# Bildziel qtmux-bilder --------------------------------------------------------
# Bilder der Update-Oberfläche in jedem Zustand (src/app/UpdatePicture.h), mit dem
# Szenen-Baustein des Hubs: ein echter Ablauf mit Wegwerf-Schlüssel, Server auf
# 127.0.0.1 und einem Installationsweg, der nie etwas startet. Er gehört NICHT in
# die App: eigene Lib, eigenes Programm, in keinem Paket (die Paketskripte nehmen
# nur qtmux). Dieselben Objekte wie qtmux (qtmux_app); neu übersetzt werden nur
# main.cpp (mit QTMUX_UPDATE_PICTURES), UpdatePicture.cpp und UpdateShowcase.cpp.
# Wächter: test_release_has_no_showcase (tests/CMakeLists.txt).
option(QTMUX_UPDATE_PICTURES "Bildziel qtmux-bilder bauen (nie Teil eines Pakets)" ON)
if(QTMUX_UPDATE_PICTURES)
    add_library(qtmux_updater_showcase STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/UpdateShowcase.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/UpdateShowcase.cpp")
    target_link_libraries(qtmux_updater_showcase PUBLIC qtmux_updater Qt6::Core Qt6::Network)
    # Keine eigenen Sätze (die Szenen nehmen jeden Satz aus dem Ablauf).
    set_target_properties(qtmux_updater_showcase PROPERTIES QT_EXCLUDE_FROM_TRANSLATION ON)

    qt_add_executable(qtmux-bilder
        src/app/main.cpp
        src/app/UpdatePicture.h
        src/app/UpdatePicture.cpp)
    target_compile_definitions(qtmux-bilder PRIVATE QTMUX_UPDATE_PICTURES=1)
    target_link_libraries(qtmux-bilder PRIVATE qtmux_app qtmux_appplugin qtmux_updater_showcase)
    # Ein Werkzeug, keine App: kein Bundle, unter Windows ein Konsolenprogramm (die
    # Meldungen gehen ohne Umweg über stderr). Es läuft aus dem Bauordner neben
    # qtmux und nutzt dessen Qt-Laufzeit (windeployqt).
    set_target_properties(qtmux-bilder PROPERTIES
        MACOSX_BUNDLE FALSE WIN32_EXECUTABLE FALSE QT_EXCLUDE_FROM_TRANSLATION ON)
endif()

# Plugins (QTMUX-8) -----------------------------------------------------------
# Demo-Plugin als Beweis der Plugin-Kette + Kopiervorlage; landet in <build>/plugins.
# 🔑 Die Plugin-Verzeichnisse stehen VOR dem i18n-Block, damit qt_add_translations
# unten das (bedingt existierende) MacPCAN-Plugin-Target in SOURCE_TARGETS aufnehmen
# kann — ein Target, das erst nach dem Aufruf entsteht, wäre für lupdate unsichtbar.
# Beide Plugins hängen ihre POST_BUILD-Schritte nur ans eigene Target; auf qtmux
# greifen sie nur per Generator-Expression zu (Auswertung zur Generierungszeit),
# die Verschiebung ändert am Build also nichts.
add_subdirectory(plugins/echo)

# MacPCAN-Plugin (QTMUX-9): erstes echtes Plugin (CAN-Bus als Terminal-Backend).
# Nur macOS; baut sich selbst nur, wenn die PCBUSB-Bibliothek gefunden wird
# (sonst still übersprungen — s. plugins/macpcan/CMakeLists.txt).
add_subdirectory(plugins/macpcan)

# Übersetzungen (i18n) — .qm werden unter :/i18n/ eingebettet ----------------
# 🔑 SOURCE_TARGETS ist Pflicht, nicht Kosmetik: Ohne die Angabe scannt lupdate ALLE
# Targets des Projekts — auch die Tests. `tests/tst_i18n.cpp` fragt fuer QTMUX-117
# gezielt Qt-eigene Texte ab (Kontext QPlatformTheme, "Cancel"/"OK"/"Save"/"Close"),
# und lupdate trug sie daraufhin in unsere .ts ein. Da Deutsch die Quellsprache ist,
# finalisierte FinishSourceLanguageTs.cmake sie prompt als "Cancel" -> "Cancel".
# Damit lag in qtmux_de ein KONKURRIERENDER Eintrag zu dem aus qtbase_de, der dort
# korrekt "Abbrechen" liefert — genau der Knopf, um den es in QTMUX-117 ging. Sichtbar
# wurde es nur nicht, weil Qt die Translator in umgekehrter Installationsreihenfolge
# befragt und qtbase zuletzt installiert wird; ein Vertauschen der beiden Zeilen in
# main.cpp haette QTMUX-117 still zurueckgedreht.
# 🔑 Die Plugin-Adapterschicht (plugins/macpcan, z. B. CanText.h/MacPcanPlugin.cpp)
# muss mitgescannt werden: Das Target existiert nur bedingt (macOS + gefundene
# PCBUSB), darum bedingte Aufnahme — ein Build ohne Plugin darf nicht brechen.
# Der vendierte Kern (plugins/macpcan/vendor/, Namespace mac_pcan) ist Qt-frei
# und liefert konstruktionsbedingt keine tr()-Strings in die Kataloge.
# qtmux_app trägt QML und den C++-Teil der Oberfläche, qtmux nur main.cpp; beide
# werden gescannt. qtmux-bilder bringt keine eigenen Sätze und steht NICHT darin.
# Die .qm hängen an qtmux_app, damit beide Programme dieselben Kataloge tragen.
set(qtmux_i18n_source_targets qtmux_app qtmux qtmux_core)
if(TARGET qtmux_macpcan_plugin)
    list(APPEND qtmux_i18n_source_targets qtmux_macpcan_plugin)
endif()
qt_add_translations(qtmux_app
    SOURCE_TARGETS
        ${qtmux_i18n_source_targets}
    TS_FILES
        i18n/qtmux_de.ts
        i18n/qtmux_en.ts
    RESOURCE_PREFIX "/i18n"
)

# Qt-EIGENE Übersetzungen mitliefern (QTMUX-117) ------------------------------
# Die Beschriftungen der Standardknöpfe ("OK", "Cancel", "Save"…) kommen NICHT aus
# unseren .ts, sondern aus Qts eigener Übersetzung (Kontext QPlatformTheme). Ohne
# sie heißt der Knopf eines AppDialog mit `standardButtons` auch auf Deutsch
# "Cancel" — das betrifft jeden Dialog der App.
#
# 🔑 Eingebettet statt mitgeliefert: Ein Deployment-Werkzeug pro Plattform anders zu
# konfigurieren (macdeployqt kopiert translations NICHT, windeployqt schon,
# linuxdeploy je nach Plugin) wäre drei Fehlerquellen für dieselbe Datei — und die
# Diagnose "Knopf ist englisch" träfe nur die gepackte App, nie den Entwickler-Build.
# Als Ressource liegt die .qm überall gleich unter :/i18n/.
if(NOT DEFINED QT_TRANSLATIONS_DIR)
    get_target_property(_qtmux_qmake Qt6::qmake IMPORTED_LOCATION)
    execute_process(COMMAND "${_qtmux_qmake}" -query QT_INSTALL_TRANSLATIONS
                    OUTPUT_VARIABLE QT_TRANSLATIONS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
set(_qtmux_qtbase_qm "")
foreach(_lang de en)
    if(EXISTS "${QT_TRANSLATIONS_DIR}/qtbase_${_lang}.qm")
        list(APPEND _qtmux_qtbase_qm "${QT_TRANSLATIONS_DIR}/qtbase_${_lang}.qm")
    endif()
endforeach()
if(_qtmux_qtbase_qm)
    # BASE schneidet den Installationspfad ab -> :/i18n/qtbase_de.qm
    qt_add_resources(qtmux_app "qtmux_qt_translations"
        PREFIX "/i18n"
        BASE "${QT_TRANSLATIONS_DIR}"
        FILES ${_qtmux_qtbase_qm}
    )
else()
    # Kein Abbruch: eine Qt-Installation ohne Übersetzungen ist baubar, die
    # Standardknöpfe bleiben dann eben englisch (bisheriges Verhalten).
    message(WARNING "qtbase_<lang>.qm nicht gefunden in '${QT_TRANSLATIONS_DIR}' — "
                    "Standardknöpfe (OK/Cancel) bleiben unübersetzt (QTMUX-117).")
endif()

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
    # Beide Versionsfelder ausdrücklich = PROJECT_VERSION (Vertrag V2 §6, K2). Das
    # Selbst-Ersetzen vergleicht CFBundleVersion des neuen Bundles mit der Version
    # aus dem Manifest; die setzte Qt bisher schon selbst. Die Kurzversion (Finder,
    # „Über“) stand ohne diese Zeilen auf „1.9“ (gemessen an 1.9.15).
    # Wächter: test_installer_self_update_contract.
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
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
    # Offscreen-Plattform-Plugin zusätzlich mitnehmen: windeployqt liefert nur
    # qwindows.dll aus, aber `--screenshot` setzt QT_QPA_PLATFORM=offscreen. Fehlt das
    # Plugin, beendet Qt den Prozess mit qFatal — unter Windows als Absturz
    # (Release 0xC0000409, Debug 0x80000003). Über das Plugin-Target statt über einen
    # festen Pfad, damit Debug (qoffscreend.dll) und Release automatisch stimmen.
    # main.cpp hat zusätzlich einen Wächter, damit eine Installation OHNE das Plugin
    # ausweicht statt abzustürzen — beides zusammen, nicht eines davon.
    if(TARGET Qt6::QOffscreenIntegrationPlugin)
        add_custom_command(TARGET qtmux POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:qtmux>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:qtmux>/platforms/"
            COMMENT "Offscreen-Plattform-Plugin fuer --screenshot mitliefern"
            VERBATIM)
    else()
        message(WARNING "Qt6::QOffscreenIntegrationPlugin nicht gefunden — "
                        "--screenshot weicht auf das sichtbare Fenster aus.")
    endif()
endif()

# Selbst-Ersetzen (Vertrag V2, Stufe 2) -----------------------------------------
# Die Helfer kommen aus dem vendierten Kern: unter macOS update-swap nach
# QTmux.app/Contents/Helpers/, unter Windows update-helper.exe neben qtmux.exe
# (das MSI muss ihn dort mitnehmen, installer/build-msi.ps1 bricht sonst ab);
# Linux braucht keinen. Ohne den Helfer bietet QTmux nur „von Hand“ an, und das
# fiele bei einem Entwicklerbau nicht auf (dort greift zuerst ein anderer Grund).
# Der Test vergleicht die eingebettete Kopie Byte für Byte mit dem gebauten Helfer.
# BUILD_ID_*: Der Helfer trägt die Build-ID von QTmux (Hub 0.8.1, MAC-132), nicht
# „unknown“. Der Kopf entsteht schon beim Konfigurieren (execute_process oben) und
# wird vom Ziel qtmux_buildid bei jedem Bau nachgeführt; davon hängt der Helfer ab.
include("${CMAKE_CURRENT_SOURCE_DIR}/third_party/updater/update/selfinstall-helpers.cmake")
set(qtmux_helper_buildid
    BUILD_ID_HEADER "${qtmux_buildid_header}"
    BUILD_ID_MACRO QTMUX_BUILD_ID
    BUILD_ID_DEPENDS qtmux_buildid)
if(QTMUX_BUILD_TESTS)
    enable_testing()
    appupdate_embed_install_helpers(qtmux CHECK_TEST test_update_helper_embedded
        ${qtmux_helper_buildid})
else()
    appupdate_embed_install_helpers(qtmux ${qtmux_helper_buildid})
endif()

# Tests ----------------------------------------------------------------------
if(QTMUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
