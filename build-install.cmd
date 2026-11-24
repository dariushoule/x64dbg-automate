@echo off
rem Configure, build (Release) and install the plugin for one bitness.
rem   build-install.cmd 32|64
rem Machine-specific settings come from local.cmd (gitignored; copy local.cmd.example)
rem or from environment variables of the same names:
rem   X64DBG_RELEASE_DIR  x64dbg release dir (holds x32\ and x64\); unset = build only, no install
rem   CMAKE_BIN           cmake.exe; default = VS-bundled cmake via vswhere, then cmake on PATH
rem   CMAKE_GENERATOR     default "Visual Studio 17 2022"
setlocal
cd /d "%~dp0"

if "%~1"=="64" goto arch64
if "%~1"=="32" goto arch32
echo usage: %~nx0 32^|64
exit /b 2
:arch64
set BUILD_DIR=build64& set ARCH=x64& set EXT=dp64& set SUBDIR=x64
goto configured
:arch32
set BUILD_DIR=build32& set ARCH=Win32& set EXT=dp32& set SUBDIR=x32
:configured

if exist "%~dp0local.cmd" call "%~dp0local.cmd"

if defined CMAKE_BIN goto have_cmake
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto cmake_from_path
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.CMake.Project -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) do set "CMAKE_BIN=%%i"
if defined CMAKE_BIN goto have_cmake
:cmake_from_path
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE_BIN set "CMAKE_BIN=%%i"
if defined CMAKE_BIN goto have_cmake
echo error: cmake not found. Install the VS "C++ CMake tools" component or set CMAKE_BIN in local.cmd.
exit /b 1
:have_cmake
if not defined CMAKE_GENERATOR set "CMAKE_GENERATOR=Visual Studio 17 2022"
echo Using cmake: %CMAKE_BIN%

"%CMAKE_BIN%" -B %BUILD_DIR% -G "%CMAKE_GENERATOR%" -A %ARCH% || exit /b 1
"%CMAKE_BIN%" --build %BUILD_DIR% --config Release || exit /b 1

if defined X64DBG_RELEASE_DIR goto install
echo X64DBG_RELEASE_DIR not set (see local.cmd.example): built %BUILD_DIR%\Release\x64dbg-automate.%EXT% but not installing.
exit /b 0
:install
set "INSTALL_DIR=%X64DBG_RELEASE_DIR%\%SUBDIR%\plugins"
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
copy /Y %BUILD_DIR%\Release\x64dbg-automate.%EXT% "%INSTALL_DIR%\" || exit /b 1
copy /Y %BUILD_DIR%\Release\libzmq-mt-4_3_5.dll "%INSTALL_DIR%\" || exit /b 1
echo Installed into %INSTALL_DIR%
