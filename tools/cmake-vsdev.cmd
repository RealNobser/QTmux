@echo off
REM ===========================================================================
REM  QTmux: cmake-Wrapper mit Visual-Studio-2022-Umgebung (QTMUX-79, Teil 3).
REM
REM  Zweck: Die CMake-Tools-Erweiterung ruft cmake SELBST auf (Build-Knopf,
REM  Befehlspalette, automatische Konfigurationslaeufe). Auf dieser Maschine
REM  geht dabei beides schief:
REM    * useVsDeveloperEnvironment "auto": injiziert die NEUESTE Installation
REM      (VS 18) -> VS-2022-cl.exe + VS-18-STL -> error STL1001
REM    * useVsDeveloperEnvironment "never": gar keine VS-Umgebung -> INCLUDE und
REM      LIB fehlen -> fatal error C1083 "type_traits", LNK1104 "iphlpapi.lib"
REM  "cmake.buildTask" hilft NICHT: findBuildTask() holt ausschliesslich Tasks
REM  vom Typ "cmake" (fetchTasks({type:"cmake"})), eine shell-Task findet es
REM  nicht und faellt auf den internen Build zurueck.
REM
REM  Loesung: die Erweiterung auf diesen Wrapper zeigen lassen
REM      "cmake.cmakePath": "d:\\projects\\_ClaudeWorkspace\\QTmux\\tools\\cmake-vsdev.cmd"
REM  (gehoert in die BENUTZER-Einstellungen, nicht ins Repo: .vscode/settings.json
REM  ist plattformuebergreifend und dieser Pfad existiert nur unter Windows.)
REM  Dann laeuft JEDER cmake-Aufruf der Erweiterung in der VS-2022-Umgebung.
REM
REM  ACHTUNG: CRLF-Zeilenenden Pflicht, rein ASCII, und %ProgramFiles(x86)% nie
REM  in einer for-/if-Klammer expandieren (die Klammer aus "(x86)" schliesst sie
REM  vorzeitig) - deshalb verzoegerte Expansion und temporaere Datei.
REM ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo [cmake-vsdev] vswhere nicht gefunden: "!VSWHERE!" 1>&2
    exit /b 1
)

set "VSPATH="
set "VSLIST=%TEMP%\qtmux-cmakevs-%RANDOM%.txt"
"!VSWHERE!" -products * -version "[17.0,18.0)" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "!VSLIST!" 2>nul
if exist "!VSLIST!" set /p VSPATH=<"!VSLIST!"
del "!VSLIST!" >nul 2>&1
if not defined VSPATH (
    echo [cmake-vsdev] Keine Visual-Studio-2022-Installation mit C++-Tools gefunden. 1>&2
    exit /b 1
)

REM vcvars64 still: es ruft intern vswhere ohne Pfad auf und meldet das je nach
REM PATH als "nicht gefunden", obwohl es durchlaeuft. Erfolg per errorlevel.
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [cmake-vsdev] vcvars64.bat fehlgeschlagen. 1>&2
    exit /b 1
)

REM Echte cmake.exe bestimmen. Bevorzugt die von VS 2022 mitgelieferte (dieselbe,
REM die die Erweiterung bisher benutzt hat), sonst die erste aus dem PATH.
set "REALCMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%REALCMAKE%" (
    set "REALCMAKE="
    for /f "delims=" %%I in ('where cmake.exe 2^>nul') do (
        if not defined REALCMAKE set "REALCMAKE=%%I"
    )
)
if not defined REALCMAKE (
    echo [cmake-vsdev] Keine cmake.exe gefunden. 1>&2
    exit /b 1
)

REM Argumente unveraendert durchgeben (%* behaelt Quoting) und Exit-Code melden -
REM die Erweiterung wertet ihn aus.
"%REALCMAKE%" %*
exit /b %ERRORLEVEL%
