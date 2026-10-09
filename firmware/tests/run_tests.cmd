@echo off
rem Bien dich va chay unit test tren PC (can gcc, vi du MinGW). Dung: tests\run_tests.cmd [basic|modbus|display|engine|ui]
cd /d "%~dp0.."
if not exist build mkdir build
gcc -std=gnu11 -Wall -Wextra -g -O0 -IApp/Inc -Itests -o build\tests.exe tests\*.c App\Src\crc.c App\Src\rtcfg.c App\Src\datafmt.c App\Src\storage.c App\Src\mb_slave.c App\Src\mb_master.c App\Src\display.c App\Src\keys.c App\Src\buzzer.c App\Src\cfg.c App\Src\cfgsrv.c App\Src\engine.c App\Src\ui.c App\Src\menu.c App\Src\app.c -lm || exit /b 1
build\tests.exe %*
