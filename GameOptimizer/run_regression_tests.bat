@echo off
setlocal enabledelayedexpansion

set FAILURE_COUNT=0
set TEST_COUNT=0

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

echo.
echo [INFO] regression summary: total=%TEST_COUNT%, failed=%FAILURE_COUNT%

if not "%FAILURE_COUNT%"=="0" (
    exit /b 1
)

echo [PASS] all regression tests passed
exit /b 0
