@echo off

curl --output glib2.pkg.tar.zst "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-glib2-2.84.0-1-any.pkg.tar.zst"
tar --extract --verbose --file=glib2.pkg.tar.zst clangarm64/bin/libglib-2.0-0.dll

curl --output winpthread.pkg.tar.zst "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-libwinpthread-git-12.0.0.r619.g850703ae4-1-any.pkg.tar.zst"
tar --extract --verbose --file=winpthread.pkg.tar.zst clangarm64/bin/libwinpthread-1.dll

curl --output zlib.pkg.tar.zst "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-zlib-1.3.1-1-any.pkg.tar.zst"
tar --extract --verbose --file=zlib.pkg.tar.zst clangarm64/bin/zlib1.dll

curl --output gettext.pkg.tar.zst "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-gettext-0.22.4-3-any.pkg.tar.zst"
tar --extract --verbose --file=gettext.pkg.tar.zst clangarm64/bin/libintl-8.dll

curl --output iconv.pkg.tar.zst "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-libiconv-1.18-1-any.pkg.tar.zst"
tar --extract --verbose --file=iconv.pkg.tar.zst clangarm64/bin/libiconv-2.dll

curl --output pcre.pkg.tar.zst "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-pcre2-10.45-1-any.pkg.tar.zst"
tar --extract --verbose --file=pcre.pkg.tar.zst clangarm64/bin/libpcre2-8-0.dll

xcopy clangarm64\bin\* . /E /I /Y
rmdir /S /Q clangarm64
del *.zst
