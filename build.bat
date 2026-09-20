@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================
rem  MD2WeChat build script (Windows / MSVC)
rem
rem  Usage:      build.bat
rem  Output:     bin\md2wechat.exe
rem
rem  Everything is auto-detected. You can override with env vars:
rem     QTDIR     Qt 5.15 (MSVC build) directory
rem     VCTOOLS   ...\VC\Tools\MSVC\<version>
rem     SDKDIR    ...\Windows Kits\10
rem     SDKVER    e.g. 10.0.26100.0
rem
rem  Tip: running this from a "x64 Native Tools Command Prompt
rem       for VS 2022" also works - cl.exe is then already on PATH.
rem ============================================================

rem ---------- 1) locate Qt ----------
rem QMAKE = full path to qmake.exe, QTDIR = the Qt prefix that contains bin\
if not defined QTDIR if not defined QMAKE (
    for /f "delims=" %%i in ('where qmake.exe 2^>nul') do (
        if not defined QMAKE set "QMAKE=%%~fi"
    )
)
if defined QMAKE if not exist "!QMAKE!" set "QMAKE="
if not defined QMAKE if defined QTDIR set "QMAKE=!QTDIR!\bin\qmake.exe"

if not defined QMAKE (
    echo [error] qmake.exe not found on PATH.
    echo         Set QTDIR to your Qt ^(MSVC build^) directory and retry, e.g.
    echo         set QTDIR=D:\Qt\5.15.2\msvc2019_64
    exit /b 1
)
if not exist "!QMAKE!" (
    echo [error] qmake not found: "!QMAKE!"
    exit /b 1
)

rem Qt prefix = the folder that contains bin\  (works for standard Qt and conda)
for %%i in ("!QMAKE!") do set "QTDIR=%%~dpi"
if "!QTDIR:~-1!"=="\" set "QTDIR=!QTDIR:~0,-1!"
if /i "!QTDIR:~-4!"=="\bin" set "QTDIR=!QTDIR:~0,-4!"

rem ---------- 2) locate MSVC + Windows SDK ----------
where cl >nul 2>nul
if not errorlevel 1 goto :toolchain

if not defined VCTOOLS call :find_msvc_root "!ProgramFiles(x86)!\Microsoft Visual Studio"
if not defined VCTOOLS call :find_msvc_root "!ProgramFiles!\Microsoft Visual Studio"

if not defined SDKDIR   set "SDKDIR=!ProgramFiles(x86)!\Windows Kits\10"
if not defined SDKVER   call :find_sdkver

if not defined VCTOOLS goto :no_tools
if not defined SDKVER  goto :no_tools
if not exist "!SDKDIR!\Include\!SDKVER!\ucrt\stddef.h" goto :no_tools

set "PATH=!VCTOOLS!\bin\Hostx64\x64;!SDKDIR!\bin\!SDKVER!\x64;!QTDIR!\bin;%PATH%"
set "INCLUDE=!VCTOOLS!\include;!SDKDIR!\Include\!SDKVER!\ucrt;!SDKDIR!\Include\!SDKVER!\shared;!SDKDIR!\Include\!SDKVER!\um;!SDKDIR!\Include\!SDKVER!\winrt"
set "LIB=!VCTOOLS!\lib\x64;!SDKDIR!\Lib\!SDKVER!\ucrt\x64;!SDKDIR!\Lib\!SDKVER!\um\x64"
goto :toolchain

:toolchain
set "PATH=!QTDIR!\bin;%PATH%"

cd /d "%~dp0"
if not exist build mkdir build
cd build

echo [1/3] qmake ...
"!QTDIR!\bin\qmake.exe" ..\md2wechat.pro CONFIG+=release
if errorlevel 1 goto :fail

echo [2/3] nmake ...
nmake /NOLOGO release
if errorlevel 1 goto :fail

echo [3/3] Done.  Output: %~dp0bin\md2wechat.exe
endlocal
exit /b 0

:no_tools
echo [error] MSVC or Windows SDK not found.
echo         Run this script from a "x64 Native Tools Command Prompt for VS 2022",
echo         or install Visual Studio Build Tools with the C++ workload,
echo         or set VCTOOLS / SDKDIR / SDKVER manually.
endlocal
exit /b 1

:fail
echo.
echo BUILD FAILED
endlocal
exit /b 1

rem ---------- helpers ----------

rem Find the newest MSVC under a given Visual Studio root.
:find_msvc_root
if defined VCTOOLS exit /b 0
if not exist "%~1" exit /b 0
for /f "delims=" %%y in ('dir /b /ad-h "%~1" 2^>nul') do (
    if not defined VCTOOLS for /f "delims=" %%e in ('dir /b /ad-h "%~1\%%y" 2^>nul') do (
        if exist "%~1\%%y\%%e\VC\Tools\MSVC" (
            for /f "delims=" %%v in ('dir /b /ad-h /o-n "%~1\%%y\%%e\VC\Tools\MSVC" 2^>nul') do (
                if not defined VCTOOLS set "VCTOOLS=%~1\%%y\%%e\VC\Tools\MSVC\%%v"
            )
        )
    )
)
exit /b 0

rem Find the newest Windows SDK version.
:find_sdkver
if defined SDKVER exit /b 0
if not defined SDKDIR set "SDKDIR=!ProgramFiles(x86)!\Windows Kits\10"
for /f "delims=" %%i in ('dir /b /ad-h /o-n "!SDKDIR!\Include" 2^>nul') do (
    if not defined SDKVER set "SDKVER=%%i"
)
exit /b 0
