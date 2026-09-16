@echo off
REM x64 build for all three Game Sync capture shims. Each links forward.c (the
REM shared forwarding worker). 64-bit games load these from System32.
REM Outputs under dist\:
REM   RzChromaSDK64.dll (+ RzChromatic64.dll copy) - Razer Chroma
REM   LightFX.dll                                   - Alienware/Dell LightFX
REM   LogitechLedEnginesWrapper.dll (+ LogitechLed.dll copy) - Logitech
setlocal
REM MSVC lives at the Build Tools path on the lab PCs and under a full Visual
REM Studio on CI runners; vswhere finds either.
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" ( echo NO_MSVC & exit /b 1 )
call "%VCVARS%" x64 >nul
cd /d %~dp0
if not exist dist mkdir dist

rc /nologo /fo dist\rzchroma.res src\rzchroma.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
rc /nologo /fo dist\lightfx.res src\lightfx.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )
rc /nologo /fo dist\logiled.res src\logiled.rc
if errorlevel 1 ( echo RC_FAILED & exit /b 1 )

cl /nologo /O2 /MT /LD /TC src\rzchroma.c src\forward.c /Fe:dist\RzChromaSDK64.dll /Fo:dist\ /link /DEF:src\rzchroma.def dist\rzchroma.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\RzChromaSDK64.dll dist\RzChromatic64.dll >nul

cl /nologo /O2 /MT /LD /TC src\lightfx.c src\forward.c /Fe:dist\LightFX.dll /Fo:dist\ /link /DEF:src\lightfx.def dist\lightfx.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )

cl /nologo /O2 /MT /LD /TC src\logiled.c src\forward.c /Fe:dist\LogitechLedEnginesWrapper.dll /Fo:dist\ /link /DEF:src\logiled.def dist\logiled.res
if errorlevel 1 ( echo BUILD_FAILED & exit /b 1 )
copy /y dist\LogitechLedEnginesWrapper.dll dist\LogitechLed.dll >nul

echo BUILD_OK
