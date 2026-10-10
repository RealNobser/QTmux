<#
.SYNOPSIS
    Fails when a shortcut in a built MSI takes its icon from the MSI Icon table.

.DESCRIPTION
    Regression gate for the "white taskbar icon after every update" failure
    (measured 2026-10-10 on RTZBLD01 with QTmux 1.9.6 -> 1.9.7).

    A <Shortcut Icon="x.ico"> fills the Shortcut table's Icon_ column. Windows
    Installer then writes the shortcut with
        IconLocation = C:\Windows\Installer\{ProductCode}\x.ico
    The ProductCode is new with every build, and MajorUpgrade removes the old
    product together with that folder. Start-menu and desktop shortcuts are
    rewritten by the update and stay correct. A shortcut PINNED to the taskbar
    is the shell's own copy under
        %APPDATA%\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar
    nobody rewrites it, so it keeps pointing at the deleted folder: blank
    white page, while the target .exe still starts.

    The rule (docs/SHARED.md, search "msi-shortcut-icon-check"): a shortcut carries
    NO Icon attribute. The shell then takes icon 0 of the target .exe (the
    embedded IDI_ICON1 from the product's .rc), and [INSTALLFOLDER]<App>.exe
    is the same path across updates. ARPPRODUCTICON may keep using the Icon
    table: the ARP entry is rewritten by every install.

    Pointing Icon_ at the installed .exe is not possible: the column is a
    foreign key into the Icon table, whose entries Windows Installer always
    copies to the per-ProductCode folder. Omitting it is the only form.

    The gate reads the Shortcut table of the PACKAGE through the Windows
    Installer COM API (the same reader msiexec uses), not the .wxs source.

    Exit codes -- each case has its own, a read failure is never a pass:
      0  every shortcut has an empty Icon_ column (and at least -MinShortcuts)
      1  at least one shortcut uses the Icon table (the regression)
      2  the MSI or its Shortcut table could not be read
      3  fewer shortcuts than -MinShortcuts: the gate did not see what it
         is meant to check (a lost shortcut is a finding, too)

.PARAMETER Msi
    Path to the built .msi.

.PARAMETER MinShortcuts
    Shortcuts the package must contain. Default 1: an MSI without any
    shortcut row would otherwise pass without having checked anything.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File platform\windows\msi-shortcut-icon-check.ps1 -Msi build\msi\MacPCAN-0.5.0-x64.msi -MinShortcuts 2
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $Msi,
    [int] $MinShortcuts = 1
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $Msi -PathType Leaf)) {
    Write-Host "msi-shortcut-icon-check: ERROR: not a file: $Msi"
    exit 2
}
$msiPath = (Resolve-Path -LiteralPath $Msi).ProviderPath

$rows = @()
$readError = $null
$installer = $null; $db = $null; $view = $null
try {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    # [string] matters: a PSObject-wrapped path is rejected by COM late
    # binding with DISP_E_TYPEMISMATCH (measured in RAFTNG build-msi.ps1).
    $db   = $installer.GetType().InvokeMember("OpenDatabase", "InvokeMethod", $null, $installer, @([string]$msiPath, 0))
    $view = $db.GetType().InvokeMember("OpenView", "InvokeMethod", $null, $db, @("SELECT Shortcut, Target, Icon_ FROM Shortcut"))
    $view.GetType().InvokeMember("Execute", "InvokeMethod", $null, $view, $null) | Out-Null
    while ($true) {
        $rec = $view.GetType().InvokeMember("Fetch", "InvokeMethod", $null, $view, $null)
        if ($null -eq $rec) { break }
        $rows += [pscustomobject]@{
            Shortcut = $rec.GetType().InvokeMember("StringData", "GetProperty", $null, $rec, 1)
            Target   = $rec.GetType().InvokeMember("StringData", "GetProperty", $null, $rec, 2)
            Icon     = $rec.GetType().InvokeMember("StringData", "GetProperty", $null, $rec, 3)
        }
        [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($rec)
    }
    $view.GetType().InvokeMember("Close", "InvokeMethod", $null, $view, $null) | Out-Null
} catch {
    $readError = $_.Exception.InnerException.Message
    if (-not $readError) { $readError = $_.Exception.Message }
} finally {
    # Release before returning: an open database handle locks the file
    # (build scripts delete a failed MSI right after this gate).
    foreach ($o in @($view, $db, $installer)) {
        if ($null -ne $o) { [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($o) }
    }
    [System.GC]::Collect(); [System.GC]::WaitForPendingFinalizers()
}

if ($null -ne $readError) {
    Write-Host "msi-shortcut-icon-check: ERROR: cannot read the Shortcut table of $msiPath ($readError)"
    exit 2
}

foreach ($r in $rows) {
    $state = if ([string]::IsNullOrEmpty($r.Icon)) { "ok    (icon from target)" } else { "FAIL  (Icon table: $($r.Icon))" }
    Write-Host ("  {0,-34} {1}  target={2}" -f $r.Shortcut, $state, $r.Target)
}

$bad = @($rows | Where-Object { -not [string]::IsNullOrEmpty($_.Icon) })
if ($bad.Count -gt 0) {
    Write-Host "msi-shortcut-icon-check: FAIL: $($bad.Count) of $($rows.Count) shortcut(s) take their icon from the MSI Icon table."
    Write-Host "  Such shortcuts point into C:\Windows\Installer\{ProductCode}\, which every update deletes;"
    Write-Host "  a taskbar-pinned copy then shows a blank page. Remove Icon=/IconIndex= from <Shortcut>."
    exit 1
}
if ($rows.Count -lt $MinShortcuts) {
    Write-Host "msi-shortcut-icon-check: ERROR: $($rows.Count) shortcut(s) found, expected at least $MinShortcuts -- the gate did not see the shortcuts it is meant to check."
    exit 3
}
Write-Host "msi-shortcut-icon-check: OK: $($rows.Count) shortcut(s), none uses the MSI Icon table."
exit 0
