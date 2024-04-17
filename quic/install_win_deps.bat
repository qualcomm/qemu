@echo off

set wiki=https://github.com/quic/toolchain_for_hexagon/wiki/QEMU-Hexagon-on-Windows-x86%%E2%%80%%9064#troubleshooting
set green=[32m
set red=[31m
set creset=[0m

echo Installing mingw libs in msys2...
echo:

C:\\msys64\\usr\\bin\\env MSYSTEM=MSYS /usr/bin/bash -lc echo
IF ERRORLEVEL 1 (
    echo %red%ERROR: failed to find msys2. Please install it from https://www.msys2.org/ and run this script again.%creset%
    exit /b
)

C:\\msys64\\usr\\bin\\env MSYSTEM=MSYS /usr/bin/bash -lc "pacman -Syy --noconfirm && pacman --noconfirm --needed -S mingw-w64-x86_64-libwinpthread-git mingw-w64-x86_64-glib2 mingw-w64-x86_64-pixman mingw-w64-x86_64-libpng mingw-w64-x86_64-gettext mingw-w64-x86_64-pcre2 mingw-w64-x86_64-libiconv"
IF ERRORLEVEL 1 (
    echo %red%ERROR: failed to install mingw libs in msys2. See troubleshooting at %wiki%.%creset%
    exit /b
)

echo:
echo Installing pathman and updating PATH register with mingw dirs...
echo:

:: pathman is a safer way to edit the user's PATH on windows without breaking it
curl.exe -sA "MS" https://webinstall.dev/pathman | powershell >NUL 2>&1
IF ERRORLEVEL 1 (
    echo %red%ERROR: failed to install pathman. See https://webinstall.dev/pathman to install it manually.%creset%
    exit /b
)

:: Must invoke a separate cmd process as order for it to see pathman
cmd /c "pathman add C:\msys64\mingw64\bin"
IF ERRORLEVEL 1 (
    echo %red%ERROR: failed to add mingw64 dir to PATH register. See troubleshooting at %wiki%.%creset%
    exit /b
)
cmd /c "pathman add C:\msys64\mingw64\lib"
IF ERRORLEVEL 1 (
    echo %red%ERROR: failed to add mingw64 dir to PATH register. See troubleshooting at %wiki%.%creset%
    exit /b
)

echo:
echo %green%SUCCESS: all dependencies for qemu-system-hexagon.exe were installed%creset%
