@echo off
setlocal
cd /d "%~dp0"
rem GU_VCVARS / GU_NVCC override the toolchain paths for other installs.
if not defined GU_VCVARS set "GU_VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined GU_NVCC set "GU_NVCC=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\nvcc.exe"
call "%GU_VCVARS%" >nul
if errorlevel 1 exit /b 1
"%GU_NVCC%" -std=c++17 -O3 -arch=sm_86 -Xcompiler /utf-8 power-benchmark.cu -lcublas -lnvml -o power-benchmark.exe
exit /b %errorlevel%
