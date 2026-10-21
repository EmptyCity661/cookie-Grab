@echo off
setlocal
chcp 65001 >nul
rem ============================================================
rem  cookie抓取工具3.0 build script (MinGW-w64 GCC, Win64)
rem  Output: cookie抓取工具3.0.exe (single file, WebView2Loader.dll embedded)
rem  Note: ld prints a harmless ".rsrc merge failure: multiple
rem        non-default manifests" warning - it drops the CRT's
rem        empty placeholder manifest and keeps ours.
rem  Note: g++ 的 -o 无法可靠创建中文名文件，故先编译为中间名 CG.exe，
rem        再由 finalize.ps1 重命名为中文名；中文资源编码亦由 finalize.ps1 处理。
rem ============================================================
set "MINGW=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64"
if exist "%MINGW%\bin\g++.exe" set "PATH=%MINGW%\bin;%PATH%"
cd /d %~dp0

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found. Install MinGW-w64 first:
    echo   winget install BrechtSanders.WinLibs.POSIX.UCRT
    exit /b 1
)

echo [1/3] windres res\app.rc
if not exist res mkdir res
copy /y include\WebView2Loader.dll res\WebView2Loader.dll >nul
powershell -NoProfile -ExecutionPolicy Bypass -File finalize.ps1 -Stage rc
windres --include-dir res --include-dir src res\_app_gbk.rc -O coff -o res\app.res
if errorlevel 1 exit /b 1

echo [2/3] g++ compile
g++ -std=c++17 -O2 -municode -mwindows -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Wall -o CG.exe src\main.cpp src\app.cpp src\theme.cpp src\webview.cpp src\shell.cpp src\capture.cpp src\logwriter.cpp src\settings.cpp src\util.cpp res\app.res -Iinclude -Isrc -lole32 -loleaut32 -luuid -lgdi32 -lcomctl32 -lshell32 -lcomdlg32 -ladvapi32 -ldwmapi -static -static-libgcc -static-libstdc++
if errorlevel 1 exit /b 1

echo [3/3] finalize name
powershell -NoProfile -ExecutionPolicy Bypass -File finalize.ps1 -Stage exe
