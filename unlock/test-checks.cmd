@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
rem GU_CLANG / GU_VCVARS override the toolchain paths for other installs.
if not defined GU_CLANG set "GU_CLANG=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe"
if not defined GU_VCVARS set "GU_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%GU_VCVARS%" >nul
if errorlevel 1 exit /b 1
set "GU_CHECKS_SOURCE=checks.c"
set "GU_CHECKS_TEST=build\checks-tests.exe"
if /I "%~1"=="--red" (
    set "GU_CHECKS_SOURCE=build\checks-red.c"
    set "GU_CHECKS_TEST=build\checks-red.exe"
) else if not "%~1"=="" (
    echo usage: test-checks.cmd [--red]
    exit /b 2
)
"%GU_CLANG%" -std=c11 -O2 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -I ..\deps\edk2\MdePkg\Include -I ..\deps\edk2\MdePkg\Include\X64 tests\test_checks.c "%GU_CHECKS_SOURCE%" -o "%GU_CHECKS_TEST%"
if errorlevel 1 exit /b 1
"%GU_CHECKS_TEST%" "..\evidence\boot-03\REPORT.TXT"
if errorlevel 1 exit /b 1
if /I "%~1"=="--red" exit /b 0
"%GU_CLANG%" -target x86_64-pc-windows-msvc -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fshort-wchar -mno-red-zone -Wall -Wextra -Werror -I ..\deps\edk2\MdePkg\Include -I ..\deps\edk2\MdePkg\Include\X64 -c checks.c -o build\checks-freestanding.obj
if errorlevel 1 exit /b 1
echo PASS freestanding x86_64-pc-windows-msvc compilation
exit /b 0
