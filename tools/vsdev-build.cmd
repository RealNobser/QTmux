@echo off
REM ===========================================================================
REM  QTmux: Build in der Visual-Studio-2022-Entwicklerumgebung.
REM
REM  Warum dieses Skript (QTMUX-79): Auf Maschinen mit MEHREREN VS-Installationen
REM  (hier VS 2022 + VS 18) waehlt die CMake-Tools-Erweiterung fuer ihre injizierte
REM  Developer-Umgebung immer die NEUESTE, also VS 18. Der Build-Cache zeigt aber
REM  auf den Compiler aus VS 2022, und die Mischung aus VS-2022-cl.exe und
REM  VS-18-Standardbibliothek endet in:
REM      error STL1001: Unexpected compiler version, expected MSVC Compiler 19.50
REM  Eine Einstellung zur Wahl der VS-Installation gibt es in CMake Tools nicht,
REM  deshalb holt dieses Skript die Umgebung selbst: ueber vswhere, eingegrenzt
REM  auf die 17er-Reihe (= VS 2022). Qt ist als msvc2022_64 gebaut, die CI nutzt
REM  ebenfalls VS 2022, und installer\build-msi.ps1 auch. VS 2022 ist der Standard.
REM
REM  ACHTUNG: Diese Datei braucht CRLF-Zeilenenden (cmd.exe stolpert sonst ueber
REM  reine LF) und bleibt bewusst rein ASCII (OEM-Codepage der Konsole).
REM  Der vswhere-Pfad wird ueberall als !VSWHERE! (verzoegerte Expansion) benutzt:
REM  %ProgramFiles(x86)% enthaelt KLAMMERN, und die wuerden bei fruehem Einsetzen
REM  die umgebende for- bzw. if-Klammer vorzeitig schliessen ("C:\Program" ist
REM  entweder falsch geschrieben ...).
REM
REM  Aufruf:  tools\vsdev-build.cmd [preset] [target]
REM             preset  Standard: windows      (z. B. windows-release)
REM             target  Standard: qtmux        ("all" baut auch die Tests)
REM ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=windows"
set "TARGET=%~2"
if "%TARGET%"=="" set "TARGET=qtmux"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo [vsdev-build] vswhere nicht gefunden: "!VSWHERE!"
    exit /b 1
)

REM -version "[17.0,18.0)" grenzt auf die 2022er-Reihe ein; -latest nimmt davon die
REM neueste Ausbaustufe. Ohne diese Eingrenzung liefert vswhere VS 18 und der
REM STL1001-Fehler ist zurueck.
REM
REM Bewusst ueber eine temporaere Datei statt `for /f ... in (`"..." args`)`: In der
REM for-Klammer scheitert ein Programmpfad MIT LEERZEICHEN am Quoting ("C:\Program"
REM ist entweder falsch geschrieben ...), und die Klammern aus %ProgramFiles(x86)%
REM bzw. aus dem Versionsbereich "18.0)" wuerden die Klammer zusaetzlich vorzeitig
REM schliessen. Ein simpler Aufruf mit Umleitung kennt beide Probleme nicht.
set "VSPATH="
set "VSLIST=%TEMP%\qtmux-vsdev-%RANDOM%.txt"
"!VSWHERE!" -products * -version "[17.0,18.0)" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "!VSLIST!" 2>nul
if exist "!VSLIST!" set /p VSPATH=<"!VSLIST!"
del "!VSLIST!" >nul 2>&1

if not defined VSPATH (
    echo [vsdev-build] Keine Visual-Studio-2022-Installation mit C++-Tools gefunden.
    echo [vsdev-build] Vorhandene Installationen:
    "!VSWHERE!" -all -prerelease -property installationPath
    exit /b 1
)

echo [vsdev-build] Toolchain: "%VSPATH%"
REM stderr mit unterdruecken: vcvars64.bat ruft intern vswhere ohne Pfad auf und
REM meldet das je nach PATH als "vswhere.exe ... nicht gefunden", obwohl es sauber
REM durchlaeuft. Der Erfolg wird ueber errorlevel geprueft, nicht ueber die Ausgabe.
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [vsdev-build] vcvars64.bat fehlgeschlagen.
    exit /b 1
)

REM Preset/Target GEQUOTET durchgeben: Der Aufrufer ist auch die VSCode-Task, und die
REM setzt das AKTIVE Preset per ${command:cmake.activeBuildPresetName} ein. Kaeme dort
REM je nach Erweiterungsversion ein Anzeigename wie "Windows (MSVC)" an, wuerde ein
REM ungequotetes %PRESET% an Leerzeichen zerfallen und die Klammern die Zeile zerlegen.
REM Mit Quoting meldet stattdessen cmake sauber "No such build preset".
echo [vsdev-build] Preset %PRESET%, Target %TARGET%
cmake --preset "%PRESET%"
if errorlevel 1 exit /b 1
cmake --build --preset "%PRESET%" --target "%TARGET%"
if errorlevel 1 exit /b 1

echo [vsdev-build] Fertig.
exit /b 0
