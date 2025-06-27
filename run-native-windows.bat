@echo off
REM Run the native Windows executable built via cross-compilation

echo ===== GPU Ray Trace Example (Native Windows) =====
echo.

REM Check if executable exists
if not exist "gpu_raytrace.exe" (
    echo ERROR: Native Windows executable not found!
    echo.
    echo Please build it first using one of these methods:
    echo   1. From WSL: ./build-cross-compile.sh
    echo   2. From WSL: TARGET=windows-native make
    echo   3. Copy gpu_raytrace.exe to this directory
    echo.
    echo The build process should automatically copy the executable here.
    echo.
    pause
    exit /b 1
)

REM Check if shaders exist
if not exist "shaders\raytrace_tlas_blas_processed.fs" (
    echo ERROR: Processed shaders not found!
    echo Please ensure the shaders directory is copied alongside the executable.
    echo.
    pause
    exit /b 1
)

echo Running native Windows executable...
echo Executable: gpu_raytrace.exe
echo Size: 
for %%A in ("gpu_raytrace.exe") do echo   %%~zA bytes

echo.
echo Starting application...
echo.

REM Run the application
gpu_raytrace.exe

echo.
echo Application finished.
pause 