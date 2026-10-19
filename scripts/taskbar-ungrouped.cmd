@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0TaskbarUngrouped.ps1" %*
exit /b %ERRORLEVEL%
