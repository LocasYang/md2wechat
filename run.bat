@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Launch MD2WeChat with the Qt runtime directory on PATH.
rem Set QTDIR if the exe cannot locate the Qt DLLs by itself.

if not defined QTDIR (
    for /f "delims=" %%i in ('where qmake.exe 2^>nul') do (
        if not defined QTDIR set "QTDIR=%%~dpi"
    )
)
if defined QTDIR if "!QTDIR:~-1!"=="\" set "QTDIR=!QTDIR:~0,-1!"
if /i "!QTDIR:~-4!"=="\bin" set "QTDIR=!QTDIR:~0,-4!"

if defined QTDIR set "PATH=!QTDIR!\bin;%PATH%"

start "" "%~dp0bin\md2wechat.exe" %*
endlocal
