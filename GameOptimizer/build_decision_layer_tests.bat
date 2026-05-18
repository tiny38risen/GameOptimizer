@echo off
setlocal enabledelayedexpansion

where cl >nul 2>nul
if errorlevel 1 (
    echo [ERROR] MSVC cl.exe was not found in PATH.
    echo [INFO]  Open "x64 Native Tools Command Prompt for VS 2022" and run this script again.
    exit /b 1
)

set OUT_DIR=build_tests
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [INFO] building LatencyDecisionLayerTests...
cl /nologo /std:c++latest /EHsc /W4 /WX /permissive- ^
    LatencyDecisionLayerTests.cpp LatencyDecisionLayer.cpp ^
    /Fe:%OUT_DIR%\LatencyDecisionLayerTests.exe ^
    /Fo:%OUT_DIR%\

if errorlevel 1 (
    echo [FAIL] build failed
    exit /b 1
)

echo [INFO] building AppliedPolicyTrackerTests...
cl /nologo /std:c++latest /EHsc /W4 /WX /permissive- ^
    AppliedPolicyTrackerTests.cpp AppliedPolicyTracker.cpp ^
    /Fe:%OUT_DIR%\AppliedPolicyTrackerTests.exe ^
    /Fo:%OUT_DIR%\

if errorlevel 1 (
    echo [FAIL] build failed
    exit /b 1
)

echo [INFO] building RuntimeValidationMonitorTests...
cl /nologo /std:c++latest /EHsc /W4 /WX /permissive- ^
    RuntimeValidationMonitorTests.cpp RuntimeValidationMonitor.cpp ^
    /Fe:%OUT_DIR%\RuntimeValidationMonitorTests.exe ^
    /Fo:%OUT_DIR%\

if errorlevel 1 (
    echo [FAIL] build failed
    exit /b 1
)

echo [INFO] building BackgroundControllerSafetyTests...
cl /nologo /std:c++latest /EHsc /W4 /WX /permissive- ^
    BackgroundControllerSafetyTests.cpp BackgroundController.cpp RollbackManager.cpp ApplyGuard.cpp ^
    /Fe:%OUT_DIR%\BackgroundControllerSafetyTests.exe ^
    /Fo:%OUT_DIR%\

if errorlevel 1 (
    echo [FAIL] build failed
    exit /b 1
)

echo [INFO] building NetworkInterruptControllerTests...
cl /nologo /std:c++latest /EHsc /W4 /WX /permissive- ^
    NetworkInterruptControllerTests.cpp NetworkInterruptController.cpp PolicyDispatcher.cpp BackgroundController.cpp RollbackManager.cpp ApplyGuard.cpp SchedulerController.cpp ThreadTracker.cpp ^
    /Fe:%OUT_DIR%\NetworkInterruptControllerTests.exe ^
    /Fo:%OUT_DIR%\ ^
    Pdh.lib

if errorlevel 1 (
    echo [FAIL] build failed
    exit /b 1
)

echo [INFO] building TimerInputControllerTests...
cl /nologo /std:c++latest /EHsc /W4 /WX /permissive- ^
    TimerInputControllerTests.cpp TimerResolutionController.cpp InputLatencyController.cpp ^
    /Fe:%OUT_DIR%\TimerInputControllerTests.exe ^
    /Fo:%OUT_DIR%\

if errorlevel 1 (
    echo [FAIL] build failed
    exit /b 1
)

echo [PASS] all test builds completed in %OUT_DIR%
exit /b 0
