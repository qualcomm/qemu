/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu_bits.h"
#include "monitor/monitor.h"
#include "monitor/hmp.h"
#include "hex_mmu.h"

void hmp_info_tlb(Monitor *mon, const QDict *qdict)
{
#if !defined(CONFIG_USER_ONLY)
    CPUArchState *env = mon_get_cpu_env(mon);
    if (!env) {
        monitor_printf(mon, "No CPU available\n");
        return;
    }

    dump_mmu(env);
#endif
}
