@echo off
rem Sets up Blitzen Plus in C:\Blitzen. To use another folder:  "Setup Blitzen.cmd" -Dir D:\Blitzen
rem Other options are listed at the top of setup-blitzen.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-blitzen.ps1" %*
echo.
pause
