@echo off
echo Building IPSetupTool as single-file executable...
cd /d "%~dp0IPSetupTool"
dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o "%~dp0publish"
echo.
echo Build complete! Output: %~dp0publish\IPSetupTool.exe
pause
