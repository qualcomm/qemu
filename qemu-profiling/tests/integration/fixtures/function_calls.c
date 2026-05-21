/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
/* function_calls.c — 3 noinline funcs called 200x each. */

volatile int sink;

__attribute__((noinline))
void func_a(int x)
{
    sink = x + 1;
}

__attribute__((noinline))
void func_b(int x)
{
    sink = x + 2;
}

__attribute__((noinline))
void func_c(int x)
{
    sink = x + 3;
}

int main(void)
{
    int i;

    for (i = 0; i < 200; i++) {
        func_a(i);
        func_b(i);
        func_c(i);
    }
    return 0;
}
