# --- Reine Logik-Tests (laufen direkt, keine echten Prozesse) ---------------
qt_add_executable(test_vtscreen tst_vtscreen.cpp)
target_link_libraries(test_vtscreen PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_vtscreen COMMAND test_vtscreen)

qt_add_executable(test_linkdetector tst_linkdetector.cpp)
target_link_libraries(test_linkdetector PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_linkdetector COMMAND test_linkdetector)

# QTMUX-138: „Im Finder/Explorer/Dateimanager zeigen" — Befehlsermittlung aller drei
# Plattformen auf jedem Host + lokale Existenzprüfung.
qt_add_executable(test_filemanagerreveal tst_filemanagerreveal.cpp)
target_link_libraries(test_filemanagerreveal PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_filemanagerreveal COMMAND test_filemanagerreveal)

# QTMUX-141: Jeder Icon-Verweis im QML/C++ (auch zusammengesetzte über icon(name))
# muss als Ressource existieren. Bettet dieselbe Liste ein wie die App
# (QTMUX_ICON_FILES im Haupt-CMakeLists) — eine eigene Liste hier verdeckte die Drift.
qt_add_executable(test_icons tst_icons.cpp)
target_link_libraries(test_icons PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(test_icons PRIVATE
    QTMUX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
qt_add_resources(test_icons "test_icons_res"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../resources/icons"
    FILES ${QTMUX_ICON_FILES}
)
add_test(NAME test_icons COMMAND test_icons)

# QTMUX-142: Statusfarben als Theme-Rollen — Kontrastvertrag beider Designs gegen die
# echten Flächen (Theme.cpp mitkompiliert, liegt im QML-Modul) + Wächter gegen
# Farbliterale in qml/. Braucht Qt6::Gui (Theme fragt die StyleHints) und offscreen.
qt_add_executable(test_statuscolors tst_statuscolors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/Theme.cpp)
target_include_directories(test_statuscolors PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels)
target_compile_definitions(test_statuscolors PRIVATE
    QTMUX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(test_statuscolors PRIVATE qtmux_core Qt6::Gui Qt6::Qml Qt6::Test)
add_test(NAME test_statuscolors COMMAND test_statuscolors)

# QTMUX-139: Stillstand-Erkennung fuer Agent-Sessions — Zeit injiziert, keine echten Minuten.
qt_add_executable(test_stalldetector tst_stalldetector.cpp)
target_link_libraries(test_stalldetector PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_stalldetector COMMAND test_stalldetector)

qt_add_executable(test_keyencoding tst_keyencoding.cpp)
target_link_libraries(test_keyencoding PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_keyencoding COMMAND test_keyencoding)

qt_add_executable(test_terminalgrid tst_terminalgrid.cpp)
target_link_libraries(test_terminalgrid PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_terminalgrid COMMAND test_terminalgrid)

qt_add_executable(test_gitinfo tst_gitinfo.cpp)
target_link_libraries(test_gitinfo PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_gitinfo COMMAND test_gitinfo)

qt_add_executable(test_terminalsearch tst_terminalsearch.cpp)
target_link_libraries(test_terminalsearch PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_terminalsearch COMMAND test_terminalsearch)

# QTMUX-126: Zugriffsregeln des MCP-Servers (Bind-Adresse, Token-Pflicht). Die Einheit
# liegt bei ihrem einzigen Nutzer in src/server (qtmux_core bleibt Qt6::Core-only, hier
# braucht es QHostAddress) -> die .cpp direkt mitkompilieren, wie bei test_settingsio.
qt_add_executable(test_mcpaccess tst_mcpaccess.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/server/McpAccess.cpp)
target_include_directories(test_mcpaccess PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/server)
target_link_libraries(test_mcpaccess PRIVATE Qt6::Test Qt6::Network)
add_test(NAME test_mcpaccess COMMAND test_mcpaccess)

# Owner-Vorgabe 2026-09-28: MCP-Anlage ohne Fokuswechsel. Prüft die Signale von
# McpServer::callTool (kein focusRequested, focus-Wert an sessionCreated/newWindow/
# splitPane) und das Schema. McpServer, SessionModel und WindowModel liegen im
# QML-Modul → .cpp direkt mitkompilieren; die generierten Versions-Header brauchen
# das qtmux_buildid-Target. Legt nur serielle Sessions auf einem nicht vorhandenen
# Port an — startet also keinen Prozess und braucht keinen run_detached-Umweg.
qt_add_executable(test_mcpfocus tst_mcpfocus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/server/McpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/server/McpAccess.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/SessionModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/Window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/WindowModel.cpp)
target_include_directories(test_mcpfocus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/server
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/plugins
    ${CMAKE_BINARY_DIR}/generated)
add_dependencies(test_mcpfocus qtmux_buildid)
target_link_libraries(test_mcpfocus PRIVATE qtmux_core Qt6::Test Qt6::Qml Qt6::Network)
add_test(NAME test_mcpfocus COMMAND test_mcpfocus)

# QTMUX-96: Scanner fuer Projekt-Befehle/Skills — arbeitet auf echten QTemporaryDirs.
qt_add_executable(test_projectcommands tst_projectcommands.cpp)
target_link_libraries(test_projectcommands PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_projectcommands COMMAND test_projectcommands)

# QTMUX-90: Kern der Prompt-Warteschlange (FIFO + Abgabe-Regel), reine Logik.
qt_add_executable(test_promptqueue tst_promptqueue.cpp)
target_link_libraries(test_promptqueue PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_promptqueue COMMAND test_promptqueue)

# Sicherheitsbefund RAFTNG (2026-08-06): readAll() auf einen fremden Pfad liess
# den Prozess auf 30,4 GB wachsen. Der Test legt eine echte Gerätedatei und ein
# echtes FIFO vor — die Regel ohne sie zu pruefen hiesse, genau den Fall
# auszulassen, um den es geht.
qt_add_executable(test_safefileread tst_safefileread.cpp)
target_link_libraries(test_safefileread PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_safefileread COMMAND test_safefileread)

# QTMUX-129: Sitzungsspeicher für Proxy-Anmeldedaten. Wächter über die
# Ein-Versuch-Regel — `appupdate` erzwingt keine Obergrenze, also muss sie hier
# sitzen, sonst sperrt ein falsches Passwort das Domänen-Konto.
qt_add_executable(test_proxycredentials tst_proxycredentials.cpp)
target_link_libraries(test_proxycredentials PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_proxycredentials COMMAND test_proxycredentials)

# QTMUX-130: Wiederhergestellter Verlauf darf erst bei der ERSTEN echten Terminalgroesse
# eingespielt werden (sonst friert der Umbruch bei 80 Spalten ein). Laeuft gegen eine
# Backend-Attrappe — geprueft wird die Reihenfolge, die an einem echten PTY vom Zufall
# abhinge.
qt_add_executable(test_restorehistory tst_restorehistory.cpp)
target_link_libraries(test_restorehistory PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_restorehistory COMMAND test_restorehistory)

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

# QTMUX-125: der vendierte Update-Kern (third_party/updater, Target qtmux_updater).
# Die Fixtures entstehen zur Laufzeit in einem QTemporaryDir und werden mit dem
# mitkompilierten Monocypher signiert — kein openssl, keine committeten Binaerdateien,
# laeuft damit auf allen drei Plattformen gleich. Gegenprobe gegen eine „nur zu sich
# selbst passende" Krypto ist der RFC-8032-Vektor im Test.
qt_add_executable(test_updater tst_updater.cpp)
target_link_libraries(test_updater PRIVATE qtmux_updater Qt6::Test)
add_test(NAME test_updater COMMAND test_updater)

# QTMUX-125: die APP-Seite (UpdateViewModel) gegen committete, mit dem
# PRODUKTIONSSCHLUESSEL signierte Fixtures. Das ViewModel liegt wie SettingsIo im
# QML-Modul -> .cpp direkt mitkompilieren; der generierte qtmux_version.h kommt
# aus dem Binaerverzeichnis der Haupt-CMakeLists.
# 🔑 Der Fixture-Baum ist absichtlich echt signiert: das ViewModel baut seinen
# Checker OHNE Schluessel-Parameter, prueft also mit den eingebauten Bytes aus
# UpdateKeys.hpp. Ein Schluesselwechsel macht diesen Test rot — genau der
# gewuenschte Alarm (Begruendung in tests/fixtures/update/README.md).
qt_add_executable(test_updateviewmodel tst_updateviewmodel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels/UpdateViewModel.cpp
    # QTMUX-129: Der Sitzungsspeicher gehört zum ViewModel. Er wird hier
    # MITKOMPILIERT statt qtmux_core zu verlinken — der Test kommt bewusst ohne
    # den ganzen Kern aus (er braucht nur Core+Network, nicht SerialPort).
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/ProxyCredentials.cpp)
target_include_directories(test_updateviewmodel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/viewmodels
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
    ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(test_updateviewmodel PRIVATE
    QTMUX_UPDATE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/update")
target_link_libraries(test_updateviewmodel PRIVATE qtmux_updater Qt6::Test Qt6::Network)
add_test(NAME test_updateviewmodel COMMAND test_updateviewmodel)

# 🔑 Diese beiden Tests auf Windows als KONSOLEN-Programme bauen.
# `qt_add_executable` macht dort standardmaessig GUI-Programme — die schreiben
# nichts auf die Konsole, und `ctest --output-on-failure` zeigt dann NUR
# „***Failed", nicht WELCHER Fall fiel. Genau das ist im CI-Windows-Job passiert:
# ein sporadischer Fehlschlag ohne jede Diagnose. Die ConPTY-Begruendung fuer
# WIN32_EXECUTABLE (Konsolen-Vererbung an Kindshells) gilt hier nicht — beide
# Tests starten keine Prozesse.
if(WIN32)
    set_target_properties(test_updater test_updateviewmodel test_mcpaccess test_mcpfocus
                          PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

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

qt_add_executable(test_pastewrite tst_pastewrite.cpp)
target_link_libraries(test_pastewrite PRIVATE qtmux_core Qt6::Test)
add_test(NAME test_pastewrite COMMAND test_pastewrite)

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

# --- Selbsttest des Vendoring-Wächters (QTMUX-144) ---------------------------
# Prüft tools/check-updater-sync.sh an Attrappen-Bäumen — braucht keinen
# MacPCAN-Hub und läuft darum auch in der CI. Der Wächter selbst bleibt lokal
# (Begründung im Skriptkopf). Nur mit bash: Auf Windows gibt es im ctest-Weg
# keins (System32\bash.exe wäre der WSL-Starter), und der Wächter läuft dort
# ohnehin nie (kein Hub, das DMG entsteht nur auf dem Mac).
if(NOT WIN32)
    find_program(QTMUX_BASH bash)
    if(QTMUX_BASH)
        add_test(NAME test_check_updater_sync
            COMMAND ${QTMUX_BASH} "${CMAKE_CURRENT_SOURCE_DIR}/test_check_updater_sync.sh"
                    "${CMAKE_SOURCE_DIR}/tools/check-updater-sync.sh")
    endif()
endif()

# --- Doku-Wächter (QTMUX-34) ------------------------------------------------
# Findet doppelte Überschriften, wie sie entstehen, wenn beim Kompaktieren einer
# langen Markdown-Datei ein Block eingefügt statt ersetzt wird. Reines CMake-Skript,
# damit der Test auf allen drei Plattformen ohne sh/grep läuft.
add_test(NAME test_doc_duplicates
    COMMAND ${CMAKE_COMMAND}
        "-DDOC_FILES=${CMAKE_SOURCE_DIR}/CLAUDE.md;${CMAKE_SOURCE_DIR}/README.md;${CMAKE_SOURCE_DIR}/docs/MCP.md;${CMAKE_SOURCE_DIR}/docs/feature-referenz.md;${CMAKE_SOURCE_DIR}/docs/e2e-fallen.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckDocDuplicates.cmake")
