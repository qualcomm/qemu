#!/usr/bin/env python3

#
# Assemble a simple test binary from a KLEE yaml test definitions,
# no toolchain required. A single instruction is tested for expected
# inputs/outputs found by KLEE, one ELF binary is produced per
# test case.
#
# Copyright (c) 2025 rev.ng Labs Srl.
#
# This work is licensed under the terms of the GNU GPL, version 2 or
# (at your option) any later version.
#
# See the LICENSE file in the top-level directory for details.
#

import argparse
import yaml
import re
import os
import struct
from math import ceil
from ctypes import c_int32
import common


expected_reg = 8
dst_reg = 9
address_reg = 10


def ashr32(x, n):
    if x & 0x80000000:
        return (x >> n) | (0xFFFFFFFF << (32 - n))
    else:
        return x >> n


def ashl32(x, n):
    return (x << n) & 0xFFFFFFFF


class InstPrinter:
    def __init__(self, inst_dir, system_mode):
        self.yamls = {}
        self.inst_dir = inst_dir
        self.bytes = bytes()
        self.system_mode = system_mode

        # Manually add encoding for two riscv32 instructions which need to be
        # emitted with different operands in tests.
        try:
            self.yamls['lw'] = yaml.safe_load(
                """
            encoding:
              match:      -----------------010-----0000011
              variables:
              - name: imm
                location: 31-20
                sign_extend: true
              - name: rd
                location: 11-7
                not: 0
              - name: rs1
                location: 19-15
            """
            )
            self.yamls['sw'] = yaml.safe_load(
                """
            encoding:
              match:      -----------------010-----0100011
              variables:
              - name: imm
                location: 31-25|11-7
                sign_extend: true
              - name: rs1
                location: 19-15
              - name: rs2
                location: 24-20
            """
            )
            self.yamls['lui'] = yaml.safe_load(
                """
            encoding:
              match:      -------------------------0110111
              variables:
              - name: imm
                location: 31-12
              - name: rd
                location: 11-7
                not: 0
            """
            )
            self.yamls['addi'] = yaml.safe_load(
                """
            encoding:
              match:      -----------------000-----0010011
              variables:
              - name: imm
                location: 31-20
              - name: rs1
                location: 19-15
                not: 0
              - name: rd
                location: 11-7
                not: 0
            """
            )
        except yaml.YAMLError as e:
            print(e)
            exit(1)

    def load(self, inst):
        self.yamls[inst] = common.load_yaml_or_exit(
            os.path.join(self.inst_dir, f"{inst}.yaml"))

    def li(self, N, reg):
        # sign extend low 12 bits
        M = ashr32(ashl32(N, 20), 20)
        # Upper 20 bits
        K = ashr32((c_int32(N).value-c_int32(M).value), 12)
        self.append('lui', K, reg)
        self.append('addi', M, reg, reg)

    def append(self, inst, *args):
        if not inst in self.yamls:
            self.load(inst)
        encoding = self.yamls[inst]['encoding']
        enc = int(re.sub(r'-', r'0', encoding['match']), 2)
        if 'variables' in encoding:
            len_expected = len(encoding['variables'])
            len_got = len(args)
            if len_got != len_expected:
                print(f'error: {inst} expected {
                      len_expected} args got {len_got}')
                return

            for i, v in enumerate(encoding['variables']):
                offset = 0

                arg_value = args[i]
                if 'left_shift' in v:
                    arg_value >>= v['left_shift']

                ranges = [p for p in common.ranges_in_location(v['location'])]
                for start, length in reversed(ranges):
                    mask = ((1 << length) - 1) << offset
                    arg_chunk = (arg_value & mask) >> offset
                    enc |= (arg_chunk << start)
                    offset += length

        enc_len = len(encoding['match'])
        num_bytes = ceil(enc_len/8)
        self.bytes += struct.pack('<Q', enc)[0:num_bytes]

    def exit(self):
        if self.system_mode:
            self.bytes += bytes.fromhex('97052000')     # auipc   a1, 0x200
            self.bytes += bytes.fromhex('938525f8')     # addi    a1, a1, -0x7e
            self.bytes += bytes.fromhex('37030200')     # lui     t1, 0x20
            self.bytes += bytes.fromhex('13036302')     # addi    t1, t1, 0x26
            self.bytes += bytes.fromhex('23b06500')     # sd      t1, 0x0(a1)
            self.bytes += bytes.fromhex('23b2a500')     # sd      a0, 0x4(a1)
            self.bytes += bytes.fromhex('13050002')     # li      a0, 0x20
            self.bytes += bytes.fromhex('13000000')     # nop
            self.bytes += bytes.fromhex('0100')         # nop
            # slli    zero, zero, 0x1f
            self.bytes += bytes.fromhex('1310f001')
            self.bytes += bytes.fromhex('73001000')     # ebreak
            # srai    zero, zero, 0x7
            self.bytes += bytes.fromhex('13507040')
            # j       0x800000ac <_exit+0x32>
            self.bytes += bytes.fromhex('6f000000')
        else:
            self.bytes += bytes.fromhex('9308d005')  # li a7 93
            self.bytes += bytes.fromhex('73000000')  # ecall

    def exit_success(self):
        self.bytes += bytes.fromhex('13050000')  # li a0, 0
        self.exit()

    def exit_failure(self):
        self.bytes += bytes.fromhex('1305f00f')  # li a0,255
        self.exit()

    def check_result(self, expected_result):
        self.li(expected_result, expected_reg)  # 8 bytes in size
        self.bytes += bytes.fromhex('63089400')  # beq s0,s1,16
        self.exit_failure()  # 12 bytes in size

    def check_branch_and_result(self, expected_result):
        # Hard code checking of expected vs returned value.
        # Only testing against state in returned register.
        self.bytes += bytes.fromhex('6f000001')  # j 16
        self.li(expected_result, expected_reg)  # 8 bytes in size
        self.bytes += bytes.fromhex('63089400')  # beq x8,x9,16
        self.exit_failure()  # 12 bytes in size

    def check_branch(self):
        self.bytes += bytes.fromhex('6f008000')  # j 8
        self.bytes += bytes.fromhex('6f000001')  # j 16
        self.exit_failure()  # 12 bytes in size


def output_elf(f, text_bytes):
    # ELF Header
    f.write(b'\x7fELF' + b'\x01'*3 + b'\x00' * 9)  # ELF Header
    f.write(struct.pack('<H', 2))                  # ET_EXEC (Executable)
    # EM_* (riscv32 architecture)
    f.write(struct.pack('<H', 243))
    f.write(struct.pack('<I', 1))                  # Version
    # Entry point (dummy address)
    f.write(struct.pack('<I', 0x10000+52+2*32))
    # f.write(struct.pack('<I', 0x80000000))             # Virtual address
    f.write(struct.pack('<I', 52))                 # Program header offset
    f.write(struct.pack('<I', 0))                  # Section header offset
    f.write(struct.pack('<I', 0))                  # Flags
    f.write(struct.pack('<H', 52))                 # ELF Header size
    f.write(struct.pack('<H', 32))                 # Program header entry size
    f.write(struct.pack('<H', 2))                  # Number of program headers
    f.write(struct.pack('<H', 0))                  # No. section headers
    f.write(struct.pack('<H', 0))                  # No. section headers
    # No. section header string table
    f.write(struct.pack('<H', 0))

    # Program Header (.text)
    f.write(struct.pack('<I', 1))                  # PT_LOAD
    f.write(struct.pack('<I', 0))                  # Offset in the file
    f.write(struct.pack('<I', 0x1000))             # Virtual address
    f.write(struct.pack('<I', 0x1000))             # Physical address
    # f.write(struct.pack('<I', 0x80000000))             # Virtual address
    # f.write(struct.pack('<I', 0x80000000))             # Physical address
    # Size of the segment in the file
    f.write(struct.pack('<I', 0))
    # Size of the segment in memory
    f.write(struct.pack('<I', 0x2000))
    f.write(struct.pack('<I', 6))                  # R (read) and E (execute)
    f.write(struct.pack('<I', 0x1000))             # Alignment

    # Program Header (.test_data)
    f.write(struct.pack('<I', 1))                  # PT_LOAD
    f.write(struct.pack('<I', 0))                  # Offset in the file
    f.write(struct.pack('<I', 0x10000))            # Virtual address
    f.write(struct.pack('<I', 0x10000))            # Physical address
    # Size of the segment in the file
    f.write(struct.pack('<I', len(text_bytes)))
    # Size of the segment in memory
    f.write(struct.pack('<I', len(text_bytes)))
    f.write(struct.pack('<I', 5))                  # R (read) and E (execute)
    f.write(struct.pack('<I', 0x1000))             # Alignment

    # Text section
    f.write(text_bytes)


def test_has_variables(test):
    return isinstance(test['variables'], list)


def main():
    parser = argparse.ArgumentParser(
        prog='assemble',
        description='Assemble KLEE output to elf tests'
    )
    parser.add_argument('--inst-dir', required=True)
    parser.add_argument('--io-file', required=True)
    parser.add_argument('--inst-name', required=True)
    parser.add_argument('--system-mode', default=False)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()

    printer = InstPrinter(args.inst_dir, args.system_mode)
    io_yaml = common.load_yaml_or_exit(args.io_file)
    io_var_map = {}
    for test in io_yaml:
        if test_has_variables(test):
            for v in test['variables']:
                io_var_map[v['name']] = v

    if not args.inst_name in printer.yamls:
        printer.load(args.inst_name)
    y = printer.yamls[args.inst_name]
    op = y['operation()']

    vars = common.variables(y)
    var_map = common.variable_map(y)

    for test_index, test in enumerate(io_yaml):
        printer.bytes = bytes()

        expected_result = None

        if 'has_jump' in test:
            if test['has_jump']['valid_test_jump'] == 0:
                continue
        if 'has_valid_test_memop' in test and test['has_valid_test_memop'] == 0:
            continue

        # for i in range(0,32):
        #    printer.li(i, i)
        printer.li(0x2800, 2)

        if 'has_load' in test:
            for loadop in test['has_load']:
                printer.li(loadop['address'], address_reg)
                printer.li(loadop['value'], dst_reg)
                printer.append('sw', 0, address_reg, dst_reg)

        inst_args = []
        if test_has_variables(test):
            for i, v in enumerate(test['variables']):
                if not 'in' in v:
                    continue

                if common.var_is_imm(op, v['name']):
                    inst_args.append(v['in'])
                else:
                    reg = dst_reg + 1 + i
                    if 'out' in v:
                        reg = dst_reg
                        expected_result = v['out']

                    if 'in' in v:
                        printer.li(v['in'], reg)

                    if common.var_is_compressed(op, v['name']):
                        reg -= 8
                    inst_args.append(reg)
        if 'has_jump' in test:
            jump_offset = int(test['has_jump']['jump_pc_offset'])
            if (jump_offset & (1 << 31)) != 0:
                printer.bytes += bytes.fromhex('6f008000')  # j 8
                printer.bytes += bytes.fromhex('6f00c000')  # j 12
        printer.append(args.inst_name, *inst_args)

        if 'has_jump' in test:
            if expected_result != None:
                printer.check_branch_and_result(expected_result)
            else:
                printer.check_branch()
        else:
            if expected_result != None:
                printer.check_result(expected_result)

        if 'has_store' in test:
            for storeop in test['has_store']:
                printer.li(storeop['address'], address_reg)
                printer.append('lw', 0, dst_reg, address_reg)
                printer.check_result(storeop['value'])

        printer.exit_success()

        with open(f'{args.out}-{test_index}', 'wb') as f:
            output_elf(f, printer.bytes)


if __name__ == '__main__':
    main()
