@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cd /d %~dp0
cl /nologo /MT modlist.c /Fe:modlist32.exe
