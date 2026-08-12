<#
.SYNOPSIS
    Asks msiexec how it parses an installer path -- both directions.

.DESCRIPTION
    Regression gate for the 0.2.0 Windows update failure. Qt hands out
    forward slashes on Windows (C:/Users/.../x.msi), our updater passed
    that path to msiexec unchanged, and msiexec -- which parses its own
    command line -- answered 1619, "This installation package could not be
    opened". The package was intact; only the separators were wrong.

    A unit test can only assert the string our code builds. It can NEVER
    show that msiexec rejects the other form, because that lives in
    msiexec's parser, not in ours. So this smoke asks the real program:

      (a) forward-slash path  -> must FAIL and extract nothing
      (b) native path         -> must succeed AND extract files

    Direction (b) is the positive anchor for (a): without it, a broken
    fixture would make (a) "pass" for the wrong reason -- a red herring
    dressed as a green gate.

    Together with the unit test
    (InstallerLauncher.MsiPathIsHandedOverWithNativeSeparators, which
    asserts the literal backslash string on Windows) the chain is closed:
    the test proves we emit native separators, this smoke proves native
    separators are what msiexec needs.

    `msiexec /a` is an ADMINISTRATIVE install: it extracts the package
    into TARGETDIR. It needs no elevation, changes nothing on the system
    and installs nothing.

    WHERE THIS RUNS: on the RTZBLD01 build path -- build-msi.ps1 (against
    the freshly built MSI) and tools\build-windows.cmd (against the
    fixture). NOT in the GitHub Actions job: under the runner's service
    context msiexec cannot reach the Windows Installer service and
    answers 1601 in BOTH directions, so the gate measures nothing there
    (measured 2026-08-11, run 31485093468, while an interactive session
    on the very same machine answered 1619/0). Anchor (b) is what caught
    that -- on direction (a) alone the step had passed green.

.PARAMETER Msi
    Package to probe. Defaults to the checked-in fixture next to this
    script, which is why the gate needs neither WiX, nor Qt, nor the PEAK
    SDK, nor a product build. Point it at a real release MSI to probe
    that one instead.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File platform\windows\msiexec-path-smoke.ps1
#>
[CmdletBinding()]
param(
    [string]$Msi
)

$ErrorActionPreference = 'Stop'

if (-not $Msi) {
    # NOT $PSScriptRoot: Windows PowerShell 5.1 leaves it empty while
    # binding parameter defaults, which made the default resolve to an
    # empty path and the script die before it measured anything.
    $here = Split-Path -Parent $MyInvocation.MyCommand.Path
    $Msi = Join-Path $here 'smoke\MacPCAN-PathSmoke.msi'
}

if (-not (Test-Path -LiteralPath $Msi)) {
    Write-Error "package not found: $Msi"
    exit 2
}

# Resolve first, THEN derive the broken form -- so both runs address the
# very same file and the only difference between them is the separator.
$native  = (Resolve-Path -LiteralPath $Msi).Path
$forward = $native -replace '\\', '/'

function Invoke-MsiExtract {
    param([string]$PackagePath, [string]$TargetDir)

    if (Test-Path -LiteralPath $TargetDir) {
        Remove-Item -LiteralPath $TargetDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null

    # Quote explicitly: TARGETDIR may sit under a path with spaces, and
    # msiexec's own parser is exactly what is on trial here.
    $proc = Start-Process -FilePath 'msiexec.exe' -Wait -PassThru `
        -ArgumentList @('/a', "`"$PackagePath`"", '/qn', "TARGETDIR=`"$TargetDir`"")
    $files = @(Get-ChildItem -LiteralPath $TargetDir -Recurse -File -ErrorAction SilentlyContinue)

    [pscustomobject]@{ ExitCode = $proc.ExitCode; FileCount = $files.Count }
}

$root      = Join-Path $env:TEMP 'macpcan-msi-path-smoke'
$dirFwd    = Join-Path $root 'forward'
$dirNative = Join-Path $root 'native'
$failed    = $false

try {
    Write-Host "package: $native"
    Write-Host ''

    $fwd = Invoke-MsiExtract -PackagePath $forward -TargetDir $dirFwd
    Write-Host ("(a) forward slashes : exit {0}, {1} file(s) extracted  [{2}]" -f `
        $fwd.ExitCode, $fwd.FileCount, $forward)

    $nat = Invoke-MsiExtract -PackagePath $native -TargetDir $dirNative
    Write-Host ("(b) native separators: exit {0}, {1} file(s) extracted  [{2}]" -f `
        $nat.ExitCode, $nat.FileCount, $native)
    Write-Host ''

    if ($nat.ExitCode -ne 0 -or $nat.FileCount -lt 1) {
        Write-Host "FAIL (b): msiexec must accept a native path and extract the package."
        Write-Host "          Without this anchor, (a) proves nothing -- a fixture that"
        Write-Host "          cannot be opened at all would satisfy (a) for free."
        $failed = $true
    }

    if ($fwd.ExitCode -eq 0 -or $fwd.FileCount -gt 0) {
        Write-Host "FAIL (a): msiexec ACCEPTED a forward-slash path (exit $($fwd.ExitCode))."
        Write-Host "          That contradicts the platform behaviour this gate is built on"
        Write-Host "          (1619 on every Windows measured so far). Do not just relax the"
        Write-Host "          gate -- find out what changed before trusting either form."
        $failed = $true
    }

    if (-not $failed) {
        Write-Host "OK: msiexec rejects '/' (exit $($fwd.ExitCode)) and accepts '\' (exit 0)."
        Write-Host "    QDir::toNativeSeparators() in src/update/InstallerLauncher.cpp is"
        Write-Host "    load-bearing, not cosmetic."
    }
}
finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failed) { exit 1 }
exit 0
