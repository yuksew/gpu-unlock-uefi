@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
rem GU_CLANG / GU_VCVARS override the toolchain paths for other installs.
if not defined GU_CLANG set "GU_CLANG=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\Llvm\x64\bin\clang.exe"
if not defined GU_VCVARS set "GU_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%GU_VCVARS%" >nul
if errorlevel 1 exit /b 1
set "CC=%GU_CLANG%"
set "FLAGS=-std=c11 -O1 -Wall -Wextra -Werror -D_CRT_SECURE_NO_WARNINGS -fno-builtin"
if "%~1"=="red" (
  "%CC%" %FLAGS% -DGU_EMPTY_RUNTIME tests\test_runtime.c -o build\runtime-red.exe
  if errorlevel 1 exit /b 1
  build\runtime-red.exe
  exit /b 1
)
"%CC%" %FLAGS% -Dmemcpy=GuTestMemcpy -Dmemset=GuTestMemset -Dmemchr=GuTestMemchr -Dmemcmp=GuTestMemcmp -Dstrcmp=GuTestStrcmp -Dstrlen=GuTestStrlen -c runtime.c -o build\runtime-host.obj
if errorlevel 1 exit /b 1
"%CC%" %FLAGS% tests\test_runtime.c format.c build\runtime-host.obj -o build\runtime-tests.exe
if errorlevel 1 exit /b 1
build\runtime-tests.exe
exit /b %errorlevel%
