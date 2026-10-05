@echo off
rem SPDX-License-Identifier: Apache-2.0
rem
rem Builds isoforge with the CMake presets, each inside the Visual Studio 2026 developer environment
rem of its architecture (x64 or x86). Works from cmd.exe, PowerShell and Git Bash
rem (cmd //c 'scripts\build.cmd').
rem
rem   scripts\build.cmd                  x64-debug, x64-release, x86-debug and x86-release
rem   scripts\build.cmd x64-release      only the given presets
rem   scripts\build.cmd --package        also create out\package\isoforge-<version>-win-x64.zip
rem                                      and isoforge-<version>-win-x86.zip
rem
rem Exits with 0 on success and 1 on any failure.
rem
rem Failures jump to a label outside the block instead of running "exit /b 1" inside a block or
rem after "||": cmd.exe loses that exit code when the script is started without "call" (e.g.
rem "cmd /c build.cmd").

setlocal

set "ROOT=%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo error: vswhere.exe was not found; install Visual Studio 2026. 1>&2
    goto fail
)
set "VSINSTALL="
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -version [18.0^,19.0^) -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
if not defined VSINSTALL (
    echo error: Visual Studio 2026 with the C++ x64/x86 tools was not found. 1>&2
    goto fail
)

rem VsDevCmd calls vswhere by name.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"

set "PRESETS="
set "PACKAGE="
:arguments
if "%~1"=="" goto run
if /i "%~1"=="--package" (
    set "PACKAGE=1"
) else (
    set "PRESETS=%PRESETS% %~1"
)
shift
goto arguments

:run
if not defined PRESETS set "PRESETS=x64-debug x64-release x86-debug x86-release"
for %%P in (%PRESETS%) do (
    call :preset %%P build
    if errorlevel 1 goto fail
)

if not defined PACKAGE goto success
for %%P in (x64-release x86-release) do (
    call :preset %%P package
    if errorlevel 1 goto fail
)

:success
endlocal
exit /b 0

:fail
endlocal
exit /b 1

rem Configures and builds preset %1 in the developer environment of its architecture, and creates its
rem zip when %2 is package. The environment is local to the call.
:preset
setlocal
set "NAME=%~1"
set "ARCH=amd64"
if /i "%NAME:~0,4%"=="x86-" set "ARCH=x86"
echo === %NAME% (%~2)
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=%ARCH% -host_arch=amd64 -no_logo
if errorlevel 1 goto preset_fail
cd /d "%ROOT%"
if errorlevel 1 goto preset_fail
cmake --preset %NAME%
if errorlevel 1 goto preset_fail
cmake --build --preset %NAME%
if errorlevel 1 goto preset_fail
if /i not "%~2"=="package" goto preset_done
cpack --preset %NAME%
if errorlevel 1 goto preset_fail
:preset_done
endlocal
exit /b 0
:preset_fail
endlocal
exit /b 1
