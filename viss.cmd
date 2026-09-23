@echo off
set "VISS_DIR=%~dp0"
set "PATH=C:\AGY\TOOLS\w64devkit\bin;%PATH%"
"%VISS_DIR%viss.exe" %*
