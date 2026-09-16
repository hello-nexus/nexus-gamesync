@echo off
REM x86 build for all three Game Sync capture shims. 32-bit games load these from
REM SysWOW64. Each links forward.c (the shared forwarding worker). x86 DLLs go in
REM dist\x86\ because LightFX.dll / LogitechLedEnginesWrapper.dll share their names
REM across arches (the x64 copies in dist\ would otherwise be clobbered). Outputs:
REM   dist\x86\RzChromaSDK.dll (+ RzChromatic.dll copy)              - Razer Chroma
REM   dist\x86\LightFX.dll                                            - Alienware/Dell LightFX
REM   dist\x86\LogitechLedEnginesWrapper.dll (+ LogitechLed.dll copy) - Logitech
setlocal
REM MSVC lives at the Build Tools path on the lab PCs and under a full Visual
REM Studio on CI runners; vswhere finds either.
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" ( echo NO_MSVC & exit /b 1 )
call "%VCVARS%" x86 >nul
cd /d %~dp0
if not exist dist\x86 mkdir dist\x86

rc /nologo /fo dist\x86\rzchroma.res src\rzchroma.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
rc /nologo /fo dist\x86\lightfx.res src\lightfx.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
rc /nologo /fo dist\x86\logiled.res src\logiled.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )

cl /nologo /O2 /MT /LD /TC src\rzchroma.c src\forward.c /Fe:dist\x86\RzChromaSDK.dll /Fo:dist\x86\ /link /DEF:src\rzchroma.def dist\x86\rzchroma.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\x86\RzChromaSDK.dll dist\x86\RzChromatic.dll >nul

cl /nologo /O2 /MT /LD /TC src\lightfx.c src\forward.c /Fe:dist\x86\LightFX.dll /Fo:dist\x86\ /link /DEF:src\lightfx.def dist\x86\lightfx.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )

cl /nologo /O2 /MT /LD /TC src\logiled.c src\forward.c /Fe:dist\x86\LogitechLedEnginesWrapper.dll /Fo:dist\x86\ /link /DEF:src\logiled.def dist\x86\logiled.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\x86\LogitechLedEnginesWrapper.dll dist\x86\LogitechLed.dll >nul

echo BUILD32_OK
