#!/usr/bin/env python3

##
##  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
##
##  SPDX-License-Identifier: GPL-2.0-or-later
##

"""
Process HLX extension instruction definitions.

Similar to gen_mmvec_ext.py but simplified for HLX which doesn't
currently have multiple architecture revisions.
"""

import sys

def main():
    """
    Read preprocessed HLX .idef file and write to output.

    For now, this is a simple pass-through since HLX doesn't have
    multiple revisions. In the future, if HLX gets revision-specific
    instructions, this can be enhanced to handle Q6INSN_REV macros
    like gen_mmvec_ext.py does.
    """
    input_file = sys.argv[1]
    output_file = sys.argv[2]

    with open(input_file, "rt") as infile:
        with open(output_file, "w") as outfile:
            for line in infile:
                # Skip preprocessor directives
                if not line.startswith("#"):
                    outfile.write(line)

if __name__ == "__main__":
    main()
