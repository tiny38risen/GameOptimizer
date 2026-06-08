@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: run_dry_run_soak_30m.bat target.exe
    exit /b 2
)

rem Validates: 30 minute completion, clean shutdown, shutdown_reason,
rem runtime_validation_status, rollback_preserved_state_count, BLOCKER count,
rem timeline monotonicity, heartbeat progression, and dry-run no-mutation markers.
set TARGET=%~1
set SCRIPT_DIR=%~dp0

pushd "%SCRIPT_DIR%"
call run_long_soak_presets.bat "%TARGET%" 30m
set SOAK_EXIT=%ERRORLEVEL%
popd

if not "%SOAK_EXIT%"=="0" (
    echo [BLOCKER] 30m dry-run soak failed
    exit /b 1
)

echo [PASS] 30m dry-run soak passed
exit /b 0
