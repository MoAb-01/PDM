@echo off
echo ===================================================
echo Compiling IDM Download Manager AB in Native C++20
echo ===================================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist build (
    mkdir build
)

cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

echo.
if exist Release\DownloadManagerAB.exe (
    copy /y "..\app_icon.ico" "Release\app_icon.ico" >nul 2>&1
    copy /y "..\app_icon.png" "Release\app_icon.png" >nul 2>&1
    echo ===================================================
    echo SUCCESS! Native C++ binaries compiled:
    echo  - build\Release\DownloadManagerAB.exe
    echo  - build\Release\IDMNativeHost.exe
    echo ===================================================
) else (
    echo Compilation failed.
)
cd ..
