@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..") do set PROJECT_DIR=%%~fI\
set UPROJECT=%PROJECT_DIR%Priordium.uproject

echo ================================================
echo  Priordium MapGenerator - Automated Tests
echo ================================================
echo.

set REPORT_DIR=%PROJECT_DIR%Saved\TestResults

"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%UPROJECT%" ^
    -ExecCmds="Automation RunTests Priordium.MapGenerator;Quit" ^
    -TestExit="Automation Test Queue Empty" ^
    -ReportExportPath="%REPORT_DIR%" ^
    -Unattended ^
    -NullRHI ^
    -NoSplash ^
    -log

set EXIT_CODE=%ERRORLEVEL%

echo.
echo ================================================

if %EXIT_CODE% == 0 (
    echo  RESULT: ALL TESTS PASSED  [exit code: 0]
) else (
    echo  RESULT: SOME TESTS FAILED [exit code: %EXIT_CODE%]
)

echo ================================================
echo.

echo Generating standalone report...
powershell -NoProfile -ExecutionPolicy Bypass -File ^
    "%SCRIPT_DIR%GenerateReport.ps1" ^
    -ReportDir "%REPORT_DIR%"

echo.
echo Results saved to: Saved\TestResults
echo.

if exist "%REPORT_DIR%\report.html" (
    echo Opening report...
    start "" "%REPORT_DIR%\report.html"
) else (
    echo WARNING: report.html not found.
)

echo.
echo ================================================
echo  Press any key to close this window...
echo ================================================

endlocal
exit /b %EXIT_CODE%
