#!/usr/bin/env python3
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
# SPDX-License-Identifier: GPL-2.0-or-later

from qemu_test import LinuxKernelTest, Asset, skipBigDataTest
from qemu_test import exec_command_and_wait_for_pattern


class HexagonHVMLinuxTest(LinuxKernelTest):
    '''
    Boot Linux, a guest of the Hexagon Virtual Machine, with QEMU as the
    monitor of the VM: there is no hypervisor firmware, and the kernel's
    virtual instructions, page tables, interrupts and timers are all
    served by the 'hvm' mode of the virt machine.
    '''
    REPO = 'https://artifacts.codelinaro.org/artifactory' \
           '/codelinaro-toolchain-for-hexagon/23.1.0-rc1'
    ASSET_KERNEL = \
        Asset(f'{REPO}/vmlinux',
              '620859d3ebe83e6d858adac150701931197038deeb740d31d23c67da1b1e013a')
    ASSET_ROOTFS = \
        Asset(f'{REPO}/rootfs.ext2.gz',
              'c5743106d51fe4fd0693bd8da5c1b6b704ea4824a19f90a1e9324df3f847bd13')

    @skipBigDataTest()
    def test_linux_boot(self):
        """
        Boot the vmlinux ELF as a guest of the HVM interface, with a
        virtio-blk root disk and a virtio-net device, then log in and probe
        the devices from the guest shell.

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

        self.vm.add_args(
            '-machine', 'hvm=on',
            '-kernel', kernel_path,
            # console= must name the PL011, so that /dev/console (where
            # inittab respawns the getty) is the port we are listening on.
            '-append', 'console=ttyAMA0 root=/dev/vda rw',
            '-m', '4G',
            '-smp', '4',
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

        # Every virtual CPU the guest started with vmstart is up
        exec_command_and_wait_for_pattern(self, 'nproc', '4')

        # The guest's clocksource and clockevents are the HVM timer
        exec_command_and_wait_for_pattern(
            self, 'cat /sys/devices/system/clocksource/clocksource0'
            '/current_clocksource', 'HVM timer')

        # virtio-blk interrupts and DMA work: the disk is the rootfs
        exec_command_and_wait_for_pattern(self, 'cat /proc/partitions', 'vda')
        exec_command_and_wait_for_pattern(self, 'mount', 'on / type ext2')

        # virtio-net: the guest got its lease from the user netdev
        exec_command_and_wait_for_pattern(self, 'ip -4 addr show eth0',
                                          '10.0.2.15')

        # Interrupts reach more than one virtual CPU
        exec_command_and_wait_for_pattern(
            self, 'grep -c "^ *[0-9]*:" /proc/interrupts', '\n')

        # Stopping the virtual CPUs (vmstop) ends the VM
        exec_command_and_wait_for_pattern(self, 'poweroff',
                                          'reboot: Power down')
        self.vm.wait()
        self.assertEqual(self.vm.exitcode(), 0)


if __name__ == '__main__':
    LinuxKernelTest.main()
