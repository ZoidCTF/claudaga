@echo off
rem Fetches the two dependencies build.bat expects but the repository does not
rem carry: the SDL2 development libraries and SDL2_mixer. Run it once after
rem cloning, from anywhere - it works out the project root from its own path.
rem
rem Both versions are pinned. They ship prebuilt .lib and .dll files, so an
rem unplanned upgrade is a change to what actually links.
rem
rem Only SDL2_mixer.dll is taken from the mixer archive. The optional decoder
rem DLLs beside it - opus, wavpack, xmp, gme - are for formats this game does
rem not use; Vorbis and MP3 are decoded by code built into SDL2_mixer itself.

setlocal
set "ROOT=%~dp0.."
set "TP=%ROOT%\third_party"
set "SDL_VER=2.32.10"
set "MIX_VER=2.8.2"

rem Call curl and tar by absolute path. Both names exist in MSYS as well, and
rem the MSYS tar is GNU tar, which cannot open a zip at all - so a run from a
rem bash shell would pick the wrong one and fail in a confusing way.
set "CURL=%SystemRoot%\System32\curl.exe"
set "TAR=%SystemRoot%\System32\tar.exe"

if not exist "%CURL%" (
    echo [deps] ERROR: %CURL% not found - Windows 10 1803 or later is expected.
    exit /b 1
)
if not exist "%TAR%" (
    echo [deps] ERROR: %TAR% not found - Windows 10 1803 or later is expected.
    exit /b 1
)

if not exist "%TP%" mkdir "%TP%"

rem --- SDL2 --------------------------------------------------------------
if exist "%TP%\SDL2\include\SDL.h" (
    echo [deps] SDL2 already present
    goto :mixer
)

echo [deps] fetching SDL2 %SDL_VER% ^(about 7 MB^)
"%CURL%" -fsSL -o "%TP%\sdl2.zip" ^
    https://github.com/libsdl-org/SDL/releases/download/release-%SDL_VER%/SDL2-devel-%SDL_VER%-VC.zip
if errorlevel 1 (
    echo [deps] ERROR: could not download SDL2
    exit /b 1
)

"%TAR%" -xf "%TP%\sdl2.zip" -C "%TP%"
if errorlevel 1 (
    echo [deps] ERROR: could not unpack SDL2
    del "%TP%\sdl2.zip" >nul 2>&1
    exit /b 1
)

rem The archive unpacks to a versioned folder; build.bat looks for third_party\SDL2.
if exist "%TP%\SDL2" rmdir /s /q "%TP%\SDL2"
move "%TP%\SDL2-%SDL_VER%" "%TP%\SDL2" >nul
if errorlevel 1 (
    echo [deps] ERROR: unexpected archive layout, no SDL2-%SDL_VER% folder
    exit /b 1
)
del "%TP%\sdl2.zip" >nul 2>&1

rem --- SDL2_mixer --------------------------------------------------------
:mixer
if exist "%TP%\SDL2_mixer\include\SDL_mixer.h" (
    echo [deps] SDL2_mixer already present
    goto :done
)

echo [deps] fetching SDL2_mixer %MIX_VER% ^(about 4 MB^)
"%CURL%" -fsSL -o "%TP%\mixer.zip" ^
    https://github.com/libsdl-org/SDL_mixer/releases/download/release-%MIX_VER%/SDL2_mixer-devel-%MIX_VER%-VC.zip
if errorlevel 1 (
    echo [deps] ERROR: could not download SDL2_mixer
    exit /b 1
)

"%TAR%" -xf "%TP%\mixer.zip" -C "%TP%"
if errorlevel 1 (
    echo [deps] ERROR: could not unpack SDL2_mixer
    del "%TP%\mixer.zip" >nul 2>&1
    exit /b 1
)

if exist "%TP%\SDL2_mixer" rmdir /s /q "%TP%\SDL2_mixer"
move "%TP%\SDL2_mixer-%MIX_VER%" "%TP%\SDL2_mixer" >nul
if errorlevel 1 (
    echo [deps] ERROR: unexpected archive layout, no SDL2_mixer-%MIX_VER% folder
    exit /b 1
)
del "%TP%\mixer.zip" >nul 2>&1

:done
echo [deps] OK - now run build.bat
exit /b 0
