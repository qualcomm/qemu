#!/usr/bin/env python3

#
# Produce a simple inline assembly C test from KLEE test definitions.
# Requires toolchain with assembly support. A single instruction is
# tested for expected # inputs/outputs found by KLEE.
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

func_exit = """
__attribute__((noreturn))
void exit(int code) {
    __asm__ volatile("addi a0, %0, 0\\n"
                     "li a7, 93\\n"
                     "ecall" :: "r"(code));
    for(;;);
}
"""

func_check = """
void check(int cond) {
    if (!cond) {
        exit(255);
    }
}
"""

should_sext = {
    'qc.beqi',
    'qc.bgei',
    'qc.blti',
    'qc.bnei',
    'qc.e.addi',
    'qc.e.beqi',
    'qc.e.bgei',
    'qc.e.bgeui',
    'qc.e.blti',
    'qc.e.bltui',
    'qc.e.bnei',
    'qc.e.j',
    'qc.e.jal',
    'qc.e.lb',
    'qc.e.lh',
    'qc.e.lhu',
    'qc.e.lw',
    'qc.e.sb',
    'qc.e.sh',
    'qc.e.sw',
    'qc.insbri',
    'qc.lieqi',
    'qc.linei',
    'qc.muliadd',
    'qc.mveqi',
    'qc.mvnei',
    'qc.selecteqi',
    'qc.selectieqi',
    'qc.selectinei',
    'qc.selectnei',
    'qc.e.lbu',
}

skip_insn = {
    'qc.c.clrint',
    'qc.c.setint',
    'qc.c.delay',
    'qc.e.beqi',
    'qc.e.bgei',
    'qc.e.bgeui',
    'qc.e.blti',
    'qc.e.bltui',
    'qc.e.bnei',
    'qc.e.j',
    'qc.e.jal',
    'qc.cm.push',
    'qc.cm.pushfp',
    'qc.cm.pop',
    'qc.cm.popfp',
}


def ashr32(x, n):
    if x & 0x80000000:
        return (x >> n) | (0xFFFFFFFF << (32 - n))
    else:
        return x >> n


def ashl32(x, n):
    return (x << n) & 0xFFFFFFFF


def sext(imm, len):
    res = c_int32(ashr32(ashl32(imm, 32-len), 32-len)).value
    return res


class CPrinter:
    def __init__(self, out, inst_dir):
        self.yamls = {}
        self.inst_dir = inst_dir
        self.bytes = bytes()
        self.out = out
        self.indent = 0

    def set_indent(self, n):
        self.indent = n

    def line(self, str):
        self.out.write(' ' * self.indent + str + '\n')

    def load(self, inst):
        self.yamls[inst] = common.load_yaml_or_exit(
            os.path.join(self.inst_dir, f"{inst}.yaml"))

    def append(self, inst, *args):
        if not inst in self.yamls:
            self.load(inst)
        encoding = self.yamls[inst]['encoding']
        enc = int(re.sub(r'-', r'0', encoding['match']), 2)
        if 'variables' in encoding:
            len_expected = len(encoding['variables'])
            len_got = len(args)
            if len_got != len_expected:
                print(f'error: {inst} expected {len_expected} args got {len_got}')
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
    parser.add_argument('--out', required=True)
    args = parser.parse_args()

    if args.inst_name in skip_insn:
        return

    io_yaml = common.load_yaml_or_exit(args.io_file)
    io_var_map = {}
    for test in io_yaml:
        if test_has_variables(test):
            for v in test['variables']:
                io_var_map[v['name']] = v

    # Skip non arithmetic tests
    for test_index, test in enumerate(io_yaml):
        if 'has_jump' in test:
            return

    with open(f'{args.out}', 'w') as f:
        printer = CPrinter(f, args.inst_dir)
        if not args.inst_name in printer.yamls:
            printer.load(args.inst_name)
        y = printer.yamls[args.inst_name]
        is_compressed = common.inst_is_compressed(y)
        vars = common.variables(y)
        var_map = common.variable_map(y)

        printer.line('#include <stddef.h>')
        printer.line('#include <stdint.h>')

        if 'has_load' in test or 'has_store' in test:
            printer.line('__attribute__((section(".mem_test_section")))')
            printer.line('char data[2*0x1000];')

        printer.line(func_exit)
        printer.line(func_check)

        printer.line('void _start() {')
        printer.set_indent(4)

        tmp_index = 0
        for test_index, test in enumerate(io_yaml):
            expected_result = None

            # TODO(anjo): Not testing jumps in C yet
            # if 'has_jump' in test:
            #    if test['has_jump']['valid_test_jump'] == 0:
            #        continue

            if 'has_valid_test_memop' in test and test['has_valid_test_memop'] == 0:
                continue

            if 'has_load' in test:
                for loadop in test['has_load']:
                    printer.line(f'intptr_t address{tmp_index} = {loadop["address"]};')
                    printer.line(
                        f'*(uint32_t *)address{tmp_index} = {loadop["value"]};')
                    tmp_index += 1

            out_args = []
            in_args = []
            variable_order = []
            fmt = ''
            asm = ''
            reg_s_index = 0
            if test_has_variables(test):
                for i, v in enumerate(reversed(test['variables'])):
                    if not 'in' in v:
                        continue

                    variable_order.append(v['name'])
                    var = vars[len(vars)-1-i]

                    if common.var_is_imm(y['operation()'], v['name']):
                        imm = v['in']
                        if args.inst_name in should_sext:
                            imm = sext(int(v['in']), common.var_size(var))

                        if var['name'] == 'width_minus1':
                            imm += 1

                        if 'left_shift' in var:
                            imm >>= var['left_shift']

                        in_args.append(('i', imm))
                    else:
                        name = None
                        value = '0'

                        is_read_write = 'out' in v and 'in' in v
                        if is_read_write:
                            expected_result = v['out']
                            name = f'out{tmp_index}'
                            value = v['in']
                            out_args.append(('+r', name))
                        elif 'out' in v:
                            expected_result = v['out']
                            name = f'out{tmp_index}'
                            out_args.append(('=r', name))
                        elif 'in' in v:
                            name = f'in{tmp_index}'
                            value = v['in']
                            in_args.append(('r', name))

                        if var['name'].startswith('r') and var['name'].endswith('s'):
                            reg_s_index += 1
                            printer.line(f'register unsigned int {name} asm("s{reg_s_index}") = {value};')
                        else:
                            printer.line(f'unsigned int {name} = {value};')

                        tmp_index += 1

                num_vars = len(test['variables'])
                fmts = [f'%{i}' for i in range(0, num_vars)]
                fmt = ', '.join(fmts)

                asm = y['assembly']
                for i, v in enumerate(variable_order):
                    name = v

                    remap_names = {
                        'width_minus1': 'width',
                    }

                    if name in remap_names:
                        name = remap_names[name]
                    elif v.startswith('rs') or v == 'rd':
                        name = 'x' + v[1:]

                    asm = asm.replace(name, f'%{i}')

            fmt_out = [f'"{r}"({o})'for r, o in out_args]
            fmt_in = [f'"{r}"({o})'for r, o in in_args]
            printer.line(f'__asm__ volatile("{args.inst_name} {asm}" : {", ".join(fmt_out)} : {", ".join(fmt_in)} :);')
            if expected_result != None:
                for _, o in out_args:
                    printer.line(f'check({o} == {expected_result});')

            if 'has_store' in test:
                for storeop in test['has_store']:
                    printer.line(f'intptr_t address{tmp_index} = {storeop["address"]};')
                    fs = "F"*int(2*int(storeop["size"])/8)
                    printer.line(
                        f'check((*(uint32_t*)address{tmp_index} & 0x{fs}) == {storeop["value"]});')
                    tmp_index += 1

        printer.line('exit(0);')
        printer.set_indent(0)
        printer.line('}')


if __name__ == '__main__':
    main()
