#!/usr/bin/env sh

pacman --noconfirm -S \
    bison flex glib2-devel make mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-diffutils mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-glib2 mingw-w64-ucrt-x86_64-meson \
    mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkg-config \
    mingw-w64-ucrt-x86_64-python3
