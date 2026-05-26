@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: run_rc_gate.bat target.exe
    exit /b 2
)

set TARGET=%~1
set SCRIPT_DIR=%~dp0
set VCVARS64=
set RC_BLOCKER=unknown
set FINAL_REGRESSION_LOG=

pushd "%SCRIPT_DIR%"

where cl >nul 2>nul
if errorlevel 1 (
    if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set VCVARS64=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat
    )
    if "%VCVARS64%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set VCVARS64=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
    )
    if "%VCVARS64%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        set VCVARS64=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
    )
    if "%VCVARS64%"=="" (
        echo [FAIL] MSVC cl.exe was not found in PATH and vcvars64.bat could not be located.
        echo [INFO] Open an x64 Native Tools Command Prompt or install Visual Studio C++ tools.
        popd
        exit /b 2
    )

    echo [INFO] MSVC cl.exe was not found in PATH; loading "%VCVARS64%"
    call "%VCVARS64%"
    if errorlevel 1 (
        echo [FAIL] failed to initialize MSVC x64 build environment.
        popd
        exit /b 2
    )
)

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
    echo [FAIL] Python 3 is required for RC gate static checks.
    popd
    exit /b 2
)

echo [RC-1] Python syntax gate
for %%F in (*.py) do (
    %PYTHON_CMD% -m py_compile "%%F"
    if errorlevel 1 (
        set RC_BLOCKER=py_compile failed
        goto fail
    )
)

echo [RC-2] git diff whitespace gate
git diff --check
if errorlevel 1 (
    set RC_BLOCKER=git diff --check failed
    goto fail
)

echo [RC-3] static release gate
%PYTHON_CMD% run_release_gate_static_checks_selftest.py
if errorlevel 1 (
    set RC_BLOCKER=static gate selftest failed
    goto fail
)

%PYTHON_CMD% run_release_gate_static_checks.py
if errorlevel 1 (
    set RC_BLOCKER=static gate failed
    goto fail
)

echo [RC-4] Release x64 MSVC build
pushd "%SCRIPT_DIR%.."
msbuild GameOptimizer.slnx /p:Configuration=Release /p:Platform=x64 /m
set BUILD_EXIT=%ERRORLEVEL%
popd
if not "%BUILD_EXIT%"=="0" (
    set RC_BLOCKER=Release x64 MSVC build failed
    goto fail
)

echo [RC-5] full regression
if not exist "%SCRIPT_DIR%release_gate_logs" mkdir "%SCRIPT_DIR%release_gate_logs"
for /f %%I in ('powershell -NoProfile -Command "(Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')"') do set RC_TIMESTAMP=%%I
set FINAL_REGRESSION_LOG=%SCRIPT_DIR%release_gate_logs\%RC_TIMESTAMP%_final_regression.log
echo [INFO] final regression log: "%FINAL_REGRESSION_LOG%"
call run_regression_tests.bat > "%FINAL_REGRESSION_LOG%" 2>&1
set REGRESSION_EXIT=%ERRORLEVEL%
type "%FINAL_REGRESSION_LOG%"
if not "%REGRESSION_EXIT%"=="0" (
    set RC_BLOCKER=regression failed
    goto fail
)

echo [RC-6] release smoke
call run_release_gate_smoke.bat "%TARGET%"
if errorlevel 1 (
    set RC_BLOCKER=release smoke failed
    goto fail
)

echo [RC-7] long soak evidence gate: 30m dry-run + 60m soft-apply
call run_long_soak_presets.bat "%TARGET%" both
if errorlevel 1 (
    set RC_BLOCKER=30m or 60m soak failed
    goto fail
)

echo [RC-8] verify RC evidence set
%PYTHON_CMD% release_gate_evidence.py verify-rc --target "%TARGET%"
if errorlevel 1 (
    set RC_BLOCKER=verify-rc failed
    goto fail
)

echo [RC-9] verify RC candidate package inputs
%PYTHON_CMD% verify_real_game_validation.py --matrix docs\release\Game_Verification_Matrix.json
if errorlevel 1 (
    set RC_BLOCKER=real game validation failed
    goto fail
)

%PYTHON_CMD% verify_rc_candidate.py --target "%TARGET%" --regression-log "%FINAL_REGRESSION_LOG%"
if errorlevel 1 (
    set RC_BLOCKER=RC candidate verification failed
    goto fail
)

echo [RC-10] create final RC evidence bundle
%PYTHON_CMD% create_rc_evidence_bundle.py --target "%TARGET%" --regression-log "%FINAL_REGRESSION_LOG%"
if errorlevel 1 (
    set RC_BLOCKER=RC evidence bundle creation failed
    goto fail
)

echo [PASS] RC gate passed. Required evidence reports and the final RC evidence bundle were generated under release_gate_logs.
popd
exit /b 0

:fail
echo [BLOCKER] RC gate failed: %RC_BLOCKER%
echo [FAIL] RC gate failed.
popd
exit /b 1
