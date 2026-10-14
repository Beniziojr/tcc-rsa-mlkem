@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > "%~dp0logs\ambiente_openssl.log" 2>&1
if errorlevel 1 exit /b 1
set "PATH=%~dp0dependencias\perl\perl\bin;%~dp0dependencias\perl\c\bin;%~dp0dependencias\nasm-2.16.03;%PATH%"
set "LC_ALL=C"
set "LANG=C"
cd /d "%~dp0dependencias\openssl-3.5.5"
nmake > "%~dp0logs\openssl-build.log" 2>&1
if errorlevel 1 exit /b 1
nmake install_sw > "%~dp0logs\openssl-install.log" 2>&1
exit /b %errorlevel%
