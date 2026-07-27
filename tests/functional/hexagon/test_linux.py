#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

from qemu_test import LinuxKernelTest, Asset, skipBigDataTest
from qemu_test import exec_command_and_wait_for_pattern

class HexagonLinuxDevsTest(LinuxKernelTest):
    REPO = 'https://artifacts.codelinaro.org/artifactory' \
           '/codelinaro-toolchain-for-hexagon/23.1.0-rc1'
    ASSET_KERNEL = \
        Asset(f'{REPO}/vmlinux',
              '620859d3ebe83e6d858adac150701931197038deeb740d31d23c67da1b1e013a')
    ASSET_ROOTFS = \
        Asset(f'{REPO}/rootfs.ext2.gz',
              'c5743106d51fe4fd0693bd8da5c1b6b704ea4824a19f90a1e9324df3f847bd13')

    def common_boot_to_shell(self, bios=None):
        """
        Boot the vmlinux ELF under the H2 hypervisor firmware (either the
        bundled default, or one selected via -bios), with a virtio-blk root
        disk and a virtio-net device, then log in and probe the devices from
        the guest shell.

        Note the guest uses two consoles in sequence: the semihosting
        'angel0' bootconsole, whose output goes to QEMU's stdout, and the
        PL011 that takes over once its driver probes.  Only the latter is
        wired to '-serial', so early boot messages never reach the console
        here -- hence the device checks below run as shell commands rather
        than matching on boot-time kernel output.
        """
        self.set_machine('virt')
        self.require_netdev('user')

        kernel_path = self.ASSET_KERNEL.fetch()
        disk_path = self.uncompress(self.ASSET_ROOTFS)
        self.vm.set_console()

        if bios:
            self.vm.add_args('-bios', bios)
        self.vm.add_args(
            '-kernel', kernel_path,
            # console= must name the PL011, so that /dev/console (where
            # inittab respawns the getty) is the port we are listening on.
            '-append', 'console=ttyAMA1 root=/dev/vda rw',
            '-m', '4G',
            '-accel', 'tcg,thread=multi',
            '-drive', f'if=none,file={disk_path},format=raw,id=hd0',
            '-device', 'virtio-blk-device,drive=hd0',
            '-netdev', 'type=user,id=net0',
            '-device', 'virtio-net-device,netdev=net0',
        )
        self.vm.launch()

        # Boot all the way to the getty and log in; root has no password.
        self.wait_for_console_pattern('buildroot login:')
        exec_command_and_wait_for_pattern(self, 'root', '# ')

        # Verify SMP: all 4 CPUs are up under MTTCG
        exec_command_and_wait_for_pattern(self, 'nproc', '4')

        # Verify the qtimer is driving the guest clocksource
        exec_command_and_wait_for_pattern(self,
            'cat /sys/devices/system/clocksource/clocksource0'
            '/current_clocksource', 'HVM timer')

        # Verify virtio-blk: the disk is present and mounted as the rootfs
        exec_command_and_wait_for_pattern(self, 'cat /proc/partitions', 'vda')
        exec_command_and_wait_for_pattern(self, 'mount', 'on / type ext2')

        # Verify virtio-net: the guest got its lease from the user netdev
        exec_command_and_wait_for_pattern(self, 'ip -4 addr show eth0',
                                          '10.0.2.15')

    @skipBigDataTest()
    def test_linux_boot(self):
        """
        Boot vmlinux with the default firmware: -bios resolves to the
        bundled hexagon_loadlinux_v81 firmware from pc-bios.
        """
        self.common_boot_to_shell()

    @skipBigDataTest()
    def test_linux_bios_boot(self):
        """
        Test the -bios path explicitly, selecting one of the bundled H2
        firmware images by name rather than relying on the default
        resolution.
        """
        self.common_boot_to_shell(bios='hexagon_loadlinux_v81')


if __name__ == '__main__':
    LinuxKernelTest.main()
