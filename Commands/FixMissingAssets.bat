@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..") do set PROJECT_DIR=%%~fI\
set UPROJECT=%PROJECT_DIR%Priordium.uproject
set PY_SCRIPT=%SCRIPT_DIR%fix_missing_assets.py

echo ================================================
echo  Priordium - Fix Missing Assets
echo ================================================
echo.
echo This script removes actors with missing Blueprint classes
echo from the persistent level (World.umap).
echo.
echo IMPORTANT: Close the Unreal Editor before running this script.
echo.
pause

"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%UPROJECT%" ^
    /Game/World ^
    -run=pythonscript ^
    -script="%PY_SCRIPT%" ^
    -stdout ^
    -FullStdOutLogOutput ^
    -unattended ^
    -nopause

set EXIT_CODE=%ERRORLEVEL%

echo.
echo ================================================
if %EXIT_CODE% == 0 (
    echo  RESULT: COMPLETED  [exit code: 0]
) else (
    echo  RESULT: CHECK LOG  [exit code: %EXIT_CODE%]
)
echo ================================================
echo.
echo Check the output above for "fix_missing_assets:" lines.
echo.
pause

endlocal
