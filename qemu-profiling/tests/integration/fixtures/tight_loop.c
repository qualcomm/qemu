/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
/* tight_loop.c — 100000-iteration loop, expect max edge count >= 100000. */

volatile int sink;

int main(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        sink = i;
    }
    return 0;
}
