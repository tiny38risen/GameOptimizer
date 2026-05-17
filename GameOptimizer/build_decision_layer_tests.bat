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
cl /nologo /std:c++latest /W4 /WX /permissive- ^
    LatencyDecisionLayerTests.cpp LatencyDecisionLayer.cpp ^
    /Fe:%OUT_DIR%\LatencyDecisionLayerTests.exe ^
    /Fo:%OUT_DIR%\

if errorlevel 1 (
    echo [FAIL] build failed
    exit /b 1
)

echo [PASS] build completed: %OUT_DIR%\LatencyDecisionLayerTests.exe
exit /b 0
