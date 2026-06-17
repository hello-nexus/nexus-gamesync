@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
if not exist dist mkdir dist
rc /nologo /fo dist\rzchroma.res src\rzchroma.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
cl /nologo /O2 /MT /LD /TC src\rzchroma.c /Fe:dist\RzChromaSDK64.dll /Fo:dist\rzchroma.obj /link /DEF:src\rzchroma.def dist\rzchroma.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\RzChromaSDK64.dll dist\RzChromatic64.dll >nul
echo BUILD_OK
