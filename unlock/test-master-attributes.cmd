@echo off
setlocal
cd /d "%~dp0"
rem GU_CLANG / GU_VCVARS override the toolchain paths, as in test-port.cmd.
if not defined GU_CLANG set "GU_CLANG=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe"
if not defined GU_VCVARS set "GU_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%GU_VCVARS%" >nul
if errorlevel 1 exit /b 1
"%GU_CLANG%" -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -D_CRT_SECURE_NO_WARNINGS -D__KERNEL__ -DGU_UEFI_PORT -I compat -I ..\deps\edk2\MdePkg\Include -I ..\deps\edk2\MdePkg\Include\X64 tests\test_master_attributes.c port.c compat.c vendor\unlock_610.c vendor\prep_610.c vendor\layout_610.c vendor\vbios_610.c vendor\gfw_eval.c -o build\master-attributes.exe
if errorlevel 1 exit /b 1
build\master-attributes.exe
exit /b %errorlevel%
