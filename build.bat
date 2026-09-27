@echo off
REM ============================================================
REM  build.bat — Build the C++ fractal tree on Windows
REM  Requires freeglut via vcpkg. If you don't have vcpkg yet:
REM
REM    git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
REM    %USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
REM    %USERPROFILE%\vcpkg\vcpkg install freeglut:x64-windows
REM    set VCPKG_ROOT=%USERPROFILE%\vcpkg
REM ============================================================

if "%VCPKG_ROOT%"=="" (
    echo ERROR: VCPKG_ROOT is not set.
    echo Set it to your vcpkg installation directory, e.g.:
    echo   set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

set BUILD_DIR=build_cpp

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

cmake .. ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 ( echo CMake configure failed & exit /b 1 )

cmake --build . --config Release

if errorlevel 1 ( echo Build failed & exit /b 1 )

echo.
echo ============================================================
echo  Build succeeded!
echo  Run:  %BUILD_DIR%\Release\fractal_tree.exe
echo ============================================================
