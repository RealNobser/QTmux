#requires -Version 5.1
<#
  build-release.ps1 — One-Click-Windows-Release für QTmux. Holt den aktuellen
  Stand, baut Release und verpackt MSI + portable ZIP über installer\build-msi.ps1.
  Gedacht für einen Desktop-Shortcut (siehe -CreateShortcut).

  Schritte:
    1. git pull (fast-forward)                        (überspringen: -NoFetch)
    2. installer\build-msi.ps1  →  dist\QTmux-<ver>-win64.msi + -portable.zip
       (build-msi.ps1 baut selbst über das windows-release-Preset; die Version
        wird hier aus CMakeLists.txt geparst, damit nichts auseinanderläuft.)

  Maschinen ohne Qt unter C:\Qt\6.11.1 (dem Preset-Default) werden über die
  Umgebungsvariable QTMUX_QT_PREFIX bedient — das windows-Preset stellt sie
  dem CMAKE_PREFIX_PATH voran; dieses Script setzt sie auf das gefundene Kit.

  Overrides (Parameter oder Umgebung):
    -VsPath          VS-2022-Installation (Default: vswhere, gepinnt auf 17.x).
    -QtDir           Qt-msvc-Kit (Default: neuestes C:\Qt\*\msvc2022_64 oder QT_DIR).
    -NoFetch         git pull überspringen.
    -OpenOutput      Explorer auf dem fertigen MSI öffnen.
    -CreateShortcut  Desktop-Shortcut für dieses Script (neu) anlegen, dann Ende.
#>
[CmdletBinding()]
param(
    [string] $VsPath = "",
    [string] $QtDir  = "",
    [switch] $NoFetch,
    [switch] $OpenOutput,
    [switch] $CreateShortcut
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ScriptPath = $MyInvocation.MyCommand.Path
$RepoRoot   = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

# ------------------------------------------------------------------
# -CreateShortcut: Desktop-.lnk auf dieses Script anlegen, dann Ende.
# ------------------------------------------------------------------
if ($CreateShortcut) {
    $desktop = [Environment]::GetFolderPath('Desktop')
    $lnk     = Join-Path $desktop "Build QTmux Installer.lnk"
    $ws      = New-Object -ComObject WScript.Shell
    $sc      = $ws.CreateShortcut($lnk)
    $sc.TargetPath       = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
    $sc.Arguments        = "-NoProfile -NoExit -ExecutionPolicy Bypass -File `"$ScriptPath`" -OpenOutput"
    $sc.WorkingDirectory = $RepoRoot
    $sc.IconLocation     = (Join-Path $RepoRoot "resources\appicon\qtmux.ico") + ",0"
    $sc.Description      = "Aktuellen Stand holen, Release bauen und den QTmux-Windows-Installer (MSI) verpacken."
    $sc.Save()
    Write-Host "Desktop-Shortcut angelegt: $lnk"
    return
}

# ------------------------------------------------------------------
# Visual Studio 2022 (17.x) finden — 18 / "2026" bewusst ausgeschlossen.
# ------------------------------------------------------------------
if ([string]::IsNullOrEmpty($VsPath)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $VsPath = & $vswhere -version "[17.0,18.0)" -latest -products * `
                             -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                             -property installationPath
    }
    if ([string]::IsNullOrEmpty($VsPath) -or -not (Test-Path $VsPath)) {
        $VsPath = "C:\Program Files\Microsoft Visual Studio\2022\Professional"
    }
}
if (-not (Test-Path $VsPath)) { throw "Visual Studio 2022 nicht gefunden. -VsPath angeben." }
$VcVars       = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
$VcRedistRoot = Join-Path $VsPath "VC\Redist\MSVC"
if (-not (Test-Path $VcVars)) { throw "vcvars64.bat nicht gefunden unter $VcVars." }

# ------------------------------------------------------------------
# Qt-msvc-Kit finden.
# ------------------------------------------------------------------
if ([string]::IsNullOrEmpty($QtDir)) {
    if (-not [string]::IsNullOrEmpty($env:QT_DIR)) {
        $QtDir = $env:QT_DIR
    } else {
        $cand = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
                Where-Object { Test-Path (Join-Path $_.FullName "msvc2022_64") } |
                Sort-Object Name -Descending | Select-Object -First 1
        if ($null -ne $cand) { $QtDir = Join-Path $cand.FullName "msvc2022_64" }
    }
}
if ([string]::IsNullOrEmpty($QtDir) -or -not (Test-Path $QtDir)) {
    throw "Qt-msvc2022_64-Kit nicht gefunden. -QtDir C:\Qt\<ver>\msvc2022_64 angeben oder QT_DIR setzen."
}

# ------------------------------------------------------------------
# Version aus CMakeLists.txt parsen (project(QTmux VERSION x.y.z …)).
# ------------------------------------------------------------------
$cmakeLists = Get-Content (Join-Path $RepoRoot "CMakeLists.txt") -Raw
if ($cmakeLists -notmatch 'project\(\s*QTmux\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "Konnte die Version nicht aus CMakeLists.txt parsen."
}
$Version = $Matches[1]

Write-Host "Repo:     $RepoRoot"
Write-Host "VS:       $VsPath"
Write-Host "Qt:       $QtDir"
Write-Host "Version:  $Version"

# ------------------------------------------------------------------
# 1. Aktuellen Stand holen.
# ------------------------------------------------------------------
if (-not $NoFetch) {
    Write-Host ""
    Write-Host "=== Aktuellen Stand holen ==="
    Push-Location $RepoRoot
    try {
        & git pull --ff-only
        if ($LASTEXITCODE -ne 0) { throw "git pull fehlgeschlagen ($LASTEXITCODE)" }
    } finally { Pop-Location }
}

# ------------------------------------------------------------------
# 2. Build + Paketierung über build-msi.ps1 (baut selbst via Preset).
#    QTMUX_QT_PREFIX versorgt das windows-Preset mit dem lokalen Qt-Kit;
#    CMAKE_BUILD_PARALLEL_LEVEL=2 schont RAM-schwache Build-Rechner.
# ------------------------------------------------------------------
$env:QTMUX_QT_PREFIX = ($QtDir -replace '\\','/')
$env:CMAKE_BUILD_PARALLEL_LEVEL = "2"

Write-Host ""
Write-Host "=== Build + MSI/ZIP (build-msi.ps1) ==="
Push-Location $RepoRoot
try {
    & powershell -NoProfile -ExecutionPolicy Bypass `
        -File (Join-Path $PSScriptRoot "build-msi.ps1") `
        -Version $Version -QtDir $QtDir -VcVars $VcVars -VcRedistRoot $VcRedistRoot
    if ($LASTEXITCODE -ne 0) { throw "build-msi.ps1 fehlgeschlagen ($LASTEXITCODE)" }
} finally { Pop-Location }

$msi = Join-Path $RepoRoot "dist\QTmux-$Version-win64.msi"
Write-Host ""
if (Test-Path $msi) {
    Write-Host "OK: $msi"
    if ($OpenOutput) { Start-Process explorer.exe "/select,`"$msi`"" }
} else {
    throw "MSI nach dem Build nicht gefunden: $msi"
}
