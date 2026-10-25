@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
rem GU_CLANG / GU_VCVARS override the toolchain paths for other installs.
if not defined GU_CLANG set "GU_CLANG=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe"
if not defined GU_VCVARS set "GU_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%GU_VCVARS%" >nul
if errorlevel 1 exit /b 1
set "PORT_FLAGS=-std=c11 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -I ..\deps\edk2\MdePkg\Include -I ..\deps\edk2\MdePkg\Include\X64"
rem Default profile (2206 on the validated host), then each other profile.
"%GU_CLANG%" %PORT_FLAGS% tests\test_port.c port.c -o build\test-port.exe
if errorlevel 1 exit /b 1
build\test-port.exe
if errorlevel 1 exit /b 1
"%GU_CLANG%" %PORT_FLAGS% -DGU_BOARD_220A tests\test_port.c port.c -o build\test-port-220a.exe
if errorlevel 1 exit /b 1
build\test-port-220a.exe
if errorlevel 1 exit /b 1
rem Confirmed RTX 3060 profile on its confirmed host: the write-capable path.
"%GU_CLANG%" %PORT_FLAGS% -DGU_BOARD_GA106_2504 -DGU_HOST_AMD_BUS5 tests\test_port.c port.c -o build\test-port-3060.exe
if errorlevel 1 exit /b 1
build\test-port-3060.exe
if errorlevel 1 exit /b 1
rem Unconfirmed board (test fixture) on an unconfirmed host.
set "DISCOVERY=-DGU_BOARD_UNCONFIRMED -DGU_HOST_DISCOVERY"
"%GU_CLANG%" %PORT_FLAGS% %DISCOVERY% -DGU_PREFLIGHT tests\test_port.c port.c -o build\test-port-unconfirmed.exe
if errorlevel 1 exit /b 1
build\test-port-unconfirmed.exe
if errorlevel 1 exit /b 1
rem An unconfirmed board or host must not compile into a write-capable image.
"%GU_CLANG%" %PORT_FLAGS% -DGU_BOARD_UNCONFIRMED -fsyntax-only port.c >build\refuse-board.log 2>&1
if not errorlevel 1 (echo FAIL: unconfirmed board compiled without GU_PREFLIGHT& exit /b 1)
findstr /c:"board profile is not confirmed" build\refuse-board.log >nul
if errorlevel 1 (echo FAIL: unconfirmed board refused for another reason& type build\refuse-board.log& exit /b 1)
"%GU_CLANG%" %PORT_FLAGS% -DGU_HOST_DISCOVERY -fsyntax-only port.c >build\refuse-host.log 2>&1
if not errorlevel 1 (echo FAIL: unconfirmed host compiled without GU_PREFLIGHT& exit /b 1)
findstr /c:"host profile is not confirmed" build\refuse-host.log >nul
if errorlevel 1 (echo FAIL: unconfirmed host refused for another reason& type build\refuse-host.log& exit /b 1)
"%GU_CLANG%" %PORT_FLAGS% -DGU_BOARD_220A -DGU_TARGET_PCI_ID=0x220610deU -fsyntax-only port.c >build\refuse-target.log 2>&1
if not errorlevel 1 (echo FAIL: mismatched GU_TARGET_PCI_ID compiled& exit /b 1)
findstr /c:"name different boards" build\refuse-target.log >nul
if errorlevel 1 (echo FAIL: target mismatch refused for another reason& type build\refuse-target.log& exit /b 1)
"%GU_CLANG%" %PORT_FLAGS% -DGU_BOARD_220A -DGU_BOARD_UNCONFIRMED -DGU_PREFLIGHT -fsyntax-only port.c >build\refuse-two.log 2>&1
if not errorlevel 1 (echo FAIL: two board profiles compiled& exit /b 1)
findstr /c:"exactly one GU_BOARD" build\refuse-two.log >nul
if errorlevel 1 (echo FAIL: two boards refused for another reason& type build\refuse-two.log& exit /b 1)
echo PASS profile refusals: unconfirmed board, unconfirmed host, mismatched target, two boards
call "%~dp0test-master-attributes.cmd"
exit /b %errorlevel%
