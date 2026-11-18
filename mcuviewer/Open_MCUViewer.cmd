@echo off
powershell.exe -NoProfile -File "%~dp0Start_MCUViewer.ps1" -Port COM14
if errorlevel 1 pause
