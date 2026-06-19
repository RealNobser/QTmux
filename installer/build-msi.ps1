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
    powershell -ExecutionPolicy Bypass -File installer\build-msi.ps1
#>
param(
    [string]$Version = "1.1.0",
    [string]$QtDir   = "C:\Qt\6.11.1\msvc2022_64",
    [string]$VcVars  = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    [string]$VcRedistRoot = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC"
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
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

# Portable Variante (ZIP) aus demselben Staging — installationsfrei, wie in der
# LIESMICH.txt beschrieben (entpacken + qtmux.exe starten).
$zip = Join-Path $repo "dist\QTmux-$Version-win64-portable.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip

Write-Host "Fertig:" -ForegroundColor Green
Write-Host "  MSI:      $msi"
Write-Host "  Portable: $zip"
