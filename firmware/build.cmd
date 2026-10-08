@echo off
rem Build firmware bang toolchain di kem STM32CubeIDE (GCC + CMake + Ninja).
rem   build.cmd                  -> Release
rem   build.cmd Debug
rem   build.cmd Release -DMODBUS_SLAVE_ID=3 -DMODBUS_BAUDRATE=19200 -DMODBUS_PARITY=EVEN
setlocal enabledelayedexpansion

if "%CUBEIDE%"=="" set "CUBEIDE=C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE"
set "PLUGINS=%CUBEIDE%\plugins"
if not exist "%PLUGINS%" (
  echo Khong tim thay STM32CubeIDE tai "%CUBEIDE%". Dat bien CUBEIDE=duong_dan\STM32CubeIDE
  exit /b 1
)

for /d %%D in ("%PLUGINS%\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*.win32_*") do set "GCC_BIN=%%D\tools\bin"
for /d %%D in ("%PLUGINS%\com.st.stm32cube.ide.mcu.externaltools.cmake.win32_*") do set "CMAKE_BIN=%%D\tools\bin"
for /d %%D in ("%PLUGINS%\com.st.stm32cube.ide.mcu.externaltools.ninja.win32_*") do set "NINJA_BIN=%%D\tools\bin"
set "PATH=%GCC_BIN%;%CMAKE_BIN%;%NINJA_BIN%;%PATH%"

set "CFG=%~1"
if "%CFG%"=="" set "CFG=Release"
rem %%* giu nguyen dau "=" trong -DNAME=value (cmd tach %%1 tai dau "=")
set "EXTRA="
for /f "tokens=1,*" %%A in ("%*") do set "EXTRA=%%B"

:run
cd /d "%~dp0"
cmake --preset %CFG% %EXTRA% || exit /b 1
cmake --build --preset %CFG% || exit /b 1
echo.
echo Output: %~dp0build\%CFG%\keyboard.elf / .hex / .bin
