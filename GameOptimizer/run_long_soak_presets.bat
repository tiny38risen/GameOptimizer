@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: run_long_soak_presets.bat target.exe [30m^|60m^|both]
    exit /b 2
)

set TARGET=%~1
set PRESET=%~2
if "%PRESET%"=="" set PRESET=30m
set EXE=GameOptimizer.exe
set LOG_DIR=release_gate_logs

if /I not "%PRESET%"=="30m" if /I not "%PRESET%"=="60m" if /I not "%PRESET%"=="both" (
    echo [FAIL] preset must be 30m, 60m, or both.
    exit /b 2
)

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

set PYTHON_CMD=
set BUNDLED_PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe
if exist "%BUNDLED_PYTHON%" (
    set PYTHON_CMD="%BUNDLED_PYTHON%"
)
if "%PYTHON_CMD%"=="" (
    where python >nul 2>nul
    if not errorlevel 1 (
        python --version >nul 2>nul
        if not errorlevel 1 (
            set PYTHON_CMD=python
        )
    )
)
if "%PYTHON_CMD%"=="" (
    where python3 >nul 2>nul
    if not errorlevel 1 (
        python3 --version >nul 2>nul
        if not errorlevel 1 (
            set PYTHON_CMD=python3
        )
    )
)
if "%PYTHON_CMD%"=="" (
    where py >nul 2>nul
    if not errorlevel 1 (
        py -3 --version >nul 2>nul
        if not errorlevel 1 (
            set PYTHON_CMD=py -3
        )
    )
)
if "%PYTHON_CMD%"=="" (
    echo [FAIL] Python 3 is required for long soak automation.
    exit /b 2
)

%PYTHON_CMD% run_release_gate_static_checks.py
if errorlevel 1 exit /b 1

if /I "%PRESET%"=="30m" call :run30
if errorlevel 1 exit /b 1
if /I "%PRESET%"=="60m" call :run60
if errorlevel 1 exit /b 1
if /I "%PRESET%"=="both" call :run30
if errorlevel 1 exit /b 1
if /I "%PRESET%"=="both" call :run60
if errorlevel 1 exit /b 1

echo Long soak preset validation passed.
exit /b 0

:run30
echo [SOAK-30M] dry-run smoke preset with hang detection
%PYTHON_CMD% run_soak_with_hang_detection.py --timeout-seconds 1920 --idle-timeout-seconds 240 --log-file "%LOG_DIR%\soak_30m_dry_run.log" -- ".\%EXE%" "%TARGET%" --dry-run --max-runtime-seconds 1800 --thread-detail-log --thread-log-interval 20
if errorlevel 1 exit /b 1
%PYTHON_CMD% run_release_gate_log_assertions.py --mode soak "%LOG_DIR%\soak_30m_dry_run.log"
if errorlevel 1 exit /b 1
exit /b 0

:run60
echo [SOAK-60M] soft-apply smoke preset with hang detection
%PYTHON_CMD% run_soak_with_hang_detection.py --timeout-seconds 3780 --idle-timeout-seconds 300 --log-file "%LOG_DIR%\soak_60m_soft_apply.log" -- ".\%EXE%" "%TARGET%" --max-runtime-seconds 3600 --thread-detail-log --thread-log-interval 40
if errorlevel 1 exit /b 1
%PYTHON_CMD% run_release_gate_log_assertions.py --mode soak "%LOG_DIR%\soak_60m_soft_apply.log"
if errorlevel 1 exit /b 1
exit /b 0
