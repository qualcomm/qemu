#! /bin/bash


#export PKG_CONFIG_PATH=/usr/lib/i386-linux-gnu/pkgconfig
export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu

rm -rf build_i686
mkdir build_i686
cd build_i686
echo Configure ...
../configure \
    --cpu=i386 \
    --disable-fdt \
    --disable-capstone \
    --disable-tools \
    --disable-guest-agent \
    --disable-slirp \
    --disable-docs \
    --disable-modules \
    --disable-plugins \
    --target-list=hexagon-linux-user \
    > CONFIG.LOG 2>&1
echo result $?
echo Build ...
make -j > BUILD.LOG 2>&1
echo result $?
echo Test ...
    make check-tcg \
    > TEST.LOG 2>&1
echo result $?
