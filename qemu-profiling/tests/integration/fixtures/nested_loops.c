/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
/* nested_loops.c — outer=100 * inner=50, expect max edge count >= 5000. */

volatile int sink;

int main(void)
{
    int i, j;

    for (i = 0; i < 100; i++) {
        for (j = 0; j < 50; j++) {
            sink = i + j;
        }
    }
    return 0;
}
