@echo off
rem PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
rem Double-click to install PocketOS on new devices. The first run sets up its tools (about a minute).
title PocketOS Factory
cd /d "%~dp0"
set "PY="
where py >nul 2>nul && set "PY=py -3"
if not defined PY where python >nul 2>nul && set "PY=python"
if not defined PY (
  echo Python 3 is needed once. The download page will open: install it, tick "Add python.exe to PATH",
  echo then double-click this file again.
  start https://www.python.org/downloads/windows/
  pause
  exit /b 1
)
set "ENV=%LOCALAPPDATA%\PocketOS-Factory"
if not exist "%ENV%\Scripts\python.exe" (
  echo Setting up for the first time...
  %PY% -m venv "%ENV%" || goto :fail
  "%ENV%\Scripts\python.exe" -m pip install --quiet --disable-pip-version-check esptool==4.8.1 || goto :fail
)
"%ENV%\Scripts\python.exe" pocketos_factory.py %*
pause
exit /b 0
:fail
echo Setup didn't finish. Check the internet connection and try again.
pause
exit /b 1
