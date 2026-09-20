@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================
rem  Build a portable release package (no Qt installation needed)
rem
rem  Usage:   release.bat
rem  Output:  release\md2wechat-<ver>-win64\
rem           release\md2wechat-<ver>-win64.zip
rem
rem  Steps: build -> collect the Qt runtime with windeployqt -> zip
rem ============================================================

set "APPVER=1.0.0"

rem ---------- 1) build ----------
call "%~dp0build.bat"
if errorlevel 1 exit /b 1

if not exist "%~dp0bin\md2wechat.exe" (
    echo [error] bin\md2wechat.exe not found after build.
    exit /b 1
)

rem ---------- 2) locate Qt (for windeployqt) ----------
if not defined QMAKE (
    for /f "delims=" %%i in ('where qmake.exe 2^>nul') do (
        if not defined QMAKE set "QMAKE=%%~fi"
    )
)
if not defined QMAKE (
    echo [error] qmake.exe not found, cannot run windeployqt.
    exit /b 1
)
for %%i in ("!QMAKE!") do set "QTBIN=%%~dpi"
if "!QTBIN:~-1!"=="\" set "QTBIN=!QTBIN:~0,-1!"

rem ---------- 3) collect files ----------
set "OUTDIR=%~dp0release\md2wechat-!APPVER!-win64"
if exist "!OUTDIR!" rmdir /s /q "!OUTDIR!"
mkdir "!OUTDIR!" >nul 2>nul

copy /y "%~dp0bin\md2wechat.exe" "!OUTDIR!\" >nul
copy /y "%~dp0README.md"         "!OUTDIR!\" >nul
copy /y "%~dp0LICENSE"           "!OUTDIR!\" >nul

echo [1/2] Collecting the Qt runtime ...
"!QTBIN!\windeployqt.exe" --release --compiler-runtime --no-system-d3d-compiler ^
    --dir "!OUTDIR!" "!OUTDIR!\md2wechat.exe"

rem ---- MSVC runtime (vcruntime / msvcp) ----
if not defined VCTOOLS call :find_msvc_root "!ProgramFiles(x86)!\Microsoft Visual Studio"
if not defined VCTOOLS call :find_msvc_root "!ProgramFiles!\Microsoft Visual Studio"
if defined VCTOOLS (
    for /f "delims=" %%r in ('dir /b /ad-h /o-n "!VCTOOLS!\redist\MSVC" 2^>nul') do (
        if not defined REDIST set "REDIST=!VCTOOLS!\redist\MSVC\%%r\x64"
    )
)
if defined REDIST if exist "!REDIST!\msvcp140.dll" (
    for %%f in (msvcp140.dll msvcp140_1.dll msvcp140_2.dll vcruntime140.dll vcruntime140_1.dll) do (
        if exist "!REDIST!\%%f" copy /y "!REDIST!\%%f" "!OUTDIR!\" >nul
    )
)

rem ---- QtWebEngine helper process + resources ----
if exist "!QTDIR!\bin\QtWebEngineProcess.exe" (
    copy /y "!QTDIR!\bin\QtWebEngineProcess.exe" "!OUTDIR!\" >nul 2>nul
    if exist "!QTDIR!\resources" xcopy /y /e /i "!QTDIR!\resources" "!OUTDIR!\resources" >nul
    if exist "!QTDIR!\translations\qtwebengine_locales" (
        xcopy /y /e /i "!QTDIR!\translations\qtwebengine_locales" "!OUTDIR!\translations\qtwebengine_locales" >nul
    )
)

rem ---------- 4) zip ----------
echo [2/2] Packing ...
set "ZIP=%~dp0release\md2wechat-!APPVER!-win64.zip"
if exist "!ZIP!" del "!ZIP!" >nul 2>nul
tar -a -c -f "!ZIP!" -C "%~dp0release" "md2wechat-!APPVER!-win64"
if errorlevel 1 (
    echo [warn] tar failed - zip it yourself: !OUTDIR!
) else (
    echo        !ZIP!
)

echo.
echo Done.  Folder: !OUTDIR!
endlocal
exit /b 0

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
