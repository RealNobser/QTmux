@echo off
REM QTmux Agenten-Ereignis aus cmd.exe (Inter-Agenten-Benachrichtigung):
REM     qtmux-event done|question|error|info "Text"
REM
REM Schreibt OSC 777;qtmux-event;<kind>;<text> ins Terminal. Da das Ausgeben eines rohen
REM ESC-Zeichens in cmd.exe unhandlich ist, wird die Sequenz ueber PowerShell erzeugt.
REM Fuer KI-Agenten-HOOKS NICHT geeignet (Hook-stdout wird gekapselt) -> dort MCP
REM 'post_event' nutzen (siehe shell-integration\qtmux-emit.cmd / docs\MCP.md).
REM
REM Ablage z. B. in einem Verzeichnis im PATH, damit 'qtmux-event' global aufrufbar ist.
setlocal
set "QK=%~1"
set "QT=%~2"
powershell -NoProfile -Command "$e=[char]27;$b=[char]7;[Console]::Write($e+']777;qtmux-event;%QK%;%QT%'+$b)"
endlocal
