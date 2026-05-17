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

echo.
echo [INFO] regression summary: total=%TEST_COUNT%, failed=%FAILURE_COUNT%

if not "%FAILURE_COUNT%"=="0" (
    exit /b 1
)

echo [PASS] all regression tests passed
exit /b 0
