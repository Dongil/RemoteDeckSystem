@echo off
setlocal

REM ============================================================
REM IntegrateController publish script
REM   - Target: net8.0-windows / win-x64 / self-contained
REM   - Output: <this folder>\IntegrateController\publish\
REM   - Runs dotnet publish into a single-file exe
REM ============================================================

set ROOT=%~dp0
set PROJECT=%ROOT%IntegrateController\IntegrateController.csproj
set OUTPUT=%ROOT%IntegrateController\publish

echo ======================================================
echo  IntegrateController Publish
echo ======================================================
echo  Project : %PROJECT%
echo  Output  : %OUTPUT%
echo ------------------------------------------------------

if not exist "%PROJECT%" (
    echo [ERROR] Project file not found: %PROJECT%
    exit /b 1
)

if exist "%OUTPUT%" (
    echo Cleaning previous output...
    rmdir /s /q "%OUTPUT%"
)

echo Building and publishing...
dotnet publish "%PROJECT%" ^
    -c Release ^
    -r win-x64 ^
    --self-contained true ^
    -p:PublishSingleFile=true ^
    -p:IncludeNativeLibrariesForSelfExtract=true ^
    -p:DebugType=none ^
    -p:DebugSymbols=false ^
    -o "%OUTPUT%"

if errorlevel 1 (
    echo.
    echo *** PUBLISH FAILED ***
    exit /b 1
)

echo.
echo ======================================================
echo  Publish Complete
echo ======================================================
echo Output files:
dir /b "%OUTPUT%"
echo ------------------------------------------------------
echo Executable: %OUTPUT%\IntegrateController.exe

endlocal
exit /b 0
