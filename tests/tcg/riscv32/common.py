#
# Common functionality across most UDB -> * translations.
#
# Copyright (c) 2025 rev.ng Labs Srl.
#
# This work is licensed under the terms of the GNU GPL, version 2 or
# (at your option) any later version.
#
# See the LICENSE file in the top-level directory for details.
#

import yaml
import sys
import re
import math

decode_only = {
    'qc.brev32.yaml',
    'qc.lwmi.yaml',
    'qc.lwm.yaml',
    'qc.swmi.yaml',
    'qc.swm.yaml',
    'qc.setwmi.yaml',
    'qc.setwm.yaml',

    'qc.c.mienter.yaml',
    'qc.c.mienter.nest.yaml',
    'qc.c.mileaveret.yaml',

    'qc.c.sync.yaml',
    'qc.c.syncr.yaml',
    'qc.c.syncwf.yaml',
    'qc.c.syncwl.yaml',
    'qc.sync.yaml',
    'qc.syncr.yaml',
    'qc.syncwf.yaml',
    'qc.syncwl.yaml',
    'qc.csrrwr.yaml',
    'qc.csrrwri.yaml',
    'qc.inw.yaml',
    'qc.outw.yaml',
}

system_only = {
    'qc.c.mienter',
    'qc.c.mienter.nest',
    'qc.c.mileaveret',
    'qc.c.sync',
    'qc.c.syncr',
    'qc.c.syncwf',
    'qc.c.syncwl',
    'qc.sync',
    'qc.syncr',
    'qc.syncwf',
    'qc.syncwl',
    'qc.inw',
    'qc.outw',
}


def round_to_power_of_two(x):
    return int(2**math.ceil(math.log2(x)))


def bit_to_c_size(x):
    return min(max(round_to_power_of_two(x), 8), 64)


def ranges_in_location(loc_str):
    for r in loc_str.split('|'):
        if '-' in r:
            offsets = [int(s) for s in r.split('-')]
            yield (offsets[1], offsets[0] - offsets[1] + 1)
        else:
            yield (int(r), 1)


def var_is_compressed(op, name):
    return f'X[{name}+8]' in op or \
        f'creg2reg({name})' in op


def var_is_imm(op, name):
    return f'X[{name}]' not in op and \
        f'X[{name}+8]' not in op and \
        f'creg2reg({name})' not in op and \
        f'creg2reg({name}+8)' not in op and \
        name != 'r1s' and name != 'r2s'


def var_size(var):
    sum = 0
    for _, length in ranges_in_location(var['location']):
        sum += length
    shift_amt = int(var['left_shift']) if 'left_shift' in var else 0
    return sum + shift_amt


def inst_is_compressed(y):
    return '.c.' in y['name']


def variables(y):
    return y['encoding']['variables'] if 'variables' in y['encoding'] else []


def variable_map(y):
    map = {}
    for v in variables(y):
        map[v['name']] = v
    return map


def load_yaml_or_exit(path):
    with open(path, 'r') as f:
        try:
            return yaml.safe_load(f)
        except yaml.YAMLError as e:
            print(f'Failed to load yaml file {path}: {e}', file=sys.stderr)
            exit(1)


def get_anyof_extensions_from_yaml(y):
    extensions = []
    if 'anyOf' in y['definedBy']:
        for e in y['definedBy']['anyOf']:
            extensions.append(e)
    else:
        extensions.append(y['definedBy'])

    extension_names = []
    for e in extensions:
        if 'name' in e:
            extension_names.append(e['name'])
        else:
            extension_names.append(e)

    return extension_names


################################################################################
# IDL substitutions to get valid C++

def sub_to_csr_address(match):
    str = match.group(1)
    if str.startswith('qc') and not ' ' in str:
        return f'{match.group(1)}'
    else:
        assert (False)


def sub_to_csr_read(match):
    str = match.group(1)
    return f'xqci_csrr_xreg(this, {str})'


def sub_to_csr_read_field(match):
    csr = match.group(1)
    field = match.group(2)
    return f'xqci_csrr_field_xreg(this, {csr}, {csr.upper()}_{field})'


def sub_to_csr_write(match):
    str = match.group(1)
    value = match.group(2)
    return f'xqci_csrw_xreg(this, {str}, {value})'


def sub_to_csr_write_field(match):
    csr = match.group(1)
    field = match.group(2)
    value = match.group(3)
    return f'xqci_csrw_field_xreg(this, {csr}, {csr.upper()}_{field}, {value});'


def op_to_cpp(op, csrs, for_klee=False):
    for csr in csrs:
        csr_name = re.sub(r'\.', '_', csr)
        op = re.sub(csr, csr_name, op)

    processed_op = ""
    for line in op.splitlines():
        if len(line) == 0:
            continue
        stripped_line = line.rstrip()
        if not '#' in stripped_line and not stripped_line[-1] in {'{', ';', '}'}:
            processed_op += stripped_line
        else:
            processed_op += stripped_line + '\n'
    op = processed_op

    op = re.sub(r'#', r'//', op)
    op = re.sub(r'\$signed', r'_signed', op)
    op = re.sub(r'\$encoding', r'0', op)

    op = re.sub(r'raise (.*) if (.*);', r'// \1', op)

    op = re.sub(r'for \(', r'#pragma unroll\nfor (', op)

    # op = re.sub(r'\(1 << ([a-zA-Z0-9]+)\)', r'(1ul << \1.value())', op)

    op = re.sub(r'Bits<{1\'b0, XLEN}\*2> pair = {X\[rs1 \+ 1\], X\[rs1\]};',
                r'uint64_t pair = ((uint64_t) X[rs1+1].value() << 32) | ((uint64_t) X[rs1].value());', op)
    op = re.sub(r'Bits<{1\'b0, MXLEN}\*2> pair = {X\[rs1 \+ 1\], X\[rs1\]};',
                r'uint64_t pair = ((uint64_t) X[rs1+1].value() << 32) | ((uint64_t) X[rs1].value());', op)
    # op = re.sub(r'{{XLEN{X\[([a-zA-Z0-9]+)\]\[xlen\(\)-1\]}}, X\[\1\]}', r'((int64_t)(int32_t)X[\1].value)', op)
    # op = re.sub(r"{{XLEN-5{1'b0}}, ([a-zA-Z0-9]+)}", r'((uint32_t) \1)', op)

    op = re.sub(r"([A-Za-z0-9]+)'b([0-9]+)", r'Bits<\1>(0b\2)', op)
    op = re.sub(r"([A-Za-z0-9]+)'h([0-9A-Fa-f]+)", r'Bits<\1>(0x\2)', op)

    op = re.sub(
        r'\[([A-Za-z0-9\(\)\+\-\*/ ]+):([A-Za-z0-9\(\)\+\-\*/ ]+)\]', r'.range<\2,\1>()', op)
    op = re.sub(r'([_a-zA-Z0-9]+)\.range', r'XReg(\1).range', op)
    op = re.sub(r'XReg(.*)=(.*)\? {(.*)} : {(.*)};',
                r'XReg\1=\2? XReg(\3) : XReg(\4);', op)
    op = re.sub(
        r'implemented\?\(ExtensionName::([a-zA-Z]*)\)', r'xqci_implemented_\1()', op)
    op = re.sub(
        r'raise\(ExceptionCode::([a-zA-Z]*)\,.*\);', r'xqci_raise_\1();', op)
    op = re.sub(
        r'set_mode\(PrivilegeMode::([a-zA-Z]*)\);', r'xqci_set_mode_\1();', op)
    op = re.sub(r'\$pc =', r'pc =', op)
    op = re.sub(r'\$pc', r'xqci_current_pc()', op)

    op = re.sub(
        r'{([A-Za-z0-9\(\)\+\-\*/ ]+){([A-Za-z0-9\(\)\[\]<>\+\-\*/, ]+)}}', r'repeat<\1>(\2)', op)

    op = re.sub(r'csr_sw_write\(', r'csr_sw_write(this, ', op)
    op = re.sub(r'csr_sw_read\(', r'csr_sw_read(this, ', op)

    op = re.sub(r'jump_halfword\(xqci_current_pc\(\)[ ]+\+[ ]+([a-z_A-Z\(\)]+)\)',
                r'xqci_jump_pcrel_bits(maybe_sext_xreg(\1))', op)
    op = re.sub(r'jump\(([a-z_A-Z0-9\[\]]+)\)', r'xqci_jump(\1, 0)', op)

    op = re.sub(r'CSR\[([a-zA-z0-9]+)\]\.address\(\)', sub_to_csr_address, op)
    op = re.sub(r'CSR\[([a-zA-z0-9 \+\*\/]+)\]\.sw_read\(\)',
                sub_to_csr_read, op)
    op = re.sub(
        r'CSR\[([a-zA-z0-9 \+\*\/]+)\]\.sw_write\((.*)\)', sub_to_csr_write, op)

    op = re.sub(r'CSR\[([a-zA-z0-9 \+\*\/]+)\]\.([A-Z]*) = (.*);',
                sub_to_csr_write_field, op)
    op = re.sub(r'CSR\[([a-zA-z0-9 \+\*\/]+)\]\.([A-Z]*)',
                sub_to_csr_read_field, op)
    op = re.sub(r'CSR\[([a-zA-z0-9 \+\*\/]+)\]', sub_to_csr_read, op)

    op = re.sub(r'Bits<xlen\(\)`\*2>', 'Bits<xlen()*2>', op)
    op = re.sub(r'(.*) = (.*) `\+ (.*);', r'\1 = wide_add(\2, \3);', op)
    op = re.sub(r'(.*) = (.*) `\- (.*);', r'\1 = wide_sub(\2, \3);', op)
    op = re.sub(r'\(([A-Za-z0-9_ ]+)`<<([A-Za-z0-9_ ]+)\)',
                r'wide_shl<\2>(\1)', op)

    op = re.sub(r'\$bits\((.*)\)', r'XReg(\1)', op)

    if not for_klee:
        op = re.sub(r'X\[(.*)\].* = (.*);', r'xqci_set_gpr_xreg(\1, \2);', op)

    return op
