@echo off
setlocal enabledelayedexpansion
set "ROOT=%~dp0"

rem %ProgramFiles(x86)% cannot be expanded inside a parenthesised block - the
rem ")" in the name closes the block early - so grab it up front.
set "PF86=%ProgramFiles(x86)%"

rem Enter the MSVC x64 environment, unless this shell is already in one.
if not defined VSCMD_ARG_TGT_ARCH call :setup_msvc
if errorlevel 1 exit /b 1

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"

set "SDL=%ROOT%third_party\SDL2"
if not exist "%SDL%\include\SDL.h" (
    echo [build] ERROR: SDL2 headers missing at %SDL%\include
    exit /b 1
)
if not exist "%ROOT%build" mkdir "%ROOT%build"

rem /W4 high warnings, /Zi debug info in a PDB, /wd4996 mutes CRT deprecations.
set "CFLAGS=/nologo /W4 /wd4996 /Zi /std:c17 /I"%SDL%\include" /I"%ROOT%third_party" /I"%ROOT%src""
if /i "%CONFIG%"=="release" (
    set "CFLAGS=!CFLAGS! /O2 /DNDEBUG"
    set "SUBSYS=WINDOWS"
) else (
    set "CFLAGS=!CFLAGS! /Od /RTC1 /D_DEBUG"
    set "SUBSYS=CONSOLE"
)

set "LIBS="%SDL%\lib\x64\SDL2.lib" "%SDL%\lib\x64\SDL2main.lib" shell32.lib"

echo [build] configuration: %CONFIG%

rem The compiler output goes to a log as well as the console so the tail of the
rem build can report a warning count. Warnings scroll past too easily otherwise,
rem and a build that says only "OK" reads as clean when it is not. Redirecting
rem rather than piping keeps cl's exit code intact.
set "LOG=%ROOT%build\build.log"
cl !CFLAGS! /Fo"%ROOT%build\\" /Fd"%ROOT%build\galaga.pdb" "%ROOT%src\*.c" /link /DEBUG /SUBSYSTEM:!SUBSYS! /OUT:"%ROOT%build\galaga.exe" !LIBS! > "%LOG%" 2>&1
set CLERR=%errorlevel%
type "%LOG%"

rem Counted by iterating the matches rather than piping into find: when this
rem script is launched from a bash shell, MSYS's find shadows the Windows one
rem on PATH, reads "/c /v" as directories to search, and walks the whole of C:.
set WARNS=0
for /f "delims=" %%c in ('findstr /c:" warning " "%LOG%"') do set /a WARNS+=1

if not "%CLERR%"=="0" (
    echo [build] FAILED
    exit /b 1
)

rem Stage the runtime DLL next to the exe.
if not exist "%ROOT%build\SDL2.dll" copy /y "%SDL%\lib\x64\SDL2.dll" "%ROOT%build\" >nul

if not "%WARNS%"=="0" (
    echo [build] OK with %WARNS% warning^(s^) -^> build\galaga.exe
) else (
    echo [build] OK -^> build\galaga.exe
)
exit /b 0

:setup_msvc
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [build] ERROR: vswhere.exe not found - is Visual Studio installed?
    exit /b 1
)
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo [build] ERROR: no VS installation with the C/C++ toolset was found.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b %errorlevel%
