@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
rem GU_CLANG / GU_VCVARS override the toolchain paths for other installs.
if not defined GU_CLANG set "GU_CLANG=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe"
if not defined GU_VCVARS set "GU_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%GU_VCVARS%" >nul
if errorlevel 1 exit /b 1
"%GU_CLANG%" -std=c11 -Wall -Wextra -Werror -I deps/edk2/MdePkg/Include -I deps/edk2/MdePkg/Include/X64 test_core.c probe_core.c probe_app.c -o build/test_core.exe
if errorlevel 1 exit /b 1
build\test_core.exe
exit /b %errorlevel%
