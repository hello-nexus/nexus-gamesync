@echo off
REM Debug 32-bit Chroma build with /DSHIM_LOG (logs SDK calls to
REM chroma-stub\gamesync-shim.log). Links forward.c for the shared logger + worker.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cd /d %~dp0
if not exist dist\x86 mkdir dist\x86
rc /nologo /fo dist\x86\rzchroma.res src\rzchroma.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
cl /nologo /O2 /MT /LD /TC /DSHIM_LOG src\rzchroma.c src\forward.c /Fe:dist\x86\RzChromaSDK.dll /Fo:dist\x86\ /link /DEF:src\rzchroma.def dist\x86\rzchroma.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\x86\RzChromaSDK.dll dist\x86\RzChromatic.dll >nul
echo BUILD32DBG_OK
