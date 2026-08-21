#!/usr/bin/env python3
#
# Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from glob import glob
from qemu_test import LinuxKernelTest, Asset
from qemu_test import exec_command_and_wait_for_pattern
from contextlib import contextmanager
from tempfile import NamedTemporaryFile

class HexagonVirtioTest(LinuxKernelTest):
    '''
    The virtio-devices project includes a baremetal test case for hexagon.
    This test boots up the guest, inspects the device tree and tests the
    virtio devices which are present.

    Console output is emitted on a PL011 UART which it expects to find at a
    fixed address.
    '''
    PKG_RGY = 'https://gitlab.qualcomm.com/rust-hexagon/virtio-devices/'
    REL_REF = 3771
    HOST_PORT_FWDED = 5555
    ASSET_TARBALL = \
        Asset(f'{PKG_RGY}/-/package_files/{REL_REF}/download',
            '793cfbdec91ffea46996cd396771261254714ae382690e963c36d96ff4a6891f')

    def test_virtio_devs(self):
        '''This test is designed to reproduce the self-test contained in the
        virtio-devices project.
        '''
        self.set_machine('virt')
        self.require_netdev('user')

        self.archive_extract(self.ASSET_TARBALL, format='tar')
        self.vm.set_console()
        with NamedTemporaryFile(mode="wb", suffix=".img") as f:
            buf = bytearray(512 * 32)
            f.write(buf)
            f.flush()

            self.vm.add_args(
                '-kernel', self.scratch_file('bin', 'virtio-devices-hexagon'),
                '-m', '4G',
                '-accel', 'tcg,thread=multi',
                '-drive', f'if=none,format=raw,file={f.name},id=hd0',
                '-device', 'virtio-blk-device,drive=hd0',
                '-device', 'virtio-rng-device',
                '-netdev',
                f'type=user,id=net0,hostfwd=tcp::{self.HOST_PORT_FWDED}-:5555',
                '-device', 'virtio-net-device,netdev=net0',
            )
            self.vm.launch()

            self.wait_for_console_pattern("Loading FDT")
            self.wait_for_console_pattern("Loaded FDT")
            self.wait_for_console_pattern("virtio-blk test finished")
            self.wait_for_console_pattern("virtio-rng test finished")
            with conn('localhost', self.HOST_PORT_FWDED) as c:
                c.sendall(b'hello')
                self.wait_for_console_pattern("virtio-net test finished")

            # Wait for VM to shut down gracefully
            self.vm.wait()

@contextmanager
def conn(host: str, port: int):
    import socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
        yield sock
    finally:
        sock.shutdown(socket.SHUT_RDWR)
        sock.close()

if __name__ == '__main__':
    LinuxKernelTest.main()
