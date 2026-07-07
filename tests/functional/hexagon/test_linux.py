#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from qemu_test import LinuxKernelTest, Asset
from unittest import skipUnless

class HexagonLinuxDevsTest(LinuxKernelTest):
    GUEST_ENTRY = 0xa0000000

    REPO = 'https://gitlab.qualcomm.com/qqvp/testing/qemu-linux-tests'
    GIT_REF = 'buildroot-v0.4'
    ASSET_TARBALL = \
        Asset(f'{REPO}/-/archive/{GIT_REF}/qemu-linux-tests-{GIT_REF}.tar.gz',
              'fc4b79fe5bdf07bffefbf6d75e80febe27ea1d498971e33ff726e977a5410a89')

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_linux_devs(self):
        self.set_machine('virt')
        self.require_netdev('user')

        kernel_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/vmlinux.bin')
        booter_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/loadlinux')
        disk_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/disk.qcow2')
        self.vm.set_console()

        # Workaround: guest limitation -- linux kernel in use doesn't
        # consume the generated fdt.  So when we have more devices
        # available, we have to push extra ones in so that the devices
        # below appear at the expected address.
        for i in range(1, 7):
            self.vm.add_args(
                '-netdev', f'type=user,id=net{i}',
                '-device', f'virtio-net-device,netdev=net{i}',
            )
        self.vm.add_args(
            # The booter is the guest entry point; suppress the default
            # firmware so it boots directly rather than under loadlinux.
            '-bios', 'none',
            '-kernel', booter_path,
            '-device',
                f'loader,addr=0x{self.GUEST_ENTRY:08x},file={kernel_path}',
            '-m', '4G',
            '-accel', 'tcg,thread=multi',
            '-drive', f'if=none,file={disk_path},id=hd0',
            '-device', 'virtio-blk-device,drive=hd0',
            '-netdev', 'type=user,id=net0',
            '-device', 'virtio-net-device,netdev=net0',
        )
        self.vm.launch()

        # Verify SMP: all 4 CPUs brought up under MTTCG
        self.wait_for_console_pattern("Brought up 4 CPUs")

        self.wait_for_console_pattern(
            "clocksource: Switched to clocksource HVM timer")

        # Verify virtio-blk device: guest kernel detects and mounts disk
        self.wait_for_console_pattern("EXT2-fs (vda)")

        # Verify virtio-net device: network init scripts complete
        self.wait_for_console_pattern("Starting network: OK")

        # Verify full boot to shell prompt
        self.wait_for_console_pattern("bash-5.2#")

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_linux_bios_boot(self):
        """
        Test -bios boot path with loader device for kernel.

        Uses -bios to load the bootloader ELF (loadlinux) as firmware,
        and -device loader for the raw binary kernel.  This exercises:
        - ELF firmware loading via load_bios (with ELF entry detection)
        - Dynamic FDT placement (firmware-only path: kernel_load_addr + 256MB)
        - Boot stub passing FDT address to firmware
        """
        self.set_machine('virt')
        self.require_netdev('user')

        kernel_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/vmlinux.bin')
        booter_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/loadlinux')
        disk_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/disk.qcow2')
        self.vm.set_console()

        for i in range(1, 7):
            self.vm.add_args(
                '-netdev', f'type=user,id=net{i}',
                '-device', f'virtio-net-device,netdev=net{i}',
            )
        self.vm.add_args(
            '-bios', booter_path,
            '-device',
                f'loader,addr=0x{self.GUEST_ENTRY:08x},file={kernel_path}',
            '-m', '4G',
            '-accel', 'tcg,thread=multi',
            '-drive', f'if=none,file={disk_path},id=hd0',
            '-device', 'virtio-blk-device,drive=hd0',
            '-netdev', 'type=user,id=net0',
            '-device', 'virtio-net-device,netdev=net0',
        )
        self.vm.launch()

        self.wait_for_console_pattern("Brought up 4 CPUs")
        self.wait_for_console_pattern(
            "clocksource: Switched to clocksource HVM timer")
        self.wait_for_console_pattern("EXT2-fs (vda)")
        self.wait_for_console_pattern("Starting network: OK")
        self.wait_for_console_pattern("bash-5.2#")

    @skipUnless(os.getenv('QEMU_TEST_ALLOW_UNTRUSTED_CODE'), 'untrusted code')
    def test_linux_default_bios_boot(self):
        """
        Test the default firmware boot path.

        Like test_linux_bios_boot, but without -bios: the machine falls
        back to the bundled hexagon_loadlinux_v81 firmware from pc-bios.
        """
        self.set_machine('virt')
        self.require_netdev('user')

        kernel_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/vmlinux.bin')
        disk_path = self.archive_extract(self.ASSET_TARBALL,
            member=f'qemu-linux-tests-{self.GIT_REF}/disk.qcow2')
        self.vm.set_console()

        for i in range(1, 7):
            self.vm.add_args(
                '-netdev', f'type=user,id=net{i}',
                '-device', f'virtio-net-device,netdev=net{i}',
            )
        self.vm.add_args(
            '-device',
                f'loader,addr=0x{self.GUEST_ENTRY:08x},file={kernel_path}',
            '-m', '4G',
            '-accel', 'tcg,thread=multi',
            '-drive', f'if=none,file={disk_path},id=hd0',
            '-device', 'virtio-blk-device,drive=hd0',
            '-netdev', 'type=user,id=net0',
            '-device', 'virtio-net-device,netdev=net0',
        )
        self.vm.launch()

        self.wait_for_console_pattern("Brought up 4 CPUs")
        self.wait_for_console_pattern(
            "clocksource: Switched to clocksource HVM timer")
        self.wait_for_console_pattern("EXT2-fs (vda)")
        self.wait_for_console_pattern("Starting network: OK")
        self.wait_for_console_pattern("bash-5.2#")


if __name__ == '__main__':
    LinuxKernelTest.main()
