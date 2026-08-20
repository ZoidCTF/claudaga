@echo off
rem Fetches the two dependencies build.bat expects but the repository does not
rem carry: the SDL2 development libraries and stb_image.h. Run it once after
rem cloning, from anywhere - it works out the project root from its own path.
rem
rem Both versions are pinned. SDL2 in particular ships prebuilt .lib and .dll
rem files, so an unplanned upgrade is a change to what actually links.

setlocal
set "ROOT=%~dp0.."
set "TP=%ROOT%\third_party"
set "SDL_VER=2.32.10"

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

rem --- stb_image.h -------------------------------------------------------
if exist "%TP%\stb_image.h" (
    echo [deps] stb_image.h already present
) else (
    echo [deps] fetching stb_image.h
    "%CURL%" -fsSL -o "%TP%\stb_image.h" ^
        https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    if errorlevel 1 (
        echo [deps] ERROR: could not download stb_image.h
        exit /b 1
    )
)

rem --- SDL2 --------------------------------------------------------------
if exist "%TP%\SDL2\include\SDL.h" (
    echo [deps] SDL2 already present
    goto :done
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

:done
echo [deps] OK - now run build.bat
exit /b 0
