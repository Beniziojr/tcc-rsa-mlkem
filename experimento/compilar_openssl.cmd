@echo off
setlocal
set "TASK_ROOT=%~dp0"
set "TASK_DEP=%TASK_ROOT%dependencias\"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > "%TASK_ROOT%logs\ambiente_openssl.log" 2>&1
if errorlevel 1 exit /b 1
set "PATH=%TASK_DEP%perl\perl\bin;%TASK_DEP%perl\c\bin;%TASK_DEP%nasm-2.16.03;%PATH%"
set "LC_ALL=C"
set "LANG=C"
cd /d "%TASK_DEP%openssl-3.5.5"
"%TASK_DEP%perl\perl\bin\perl.exe" Configure VC-WIN64A shared --prefix="%TASK_DEP%openssl-msvc-3.5.5" --openssldir="%TASK_DEP%openssl-msvc-3.5.5\ssl" > "%TASK_ROOT%logs\openssl-configure.log" 2>&1
if errorlevel 1 exit /b 1
nmake > "%TASK_ROOT%logs\openssl-build.log" 2>&1
if errorlevel 1 exit /b 1
nmake install_sw > "%TASK_ROOT%logs\openssl-install.log" 2>&1
exit /b %errorlevel%
