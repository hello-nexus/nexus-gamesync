@echo off
REM 32-bit build for 32-bit Chroma games (loaded from SysWOW64). Produces
REM RzChromaSDK.dll + RzChromatic.dll (x86). Pairs with build.bat (x64).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cd /d %~dp0
if not exist dist mkdir dist
rc /nologo /fo dist\rzchroma32.res src\rzchroma.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
cl /nologo /O2 /MT /LD /TC src\rzchroma.c /Fe:dist\RzChromaSDK.dll /Fo:dist\rzchroma32.obj /link /DEF:src\rzchroma.def dist\rzchroma32.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\RzChromaSDK.dll dist\RzChromatic.dll >nul
echo BUILD32_OK
