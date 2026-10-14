@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > "%~dp0logs\ambiente_benchmark.log" 2>&1
if errorlevel 1 exit /b 1
cd /d "%~dp0"
set "TASK_SSL=%~dp0dependencias\openssl-msvc-3.5.5"
cl /nologo /W4 /WX /O2 /MD /TC /I"%TASK_SSL%\include" benchmark.c /Fo:build\benchmark.obj /Fe:build\benchmark.exe /link /LIBPATH:"%TASK_SSL%\lib" libcrypto.lib > logs\benchmark-build.log 2>&1
exit /b %errorlevel%
