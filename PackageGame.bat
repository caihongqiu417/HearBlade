@echo off
rem One-click Windows package for HearBlade. CLOSE THE UNREAL EDITOR FIRST.
rem (The editor holds port 8000 for the MCP plugin; the cook process would fail on it.)
tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >/dev/null && (echo Please close the Unreal Editor first, then run this again. & pause & exit /b 1)
taskkill /IM LiveCodingConsole.exe /F >/dev/null 2>&1
call "D:\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="D:\Udev\HearBlade\HearBlade.uproject" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -compressed -package -archive -archivedirectory="D:\Udev\HearBlade_Build" -utf8output -unrealexe="D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if errorlevel 1 (echo. & echo PACKAGE FAILED - send the log above to Claude. & pause & exit /b 1)
echo.
echo DONE: D:\Udev\HearBlade_Build\Windows\HearBlade.exe
start "" "D:\Udev\HearBlade_Build\Windows"
pause
