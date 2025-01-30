/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "qemu/osdep.h"
#include "cpu.h"
#include "cpu_bits.h"
#include "monitor/monitor.h"
#include "monitor/hmp-target.h"
#include "monitor/hmp.h"
#include "hex_mmu.h"

const MonitorDef monitor_defs[] = {
    { "cycle_count", offsetof(CPUHexagonState,  t_cycle_count) },
#if !defined(CONFIG_USER_ONLY)
    { "tlb_lock", offsetof(CPUHexagonState, tlb_lock_state) },
    { "k0_lock", offsetof(CPUHexagonState,  k0_lock_state) },
    { "wait_next_pc", offsetof(CPUHexagonState,  wait_next_pc) },
#endif
    { NULL },
};

const MonitorDef *target_monitor_defs(void)
{
    return monitor_defs;
}

void hmp_info_tlb(Monitor *mon, const QDict *qdict)
{
    CPUArchState *env = mon_get_cpu_env(mon);
    if (!env) {
        monitor_printf(mon, "No CPU available\n");
        return;
    }

    dump_mmu(env);
}
