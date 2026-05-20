@echo off
setlocal enabledelayedexpansion

set FAILURE_COUNT=0
set TEST_COUNT=0
set PYTHON_CMD=

call build_decision_layer_tests.bat
if errorlevel 1 (
    echo [FAIL] regression aborted because build failed
    exit /b 1
)

set /a TEST_COUNT+=1
echo [INFO] running LatencyDecisionLayerTests...
build_tests\LatencyDecisionLayerTests.exe
if errorlevel 1 (
    echo [FAIL] LatencyDecisionLayerTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] LatencyDecisionLayerTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running AppliedPolicyTrackerTests...
build_tests\AppliedPolicyTrackerTests.exe
if errorlevel 1 (
    echo [FAIL] AppliedPolicyTrackerTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] AppliedPolicyTrackerTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running RuntimeValidationMonitorTests...
build_tests\RuntimeValidationMonitorTests.exe
if errorlevel 1 (
    echo [FAIL] RuntimeValidationMonitorTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] RuntimeValidationMonitorTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running BackgroundControllerSafetyTests...
build_tests\BackgroundControllerSafetyTests.exe
if errorlevel 1 (
    echo [FAIL] BackgroundControllerSafetyTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] BackgroundControllerSafetyTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running TopologyAnalyzerTests...
build_tests\TopologyAnalyzerTests.exe
if errorlevel 1 (
    echo [FAIL] TopologyAnalyzerTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] TopologyAnalyzerTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running ProcessorGroupHedtEvidenceTests...
build_tests\ProcessorGroupHedtEvidenceTests.exe
if errorlevel 1 (
    echo [FAIL] ProcessorGroupHedtEvidenceTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] ProcessorGroupHedtEvidenceTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running NetworkInterruptControllerTests...
build_tests\NetworkInterruptControllerTests.exe
if errorlevel 1 (
    echo [FAIL] NetworkInterruptControllerTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] NetworkInterruptControllerTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running TimerInputControllerTests...
build_tests\TimerInputControllerTests.exe
if errorlevel 1 (
    echo [FAIL] TimerInputControllerTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] TimerInputControllerTests passed
)

set /a TEST_COUNT+=1
echo [INFO] running AntiCheatFallbackTests...
build_tests\AntiCheatFallbackTests.exe
if errorlevel 1 (
    echo [FAIL] AntiCheatFallbackTests failed
    set /a FAILURE_COUNT+=1
) else (
    echo [PASS] AntiCheatFallbackTests passed
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
    where py >nul 2>nul
    if not errorlevel 1 (
        py -3 --version >nul 2>nul
        if not errorlevel 1 (
            set PYTHON_CMD=py -3
        )
    )
)

set /a TEST_COUNT+=1
echo [INFO] running release_gate_evidence_selftest...
if "%PYTHON_CMD%"=="" (
    echo [FAIL] release_gate_evidence_selftest failed because Python 3 was not found
    set /a FAILURE_COUNT+=1
) else (
    %PYTHON_CMD% release_gate_evidence_selftest.py
    if errorlevel 1 (
        echo [FAIL] release_gate_evidence_selftest failed
        set /a FAILURE_COUNT+=1
    ) else (
        echo [PASS] release_gate_evidence_selftest passed
    )
)

echo.
echo [INFO] regression summary: total=%TEST_COUNT%, failed=%FAILURE_COUNT%

if not "%FAILURE_COUNT%"=="0" (
    exit /b 1
)

echo [PASS] all regression tests passed
exit /b 0
