#!/usr/bin/env python3

##
##  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
##
##  SPDX-License-Identifier: GPL-2.0-or-later
##

import sys
import re
import string
import hex_common


##
## Generate the case statement body for this instruction
##     For M8_mxmem
##     We produce:
##         case M8_mxmem:
##         {
##             int32_t RsV = args.arg1;
##             int32_t RtV = args.arg2;
##             <instruction semantics>
##         }
##         break;
##
def gen_coproc_case(f, tag, tagregs, tagimms):
    regs = tagregs[tag]
    imms = tagimms[tag]

    f.write(f"case {tag}:\n")
    f.write("{\n")

    i = 1
    for regtype, regid in regs:
        f.write(f"    int32_t {regtype}{regid}V = args.arg{i};\n")
        i += 1

    if hex_common.need_ea(tag):
        f.write("    uint32_t EA __attribute__((unused));\n")

    if hex_common.is_coproc_act(tag):
        slot = 1
    else:
        slot = 0
    f.write(f"    Insn tmp_insn = {{ .slot = {slot} }};\n")
    f.write("    Insn *insn __attribute__((unused)) = &tmp_insn;\n")

    f.write(f"    {hex_common.semdict[tag]}\n")

    f.write("}\nbreak;\n\n")
    ## End of the helper definition


def main():
    hex_common.read_semantics_file(sys.argv[1])
    hex_common.read_attribs_file(sys.argv[2])

    hex_common.calculate_attribs()
    tagregs = hex_common.get_tagregs()
    tagimms = hex_common.get_tagimms()

    output_file = sys.argv[-1]
    with open(output_file, "w") as f:
        for tag in hex_common.get_user_tags():
            if "A_COPROC" not in hex_common.attribdict[tag]:
                continue
            if not hex_common.tag_ignore(tag):
                gen_coproc_case(f, tag, tagregs, tagimms)


if __name__ == "__main__":
    main()
