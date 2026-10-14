@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" > "%~dp0logs\ambiente_testes_openssl.log" 2>&1
if errorlevel 1 exit /b 1
set "PATH=%~dp0dependencias\perl\perl\bin;%~dp0dependencias\perl\c\bin;%~dp0dependencias\nasm-2.16.03;%PATH%"
set "LC_ALL=C"
set "LANG=C"
cd /d "%~dp0dependencias\openssl-3.5.5"
nmake test TESTS="test_internal_ml_kem test_evp_extra_ml_kem test_ml_kem_codecs test_rsa test_rsaoaep test_rand" > "%~dp0logs\openssl-testes.log" 2>&1
exit /b %errorlevel%
