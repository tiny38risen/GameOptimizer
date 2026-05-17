@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: run_release_gate_smoke.bat target.exe
    exit /b 2
)

set TARGET=%~1
set EXE=GameOptimizer.exe
set LOG_DIR=release_gate_logs

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

where python >nul 2>nul
if errorlevel 1 (
    echo [FAIL] python is required for release gate static/log assertions.
    exit /b 2
)

python run_release_gate_static_checks.py
if errorlevel 1 exit /b 1

echo [RG-1] dry-run smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { & '.\%EXE%' '%TARGET%' --dry-run --max-runtime-seconds 60 2>&1 | Tee-Object -FilePath '%LOG_DIR%\rg1_dry_run.log'; exit $LASTEXITCODE }"
if errorlevel 1 exit /b 1
python run_release_gate_log_assertions.py --mode dry-run "%LOG_DIR%\rg1_dry_run.log"
if errorlevel 1 exit /b 1

echo [RG-2] soft-apply smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { & '.\%EXE%' '%TARGET%' --max-runtime-seconds 120 --thread-detail-log --thread-log-interval 4 2>&1 | Tee-Object -FilePath '%LOG_DIR%\rg2_soft_apply.log'; exit $LASTEXITCODE }"
if errorlevel 1 exit /b 1
python run_release_gate_log_assertions.py --mode soft-apply "%LOG_DIR%\rg2_soft_apply.log"
if errorlevel 1 exit /b 1

echo [RG-3] apply smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { & '.\%EXE%' '%TARGET%' --apply --latency-ping 8.8.8.8 --background-filter background_filter_example.txt --max-runtime-seconds 180 2>&1 | Tee-Object -FilePath '%LOG_DIR%\rg3_apply.log'; exit $LASTEXITCODE }"
if errorlevel 1 exit /b 1
python run_release_gate_log_assertions.py --mode apply "%LOG_DIR%\rg3_apply.log"
if errorlevel 1 exit /b 1

echo [RG-4] timeout safe-point smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { & '.\%EXE%' '%TARGET%' --dry-run --max-runtime-seconds 5 2>&1 | Tee-Object -FilePath '%LOG_DIR%\rg4_timeout.log'; exit $LASTEXITCODE }"
if errorlevel 1 exit /b 1
python run_release_gate_log_assertions.py --mode timeout "%LOG_DIR%\rg4_timeout.log"
if errorlevel 1 exit /b 1

echo Release gate smoke passed.
exit /b 0
