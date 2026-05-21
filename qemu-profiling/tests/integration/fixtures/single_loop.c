/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
/* single_loop.c — 1000-iteration loop, expect max edge count >= 1000. */

volatile int sink;

int main(void)
{
    int i;

    for (i = 0; i < 1000; i++) {
        sink = i;
    }
    return 0;
}
