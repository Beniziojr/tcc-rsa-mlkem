@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > "%~dp0logs\ambiente_validacao.log" 2>&1
if errorlevel 1 exit /b 1
cd /d "%~dp0"
set "TASK_SSL=%~dp0dependencias\openssl-msvc-3.5.5"
set "TASK_SSL_LIB=%TASK_SSL%\lib"
if not exist "%TASK_SSL%\bin\libcrypto-3-x64.dll" (
  set "TASK_SSL=%~dp0dependencias\openssl-3.5.5"
  set "TASK_SSL_LIB=%~dp0dependencias\openssl-3.5.5"
)
set "PATH=%TASK_SSL%;%TASK_SSL%\bin;%PATH%"
cl /nologo /W4 /WX /O2 /MD /TC /I"%TASK_SSL%\include" validacao_evp.c /Fo:build\validacao_evp.obj /Fe:build\validacao_evp.exe /link /LIBPATH:"%TASK_SSL_LIB%" libcrypto.lib > logs\validacao-build.log 2>&1
if errorlevel 1 exit /b 1
build\validacao_evp.exe > logs\validacao-evp.json 2> logs\validacao-evp-erros.log
exit /b %errorlevel%
