<#
  Baut den unsignierten Windows-Installer (MSI) für QTmux — reproduzierbar.

  Pipeline:
    1. Release-Build über das windows-release-Preset (build\windows-release).
    2. Selbst-enthaltene Laufzeit per windeployqt in ein Staging-Verzeichnis
       (Qt-DLLs/QML/Plugins) + lose VC-Runtime-DLLs + LIESMICH.txt.
    3. MSI via WiX (wix build, installer\QTmux.wxs).

  Voraussetzungen:
    - Visual Studio 2022 (MSVC + CMake + Ninja).
    - Qt 6.x msvc2022_64 inkl. SerialPort-Add-on.
    - WiX CLI als dotnet-Tool, FREIE Version (v4 oder v5 — v6/v7 verlangen die
      OSMF-Fee):  dotnet tool install --global wix --version 5.0.2
                  wix extension add -g WixToolset.UI.wixext/5.0.2   (optional)

  Aufruf (aus der Repo-Wurzel, in einer Dev-Shell / vcvars):
    powershell -ExecutionPolicy Bypass -File installer\build-msi.ps1 -Version 1.9.2
#>
param(
    # PFLICHT, bewusst ohne Vorgabewert (2026-08-06). Hier stand "1.2.0" — sechs
    # Minor-Versionen hinter dem Projekt. Parameterlos gebaut waere daraus ein MSI
    # mit falscher ProductVersion geworden, ohne dass Build oder Test rot werden.
    # Ein Pflichtparameter kann nicht veralten; eine Vorgabe ist genau so lange
    # richtig, wie jemand sie pflegt — und niemand merkt, wenn nicht.
    # (Dieselbe Fehlerklasse wie die zwei Wrapper, die still die alte Version
    # bauten — s. CLAUDE.md, "Fehler, die wie ein normaler Lauf aussehen".)
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version,
    # Leer lassen = automatisch bestimmen (s. u.). Ein explizit uebergebener Wert
    # gewinnt immer — so bleibt das Skript auf Sondermaschinen ueberschreibbar.
    [string]$QtDir        = "",
    [string]$VcVars       = "",
    [string]$VcRedistRoot = ""
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent

# --- Werkzeug-Pfade automatisch bestimmen -------------------------------------
# Damit laeuft das Skript OHNE Parameter auf jeder korrekt eingerichteten Maschine;
# frueher waren carhack-Pfade fest verdrahtet und jeder Build-Server-Wechsel brach es
# (carhack -> RTZBLD01, 2026-07-30). Explizit uebergebene Werte haben Vorrang.
if (-not $QtDir) {
    if ($env:QTMUX_QT_PREFIX) {
        # Dieselbe Env-Var wie das windows-Preset (CMAKE_PREFIX_PATH); erster Eintrag,
        # Forward Slashes -> Backslashes fuer windeployqt.
        $QtDir = (($env:QTMUX_QT_PREFIX -split ';')[0]).Trim() -replace '/','\'
    } else {
        # Neueste C:\Qt\6.*\msvc2022_64. Versionssortierung als [version], NICHT als
        # String — sonst gilt "6.8.3" > "6.10.3", weil "8" > "1".
        $qv = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
              Where-Object { $_.Name -match '^\d+\.\d+' -and
                             (Test-Path (Join-Path $_.FullName 'msvc2022_64\bin\windeployqt.exe')) } |
              Sort-Object { [version]$_.Name } | Select-Object -Last 1
        if ($qv) { $QtDir = Join-Path $qv.FullName 'msvc2022_64' }
    }
}
if (-not $QtDir -or -not (Test-Path (Join-Path $QtDir 'bin\windeployqt.exe'))) {
    throw "Qt nicht gefunden. QTMUX_QT_PREFIX setzen oder -QtDir uebergeben (ermittelt: '$QtDir')."
}
# VS ueber vswhere lokalisieren. Die Begrenzung [17.0,18.0) ist PFLICHT: liegt ein
# neueres VS (z. B. "18"/2026) daneben, ergaebe dessen STL mit dem 2022-cl den Fehler
# STL1001 (QTMUX-79). `-requires` stellt sicher, dass das C++-Toolset auch da ist.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsBase  = $null
if (Test-Path $vswhere) {
    $vsBase = & $vswhere -products * -version '[17.0,18.0)' -latest `
                 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                 -property installationPath 2>$null | Select-Object -First 1
}
if (-not $VcVars -and $vsBase)       { $VcVars       = Join-Path $vsBase 'VC\Auxiliary\Build\vcvars64.bat' }
if (-not $VcRedistRoot -and $vsBase) { $VcRedistRoot = Join-Path $vsBase 'VC\Redist\MSVC' }
if (-not $VcVars -or -not (Test-Path $VcVars)) {
    throw "vcvars64.bat nicht gefunden (VS 2022 mit C++-Toolset?). -VcVars uebergeben (ermittelt: '$VcVars')."
}
Write-Host ("    Qt:      {0}" -f $QtDir)        -ForegroundColor DarkGray
Write-Host ("    VcVars:  {0}" -f $VcVars)       -ForegroundColor DarkGray
Write-Host ("    Redist:  {0}" -f $VcRedistRoot) -ForegroundColor DarkGray
# Gemeinsames Release-Verzeichnis mit dem windows-release-Preset (kein separates
# build\release-win mehr — das war doppelt zum Preset). So gibt es nur zwei
# Windows-Build-Verzeichnisse: build\windows (Debug) und build\windows-release (Release).
$build = Join-Path $repo "build\windows-release"
$stage = Join-Path $repo "dist\QTmux"
$wix   = Join-Path $env:USERPROFILE ".dotnet\tools\wix.exe"

Write-Host "==> 1/3 Release-Build (Preset windows-release)" -ForegroundColor Cyan
& cmd /c "`"$VcVars`" >nul 2>&1 && cmake --preset windows-release >nul 2>&1 && cmake --build --preset windows-release"
if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen" }

Write-Host "==> 2/3 Laufzeit deployen (windeployqt + VC-Runtime)" -ForegroundColor Cyan
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item (Join-Path $build "qtmux.exe") $stage
& cmd /c "`"$VcVars`" >nul 2>&1 && `"$QtDir\bin\windeployqt.exe`" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --qmldir `"$repo\qml`" `"$stage\qtmux.exe`""
# Offscreen-Plattform-Plugin mitliefern. windeployqt legt nur qwindows.dll ins Paket;
# setzt jemand QT_QPA_PLATFORM=offscreen (Skripte, CI, Fernwartung), beendet Qt den Prozess
# mit qFatal - in einer GUI-App also ein Absturz ohne sichtbare Meldung (Release 0xC0000409).
# Die CMake-Regel deckt nur das Build-Verzeichnis ab; das Paket staged separat, darum hier
# nochmal. (Der --screenshot-Weg von QTmux setzt die Variable unter Windows bewusst NICHT,
# s. src/app/main.cpp - das hier ist die zweite Sicherung fuer alle anderen Aufrufer.)
$offscreen = Join-Path $QtDir "plugins\platforms\qoffscreen.dll"
if (Test-Path $offscreen) {
    $platformsDst = Join-Path $stage "platforms"
    New-Item -ItemType Directory -Force -Path $platformsDst | Out-Null
    Copy-Item $offscreen $platformsDst -Force
    Write-Host "    qoffscreen.dll eingebunden (kein qFatal bei QT_QPA_PLATFORM=offscreen)"
} else {
    Write-Warning "qoffscreen.dll nicht in $QtDir gefunden - Paket ohne Offscreen-Plattform"
}

# Eigene QTmux-Plugins (Phase 5, QTMUX-8) mitliefern, falls gebaut. windeployqt
# kennt nur Qt-eigene Plugins; unsere Backend-Provider-Plugins liegen in
# <build>\plugins und müssen separat nach <stage>\plugins (Suchpfad 2 des
# PluginHost: <App>\plugins). Nicht fatal, wenn (noch) keine vorhanden sind.
$pluginSrc = Join-Path $build "plugins"
if (Test-Path $pluginSrc) {
    $pluginDlls = Get-ChildItem (Join-Path $pluginSrc "*.dll") -ErrorAction SilentlyContinue
    if ($pluginDlls) {
        $pluginDst = Join-Path $stage "plugins"
        New-Item -ItemType Directory -Force -Path $pluginDst | Out-Null
        $pluginDlls | Copy-Item -Destination $pluginDst -Force
        Write-Host ("    {0} QTmux-Plugin(s) eingebunden" -f $pluginDlls.Count)
    }
}

# Lose VC-Runtime-DLLs (damit ohne separat installiertes Redist lauffähig).
$crt = Get-ChildItem $VcRedistRoot -Directory | ForEach-Object { Get-ChildItem (Join-Path $_.FullName "x64") -Filter "Microsoft.VC*.CRT" -Directory -ErrorAction SilentlyContinue } | Select-Object -Last 1
Copy-Item (Join-Path $crt.FullName "*.dll") $stage -Force
Remove-Item (Join-Path $stage "vc_redist.x64.exe") -ErrorAction SilentlyContinue
Copy-Item (Join-Path $PSScriptRoot "LIESMICH.txt") $stage -Force

Write-Host "==> 3/3 MSI + portable ZIP bauen" -ForegroundColor Cyan
$msi = Join-Path $repo "dist\QTmux-$Version-win64.msi"
& $wix build (Join-Path $PSScriptRoot "QTmux.wxs") -arch x64 -d "Version=$Version" -d "PayloadDir=$stage" -o $msi
if ($LASTEXITCODE -ne 0) { throw "WiX-Build fehlgeschlagen" }

# msiexec-Pfad-Smoke (1619-Riegel) gegen das FRISCH GEBAUTE MSI. Fragt msiexec
# selbst, ob es das Paket mit nativen Trennern oeffnet (Exit 0 UND Dateien
# entpackt) und mit Vorwaerts-Slashes ablehnt (1619) — genau der Parser-Fehler,
# der das Windows-Selbst-Update seit der ersten Auslieferung totgelegt hatte
# (Fix: QDir::toNativeSeparators() im vendierten Updater, MacPCAN 796575f).
# Ein Release, dessen eigener Installer sich nicht oeffnen laesst, ist keines —
# Fehlschlag bricht den Bau ab. Bewusst NICHT im GitHub-Actions-Job: im
# Dienstkontext des Runners antwortet msiexec in BEIDEN Richtungen 1601, das
# Gate waere dort blind gruen (Herkunft/Details: installer\smoke\UPSTREAM.md).
Write-Host "==> msiexec-Pfad-Smoke (1619-Riegel)" -ForegroundColor Cyan
$smoke = Join-Path $PSScriptRoot "msiexec-path-smoke.ps1"
& powershell -NoProfile -ExecutionPolicy Bypass -File $smoke -Msi $msi
if ($LASTEXITCODE -ne 0) { throw "msiexec-Pfad-Smoke fehlgeschlagen ($LASTEXITCODE) fuer $msi" }

# Portable Variante (ZIP) aus demselben Staging — installationsfrei, wie in der
# LIESMICH.txt beschrieben (entpacken + qtmux.exe starten).
$zip = Join-Path $repo "dist\QTmux-$Version-win64-portable.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip

Write-Host "Fertig:" -ForegroundColor Green
Write-Host "  MSI:      $msi"
Write-Host "  Portable: $zip"
