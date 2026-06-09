@echo off
setlocal EnableExtensions

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
  echo Erro: chat-dock.dll nao encontrado.
  echo Esperado em:
  echo %SCRIPT_DIR%chat-dock.dll
  echo %ROOT_DIR%\dist\windows\chat-dock.dll
  goto error
)

set "TARGET_DIR=%OBS_PLUGIN_PREFIX%\chat-dock\bin\64bit"

if not exist "%TARGET_DIR%\" (
  mkdir "%TARGET_DIR%" >nul 2>nul
  if errorlevel 1 (
    echo Erro: nao foi possivel criar "%TARGET_DIR%".
    goto error
  )
)

copy /Y "%DLL_PATH%" "%TARGET_DIR%\chat-dock.dll" >nul
if errorlevel 1 (
  echo Erro: nao foi possivel copiar chat-dock.dll.
  goto error
)

echo Instalado:
echo %TARGET_DIR%\chat-dock.dll
goto done

:error
echo Falhou.
goto done

:done
pause
