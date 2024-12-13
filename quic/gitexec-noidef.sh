#!/bin/bash

PR="pull-hex-$(date +%Y%m%d)"
flavor=noidef
rm -rf test_${flavor}_${PR}
mkdir test_${flavor}_${PR}
cd test_${flavor}_${PR}
echo Configure ...
../configure \
    --disable-hexagon-idef-parser \
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
echo -e "\tresult $?"
echo Build ...
make -j > BUILD.LOG 2>&1 || tail BUILD.LOG
build_res=$?
echo -e "\tresult ${build_res}"
if [[ ${build_res} -ne 0 ]]; then
    mv BUILD.LOG ../build_fail_${flavor}_$(git rev-parse HEAD).log
fi
echo Test ...
make check check-tcg \
    > TEST.LOG 2>&1
test_res=$?
echo -e "\tresult ${test_res}"
if [[ ${test_res} -ne 0 ]]; then
    mv CONFIG.LOG ../config_pass_${flavor}_$(git rev-parse HEAD).log
    mv BUILD.LOG ../build_pass_${flavor}_$(git rev-parse HEAD).log
    mv TEST.LOG ../test_fail_${flavor}_$(git rev-parse HEAD).log
fi
