@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: run_soft_apply_soak_60m.bat target.exe
    exit /b 2
)

rem Validates: shutdown_reason, runtime_validation_status, rollback_preserved_state_count,
rem BLOCKER count, timeline monotonicity, heartbeat progression.
set TARGET=%~1
set SCRIPT_DIR=%~dp0

pushd "%SCRIPT_DIR%"
call run_long_soak_presets.bat "%TARGET%" 60m
set SOAK_EXIT=%ERRORLEVEL%
popd

if not "%SOAK_EXIT%"=="0" (
    echo [BLOCKER] 60m soft-apply soak failed
    exit /b 1
)

echo [PASS] 60m soft-apply soak passed
exit /b 0
