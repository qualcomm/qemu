#
# Copyright(c) 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

from glob import glob
import os
import textwrap

def read_skip_file(dirname):
    with open(f'{dirname}/SKIP') as f:
        lines = [line.strip() for line in f.readlines()]
        skip_names = filter(lambda line: line[0] != "#", lines)
    return set(skip_names)

def list_test_cases(dirname):
    return glob(f'{dirname}/*.pbn') + glob(f'{dirname}/*.elf')


class HexagonCheckError(Exception):
    pass

def run_tests(test, dirname, timeout, check):
    skip = read_skip_file(dirname)
    success, fail = 0, 0
    for test_bin in list_test_cases(dirname):
        test_name = os.path.basename(test_bin)
        if test_name in skip:
            print(f'#   SKIP {test_name}')
            continue
        print(f'#   Testing {test_name}')
        test.set_vm_arg('-kernel', test_bin)
        test.vm.launch()
        test.vm.wait(timeout=timeout)
        try:
            check()
            success += 1
        except HexagonCheckError as e:
            fail += 1
            err_msg = f'FAILED (exit code {test.vm.exitcode()})\n' + \
                      '----------\n' + str(e) + '\n----------'
            print(textwrap.indent(err_msg, "#     ", lambda _: True))
    if fail > 0:
        print(f'# FAIL:  {fail}')
    else:
        print(f'# All pass ({success})')
    return fail == 0
