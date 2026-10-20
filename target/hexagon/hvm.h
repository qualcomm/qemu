/*
 * Hexagon Virtual Machine (HVM) interface, implemented by QEMU itself
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_HVM_H
#define HEXAGON_HVM_H

#include "cpu.h"

/* The operations of the virtual instruction trap, trap1(#n). */
enum {
    HVM_TRAP1_VMVERSION = 0,
    HVM_TRAP1_VMRTE = 1,
    HVM_TRAP1_VMSETVEC = 2,
    HVM_TRAP1_VMSETIE = 3,
    HVM_TRAP1_VMGETIE = 4,
    HVM_TRAP1_VMINTOP = 5,
    HVM_TRAP1_VMCLRMAP = 10,
    HVM_TRAP1_VMNEWMAP = 11,
    HVM_TRAP1_VMCACHE = 13,
    HVM_TRAP1_VMGETTIME = 14,
    HVM_TRAP1_VMSETTIME = 15,
    HVM_TRAP1_VMWAIT = 16,
    HVM_TRAP1_VMYIELD = 17,
    HVM_TRAP1_VMSTART = 18,
    HVM_TRAP1_VMSTOP = 19,
    HVM_TRAP1_VMVPID = 20,
    HVM_TRAP1_VMSETREGS = 21,
    HVM_TRAP1_VMGETREGS = 22,
    HVM_TRAP1_VMTIMEROP = 24,
    HVM_TRAP1_VMPMUCONFIG = 25,
    HVM_TRAP1_VMGETINFO = 26,
};

/* vmnewmap's translation types */
enum {
    HVM_TRANS_LINEAR = 0,
    HVM_TRANS_TABLE = 1,
    HVM_TRANS_OFFSET = 2,
    HVM_TRANS_VARADIX = 3,
};

/* PTE page sizes (log4 of 4K pages) and bits of the table translation */
#define HVM_PTE_PGSIZE_1M       4
#define HVM_PTE_PGSIZE_4M       5
#define HVM_PTE_PGSIZE_INVALID  7
#define HVM_PTE_U_BIT           5

/* The last valid vmcache operation: ickill .. idsync */
#define HVM_CACHE_IDSYNC        5

/* The value vmversion returns. */
#define HVM_VERSION 0x00000800

/* trap0(#31) is the hardware configuration call. */
#define HVM_TRAP0_HWCONFIG 31

/* Event causes the monitor itself raises. */
#define HVM_CAUSE_PRIVILEGE HEX_CAUSE_PRIV_USER_NO_SINSN

/* Put a virtual CPU in the state a guest kernel starts in. */
void hexagon_hvm_cpu_reset(CPUHexagonState *env);

/*
 * Handle an exception on a virtual CPU.  Returns false if it is not one
 * that the monitor takes.
 */
bool hexagon_hvm_do_interrupt(CPUState *cs);

/* The virtual CPU takes a virtual interrupt, or wakes from vmwait. */
bool hexagon_hvm_exec_interrupt(CPUState *cs, int interrupt_request);

/* Whether a virtual interrupt is waiting for the guest to enable them. */
void hexagon_hvm_irq_recheck(CPUHexagonState *env);

/* Translate a guest virtual address with the guest's own page tables. */
bool hexagon_hvm_tlb_fill(CPUState *cs, vaddr address, int size,
                          MMUAccessType access_type, int mmu_idx, bool probe,
                          uintptr_t retaddr);
hwaddr hexagon_hvm_get_phys_addr_debug(CPUState *cs, vaddr addr);

#endif /* HEXAGON_HVM_H */
