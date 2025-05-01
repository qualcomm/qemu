Currently there's no way of running automated tests in CI for the QEMU aarch64
build. This is because of two limitations:

1. There is no aarch64 hardware available for CI to run Windows
2. Gitlab runner is not available for aarch64

Therefore tests need to be run manually, which can be done by doing the
following:

1. Download QEMU Hexagon tarball from CI
2. Download tests either from QEMU itself (e.g. standalone_hw) or from
   qemu-hexagon-benchmarks (e.g. coremark, gzip)
3. Transfer QEMU Hexagon tarball and selected tests to a Windows aarch64 machine
4. Run the script for installing QEMU Hexagon aarch64 dependecies
5. Run the selected tests
