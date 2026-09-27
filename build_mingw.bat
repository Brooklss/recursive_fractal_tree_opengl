@echo off
REM ============================================================
REM  build_mingw.bat — Build the C++ fractal tree with MinGW/MSYS2
REM  Requires MSYS2 installed at C:\msys64
REM  Run from the project root directory.
REM ============================================================

set GPP=C:\msys64\mingw64\bin\g++.exe
set INC=C:\msys64\mingw64\include
set LIB=C:\msys64\mingw64\lib

if not exist "%GPP%" (
    echo ERROR: g++ not found at %GPP%
    echo Install MSYS2 from https://msys2.org, then run:
    echo   C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut"
    exit /b 1
)

echo Building fractal_tree.cpp ...
"%GPP%" fractal_tree.cpp -o fractal_tree.exe ^
    -std=c++17 -O2 -Wall ^
    -I"%INC%" -L"%LIB%" ^
    -lfreeglut -lopengl32 -lglu32

if errorlevel 1 (
    echo.
    echo Build FAILED.
    exit /b 1
)

echo.
echo Build succeeded!  fractal_tree.exe is ready.
echo Run it with:
echo   set PATH=C:\msys64\mingw64\bin;%%PATH%%
echo   fractal_tree.exe
