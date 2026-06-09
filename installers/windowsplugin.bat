@echo off
setlocal EnableExtensions

echo OBS YouTube Chat Dock installer
echo.
echo Close OBS before installing or updating the plugin.
echo.

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
for %%i in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fi"

if not defined OBS_PLUGIN_PREFIX (
  set "OBS_PLUGIN_PREFIX=%ProgramData%\obs-studio\plugins"
)

set "DLL_PATH="

if exist "%SCRIPT_DIR%chat-dock.dll" (
  set "DLL_PATH=%SCRIPT_DIR%chat-dock.dll"
)

if not defined DLL_PATH (
  if exist "%ROOT_DIR%\dist\windows\chat-dock.dll" (
    set "DLL_PATH=%ROOT_DIR%\dist\windows\chat-dock.dll"
  )
)

if not defined DLL_PATH (
  echo Error: chat-dock.dll was not found.
  echo Expected in:
  echo %SCRIPT_DIR%chat-dock.dll
  echo %ROOT_DIR%\dist\windows\chat-dock.dll
  goto error
)

set "TARGET_DIR=%OBS_PLUGIN_PREFIX%\chat-dock\bin\64bit"

if not exist "%TARGET_DIR%\" (
  mkdir "%TARGET_DIR%" >nul 2>nul
  if errorlevel 1 (
    echo Error: could not create "%TARGET_DIR%".
    echo Try running this installer as administrator.
    goto error
  )
)

copy /Y "%DLL_PATH%" "%TARGET_DIR%\chat-dock.dll" >nul
if errorlevel 1 (
  echo Error: could not copy chat-dock.dll.
  echo Make sure OBS is closed and try running this installer as administrator.
  goto error
)

echo Installed:
echo %TARGET_DIR%\chat-dock.dll
goto done

:error
echo Failed.
goto done

:done
pause
