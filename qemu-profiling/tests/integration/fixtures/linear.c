/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
/* linear.c — no branches in user code, baseline test. */

volatile int sink;

int main(void)
{
    sink = 1;
    sink = 2;
    sink = 3;
    sink = 4;
    sink = 5;
    return 0;
}
