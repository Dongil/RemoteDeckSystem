@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ============================================
echo  RemoteDeck PC - Firmware Builder
echo ============================================
echo.

set PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe
if not exist "%PIO%" (
    echo [ERROR] PlatformIO CLI not found: %PIO%
    pause
    exit /b 1
)

cd /d "%~dp0"

for /f "tokens=2 delims=:, " %%a in ('findstr /C:"\"version\"" data\deviceconfig.json') do (
    set RAW=%%~a
)
set VERSION=%RAW:"=%
echo  Version: %VERSION%

set DATESTR=%date:~0,4%%date:~5,2%%date:~8,2%
echo  Date:    %DATESTR%

set FILENAME=RemoteDeck_PC_V%VERSION%_%DATESTR%.bin
echo  Output:  firmware\%FILENAME%
echo.

echo [1/2] Building firmware...
"%PIO%" run
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)
echo.

if not exist firmware mkdir firmware
copy /Y "%~dp0.pio\build\esp32dev\firmware.bin" "%~dp0firmware\%FILENAME%" >nul
if errorlevel 1 (
    echo [ERROR] Copy failed!
    pause
    exit /b 1
)

echo [2/2] Firmware copied to: firmware\%FILENAME%
echo.

for %%F in ("%~dp0firmware\%FILENAME%") do set "FSIZE=%%~zF"
if defined FSIZE (
    set /a "FSIZE_KB=FSIZE / 1024"
    echo  Size: !FSIZE_KB! KB
) else (
    echo  Size: unknown
)
echo.
echo ============================================
echo  Build complete!
echo  firmware\%FILENAME%
echo ============================================
pause
