@echo off
REM QTmux: Agenten-Ereignis aus einem HOOK (cmd/Windows) via MCP 'post_event'.
REM Zuverlaessig aus KI-Agenten-Hooks (deren stdout gekapselt wird) — anders als die
REM OSC-Variante qtmux-event. Reicht alle Argumente an qtmux-emit.ps1 durch.
REM     qtmux-emit.cmd done "Aufgabe erledigt"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0qtmux-emit.ps1" %*
