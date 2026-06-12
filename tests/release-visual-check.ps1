<#
  QTmux — Visueller Release-Check (Windows)

  Startet die Release-EXE und erzeugt automatisiert Screenshots ALLER Menüs in
  BEIDEM Theme (Dunkel + Hell) plus des Hauptfensters, damit vor jedem Release
  geprüft werden kann:
    - Menü-Icons haben die richtige Farbe (folgen dem App-Theme, nicht dem System),
    - kein abgeschnittener Menü-Text und keine abgeschnittenen/überlappenden Kürzel,
    - Menü-Popups folgen dem App-Theme (heller Hintergrund im Hell-Modus),
    - alle Menüeinträge sind vorhanden/lesbar.
  Zusätzlich ein MCP-Smoke-Test (Server erreichbar, tools/list vollständig).

  Hintergrund: Diese Prüfungen lassen sich NICHT über die Unit-Tests abdecken
  (reines Rendering/Theming). Sie wurden eingeführt, nachdem mehrfach Menü-Regressionen
  (dunkle Icons im Dark-Mode, abgeschnittene Texte/Kürzel) erst visuell auffielen.

  Bedienung (Repo-Wurzel):
    powershell -ExecutionPolicy Bypass -File tests\release-visual-check.ps1
  Ergebnis: PNGs unter dist\release-check\ — manuell sichten.

  Technik: Menüs werden per UI-Automation (InvokePattern) geöffnet; das umgeht den
  Windows-Foreground-Lock, der synthetische Klicks/Tasten an Hintergrundfenster
  verhindert. Ein Alt-Tastendruck löst den Lock, danach SetForegroundWindow + Invoke.
#>
param(
    [string]$Exe = "build\windows-release\qtmux.exe",
    [int]$McpPort = 7345
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$exePath = Join-Path $repo $Exe
if (-not (Test-Path $exePath)) { throw "Release-EXE nicht gefunden: $exePath (zuerst 'cmake --build --preset windows-release')" }
$outDir = Join-Path $repo "dist\release-check"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Windows.Forms,System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class QtmuxWin {
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);
 [DllImport("user32.dll")] public static extern void keybd_event(byte k,byte s,uint f,IntPtr e);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; } }
"@

function Mcp($name,$argsJson) {
  $body = '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"' + $name + '","arguments":' + $argsJson + '}}'
  (Invoke-RestMethod -Uri "http://127.0.0.1:$McpPort/mcp" -Method Post -Body $body -ContentType "application/json" -Proxy $null).result.content[0].text
}
function SetTheme($m) { Mcp "set_theme" "{`"mode`":`"$m`"}" | Out-Null }

# --- App starten -----------------------------------------------------------
Get-Process qtmux -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
Start-Process $exePath
Start-Sleep -Seconds 3

# --- MCP-Smoke-Test --------------------------------------------------------
$conn = Test-NetConnection -ComputerName 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue
if (-not $conn.TcpTestSucceeded) { Write-Warning "MCP-Server nicht erreichbar (Agent-Steuerung evtl. aus). Menü-Checks laufen trotzdem." }
else {
  $tl = (Invoke-RestMethod -Uri "http://127.0.0.1:$McpPort/mcp" -Method Post -Proxy $null -ContentType "application/json" `
         -Body '{"jsonrpc":"2.0","id":1,"method":"tools/list"}').result.tools.name
  Write-Host ("MCP tools/list ({0}): {1}" -f $tl.Count, ($tl -join ", ")) -ForegroundColor Cyan
}

# --- Fenster/Automation finden --------------------------------------------
$proc = Get-Process qtmux | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$hwnd = $proc.MainWindowHandle
$root = [System.Windows.Automation.AutomationElement]::RootElement
$win  = $root.FindFirst([System.Windows.Automation.TreeScope]::Children,
        (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)))

function CaptureWindow($file) {
  [QtmuxWin]::keybd_event(0x12,0,0,[IntPtr]::Zero); [QtmuxWin]::keybd_event(0x12,0,2,[IntPtr]::Zero)
  [QtmuxWin]::ShowWindow($hwnd,9) | Out-Null; [QtmuxWin]::SetForegroundWindow($hwnd) | Out-Null
  Start-Sleep -Milliseconds 300
  $r = New-Object QtmuxWin+RECT; [QtmuxWin]::GetWindowRect($hwnd,[ref]$r) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap(($r.R-$r.L),($r.B-$r.T)); $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.L,$r.T,0,0,$bmp.Size); $bmp.Save($file,[System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
}
function CaptureMenu($menuName,$file) {
  [QtmuxWin]::keybd_event(0x12,0,0,[IntPtr]::Zero); [QtmuxWin]::keybd_event(0x12,0,2,[IntPtr]::Zero)
  [QtmuxWin]::ShowWindow($hwnd,9) | Out-Null; [QtmuxWin]::SetForegroundWindow($hwnd) | Out-Null
  Start-Sleep -Milliseconds 300
  $m = $win.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
       (New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty, $menuName)))
  if (-not $m) { Write-Warning "Menü '$menuName' nicht gefunden (MenuBar leer?)"; return }
  $m.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
  Start-Sleep -Milliseconds 650
  CaptureWindow $file
}

$menus = @("Datei","Bearbeiten","Ansicht","Sprache","Agent","Agent-Steuerung","Hilfe")
foreach ($theme in @("dark","light")) {
  SetTheme $theme; Start-Sleep -Milliseconds 400
  CaptureWindow (Join-Path $outDir "main-$theme.png")
  foreach ($mn in $menus) {
    $safe = ($mn -replace '[^A-Za-z]','').ToLower()
    CaptureMenu $mn (Join-Path $outDir "menu-$safe-$theme.png")
  }
  Write-Host "Theme '$theme' erfasst." -ForegroundColor Green
}

Write-Host "`nFertig. Screenshots zum manuellen Sichten unter:`n  $outDir" -ForegroundColor Green
Write-Host "Prüfen: Icon-Farbe folgt Theme, kein abgeschnittener Text/Kürzel, helle Menüfläche im Hell-Modus."
