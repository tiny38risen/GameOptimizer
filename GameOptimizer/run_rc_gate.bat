@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
    echo Usage: run_rc_gate.bat target.exe
    exit /b 2
)

set TARGET=%~1
set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..
set VCVARS64=
set PYTHON_CMD=
set RC_BLOCKER=unknown
set RC_LOG_DIR=
set RC_REGRESSION_LOG=

pushd "%SCRIPT_DIR%"

for /f %%I in ('powershell -NoProfile -Command "(Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')"') do set RC_TIMESTAMP=%%I
if "%RC_TIMESTAMP%"=="" set RC_TIMESTAMP=unknown-time
set RC_LOG_DIR=%REPO_ROOT%\artifacts\rc\%RC_TIMESTAMP%
set RC_REGRESSION_LOG=%RC_LOG_DIR%\rc6_full_regression.log
if not exist "%RC_LOG_DIR%" mkdir "%RC_LOG_DIR%"
echo [INFO] RC gate artifact directory: "%RC_LOG_DIR%"

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

echo [RC-1] git diff whitespace gate
call :run_logged rc1_git_diff_check git diff --check
if errorlevel 1 (
    set RC_BLOCKER=git diff --check failed
    goto fail
)

echo [RC-2] Python syntax gate
call :run_py_compile
if errorlevel 1 (
    set RC_BLOCKER=py_compile failed
    goto fail
)

echo [RC-3] static release gate
call :run_logged rc3_static_selftest %PYTHON_CMD% run_release_gate_static_checks_selftest.py
if errorlevel 1 (
    set RC_BLOCKER=static gate selftest failed
    goto fail
)
call :run_logged rc3_static_gate %PYTHON_CMD% run_release_gate_static_checks.py
if errorlevel 1 (
    set RC_BLOCKER=static gate failed
    goto fail
)

echo [RC-4] evidence self-test
call :run_logged rc4_evidence_selftest %PYTHON_CMD% release_gate_evidence_selftest.py
if errorlevel 1 (
    set RC_BLOCKER=evidence self-test failed
    goto fail
)

echo [RC-5] Release x64 MSVC build
call :run_release_build
if errorlevel 1 (
    set RC_BLOCKER=Release x64 MSVC build failed
    goto fail
)

echo [RC-6] full regression
call :run_logged rc6_full_regression cmd.exe /d /c call run_regression_tests.bat
if errorlevel 1 (
    set RC_BLOCKER=regression failed
    goto fail
)

echo [RC-7] release smoke
call :run_logged rc7_release_smoke cmd.exe /d /c call run_release_gate_smoke.bat "%TARGET%"
if errorlevel 1 (
    set RC_BLOCKER=release smoke failed
    goto fail
)

echo [RC-8] evidence bundle generation
call :run_logged rc8_evidence_bundle %PYTHON_CMD% create_rc_evidence_bundle.py --target "%TARGET%" --regression-log "%RC_REGRESSION_LOG%"
if errorlevel 1 (
    set RC_BLOCKER=evidence bundle generation failed
    goto fail
)

echo [RC-9] verify-rc
call :run_logged rc9_verify_rc %PYTHON_CMD% release_gate_evidence.py verify-rc --target "%TARGET%"
if errorlevel 1 (
    set RC_BLOCKER=verify-rc failed
    goto fail
)

echo [PASS] RC gate passed. Step logs were written under "%RC_LOG_DIR%".
popd
exit /b 0

:run_py_compile
set STEP_LOG=%RC_LOG_DIR%\rc2_py_compile.log
set STEP_EXIT=0
(
    for %%F in (*.py) do (
        echo [INFO] py_compile %%F
        %PYTHON_CMD% -m py_compile "%%F"
        if errorlevel 1 (
            echo [FAIL] py_compile failed for %%F
            set STEP_EXIT=1
        )
    )
) > "%STEP_LOG%" 2>&1
type "%STEP_LOG%"
exit /b %STEP_EXIT%

:run_release_build
set STEP_LOG=%RC_LOG_DIR%\rc5_release_x64_build.log
pushd "%REPO_ROOT%"
msbuild GameOptimizer.slnx /p:Configuration=Release /p:Platform=x64 /m > "%STEP_LOG%" 2>&1
set STEP_EXIT=%ERRORLEVEL%
popd
type "%STEP_LOG%"
exit /b %STEP_EXIT%

:run_logged
set STEP_NAME=%~1
set STEP_LOG=%RC_LOG_DIR%\%STEP_NAME%.log
echo [INFO] running: %2 %3 %4 %5 %6 %7 %8 %9
%2 %3 %4 %5 %6 %7 %8 %9 > "%STEP_LOG%" 2>&1
set STEP_EXIT=%ERRORLEVEL%
type "%STEP_LOG%"
exit /b %STEP_EXIT%

:fail
echo [BLOCKER] RC gate failed: %RC_BLOCKER%
echo [INFO] RC gate artifact directory: "%RC_LOG_DIR%"
echo [FAIL] RC gate failed.
popd
exit /b 1
