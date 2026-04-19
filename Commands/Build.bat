@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..") do set PROJECT_DIR=%%~fI\
set UPROJECT=%PROJECT_DIR%Priordium.uproject
set UBT_LOG=%LOCALAPPDATA%\UnrealBuildTool\Log.txt
set MAX_RETRIES=5
set RETRY_DELAY=2
set ATTEMPT=1

echo ================================================
echo  Priordium - Build
echo ================================================
echo.

:build_attempt
echo Attempt %ATTEMPT%/%MAX_RETRIES%...

call "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" ^
    PriordiumEditor ^
    Win64 ^
    Development ^
    "%UPROJECT%" ^
    -WaitMutex ^
    -NoHotReloadFromIDE

set EXIT_CODE=%ERRORLEVEL%

if %EXIT_CODE% == 0 goto build_done

if exist "%UBT_LOG%" (
    findstr /C:"EditorPerProjectUserSettings.ini' because it is being used by another process" "%UBT_LOG%" > nul
    if not errorlevel 1 (
        if %ATTEMPT% LSS %MAX_RETRIES% (
            echo.
            echo Detected temporary lock on EditorPerProjectUserSettings.ini.
            echo Retrying in %RETRY_DELAY%s...
            timeout /t %RETRY_DELAY% /nobreak > nul
            set /a ATTEMPT+=1
            goto build_attempt
        )
    )
)

:build_done

echo.
echo ================================================

if %EXIT_CODE% == 0 (
    echo  RESULT: BUILD SUCCEEDED  [exit code: 0]
) else (
    echo  RESULT: BUILD FAILED     [exit code: %EXIT_CODE%]
)

echo ================================================
echo.

endlocal & set BUILD_EXIT=%EXIT_CODE%
