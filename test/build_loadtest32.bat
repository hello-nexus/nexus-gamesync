@echo off
REM x86 loadtest, to prove the 32-bit shims load + forward. Run from a dir where
REM the x86 LightFX.dll + LogitechLedEnginesWrapper.dll (dist\x86\) are resolvable.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cd /d %~dp0
cl /nologo /MT loadtest.c /Fe:loadtest32.exe
