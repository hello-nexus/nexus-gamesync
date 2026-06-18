@echo off
REM x64 loadtest. Run from a dir where LightFX.dll + LogitechLedEnginesWrapper.dll
REM (x64) are resolvable (copy dist\ DLLs next to loadtest.exe, or install them).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /MT loadtest.c /Fe:loadtest.exe
