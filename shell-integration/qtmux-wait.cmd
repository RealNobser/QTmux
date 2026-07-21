@echo off
REM QTmux: auf ein Agenten-Ereignis WARTEN (cmd/Windows) — blockiert und endet beim
REM ersten Treffer, damit ein Agent per Hintergrundprozess GEWECKT wird, statt selbst
REM abholen zu muessen. Gegenstueck zu qtmux-emit.cmd; reicht alles an qtmux-wait.ps1 durch.
REM     qtmux-wait.cmd -Sessions 2,3 -Kinds done,question
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0qtmux-wait.ps1" %*
