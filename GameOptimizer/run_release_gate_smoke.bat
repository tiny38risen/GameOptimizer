@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: run_release_gate_smoke.bat target.exe
    exit /b 2
)

set TARGET=%~1
set EXE_NAME=GameOptimizer.exe
set EXE_PATH=
set CALL_DIR=%CD%
set SCRIPT_DIR=%~dp0
set LOG_DIR=release_gate_logs
set RUN_DIR=
set RUN_DIR_FILE=%TEMP%\gameoptimizer_smoke_run_dir.txt

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
    echo [FAIL] Python 3 is required for release gate static/log assertions.
    exit /b 2
)

%PYTHON_CMD% release_gate_evidence.py init --kind smoke --target "%TARGET%" --exe "%EXE_PATH%" > "%RUN_DIR_FILE%"
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
if not "%STEP_EXIT%"=="0" goto fail

echo [RG-1] dry-run smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { $ErrorActionPreference='Stop'; try { & '%EXE_PATH%' '%TARGET%' --dry-run --max-runtime-seconds 60 2>&1 | Tee-Object -FilePath '%RUN_DIR%\logs\rg1_dry_run.log'; exit $LASTEXITCODE } catch { $_ | Out-String | Tee-Object -FilePath '%RUN_DIR%\logs\rg1_dry_run.log'; exit 1 } }"
set STEP_EXIT=%ERRORLEVEL%
%PYTHON_CMD% run_release_gate_log_assertions.py --mode dry-run "%RUN_DIR%\logs\rg1_dry_run.log"
set ASSERT_EXIT=%ERRORLEVEL%
%PYTHON_CMD% release_gate_evidence.py record --run-dir "%RUN_DIR%" --step rg1_dry_run --mode dry-run --log-file "%RUN_DIR%\logs\rg1_dry_run.log" --exit-code %STEP_EXIT% --assertion-exit-code %ASSERT_EXIT% --command "GameOptimizer.exe target --dry-run --max-runtime-seconds 60"
if not "%STEP_EXIT%"=="0" goto fail
if not "%ASSERT_EXIT%"=="0" goto fail

echo [RG-2] soft-apply smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { $ErrorActionPreference='Stop'; try { & '%EXE_PATH%' '%TARGET%' --max-runtime-seconds 120 --thread-detail-log --thread-log-interval 4 2>&1 | Tee-Object -FilePath '%RUN_DIR%\logs\rg2_soft_apply.log'; exit $LASTEXITCODE } catch { $_ | Out-String | Tee-Object -FilePath '%RUN_DIR%\logs\rg2_soft_apply.log'; exit 1 } }"
set STEP_EXIT=%ERRORLEVEL%
%PYTHON_CMD% run_release_gate_log_assertions.py --mode soft-apply "%RUN_DIR%\logs\rg2_soft_apply.log"
set ASSERT_EXIT=%ERRORLEVEL%
%PYTHON_CMD% release_gate_evidence.py record --run-dir "%RUN_DIR%" --step rg2_soft_apply --mode soft-apply --log-file "%RUN_DIR%\logs\rg2_soft_apply.log" --exit-code %STEP_EXIT% --assertion-exit-code %ASSERT_EXIT% --command "GameOptimizer.exe target --max-runtime-seconds 120 --thread-detail-log --thread-log-interval 4"
if not "%STEP_EXIT%"=="0" goto fail
if not "%ASSERT_EXIT%"=="0" goto fail

echo [RG-3] apply smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { $ErrorActionPreference='Stop'; try { & '%EXE_PATH%' '%TARGET%' --apply --latency-ping 8.8.8.8 --background-filter background_filter_example.txt --max-runtime-seconds 180 2>&1 | Tee-Object -FilePath '%RUN_DIR%\logs\rg3_apply.log'; exit $LASTEXITCODE } catch { $_ | Out-String | Tee-Object -FilePath '%RUN_DIR%\logs\rg3_apply.log'; exit 1 } }"
set STEP_EXIT=%ERRORLEVEL%
%PYTHON_CMD% run_release_gate_log_assertions.py --mode apply "%RUN_DIR%\logs\rg3_apply.log"
set ASSERT_EXIT=%ERRORLEVEL%
%PYTHON_CMD% release_gate_evidence.py record --run-dir "%RUN_DIR%" --step rg3_apply --mode apply --log-file "%RUN_DIR%\logs\rg3_apply.log" --exit-code %STEP_EXIT% --assertion-exit-code %ASSERT_EXIT% --command "GameOptimizer.exe target --apply --latency-ping 8.8.8.8 --background-filter background_filter_example.txt --max-runtime-seconds 180"
if not "%STEP_EXIT%"=="0" goto fail
if not "%ASSERT_EXIT%"=="0" goto fail

echo [RG-4] timeout safe-point smoke
powershell -NoProfile -ExecutionPolicy Bypass -Command "& { $ErrorActionPreference='Stop'; try { & '%EXE_PATH%' '%TARGET%' --dry-run --max-runtime-seconds 5 2>&1 | Tee-Object -FilePath '%RUN_DIR%\logs\rg4_timeout.log'; exit $LASTEXITCODE } catch { $_ | Out-String | Tee-Object -FilePath '%RUN_DIR%\logs\rg4_timeout.log'; exit 1 } }"
set STEP_EXIT=%ERRORLEVEL%
%PYTHON_CMD% run_release_gate_log_assertions.py --mode timeout "%RUN_DIR%\logs\rg4_timeout.log"
set ASSERT_EXIT=%ERRORLEVEL%
%PYTHON_CMD% release_gate_evidence.py record --run-dir "%RUN_DIR%" --step rg4_timeout --mode timeout --log-file "%RUN_DIR%\logs\rg4_timeout.log" --exit-code %STEP_EXIT% --assertion-exit-code %ASSERT_EXIT% --command "GameOptimizer.exe target --dry-run --max-runtime-seconds 5"
if not "%STEP_EXIT%"=="0" goto fail
if not "%ASSERT_EXIT%"=="0" goto fail

%PYTHON_CMD% release_gate_evidence.py finalize --run-dir "%RUN_DIR%"
if errorlevel 1 exit /b 1
echo Release gate smoke passed.
exit /b 0

:fail
%PYTHON_CMD% release_gate_evidence.py finalize --run-dir "%RUN_DIR%"
exit /b 1
