/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
/* if_else_chain.c — classify(x) with 3 paths, called 500x. */

volatile int sink;

__attribute__((noinline))
int classify(int x)
{
    if (x % 3 == 0) {
        return 1;
    } else if (x % 3 == 1) {
        return 2;
    } else {
        return 3;
    }
}

int main(void)
{
    int i;

    for (i = 0; i < 500; i++) {
        sink = classify(i);
    }
    return 0;
}
