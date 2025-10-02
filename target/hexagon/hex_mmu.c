/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/qemu-print.h"
#include "qemu/units.h"
#include "cpu.h"
#include "system/cpus.h"
#include "internal.h"
#include "hex_mmu.h"
#include "macros.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "sys_macros.h"
#include "reg_fields.h"
#include "trace.h"

#define GET_TLB_FIELD(ENTRY, FIELD)                               \
    ((uint64_t)fEXTRACTU_BITS(ENTRY, reg_field_info[FIELD].width, \
                              reg_field_info[FIELD].offset))

/* PPD (physical page descriptor) */
static inline uint64_t GET_PPD(uint64_t entry)
{
    if (GET_TLB_FIELD(entry, PTE_HSV39)) {
        int PA4543_shift = reg_field_info[PTE_PPD].width;
        int PA4544_shift = PA4543_shift + reg_field_info[PTE_PA43].width;
        return GET_TLB_FIELD(entry, PTE_PPD) |
               (GET_TLB_FIELD(entry, PTE_PA43) << PA4543_shift) |
               (GET_TLB_FIELD(entry, PTE_PA4544) << PA4544_shift);
    } else {
        return GET_TLB_FIELD(entry, PTE_PPD) |
               (GET_TLB_FIELD(entry, PTE_PA35) <<
                reg_field_info[PTE_PPD].width);
    }
}

static const char *pgsize_str[] = {
    "4K", "16K", "64K", "256K", "1M", "4M", "16M", "64M",
    "256M", "1G", "4G", "16G", "64G"
};

static inline int hex_tlb_pgsize_type(uint64_t entry)
{
    if (entry == 0) {
        return 0;
    }
    int size = ctz64(entry) + (GET_TLB_FIELD(entry, PTE_HSV39) ? 4 : 0);
    return size;
}

static inline uint64_t hex_tlb_page_size_bytes(uint64_t entry)
{
    return 1ull << (TARGET_PAGE_BITS + 2 * hex_tlb_pgsize_type(entry));
}

static inline uint64_t hex_tlb_phys_addr(uint64_t entry)
{
    static const uint64_t encmask_2_mask[] = {
        0x0fffLL, 0x3fffLL, 0xffffLL, 0x3ffffLL, 0xfffffLL, 0x3fffffLL,
        0xffffffLL, 0x3ffffffLL, 0xfffffffLL, 0x3fffffffLL, 0xffffffffLL,
        0x3ffffffffLL, 0xfffffffffLL, 0xffffffffLL
    };
    uint32_t ppd = GET_PPD(entry);
    uint64_t pagenum = ppd >> 1;
    uint64_t pagemask = encmask_2_mask[hex_tlb_pgsize_type(entry)];
    uint64_t PA = (pagenum << TARGET_PAGE_BITS) & (~pagemask);
    return PA;
}

static inline uint64_t hex_tlb_virt_addr(uint64_t entry)
{
    int shift = GET_TLB_FIELD(entry, PTE_HSV39) ? 20 : TARGET_PAGE_BITS;
    return (uint64_t)GET_TLB_FIELD(entry, PTE_VPN) << shift;
}

static void hex_log_tlbw(uint32_t index, uint64_t entry)
{
    if (qemu_loglevel_mask(CPU_LOG_MMU)) {
        if (qemu_log_enabled()) {
            FILE *logfile = qemu_log_trylock();
            if (logfile) {
                fprintf(logfile, "tlbw[%03d]: ", index);
                if (GET_TLB_FIELD(entry, PTE_V)) {
                    fprintf(logfile, "0x%016" PRIx64 ": ", entry);
                    uint64_t PA = hex_tlb_phys_addr(entry);
                    uint64_t VA = hex_tlb_virt_addr(entry);
                    fprintf(logfile, "V:%" PRId64 " G:%" PRId64
                            " A1:%" PRId64 " A0:%" PRId64,
                            GET_TLB_FIELD(entry, PTE_V),
                            GET_TLB_FIELD(entry, PTE_G),
                            GET_TLB_FIELD(entry, PTE_ATR1),
                            GET_TLB_FIELD(entry, PTE_ATR0));
                    fprintf(logfile, " ASID:0x%02" PRIx64 " VA:0x%08" PRIx64,
                            GET_TLB_FIELD(entry, PTE_ASID), VA);
                    fprintf(logfile,
                            " X:%" PRId64 " W:%" PRId64 " R:%" PRId64
                            " U:%" PRId64 " C:%" PRId64,
                            GET_TLB_FIELD(entry, PTE_X),
                            GET_TLB_FIELD(entry, PTE_W),
                            GET_TLB_FIELD(entry, PTE_R),
                            GET_TLB_FIELD(entry, PTE_U),
                            GET_TLB_FIELD(entry, PTE_C));
                    fprintf(logfile,
                            " PA:0x%09" PRIx64 " SZ:%s (0x%" PRIx64 ")",
                            PA, pgsize_str[hex_tlb_pgsize_type(entry)],
                            hex_tlb_page_size_bytes(entry));
                    fprintf(logfile, "\n");
                } else {
                    fprintf(logfile, "invalid\n");
                }
                qemu_log_unlock(logfile);
            }
        }
    }
}

void hex_tlbw(CPUHexagonState *env, uint32_t index, uint64_t value)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->tlb) {
        hex_log_tlbw(index, value);
        uint32_t idx = TLB_WRAP_INDEX(index);
        bool old_entry_valid = GET_TLB_FIELD(
            hexagon_tlb_read(cpu->tlb, idx), PTE_V);
        bool mmu_enabled = cpu->globalregs ?
            GET_SYSCFG_FIELD(SYSCFG_MMUEN,
                             arch_get_system_reg(env, HEX_SREG_SYSCFG)) : 0;
        if (old_entry_valid && mmu_enabled) {
            /* FIXME - Do we have to invalidate everything here? */
            CPUState *cs = env_cpu(env);
            tlb_flush(cs);
        }
        hexagon_tlb_write(cpu->tlb, index, value, old_entry_valid,
                          mmu_enabled, env->threadId, idx);
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "TLB write attempted but TLB not initialized\n");
    }
}


void hex_mmu_on(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    qemu_log_mask(CPU_LOG_MMU, "Hexagon MMU turned on!\n");
    tlb_flush(cs);
}

void hex_mmu_off(CPUHexagonState *env)
{
    CPUState *cs = env_cpu(env);
    qemu_log_mask(CPU_LOG_MMU, "Hexagon MMU turned off!\n");
    tlb_flush(cs);
}

void hex_mmu_mode_change(CPUHexagonState *env)
{
    qemu_log_mask(CPU_LOG_MMU, "Hexagon mode change!\n");
    CPUState *cs = env_cpu(env);
    tlb_flush(cs);
}

bool hex_tlb_find_match(CPUHexagonState *env, target_ulong VA,
                        MMUAccessType access_type, hwaddr *PA, int *prot,
                        uint64_t *size, int32_t *excp, int mmu_idx)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->tlb) {
        uint32_t ssr = arch_get_system_reg(env, HEX_SREG_SSR);
        uint8_t asid = GET_SSR_FIELD(SSR_ASID, ssr);
        int32_t cause_code = 0;
        bool result = hexagon_tlb_find_match(cpu->tlb, asid, VA, access_type,
                                             PA, prot, size, excp, &cause_code,
                                             mmu_idx);
        if (cause_code) {
            env->cause_code = cause_code;
        }
        return result;
    }
    /* No TLB - return miss */
    *PA = 0;
    *prot = 0;
    *size = 0;
    *excp = 0;
    return false;
}

/* Called from tlbp instruction */
uint32_t hex_tlb_lookup(CPUHexagonState *env, uint32_t ssr, uint32_t VA)
{
    HexagonCPU *cpu = env_archcpu(env);
    uint32_t result;
    uint8_t asid = GET_SSR_FIELD(SSR_ASID, ssr);
    if (cpu->tlb) {
        uint32_t imprecise_exception = 0;
        int32_t cause_code = 0;
        result = hexagon_tlb_lookup(cpu->tlb, asid, VA, &imprecise_exception,
                                   &cause_code);
        if (imprecise_exception) {
            env->imprecise_exception = imprecise_exception;
        }
        if (cause_code) {
            env->cause_code = cause_code;
        }
    } else {
        /* No TLB - return not found */
        result = 0x80000000;
    }

    if (result == 0x80000000) {
        qemu_log_mask(CPU_LOG_MMU, "hex_tlb_lookup: 0x%x, 0x%x => NOT FOUND\n",
                      asid, VA);
    } else {
        qemu_log_mask(CPU_LOG_MMU, "hex_tlb_lookup: 0x%x, 0x%x => %"PRId32"\n",
                      asid, VA, result);
    }

    return result;
}

uint32_t hex_tlb_lookup_extended(CPUHexagonState *env, uint32_t ssr,
                                 uint64_t VA)
{
    HexagonCPU *cpu = env_archcpu(env);
    uint32_t result;

    if (VA < 4 * GiB) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "tlbpp (TLB Probe64) with size less than "
                      "4G. VA: 0x%016"PRIx64" PC: 0x%x\n", VA,
                      env->gpr[HEX_REG_PC]);
    }

    uint8_t asid = GET_SSR_FIELD(SSR_ASID, ssr);
    if (cpu->tlb) {
        uint32_t imprecise_exception = 0;
        int32_t cause_code = 0;
        result = hexagon_tlb_lookup_extended(cpu->tlb, asid, VA,
                                            &imprecise_exception, &cause_code);
        if (imprecise_exception) {
            env->imprecise_exception = imprecise_exception;
        }
        if (cause_code) {
            env->cause_code = cause_code;
        }
    } else {
        /* No TLB - return not found */
        result = 0x80000000;
    }

    if (result == 0x80000000) {
        qemu_log_mask(CPU_LOG_MMU,
                      "hex_tlb_lookup_extended: 0x%x, 0x%016"PRIx64
                      " => NOT FOUND\n",
                      asid, VA);
    } else {
        qemu_log_mask(CPU_LOG_MMU,
                      "hex_tlb_lookup_extended: 0x%x, 0x%016"PRIx64
                      " => %"PRId32"\n",
                      asid, VA, result);
    }

    return result;
}

/*
 * Return codes:
 * 0 or positive             index of match
 * -1                        multiple matches
 * -2                        no match
 */
int hex_tlb_check_overlap(CPUHexagonState *env, uint64_t entry, uint64_t index)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->tlb) {
        return hexagon_tlb_check_overlap(cpu->tlb, entry, index);
    }
    /* No TLB - no overlap */
    return -2;
}

static inline void print_thread(const char *str, CPUState *cs)
{
    g_assert(bql_locked());
    CPUHexagonState *thread = cpu_env(cs);
    bool is_stopped = cpu_is_stopped(cs);
    int exe_mode = get_exe_mode(thread);
    hex_lock_state_t lock_state = thread->tlb_lock_state;
    qemu_log_mask(CPU_LOG_MMU,
           "%s: threadId = %d: %s, exe_mode = %s, tlb_lock_state = %s\n",
           str,
           thread->threadId,
           is_stopped ? "stopped" : "running",
           exe_mode == HEX_EXE_MODE_OFF ? "off" :
           exe_mode == HEX_EXE_MODE_RUN ? "run" :
           exe_mode == HEX_EXE_MODE_WAIT ? "wait" :
           exe_mode == HEX_EXE_MODE_DEBUG ? "debug" :
           "unknown",
           lock_state == HEX_LOCK_UNLOCKED ? "unlocked" :
           lock_state == HEX_LOCK_WAITING ? "waiting" :
           lock_state == HEX_LOCK_OWNER ? "owner" :
           "unknown");
}

static inline void print_thread_states(const char *str)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        print_thread(str, cs);
    }
}

void hex_tlb_lock(CPUHexagonState *env)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (!cpu->tlb) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "TLB lock attempted but TLB not initialized\n");
        return;
    }

    qemu_log_mask(CPU_LOG_MMU, "hex_tlb_lock: %d\n", env->threadId);
    BQL_LOCK_GUARD();
    trace_hexagon_tlb_lock(env->threadId, env->next_PC, env->tlb_lock_count);
    g_assert((env->tlb_lock_count == 0) || (env->tlb_lock_count == 1));

    uint32_t syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);
    uint8_t tlb_lock = GET_SYSCFG_FIELD(SYSCFG_TLBLOCK, syscfg);
    if (tlb_lock) {
        if (env->tlb_lock_state == HEX_LOCK_QUEUED) {
            env->next_PC += 4;
            env->tlb_lock_count++;
            env->tlb_lock_state = HEX_LOCK_OWNER;
            SET_SYSCFG_FIELD(env, SYSCFG_TLBLOCK, 1);
            trace_hexagon_tlb_lock_info(env->threadId,
                                       "queued thread waiting gets tlb_lock\n");
            return;
        }
        if (env->tlb_lock_state == HEX_LOCK_OWNER) {
            qemu_log_mask(CPU_LOG_MMU | LOG_GUEST_ERROR,
                          "Double tlblock at PC: 0x%x, thread may hang\n",
                          env->next_PC);
            env->next_PC += 4;
            CPUState *cs = env_cpu(env);
            cpu_interrupt(cs, CPU_INTERRUPT_HALT);
            return;
        }
        trace_hexagon_tlb_lock_info(env->threadId, "Waiting for tlb_lock");
        env->tlb_lock_state = HEX_LOCK_WAITING;
        CPUState *cs = env_cpu(env);
        cpu_interrupt(cs, CPU_INTERRUPT_HALT);
    } else {
        trace_hexagon_tlb_lock_info(env->threadId, "Acquired tlb_lock");
        env->next_PC += 4;
        env->tlb_lock_count++;
        env->tlb_lock_state = HEX_LOCK_OWNER;
        SET_SYSCFG_FIELD(env, SYSCFG_TLBLOCK, 1);
    }

    if (qemu_loglevel_mask(CPU_LOG_MMU)) {
        qemu_log_mask(CPU_LOG_MMU, "Threads after hex_tlb_lock:\n");
        print_thread_states("\tThread");
    }
    trace_hexagon_tlb_lock_info(env->threadId, "After hex_tlb_lock\n");
}

void hex_tlb_unlock(CPUHexagonState *env)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (!cpu->tlb) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "TLB unlock attempted but TLB not initialized\n");
        return;
    }

    trace_hexagon_tlb_lock_info(env->threadId, "hex_tlb_unlock\n");
    BQL_LOCK_GUARD();
    trace_hexagon_tlb_lock(env->threadId, env->next_PC, env->tlb_lock_count);
    g_assert((env->tlb_lock_count == 0) || (env->tlb_lock_count == 1));

    /* Nothing to do if the TLB isn't locked by this thread */
    uint32_t syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);
    uint8_t tlb_lock = GET_SYSCFG_FIELD(SYSCFG_TLBLOCK, syscfg);
    if ((tlb_lock == 0) ||
        (env->tlb_lock_state != HEX_LOCK_OWNER)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "thread %d attempted to tlbunlock without having the "
                      "lock, tlb_lock state = %d\n",
                      env->threadId, env->tlb_lock_state);
        g_assert(env->tlb_lock_state != HEX_LOCK_WAITING);
        return;
    }

    trace_hexagon_tlb_lock_info(env->threadId, "Unlocking tlb_lock");
    env->tlb_lock_count--;
    env->tlb_lock_state = HEX_LOCK_UNLOCKED;
    SET_SYSCFG_FIELD(env, SYSCFG_TLBLOCK, 0);

    /* Look for a thread to unlock */
    unsigned int this_threadId = env->threadId;
    CPUHexagonState *unlock_thread = NULL;
    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *thread = cpu_env(cs);

        /*
         * The hardware implements round-robin fairness, so we look for threads
         * starting at env->threadId + 1 and incrementing modulo the number of
         * threads.
         *
         * To implement this, we check if thread is a earlier in the modulo
         * sequence than unlock_thread.
         *     if unlock thread is higher than this thread
         *         thread must be between this thread and unlock_thread
         *     else
         *         thread higher than this thread is ahead of unlock_thread
         *         thread must be lower then unlock thread
         */
        if (thread->tlb_lock_state == HEX_LOCK_WAITING) {
            if (!unlock_thread) {
                unlock_thread = thread;
            } else if (unlock_thread->threadId >
                       this_threadId) {
                if (this_threadId < thread->threadId &&
                    thread->threadId <
                        unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            } else {
                if (thread->threadId > this_threadId) {
                    unlock_thread = thread;
                }
                if (thread->threadId <
                        unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            }
        }
    }
    if (unlock_thread) {
        cs = env_cpu(unlock_thread);
        print_thread("\tWaiting thread found", cs);
        trace_hexagon_tlb_lock_info(
            unlock_thread->threadId,
            "Will get the next tlb_lock");
        unlock_thread->tlb_lock_state = HEX_LOCK_QUEUED;
        SET_SYSCFG_FIELD(unlock_thread, SYSCFG_TLBLOCK, 1);
        cpu_interrupt(cs, CPU_INTERRUPT_TLB_UNLOCK);
    }

    if (qemu_loglevel_mask(CPU_LOG_MMU)) {
        qemu_log_mask(CPU_LOG_MMU, "Threads after hex_tlb_unlock:\n");
        print_thread_states("\tThread");
    }

    trace_hexagon_tlb_lock_info(env->threadId, "After hex_tlb_unlock\n");
}

uint64_t hex_tlb_read(CPUHexagonState *env, uint32_t index)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->tlb) {
        uint32_t myidx = TLB_WRAP_INDEX(index);
        return hexagon_tlb_read(cpu->tlb, myidx);
    }
    /* No TLB - return 0 */
    return 0;
}

void dump_mmu(CPUHexagonState *env)
{
    HexagonCPU *cpu = env_archcpu(env);
    if (cpu->tlb) {
        hexagon_tlb_dump(cpu->tlb);
    } else {
        qemu_printf("TLB not initialized\n");
    }
}
