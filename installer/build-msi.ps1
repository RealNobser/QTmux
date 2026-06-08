<#
  Baut den unsignierten Windows-Installer (MSI) für QTmux — reproduzierbar.

  Pipeline:
    1. Release-Build (separates build-Verzeichnis).
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
    [string]$Version = "0.1.0",
    [string]$QtDir   = "C:\Qt\6.11.1\msvc2022_64",
    [string]$VcVars  = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    [string]$VcRedistRoot = "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC"
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$build = Join-Path $repo "build\release-win"
$stage = Join-Path $repo "dist\QTmux"
$wix   = Join-Path $env:USERPROFILE ".dotnet\tools\wix.exe"

Write-Host "==> 1/3 Release-Build" -ForegroundColor Cyan
& cmd /c "`"$VcVars`" >nul 2>&1 && cmake --preset windows -B `"$build`" -DCMAKE_BUILD_TYPE=Release -DQTMUX_BUILD_TESTS=OFF >nul 2>&1 && cmake --build `"$build`""
if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen" }

Write-Host "==> 2/3 Laufzeit deployen (windeployqt + VC-Runtime)" -ForegroundColor Cyan
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item (Join-Path $build "qtmux.exe") $stage
& cmd /c "`"$VcVars`" >nul 2>&1 && `"$QtDir\bin\windeployqt.exe`" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --qmldir `"$repo\qml`" `"$stage\qtmux.exe`""
# Lose VC-Runtime-DLLs (damit ohne separat installiertes Redist lauffähig).
$crt = Get-ChildItem $VcRedistRoot -Directory | ForEach-Object { Get-ChildItem (Join-Path $_.FullName "x64") -Filter "Microsoft.VC*.CRT" -Directory -ErrorAction SilentlyContinue } | Select-Object -Last 1
Copy-Item (Join-Path $crt.FullName "*.dll") $stage -Force
Remove-Item (Join-Path $stage "vc_redist.x64.exe") -ErrorAction SilentlyContinue
Copy-Item (Join-Path $PSScriptRoot "LIESMICH.txt") $stage -Force

Write-Host "==> 3/3 MSI bauen (WiX)" -ForegroundColor Cyan
$msi = Join-Path $repo "dist\QTmux-$Version-win64.msi"
& $wix build (Join-Path $PSScriptRoot "QTmux.wxs") -arch x64 -d "Version=$Version" -d "PayloadDir=$stage" -o $msi
if ($LASTEXITCODE -ne 0) { throw "WiX-Build fehlgeschlagen" }

Write-Host "Fertig: $msi" -ForegroundColor Green
