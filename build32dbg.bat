@echo off
REM Debug 32-bit build with /DSHIM_LOG (logs SDK calls to chroma-stub\gamesync-shim.log).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cd /d %~dp0
if not exist dist mkdir dist
rc /nologo /fo dist\rzchroma32.res src\rzchroma.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
cl /nologo /O2 /MT /LD /TC /DSHIM_LOG src\rzchroma.c /Fe:dist\RzChromaSDK.dll /Fo:dist\rzchroma32dbg.obj /link /DEF:src\rzchroma.def dist\rzchroma32.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\RzChromaSDK.dll dist\RzChromatic.dll >nul
echo BUILD32DBG_OK
