@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: run_long_soak_presets.bat target.exe [30m^|60m^|both]
    exit /b 2
)

set TARGET=%~1
set PRESET=%~2
if "%PRESET%"=="" set PRESET=both
set EXE_NAME=GameOptimizer.exe
set EXE_PATH=
set CALL_DIR=%CD%
set SCRIPT_DIR=%~dp0
set LOG_DIR=release_gate_logs
set RUN_DIR=
set RUN_DIR_FILE=%TEMP%\gameoptimizer_soak_run_dir.txt
set FINALIZE_FLAGS=

if /I not "%PRESET%"=="30m" if /I not "%PRESET%"=="60m" if /I not "%PRESET%"=="both" (
    echo [FAIL] preset must be 30m, 60m, or both.
    exit /b 2
)
if /I "%PRESET%"=="both" set FINALIZE_FLAGS=--require-soak-both

if exist "%CALL_DIR%\%EXE_NAME%" set EXE_PATH=%CALL_DIR%\%EXE_NAME%
if "%EXE_PATH%"=="" if exist "%SCRIPT_DIR%%EXE_NAME%" set EXE_PATH=%SCRIPT_DIR%%EXE_NAME%
if "%EXE_PATH%"=="" if exist "%SCRIPT_DIR%..\x64\Release\%EXE_NAME%" set EXE_PATH=%SCRIPT_DIR%..\x64\Release\%EXE_NAME%
if "%EXE_PATH%"=="" (
    echo [FAIL] GameOptimizer.exe was not found in current directory, script directory, or ..\x64\Release.
    exit /b 2
)

pushd "%SCRIPT_DIR%"

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

%PYTHON_CMD% release_gate_evidence.py init --kind soak --target "%TARGET%" --exe "%EXE_PATH%" > "%RUN_DIR_FILE%"
if errorlevel 1 exit /b 1
set /p RUN_DIR=<"%RUN_DIR_FILE%"
del "%RUN_DIR_FILE%" >nul 2>nul
if "%RUN_DIR%"=="" (
    echo [FAIL] failed to initialize release evidence directory.
    exit /b 1
)

%PYTHON_CMD% run_release_gate_static_checks.py
set STEP_EXIT=%ERRORLEVEL%
%PYTHON_CMD% release_gate_evidence.py record --run-dir "%RUN_DIR%" --step static_checks --mode static --exit-code %STEP_EXIT% --command "run_release_gate_static_checks.py"
if not "%STEP_EXIT%"=="0" (
    echo [BLOCKER] long soak gate failed: static checks failed
    goto fail
)

if /I "%PRESET%"=="30m" call :run30
if errorlevel 1 (
    echo [BLOCKER] long soak gate failed: 30m dry-run soak failed
    goto fail
)
if /I "%PRESET%"=="60m" call :run60
if errorlevel 1 (
    echo [BLOCKER] long soak gate failed: 60m soft-apply soak failed
    goto fail
)
if /I "%PRESET%"=="both" call :run30
if errorlevel 1 (
    echo [BLOCKER] long soak gate failed: 30m dry-run soak failed
    goto fail
)
if /I "%PRESET%"=="both" call :run60
if errorlevel 1 (
    echo [BLOCKER] long soak gate failed: 60m soft-apply soak failed
    goto fail
)

%PYTHON_CMD% release_gate_evidence.py finalize --run-dir "%RUN_DIR%" %FINALIZE_FLAGS%
if errorlevel 1 exit /b 1
echo Long soak preset validation passed.
exit /b 0

:run30
echo [SOAK-30M] dry-run smoke preset with hang detection
%PYTHON_CMD% run_soak_with_hang_detection.py --timeout-seconds 1920 --idle-timeout-seconds 240 --log-file "%RUN_DIR%\logs\soak_30m_dry_run.log" -- "%EXE_PATH%" "%TARGET%" --dry-run --max-runtime-seconds 1800 --thread-detail-log --thread-log-interval 20
set STEP_EXIT=%ERRORLEVEL%
%PYTHON_CMD% run_release_gate_log_assertions.py --mode soak "%RUN_DIR%\logs\soak_30m_dry_run.log"
set ASSERT_EXIT=%ERRORLEVEL%
%PYTHON_CMD% release_gate_evidence.py record --run-dir "%RUN_DIR%" --step soak_30m_dry_run --mode soak --log-file "%RUN_DIR%\logs\soak_30m_dry_run.log" --exit-code %STEP_EXIT% --assertion-exit-code %ASSERT_EXIT% --command "GameOptimizer.exe %TARGET% --dry-run --max-runtime-seconds 1800 --thread-detail-log --thread-log-interval 20"
if not "%STEP_EXIT%"=="0" (
    echo [BLOCKER] SOAK-30M command failed with exit code %STEP_EXIT%
    exit /b 1
)
if not "%ASSERT_EXIT%"=="0" (
    echo [BLOCKER] SOAK-30M log assertions failed with exit code %ASSERT_EXIT%
    exit /b 1
)
exit /b 0

:run60
echo [SOAK-60M] soft-apply smoke preset with hang detection
%PYTHON_CMD% run_soak_with_hang_detection.py --timeout-seconds 3780 --idle-timeout-seconds 300 --log-file "%RUN_DIR%\logs\soak_60m_soft_apply.log" -- "%EXE_PATH%" "%TARGET%" --max-runtime-seconds 3600 --thread-detail-log --thread-log-interval 40
set STEP_EXIT=%ERRORLEVEL%
%PYTHON_CMD% run_release_gate_log_assertions.py --mode soak "%RUN_DIR%\logs\soak_60m_soft_apply.log"
set ASSERT_EXIT=%ERRORLEVEL%
%PYTHON_CMD% release_gate_evidence.py record --run-dir "%RUN_DIR%" --step soak_60m_soft_apply --mode soak --log-file "%RUN_DIR%\logs\soak_60m_soft_apply.log" --exit-code %STEP_EXIT% --assertion-exit-code %ASSERT_EXIT% --command "GameOptimizer.exe %TARGET% --max-runtime-seconds 3600 --thread-detail-log --thread-log-interval 40"
if not "%STEP_EXIT%"=="0" (
    echo [BLOCKER] SOAK-60M command failed with exit code %STEP_EXIT%
    exit /b 1
)
if not "%ASSERT_EXIT%"=="0" (
    echo [BLOCKER] SOAK-60M log assertions failed with exit code %ASSERT_EXIT%
    exit /b 1
)
exit /b 0

:fail
%PYTHON_CMD% release_gate_evidence.py finalize --run-dir "%RUN_DIR%" %FINALIZE_FLAGS%
exit /b 1
