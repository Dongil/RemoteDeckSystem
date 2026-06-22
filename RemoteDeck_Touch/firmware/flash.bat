@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set COUNT=0

:select_firmware
echo.
echo ==============================
echo  RemoteDeck_PC Firmware Flasher
echo ==============================
echo.
echo .bin files in this folder:
echo ------------------------------
set BIN_COUNT=0

for %%F in (*.bin) do (
    set /a BIN_COUNT+=1
    set BIN_!BIN_COUNT!=%%F
    echo   [!BIN_COUNT!] %%F
)

if !BIN_COUNT! EQU 0 (
    echo No .bin file found. Put firmware .bin in this folder.
    pause
    exit /b 1
)

echo.
if !BIN_COUNT! EQU 1 (
    set FIRMWARE=!BIN_1!
    echo Auto-selected: !FIRMWARE!
    goto detect_mode
)

set /p FSEL=Select firmware number (1-!BIN_COUNT!):
if "!FSEL!"=="" goto select_firmware
set FIRMWARE=!BIN_%FSEL%!
if "!FIRMWARE!"=="" (
    echo Invalid selection.
    goto select_firmware
)

:detect_mode
rem  *_FULL_*.bin -> 0x0  (bootloader+partitions+app+spiffs merged image)
rem  *_OTA_*.bin  -> 0x10000  (app0 partition only)
set OFFSET=
set FLASH_MODE=
set DO_ERASE=0

echo !FIRMWARE! | findstr /i "_FULL_" > nul
if !errorlevel! EQU 0 (
    set OFFSET=0x0
    set FLASH_MODE=FULL
    set DO_ERASE=1
    goto select_port
)

echo !FIRMWARE! | findstr /i "_OTA_" > nul
if !errorlevel! EQU 0 (
    set OFFSET=0x10000
    set FLASH_MODE=OTA
    set DO_ERASE=0
    goto select_port
)

echo.
echo Could not detect mode from filename.
echo   [1] FULL mode - offset 0x0, erase_flash first (factory init)
echo   [2] OTA  mode - offset 0x10000, write app partition only
set /p MSEL=Select mode (1/2):
if "!MSEL!"=="1" (
    set OFFSET=0x0
    set FLASH_MODE=FULL
    set DO_ERASE=1
    goto select_port
)
if "!MSEL!"=="2" (
    set OFFSET=0x10000
    set FLASH_MODE=OTA
    set DO_ERASE=0
    goto select_port
)
echo Invalid selection.
goto select_firmware

:select_port
echo.
echo Available COM ports:
echo ------------------------------
set PORT_COUNT=0

for /f "tokens=3" %%A in ('reg query HKLM\HARDWARE\DEVICEMAP\SERIALCOMM 2^>nul ^| findstr /r "COM[0-9]"') do (
    set /a PORT_COUNT+=1
    set PORT_!PORT_COUNT!=%%A
    echo   [!PORT_COUNT!] %%A
)

if !PORT_COUNT! EQU 0 goto no_port
if !PORT_COUNT! EQU 1 goto one_port
goto many_port

:no_port
echo No COM port detected automatically.
echo   - Connect the board via USB, or
echo   - Type a port name manually such as COM3 and press Enter.
echo   - Just press Enter to rescan.
set /p MANUAL=Port:
if "!MANUAL!"=="" goto select_port
set PORT=!MANUAL!
echo Manual port: !PORT!
goto start

:one_port
set PORT=!PORT_1!
echo Auto-selected: !PORT_1!
goto start

:many_port
echo   [M] Type port name manually
set /p SEL=Select port number 1-!PORT_COUNT! or M:
if /i "!SEL!"=="M" goto manual_port
set PORT=!PORT_%SEL%!
if "!PORT!"=="" (
    echo Invalid selection.
    goto select_port
)
goto start

:manual_port
set /p MANUAL=Port name such as COM3:
if "!MANUAL!"=="" goto select_port
set PORT=!MANUAL!
goto start

:start
echo.
echo ------------------------------
echo  Firmware : !FIRMWARE!
echo  Mode     : !FLASH_MODE!
echo  Offset   : !OFFSET!
echo  Port     : !PORT!
echo ------------------------------

:loop
set /a COUNT+=1
echo.
echo ==============================
echo  Board #!COUNT! - start flashing
echo ==============================
echo Connect a new board to !PORT! and press Enter...
echo (Press Ctrl+C to change port and restart)
pause > nul

if "!DO_ERASE!"=="1" (
    echo [1/2] Erasing flash...
    esptool.exe --chip esp32 --port !PORT! erase_flash
    if errorlevel 1 (
        echo.
        echo Error! Check board connection and press Enter to retry...
        pause > nul
        set /a COUNT-=1
        goto loop
    )
    echo [2/2] Writing firmware at offset !OFFSET! ...
) else (
    echo [1/1] Writing firmware at offset !OFFSET! - erase skipped ...
)

esptool.exe --chip esp32 --port !PORT! --baud 921600 write_flash !OFFSET! "!FIRMWARE!"
if errorlevel 1 (
    echo.
    echo Error! Check board connection and press Enter to retry...
    pause > nul
    set /a COUNT-=1
    goto loop
)

echo.
echo [DONE] Board #!COUNT! finished!
echo Press Enter for the next board, Ctrl+C to quit.
pause > nul
goto loop
