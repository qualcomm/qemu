#!/usr/bin/env python3

##
##  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, see <http://www.gnu.org/licenses/>.
##

import sys
import re
import string
import hex_common

def gen_decl_insn(f, slot):
    f.write(hex_common.code_fmt(f"""\
        Insn tmp_insn = {{ .slot = {slot} }};\n
        Insn *insn __attribute__((unused)) = &tmp_insn;\n
    """))

mem_init_attrs = [
    'A_LOAD',
    'A_STORE',
]
def needs_page_size(tag):
    return any(A in hex_common.attribdict[tag] for A in mem_init_attrs)

def needs_cpu_memop_pc(tag):
    return any(A in hex_common.attribdict[tag] for A in mem_init_attrs + ['A_DMA'])

##
## Generate the TCG code to call the helper
##     For A2_add: Rd32=add(Rs32,Rt32), { RdV=RsV+RtV;}
##     We produce:
##       int32_t HELPER(A2_add)(CPUHexagonState *env, int32_t RsV, int32_t RtV)
##       {
##           int32_t RdV = 0;
##           { RdV=RsV+RtV;}
##           return RdV;
##       }
##
def gen_helper_function(f, tag, tagregs, tagimms):
    regs = tagregs[tag]
    imms = tagimms[tag]

    ret_type = hex_common.helper_ret_type(tag, regs).func_arg

    declared = []
    for arg in hex_common.helper_args(tag, regs, imms):
        declared.append(arg.func_arg)

    arguments = ", ".join(declared)
    f.write(f"{ret_type} HELPER({tag})({arguments})\n")
    f.write("{\n")

    if hex_common.need_ea(tag):
        f.write(hex_common.code_fmt(f"""\
            uint32_t EA __attribute__((unused));
        """))

    if hex_common.is_scatter_gather(tag):
        gen_decl_insn(f, 1 if hex_common.is_gather(tag) else 0)
    if hex_common.is_coproc(tag):
        gen_decl_insn(f, 1 if hex_common.is_coproc_act(tag) else 0)

    ## Declare the return variable
    if not hex_common.is_predicated(tag):
        for regtype, regid in regs:
            reg = hex_common.get_register(tag, regtype, regid)
            if reg.is_writeonly() and not reg.is_hvx_reg():
                f.write(hex_common.code_fmt(f"""\
                    {reg.helper_arg_type()} {reg.helper_arg_name()} = 0;
                """))

    ## Print useful information about HVX registers
    for regtype, regid in regs:
        reg = hex_common.get_register(tag, regtype, regid)
        if reg.is_hvx_reg():
            reg.helper_hvx_desc(f)

    if needs_cpu_memop_pc(tag):
        f.write(hex_common.code_fmt("CPU_MEMOP_PC_SET(env);\n"))

    if hex_common.need_slot(tag):
        if "A_LOAD" in hex_common.attribdict[tag]:
            f.write(hex_common.code_fmt(f"""\
                bool pkt_has_store_s1 = slotval & 0x1;
            """))
        f.write(hex_common.code_fmt(f"""\
            uint32_t slot = slotval >> 1;
        """))

    if "A_COPROC" in hex_common.attribdict[tag]:
        f.write("    paddr_t __attribute__((unused)) phys = 0;\n")
        f.write("    int __attribute__((unused)) prot;\n")
        f.write("    int32_t __attribute__((unused)) excp;\n")
        f.write("    CoprocArgs args = {0};\n")
        f.write(f"    args.opcode = {tag};\n")
        f.write(f"    args.unit = env->threadId;\n")
        arg = 1
        for regtype, regid in regs:
            if (regtype == 'R' and regid == 's'):
                f.write(f"    hex_tlb_find_match(env, {regtype}{regid}V,"
                        + " MMU_DATA_LOAD, &phys, &prot, &args.page_size,"
                        + " &excp, cpu_mmu_index(env_cpu(thread), false));\n")
                f.write(f"    args.arg{arg} = phys + ({regtype}{regid}V"
                        + " & (args.page_size - 1));\n")
            else:
                f.write(f"    args.arg{arg} = {regtype}{regid}V;\n")
            arg += 1;
        f.write("    coproc(&args);\n")
        f.write("}\n\n")
        ## End of the helper definition
        return

    if "A_FPOP" in hex_common.attribdict[tag]:
        f.write(hex_common.code_fmt(f"""\
            arch_fpop_start(env);
        """))

    f.write(hex_common.code_fmt(f"""\
        {hex_common.semdict[tag]}
        COUNT_HELPER({tag});
    """))

    if "A_FPOP" in hex_common.attribdict[tag]:
        f.write(hex_common.code_fmt(f"""\
            arch_fpop_end(env);
        """))

    ## Return the scalar result
    for regtype, regid in regs:
        reg = hex_common.get_register(tag, regtype, regid)
        if reg.is_written() and not reg.is_hvx_reg():
            f.write(hex_common.code_fmt(f"""\
                return {reg.helper_arg_name()};
            """))

    f.write("}\n\n")
    ## End of the helper definition


def main():
    hex_common.read_semantics_file(sys.argv[1])
    hex_common.read_attribs_file(sys.argv[2])
    hex_common.read_overrides_file(sys.argv[3])
    hex_common.read_overrides_file(sys.argv[4])
    hex_common.read_overrides_file(sys.argv[5])
    ## Whether or not idef-parser is enabled is
    ## determined by the number of arguments to
    ## this script:
    ##
    ##   6 args. -> not enabled,
    ##   7 args. -> idef-parser enabled.
    ##
    ## The 7:th arg. then holds a list of the successfully
    ## parsed instructions.
    is_idef_parser_enabled = len(sys.argv) > 7
    if is_idef_parser_enabled:
        hex_common.read_idef_parser_enabled_file(sys.argv[6])
    hex_common.calculate_attribs()
    hex_common.init_registers()
    tagregs = hex_common.get_tagregs()
    tagimms = hex_common.get_tagimms()

    output_file = sys.argv[-1]
    with open(output_file, "w") as f:
        for tag in hex_common.get_user_tags():
            if hex_common.tag_ignore(tag):
                continue
            if hex_common.skip_qemu_helper(tag):
                continue
            if hex_common.is_idef_parser_enabled(tag):
                continue
            gen_helper_function(f, tag, tagregs, tagimms)

        f.write("#if !defined(CONFIG_USER_ONLY)\n")
        for tag in hex_common.get_sys_tags():
            if hex_common.skip_qemu_helper(tag):
                continue
            if hex_common.is_idef_parser_enabled(tag):
                continue
            gen_helper_function(f, tag, tagregs, tagimms)
        f.write("#endif\n")


if __name__ == "__main__":
    main()
