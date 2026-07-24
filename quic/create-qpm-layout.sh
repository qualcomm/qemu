#!/usr/bin/env sh

# The idea behind this script is to simulate the default directory layout
# required by qpm.

set -ex

readonly BUILD_DIR="${1}"

readonly QEMU_DIR="Tools/QEMUHexagon"
readonly COPROC_DIR="Tools/QEMUCoprocPlugin"

if test -z "${BUILD_DIR}"; then
  echo "Eror: BUILD_DIR must be defined"
  exit 1
fi

mkdir -p "${BUILD_DIR}"/"${QEMU_DIR}"/bin

mv "${BUILD_DIR}"/qemu-system-hexagon "${BUILD_DIR}"/"${QEMU_DIR}"/bin

ln -s "${QEMU_DIR}"/bin/qemu-system-hexagon "${BUILD_DIR}"
