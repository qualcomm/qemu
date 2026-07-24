#!/usr/bin/env python3

##
##  Copyright(c) 2019-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
import argparse

def read_tag_rev_info(fname):
    regex = re.compile("\[(.*)\].*introduced *= *([^,]*),.*.removed *= *([^ }]*).*")
    introduced, removed = {}, {}
    with open(fname, "r") as f:
        for line in f.readlines():
            line = line.strip()
            if line.startswith("["):
                tag, tag_introduced, tag_removed = regex.match(line).groups()
                introduced[tag] = int(tag_introduced, base=16)
                removed[tag] = int(tag_removed, base=16)
    return introduced, removed

def main():
    parser = argparse.ArgumentParser(
        description="Emit opaque macro calls with instruction names"
    )
    parser.add_argument("semantics", help="semantics file")
    parser.add_argument("tag_rev_info", help="tag rev info")
    parser.add_argument("hmx_out", help="output file for hmx")
    parser.add_argument("out", help="output file")
    args = parser.parse_args()
    hex_common.read_semantics_file(args.semantics)

    introduced, _ = read_tag_rev_info(args.tag_rev_info)
    hmx_tags = [
        tag for tag in hex_common.get_all_tags() if hex_common.is_hmx(tag)
    ]
    hmx_tags.sort() # Alphabetically first
    hmx_tags.sort(key=lambda tag: introduced.get(tag, 0))

    ##
    ##     Generate a list of all the opcodes
    ##
    with open(args.out, "w") as f:
        for tag in hex_common.get_all_tags():
            f.write(f"OPCODE({tag}),\n")

    with open(args.hmx_out, "w") as f:
        for tag in hmx_tags:
            f.write(f"OPCODE({tag}),\n")

if __name__ == "__main__":
    main()
