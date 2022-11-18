/*
 *  Copyright(c) 2019-2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "exec/helper-proto.h"
#include "fpu/softfloat.h"
#include "cpu.h"
#include "internal.h"
#include "macros.h"
#include "sys_macros.h"
#include "arch.h"
#include "hex_arch_types.h"
#include "op_helper.h"
#include "fma_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros_auto.h"
#include "mmvec/mmvec_qfloat.h"
#include "arch_options_calc.h"
#include "hmx/hmx.h"
#include "hmx/macros_auto.h"
#include "hmx/ext_hmx.h"
#include "hmx/hmx_int_ops.h"
#include "system.h"
#include "dma_adapter.h"
#ifndef CONFIG_USER_ONLY
#include "sysemu/cpus.h"
#include "hw/boards.h"
#include "hw/hexagon/hexagon.h"
#include "hex_mmu.h"
#include "hw/intc/l2vic.h"
#include "hw/timer/qct-qtimer.h"
#include "hex_interrupts.h"
#endif

#define SF_BIAS        127
#define SF_MANTBITS    23

#if COUNT_HEX_HELPERS
#include "opcodes.h"

typedef struct {
    int count;
    const char *tag;
} HelperCount;

HelperCount helper_counts[] = {
#define OPCODE(TAG)    { 0, #TAG }
#include "opcodes_def_generated.h.inc"
#undef OPCODE
    { 0, NULL }
};

#define COUNT_HELPER(TAG) \
    do { \
        helper_counts[(TAG)].count++; \
    } while (0)

void print_helper_counts(void)
{
    HelperCount *p;

    printf("HELPER COUNTS\n");
    for (p = helper_counts; p->tag; p++) {
        if (p->count) {
            printf("\t%d\t\t%s\n", p->count, p->tag);
        }
    }
}
#else
#define COUNT_HELPER(TAG)              /* Nothing */
#endif

/* Exceptions processing helpers */
G_NORETURN
void do_raise_exception_err(CPUHexagonState *env,
                            uint32_t exception,
                            uintptr_t pc)
{
    CPUState *cs = env_cpu(env);
#ifdef CONFIG_USER_ONLY
    qemu_log_mask(CPU_LOG_INT, "%s: %d\n", __func__, exception);
#else
    qemu_log_mask(CPU_LOG_INT, "%s: %d, @ %08x | %08lx, tbl =%d\n", __func__,
                  exception, env->gpr[HEX_REG_PC], pc,
                  env->gpr[HEX_REG_QEMU_CPU_TB_CNT]);
#endif

    cs->exception_index = exception;
    cpu_loop_exit_restore(cs, pc);
}


G_NORETURN void HELPER(raise_exception)(CPUHexagonState *env, uint32_t excp)
{
    do_raise_exception_err(env, excp, 0);
}

static void log_reg_write(CPUHexagonState *env, int rnum,
                          target_ulong val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_reg_write[%d] = " TARGET_FMT_ld " (0x" TARGET_FMT_lx ")",
                  rnum, val, val);
    if (val == env->gpr[rnum]) {
        HEX_DEBUG_LOG(" NO CHANGE");
    }
    HEX_DEBUG_LOG("\n");

    env->new_value[rnum] = val;
    if (HEX_DEBUG) {
        /* Do this so HELPER(debug_commit_end) will know */
        env->reg_written[rnum] = 1;
    }
}

static void log_pred_write(CPUHexagonState *env, int pnum, target_ulong val)
{
    HEX_DEBUG_LOG("log_pred_write[%d] = " TARGET_FMT_ld
                  " (0x" TARGET_FMT_lx ")\n",
                  pnum, val, val);

    /* Multiple writes to the same preg are and'ed together */
    if (env->pred_written & (1 << pnum)) {
        env->new_pred_value[pnum] &= val & 0xff;
    } else {
        env->new_pred_value[pnum] = val & 0xff;
        env->pred_written |= 1 << pnum;
    }
}

static void write_new_pc(CPUHexagonState *env, bool pkt_has_multi_cof,
                         target_ulong addr)
{
    HEX_DEBUG_LOG("write_new_pc(0x" TARGET_FMT_lx ")\n", addr);

    if (pkt_has_multi_cof) {
        /*
         * If more than one branch is taken in a packet, only the first one
         * is actually done.
         */
        if (env->branch_taken) {
            HEX_DEBUG_LOG("INFO: multiple branches taken in same packet, "
                          "ignoring the second one\n");
        } else {
            fCHECK_PCALIGN(addr);
            env->branch_taken = 1;
            env->next_PC = addr;
        }
    } else {
        fCHECK_PCALIGN(addr);
        env->next_PC = addr;
    }
}

/* Handy place to set a breakpoint */
void HELPER(debug_start_packet)(CPUHexagonState *env)
{
#ifdef CONFIG_USER_ONLY
    HEX_DEBUG_LOG("Start packet: pc = 0x" TARGET_FMT_lx "\n",
                  env->gpr[HEX_REG_PC]);
#else
    HEX_DEBUG_LOG("Start packet: pc = 0x" TARGET_FMT_lx " tid = %d\n",
                  env->gpr[HEX_REG_PC], env->threadId);
#endif

    for (int i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        env->reg_written[i] = 0;
    }
#ifndef CONFIG_USER_ONLY
    for (int i = 0; i < NUM_GREGS; i++) {
        env->greg_written[i] = 0;
    }

    for (int i = 0; i < NUM_SREGS; i++) {
        if (i < HEX_SREG_GLB_START) {
            env->t_sreg_written[i] = 0;
        }
    }
#endif
}

/* Checks for bookkeeping errors between disassembly context and runtime */
void HELPER(debug_check_store_width)(CPUHexagonState *env, int slot, int check)
{
    if (env->mem_log_stores[slot].width != check) {
        HEX_DEBUG_LOG("ERROR: %d != %d\n",
                      env->mem_log_stores[slot].width, check);
        g_assert_not_reached();
    }
}

void HELPER(commit_store)(CPUHexagonState *env, int slot_num)
{
    uintptr_t ra = GETPC();
    uint8_t width = env->mem_log_stores[slot_num].width;
    target_ulong va = env->mem_log_stores[slot_num].va;

    switch (width) {
    case 1:
        cpu_stb_data_ra(env, va, env->mem_log_stores[slot_num].data32, ra);
        break;
    case 2:
        cpu_stw_data_ra(env, va, env->mem_log_stores[slot_num].data32, ra);
        break;
    case 4:
        cpu_stl_data_ra(env, va, env->mem_log_stores[slot_num].data32, ra);
        break;
    case 8:
        cpu_stq_data_ra(env, va, env->mem_log_stores[slot_num].data64, ra);
        break;
    default:
        g_assert_not_reached();
    }
}

void HELPER(gather_store)(CPUHexagonState *env, uint32_t addr, int slot)
{
    mem_gather_store(env, addr, slot);
}

void HELPER(commit_hvx_stores)(CPUHexagonState *env)
{
    uintptr_t ra = GETPC();
    int i;

    /* Normal (possibly masked) vector store */
    for (i = 0; i < VSTORES_MAX; i++) {
        if (env->vstore_pending[i]) {
            env->vstore_pending[i] = 0;
            target_ulong va = env->vstore[i].va;
            int size = env->vstore[i].size;
            for (int j = 0; j < size; j++) {
                if (test_bit(j, env->vstore[i].mask)) {
                    cpu_stb_data_ra(env, va + j, env->vstore[i].data.ub[j], ra);
                }
            }
        }
    }

    /* Scatter store */
    if (env->vtcm_pending) {
        env->vtcm_pending = false;
        if (env->vtcm_log.op) {
            /* Need to perform the scatter read/modify/write at commit time */
            if (env->vtcm_log.op_size == 2) {
                SCATTER_OP_WRITE_TO_MEM(uint16_t);
            } else if (env->vtcm_log.op_size == 4) {
                /* Word Scatter += */
                SCATTER_OP_WRITE_TO_MEM(uint32_t);
            } else {
                g_assert_not_reached();
            }
        } else {
            for (i = 0; i < sizeof(MMVector); i++) {
                if (test_bit(i, env->vtcm_log.mask)) {
                    cpu_stb_data_ra(env, env->vtcm_log.va[i],
                                    env->vtcm_log.data.ub[i], ra);
                    clear_bit(i, env->vtcm_log.mask);
                    env->vtcm_log.data.ub[i] = 0;
#ifndef CONFIG_USER_ONLY
                    env->vtcm_log.offsets.ub[i] = 0;
#endif
                }

            }
        }
    }
}

static void print_store(CPUHexagonState *env, int slot)
{
    if (!(env->slot_cancelled & (1 << slot))) {
        uint8_t width = env->mem_log_stores[slot].width;
        if (width == 1) {
            uint32_t data = env->mem_log_stores[slot].data32 & 0xff;
            HEX_DEBUG_LOG("\tmemb[0x" TARGET_FMT_lx "] = %" PRId32
                          " (0x%02" PRIx32 ")\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 2) {
            uint32_t data = env->mem_log_stores[slot].data32 & 0xffff;
            HEX_DEBUG_LOG("\tmemh[0x" TARGET_FMT_lx "] = %" PRId32
                          " (0x%04" PRIx32 ")\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 4) {
            uint32_t data = env->mem_log_stores[slot].data32;
            HEX_DEBUG_LOG("\tmemw[0x" TARGET_FMT_lx "] = %" PRId32
                          " (0x%08" PRIx32 ")\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 8) {
            HEX_DEBUG_LOG("\tmemd[0x" TARGET_FMT_lx "] = %" PRId64
                          " (0x%016" PRIx64 ")\n",
                          env->mem_log_stores[slot].va,
                          env->mem_log_stores[slot].data64,
                          env->mem_log_stores[slot].data64);
        } else {
            HEX_DEBUG_LOG("\tBad store width %d\n", width);
            g_assert_not_reached();
        }
    }
}

/* This function is a handy place to set a breakpoint */
void HELPER(debug_commit_end)(CPUHexagonState *env, int has_st0, int has_st1)
{
    bool reg_printed = false;
    bool pred_printed = false;
    int i;

#ifdef CONFIG_USER_ONLY
    HEX_DEBUG_LOG("Packet committed: pc = 0x" TARGET_FMT_lx "\n",
                  env->this_PC);
#else
    HEX_DEBUG_LOG("Packet committed: tid = %d, pc = 0x" TARGET_FMT_lx "\n",
                  env->threadId, env->this_PC);
#endif
    HEX_DEBUG_LOG("slot_cancelled = %d\n", env->slot_cancelled);

    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        if (env->reg_written[i]) {
            if (!reg_printed) {
                HEX_DEBUG_LOG("Regs written\n");
                reg_printed = true;
            }
            HEX_DEBUG_LOG("\tr%d = " TARGET_FMT_ld " (0x" TARGET_FMT_lx " )\n",
                          i, env->new_value[i], env->new_value[i]);
        }
    }

#ifndef CONFIG_USER_ONLY
    bool greg_printed = false;
    for (i = 0; i < NUM_GREGS; i++) {
        if (env->greg_written[i]) {
            if (!greg_printed) {
                HEX_DEBUG_LOG("GRegs written\n");
                greg_printed = true;
            }
            HEX_DEBUG_LOG("\tset g%d (%s) = " TARGET_FMT_ld
                " (0x" TARGET_FMT_lx " )\n",
                i, hexagon_gregnames[i], env->greg_new_value[i],
                env->greg_new_value[i]);
        }
    }

    bool sreg_printed = false;
    for (i = 0; i < NUM_SREGS; i++) {
        if (i < HEX_SREG_GLB_START) {
            if (env->t_sreg_written[i]) {
                if (!sreg_printed) {
                    HEX_DEBUG_LOG("SRegs written\n");
                    sreg_printed = true;
                }
                HEX_DEBUG_LOG("\tset s%d (%s) = " TARGET_FMT_ld
                    " (0x" TARGET_FMT_lx " )\n",
                    i, hexagon_sregnames[i], env->t_sreg_new_value[i],
                    env->t_sreg_new_value[i]);
            }
        }
    }
#endif

    for (i = 0; i < NUM_PREGS; i++) {
        if (env->pred_written & (1 << i)) {
            if (!pred_printed) {
                HEX_DEBUG_LOG("Predicates written\n");
                pred_printed = true;
            }
            HEX_DEBUG_LOG("\tp%d = 0x" TARGET_FMT_lx "\n",
                          i, env->new_pred_value[i]);
        }
    }

    if (has_st0 || has_st1) {
        HEX_DEBUG_LOG("Stores\n");
        if (has_st0) {
            print_store(env, 0);
        }
        if (has_st1) {
            print_store(env, 1);
        }
    }

#ifdef CONFIG_USER_ONLY
    HEX_DEBUG_LOG("Next PC = " TARGET_FMT_lx "\n", env->next_PC);
#else
    HEX_DEBUG_LOG("tid[%d], Next PC = 0x%x\n", env->threadId, env->next_PC);
#endif
    HEX_DEBUG_LOG("Exec counters: pkt = " TARGET_FMT_lx
                  ", insn = " TARGET_FMT_lx
                  ", hvx = " TARGET_FMT_lx "\n",
                  env->gpr[HEX_REG_QEMU_PKT_CNT],
                  env->gpr[HEX_REG_QEMU_INSN_CNT],
                  env->gpr[HEX_REG_QEMU_HVX_CNT]);

}

int32_t HELPER(fcircadd)(int32_t RxV, int32_t offset, int32_t M, int32_t CS)
{
    uint32_t K_const = extract32(M, 24, 4);
    uint32_t length = extract32(M, 0, 17);
    uint32_t new_ptr = RxV + offset;
    uint32_t start_addr;
    uint32_t end_addr;

    if (K_const == 0 && length >= 4) {
        start_addr = CS;
        end_addr = start_addr + length;
    } else {
        /*
         * Versions v3 and earlier used the K value to specify a power-of-2 size
         * 2^(K+2) that is greater than the buffer length
         */
        int32_t mask = (1 << (K_const + 2)) - 1;
        start_addr = RxV & (~mask);
        end_addr = start_addr | length;
    }

    if (new_ptr >= end_addr) {
        new_ptr -= length;
    } else if (new_ptr < start_addr) {
        new_ptr += length;
    }

    return new_ptr;
}

uint32_t HELPER(fbrev)(uint32_t addr)
{
    /*
     *  Bit reverse the low 16 bits of the address
     */
    return deposit32(addr, 0, 16, revbit16(addr));
}

static float32 build_float32(uint8_t sign, uint32_t exp, uint32_t mant)
{
    return make_float32(
        ((sign & 1) << 31) |
        ((exp & 0xff) << SF_MANTBITS) |
        (mant & ((1 << SF_MANTBITS) - 1)));
}

/*
 * sfrecipa, sfinvsqrta have two 32-bit results
 *     r0,p0=sfrecipa(r1,r2)
 *     r0,p0=sfinvsqrta(r1)
 *
 * Since helpers can only return a single value, we pack the two results
 * into a 64-bit value.
 */
uint64_t HELPER(sfrecipa)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    int32_t PeV = 0;
    float32 RdV;
    int idx;
    int adjust;
    int mant;
    int exp;

    arch_fpop_start(env);
    if (arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status)) {
        PeV = adjust;
        idx = (RtV >> 16) & 0x7f;
        mant = (recip_lookup_table[idx] << 15) | 1;
        exp = SF_BIAS - (float32_getexp(RtV) - SF_BIAS) - 1;
        RdV = build_float32(extract32(RtV, 31, 1), exp, mant);
    }
    arch_fpop_end(env);
    return ((uint64_t)RdV << 32) | PeV;
}

uint64_t HELPER(sfinvsqrta)(CPUHexagonState *env, float32 RsV)
{
    int PeV = 0;
    float32 RdV;
    int idx;
    int adjust;
    int mant;
    int exp;

    arch_fpop_start(env);
    if (arch_sf_invsqrt_common(&RsV, &RdV, &adjust, &env->fp_status)) {
        PeV = adjust;
        idx = (RsV >> 17) & 0x7f;
        mant = (invsqrt_lookup_table[idx] << 15);
        exp = SF_BIAS - ((float32_getexp(RsV) - SF_BIAS) >> 1) - 1;
        RdV = build_float32(extract32(RsV, 31, 1), exp, mant);
    }
    arch_fpop_end(env);
    return ((uint64_t)RdV << 32) | PeV;
}

int64_t HELPER(vacsh_val)(CPUHexagonState *env,
                           int64_t RxxV, int64_t RssV, int64_t RttV)
{
    for (int i = 0; i < 4; i++) {
        int xv = sextract64(RxxV, i * 16, 16);
        int sv = sextract64(RssV, i * 16, 16);
        int tv = sextract64(RttV, i * 16, 16);
        int max;
        xv = xv + tv;
        sv = sv - tv;
        max = xv > sv ? xv : sv;
        /* Note that fSATH can set the OVF bit in usr */
        RxxV = deposit64(RxxV, i * 16, 16, fSATH(max));
    }
    return RxxV;
}

int32_t HELPER(vacsh_pred)(CPUHexagonState *env,
                           int64_t RxxV, int64_t RssV, int64_t RttV)
{
    int32_t PeV = 0;
    for (int i = 0; i < 4; i++) {
        int xv = sextract64(RxxV, i * 16, 16);
        int sv = sextract64(RssV, i * 16, 16);
        int tv = sextract64(RttV, i * 16, 16);
        xv = xv + tv;
        sv = sv - tv;
        PeV = deposit32(PeV, i * 2, 1, (xv > sv));
        PeV = deposit32(PeV, i * 2 + 1, 1, (xv > sv));
    }
    return PeV;
}

#ifndef CONFIG_USER_ONLY
void HELPER(data_cache_op)(CPUHexagonState *env, target_ulong RsV,
                           int slot, int mmu_idx)
{
    if (hexagon_cpu_mmu_enabled(env)) {
        hwaddr phys;
        int prot;
        int size;
        int32_t excp;
        /* Look for a match in the TLB */
        if (hex_tlb_find_match(env, RsV, MMU_DATA_LOAD, &phys, &prot, &size,
                               &excp, mmu_idx)) {
            if (excp == HEX_EVENT_PRECISE) {
                /*
                 * If a matching entry was found but doesn't have read or write
                 * permission, raise a permission execption
                 */
                bool read_perm = (prot & (PAGE_VALID | PAGE_READ)) ==
                                 (PAGE_VALID | PAGE_READ);
                bool write_perm = (prot & (PAGE_VALID | PAGE_WRITE)) ==
                                  (PAGE_VALID | PAGE_WRITE);
                if (!read_perm && !write_perm) {
                    CPUState *cs = env_cpu(env);
                    uintptr_t retaddr = GETPC();
                    raise_perm_exception(cs, RsV, slot, MMU_DATA_LOAD, excp);
                    do_raise_exception_err(env, cs->exception_index, retaddr);
                }
            }
        } else {
            /* If no TLB match found, raise a TLB miss exception */
            CPUState *cs = env_cpu(env);
            uintptr_t retaddr = GETPC();
            raise_tlbmiss_exception(cs, RsV, slot, MMU_DATA_LOAD);
            do_raise_exception_err(env, cs->exception_index, retaddr);
        }
    }
}

void HELPER(insn_cache_op)(CPUHexagonState *env, target_ulong RsV,
                           int slot, int mmu_idx)
{
    if (hexagon_cpu_mmu_enabled(env)) {
        hwaddr phys;
        int prot;
        int size;
        int32_t excp;
        /*
         * Look for a match in the TLB
         * Note that insn cache ops do NOT raise the privilege exception
         */
        if (!hex_tlb_find_match(env, RsV, MMU_INST_FETCH, &phys, &prot, &size,
                                &excp, mmu_idx)) {
            /* If not TLB match found, raise a TLB miss exception */
            CPUState *cs = env_cpu(env);
            uintptr_t retaddr = GETPC();
            raise_tlbmiss_exception(cs, RsV, slot, MMU_INST_FETCH);
            do_raise_exception_err(env, cs->exception_index, retaddr);
        }
    }
}
#endif

static void probe_store(CPUHexagonState *env, int slot, int mmu_idx)
{
    if (!(env->slot_cancelled & (1 << slot))) {
        size1u_t width = env->mem_log_stores[slot].width;
        target_ulong va = env->mem_log_stores[slot].va;
        uintptr_t ra = GETPC();
        probe_write(env, va, width, mmu_idx, ra);
    }
}

/* Called during packet commit when there are two scalar stores */
void HELPER(probe_pkt_scalar_store_s0)(CPUHexagonState *env, int mmu_idx)
{
    probe_store(env, 0, mmu_idx);
}

void HELPER(probe_hvx_stores)(CPUHexagonState *env, int mmu_idx)
{
    uintptr_t retaddr = GETPC();
    int i;

    /* Normal (possibly masked) vector store */
    for (i = 0; i < VSTORES_MAX; i++) {
        if (env->vstore_pending[i]) {
            target_ulong va = env->vstore[i].va;
            int size = env->vstore[i].size;
            for (int j = 0; j < size; j++) {
                if (test_bit(j, env->vstore[i].mask)) {
                    probe_write(env, va + j, 1, mmu_idx, retaddr);
                }
            }
        }
    }

    /* Scatter store */
    if (env->vtcm_pending) {
        if (env->vtcm_log.op) {
            /* Need to perform the scatter read/modify/write at commit time */
            if (env->vtcm_log.op_size == 2) {
                SCATTER_OP_PROBE_MEM(size2u_t, mmu_idx, retaddr);
            } else if (env->vtcm_log.op_size == 4) {
                /* Word Scatter += */
                SCATTER_OP_PROBE_MEM(size4u_t, mmu_idx, retaddr);
            } else {
                g_assert_not_reached();
            }
        } else {
            for (int i = 0; i < sizeof(MMVector); i++) {
                if (test_bit(i, env->vtcm_log.mask)) {
                    probe_write(env, env->vtcm_log.va[i], 1, mmu_idx, retaddr);
                }

            }
        }
    }
}

void HELPER(probe_pkt_scalar_hvx_stores)(CPUHexagonState *env, int mask,
                                         int mmu_idx)
{
    bool has_st0        = (mask >> 0) & 1;
    bool has_st1        = (mask >> 1) & 1;
    bool has_hvx_stores = (mask >> 2) & 1;

    if (has_st0) {
        probe_store(env, 0, mmu_idx);
    }
    if (has_st1) {
        probe_store(env, 1, mmu_idx);
    }
    if (has_hvx_stores) {
        HELPER(probe_hvx_stores)(env, mmu_idx);
    }
}

/* Floating point */
float64 HELPER(conv_sf2df)(CPUHexagonState *env, float32 RsV)
{
    float64 out_f64;
    arch_fpop_start(env);
    out_f64 = float32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env);
    return out_f64;
}

float32 HELPER(conv_df2sf)(CPUHexagonState *env, float64 RssV)
{
    float32 out_f32;
    arch_fpop_start(env);
    out_f32 = float64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env);
    return out_f32;
}

float32 HELPER(conv_uw2sf)(CPUHexagonState *env, int32_t RsV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = uint32_to_float32(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_uw2df)(CPUHexagonState *env, int32_t RsV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = uint32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(conv_w2sf)(CPUHexagonState *env, int32_t RsV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = int32_to_float32(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_w2df)(CPUHexagonState *env, int32_t RsV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = int32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(conv_ud2sf)(CPUHexagonState *env, int64_t RssV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = uint64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_ud2df)(CPUHexagonState *env, int64_t RssV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = uint64_to_float64(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(conv_d2sf)(CPUHexagonState *env, int64_t RssV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = int64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_d2df)(CPUHexagonState *env, int64_t RssV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = int64_to_float64(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

uint32_t HELPER(conv_sf2uw)(CPUHexagonState *env, float32 RsV)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float32_to_uint32(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_sf2w)(CPUHexagonState *env, float32 RsV)
{
    int32_t RdV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = -1;
    } else {
        RdV = float32_to_int32(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

uint64_t HELPER(conv_sf2ud)(CPUHexagonState *env, float32 RsV)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float32_to_uint64(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_sf2d)(CPUHexagonState *env, float32 RsV)
{
    int64_t RddV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = -1;
    } else {
        RddV = float32_to_int64(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

uint32_t HELPER(conv_df2uw)(CPUHexagonState *env, float64 RssV)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float64_to_uint32(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_df2w)(CPUHexagonState *env, float64 RssV)
{
    int32_t RdV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = -1;
    } else {
        RdV = float64_to_int32(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

uint64_t HELPER(conv_df2ud)(CPUHexagonState *env, float64 RssV)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float64_to_uint64(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_df2d)(CPUHexagonState *env, float64 RssV)
{
    int64_t RddV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = -1;
    } else {
        RddV = float64_to_int64(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

uint32_t HELPER(conv_sf2uw_chop)(CPUHexagonState *env, float32 RsV)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float32_to_uint32_round_to_zero(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_sf2w_chop)(CPUHexagonState *env, float32 RsV)
{
    int32_t RdV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = -1;
    } else {
        RdV = float32_to_int32_round_to_zero(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

uint64_t HELPER(conv_sf2ud_chop)(CPUHexagonState *env, float32 RsV)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float32_to_uint64_round_to_zero(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_sf2d_chop)(CPUHexagonState *env, float32 RsV)
{
    int64_t RddV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float32_is_any_nan(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = -1;
    } else {
        RddV = float32_to_int64_round_to_zero(RsV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

uint32_t HELPER(conv_df2uw_chop)(CPUHexagonState *env, float64 RssV)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float64_to_uint32_round_to_zero(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_df2w_chop)(CPUHexagonState *env, float64 RssV)
{
    int32_t RdV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = -1;
    } else {
        RdV = float64_to_int32_round_to_zero(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RdV;
}

uint64_t HELPER(conv_df2ud_chop)(CPUHexagonState *env, float64 RssV)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float64_to_uint64_round_to_zero(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_df2d_chop)(CPUHexagonState *env, float64 RssV)
{
    int64_t RddV;
    arch_fpop_start(env);
    /* Hexagon returns -1 for NaN */
    if (float64_is_any_nan(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = -1;
    } else {
        RddV = float64_to_int64_round_to_zero(RssV, &env->fp_status);
    }
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(sfadd)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_add(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sfsub)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_sub(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(sfcmpeq)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float32_eq_quiet(RsV, RtV, &env->fp_status));
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(sfcmpgt)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float32_compare_quiet(RsV, RtV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(sfcmpge)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float32_compare_quiet(RsV, RtV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater ||
                   cmp == float_relation_equal);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(sfcmpuo)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float32_unordered_quiet(RsV, RtV, &env->fp_status));
    arch_fpop_end(env);
    return PdV;
}

float32 HELPER(sfmax)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_maximum_number(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sfmin)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_minimum_number(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(sfclass)(CPUHexagonState *env, float32 RsV, int32_t uiV)
{
    int32_t PdV = 0;
    arch_fpop_start(env);
    if (fGETBIT(0, uiV) && float32_is_zero(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(1, uiV) && float32_is_normal(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(2, uiV) && float32_is_denormal(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(3, uiV) && float32_is_infinity(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(4, uiV) && float32_is_any_nan(RsV)) {
        PdV = 0xff;
    }
    set_float_exception_flags(0, &env->fp_status);
    arch_fpop_end(env);
    return PdV;
}

float32 HELPER(sffixupn)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV = 0;
    int adjust;
    arch_fpop_start(env);
    arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status);
    RdV = RsV;
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sffixupd)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV = 0;
    int adjust;
    arch_fpop_start(env);
    arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status);
    RdV = RtV;
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sffixupr)(CPUHexagonState *env, float32 RsV)
{
    float32 RdV = 0;
    int adjust;
    arch_fpop_start(env);
    arch_sf_invsqrt_common(&RsV, &RdV, &adjust, &env->fp_status);
    RdV = RsV;
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(dfadd)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_add(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfsub)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_sub(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfmax)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_maximum_number(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfmin)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_minimum_number(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int32_t HELPER(dfcmpeq)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float64_eq_quiet(RssV, RttV, &env->fp_status));
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfcmpgt)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float64_compare_quiet(RssV, RttV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfcmpge)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float64_compare_quiet(RssV, RttV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater ||
                   cmp == float_relation_equal);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfcmpuo)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float64_unordered_quiet(RssV, RttV, &env->fp_status));
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfclass)(CPUHexagonState *env, float64 RssV, int32_t uiV)
{
    int32_t PdV = 0;
    arch_fpop_start(env);
    if (fGETBIT(0, uiV) && float64_is_zero(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(1, uiV) && float64_is_normal(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(2, uiV) && float64_is_denormal(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(3, uiV) && float64_is_infinity(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(4, uiV) && float64_is_any_nan(RssV)) {
        PdV = 0xff;
    }
    set_float_exception_flags(0, &env->fp_status);
    arch_fpop_end(env);
    return PdV;
}

float32 HELPER(sfmpy)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = internal_mpyf(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sffma)(CPUHexagonState *env, float32 RxV,
                      float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    RxV = internal_fmafx(RsV, RtV, RxV, 0, &env->fp_status);
    arch_fpop_end(env);
    return RxV;
}

static bool is_zero_prod(float32 a, float32 b)
{
    return ((float32_is_zero(a) && is_finite(b)) ||
            (float32_is_zero(b) && is_finite(a)));
}

static float32 check_nan(float32 dst, float32 x, float_status *fp_status)
{
    float32 ret = dst;
    if (float32_is_any_nan(x)) {
        if (extract32(x, 22, 1) == 0) {
            float_raise(float_flag_invalid, fp_status);
        }
        ret = make_float32(0xffffffff);    /* nan */
    }
    return ret;
}

float32 HELPER(sffma_sc)(CPUHexagonState *env, float32 RxV,
                         float32 RsV, float32 RtV, float32 PuV)
{
    size4s_t tmp;
    arch_fpop_start(env);
    RxV = check_nan(RxV, RxV, &env->fp_status);
    RxV = check_nan(RxV, RsV, &env->fp_status);
    RxV = check_nan(RxV, RtV, &env->fp_status);
    tmp = internal_fmafx(RsV, RtV, RxV, fSXTN(8, 64, PuV), &env->fp_status);
    if (!(float32_is_zero(RxV) && is_zero_prod(RsV, RtV))) {
        RxV = tmp;
    }
    arch_fpop_end(env);
    return RxV;
}

float32 HELPER(sffms)(CPUHexagonState *env, float32 RxV,
                      float32 RsV, float32 RtV)
{
    float32 neg_RsV;
    arch_fpop_start(env);
    neg_RsV = float32_sub(float32_zero, RsV, &env->fp_status);
    RxV = internal_fmafx(neg_RsV, RtV, RxV, 0, &env->fp_status);
    arch_fpop_end(env);
    return RxV;
}

static bool is_inf_prod(int32_t a, int32_t b)
{
    return (float32_is_infinity(a) && float32_is_infinity(b)) ||
           (float32_is_infinity(a) && is_finite(b) && !float32_is_zero(b)) ||
           (float32_is_infinity(b) && is_finite(a) && !float32_is_zero(a));
}

float32 HELPER(sffma_lib)(CPUHexagonState *env, float32 RxV,
                          float32 RsV, float32 RtV)
{
    bool infinp;
    bool infminusinf;
    float32 tmp;

    arch_fpop_start(env);
    set_float_rounding_mode(float_round_nearest_even, &env->fp_status);
    infminusinf = float32_is_infinity(RxV) &&
                  is_inf_prod(RsV, RtV) &&
                  (fGETBIT(31, RsV ^ RxV ^ RtV) != 0);
    infinp = float32_is_infinity(RxV) ||
             float32_is_infinity(RtV) ||
             float32_is_infinity(RsV);
    RxV = check_nan(RxV, RxV, &env->fp_status);
    RxV = check_nan(RxV, RsV, &env->fp_status);
    RxV = check_nan(RxV, RtV, &env->fp_status);
    tmp = internal_fmafx(RsV, RtV, RxV, 0, &env->fp_status);
    if (!(float32_is_zero(RxV) && is_zero_prod(RsV, RtV))) {
        RxV = tmp;
    }
    set_float_exception_flags(0, &env->fp_status);
    if (float32_is_infinity(RxV) && !infinp) {
        RxV = RxV - 1;
    }
    if (infminusinf) {
        RxV = 0;
    }
    arch_fpop_end(env);
    return RxV;
}

float32 HELPER(sffms_lib)(CPUHexagonState *env, float32 RxV,
                          float32 RsV, float32 RtV)
{
    bool infinp;
    bool infminusinf;
    float32 tmp;

    arch_fpop_start(env);
    set_float_rounding_mode(float_round_nearest_even, &env->fp_status);
    infminusinf = float32_is_infinity(RxV) &&
                  is_inf_prod(RsV, RtV) &&
                  (fGETBIT(31, RsV ^ RxV ^ RtV) == 0);
    infinp = float32_is_infinity(RxV) ||
             float32_is_infinity(RtV) ||
             float32_is_infinity(RsV);
    RxV = check_nan(RxV, RxV, &env->fp_status);
    RxV = check_nan(RxV, RsV, &env->fp_status);
    RxV = check_nan(RxV, RtV, &env->fp_status);
    float32 minus_RsV = float32_sub(float32_zero, RsV, &env->fp_status);
    tmp = internal_fmafx(minus_RsV, RtV, RxV, 0, &env->fp_status);
    if (!(float32_is_zero(RxV) && is_zero_prod(RsV, RtV))) {
        RxV = tmp;
    }
    set_float_exception_flags(0, &env->fp_status);
    if (float32_is_infinity(RxV) && !infinp) {
        RxV = RxV - 1;
    }
    if (infminusinf) {
        RxV = 0;
    }
    arch_fpop_end(env);
    return RxV;
}

float64 HELPER(dfmpyfix)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    int64_t RddV;
    arch_fpop_start(env);
    if (float64_is_denormal(RssV) &&
        (float64_getexp(RttV) >= 512) &&
        float64_is_normal(RttV)) {
        RddV = float64_mul(RssV, make_float64(0x4330000000000000),
                           &env->fp_status);
    } else if (float64_is_denormal(RttV) &&
               (float64_getexp(RssV) >= 512) &&
               float64_is_normal(RssV)) {
        RddV = float64_mul(RssV, make_float64(0x3cb0000000000000),
                           &env->fp_status);
    } else {
        RddV = RssV;
    }
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfmpyhh)(CPUHexagonState *env, float64 RxxV,
                        float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    RxxV = internal_mpyhh(RssV, RttV, RxxV, &env->fp_status);
    arch_fpop_end(env);
    return RxxV;
}

#ifndef CONFIG_USER_ONLY
void HELPER(raise_stack_overflow)(CPUHexagonState *env, uint32_t slot,
                                  target_ulong badva)
{
    /*
     * Per section 7.3.1 of the V67 Programmer's Reference,
     * stack limit exception isn't raised in monotor mode.
     */
    if (sys_in_monitor_mode(env)) {
        return;
    }

    CPUState *cs = env_cpu(env);
    cs->exception_index = HEX_EVENT_PRECISE;
    env->cause_code = HEX_CAUSE_STACK_LIMIT;

    if (slot == 0) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA0, badva);
        SET_SSR_FIELD(env, SSR_V0, 1);
        SET_SSR_FIELD(env, SSR_V1, 0);
        SET_SSR_FIELD(env, SSR_BVS, 0);
    } else if (slot == 1) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA1, badva);
        SET_SSR_FIELD(env, SSR_V0, 0);
        SET_SSR_FIELD(env, SSR_V1, 1);
        SET_SSR_FIELD(env, SSR_BVS, 1);
    } else {
        g_assert_not_reached();
    }
    cpu_loop_exit_restore(cs, 0);
}
#endif

#ifndef CONFIG_USER_ONLY
static void log_sreg_write(CPUHexagonState *env, int rnum,
                                 target_ulong val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_sreg_write[%s/%d] = " TARGET_FMT_ld
        " (0x" TARGET_FMT_lx ")", hexagon_sregnames[rnum], rnum, val, val);
    if (env->slot_cancelled & (1 << slot)) {
        HEX_DEBUG_LOG(" CANCELLED");
    }

    target_ulong crnt_val = ARCH_GET_SYSTEM_REG(env, rnum);
    if (val == crnt_val) {
        HEX_DEBUG_LOG(" NO CHANGE");
    }
    HEX_DEBUG_LOG("\n");
    if (!(env->slot_cancelled & (1 << slot))) {
        HEX_DEBUG_LOG("\tsreg register set to 0x%x\n", val);
        if (rnum < HEX_SREG_GLB_START) {
            env->t_sreg_new_value[rnum] = val;
        } else {
            env->g_sreg[rnum] = val;
        }
    }
    else
      HEX_DEBUG_LOG("\tsreg register not set\n");
}

#endif

void HELPER(debug_print_vec)(CPUHexagonState *env, int rnum, void *vecptr)

{
    unsigned char *vec = (unsigned char *)vecptr;
    printf("vec[%d] = 0x", rnum);
    for (int i = MAX_VEC_SIZE_BYTES - 2; i >= 0; i--) {
        printf("%02x", vec[i]);
    }
    printf("\n");
}



#ifndef CONFIG_USER_ONLY
void HELPER(modify_ssr)(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    hexagon_modify_ssr(env, new, old);
}
#endif


#ifndef CONFIG_USER_ONLY
#if HEX_DEBUG
static void print_thread(const char *str, CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *thread = &cpu->env;
    bool is_stopped = cpu_is_stopped(cs);
    int exe_mode = get_exe_mode(thread);
    hex_lock_state_t lock_state = thread->k0_lock_state;
    HEX_DEBUG_LOG("%s: threadId = %d: %s, exe_mode = %s, k0_lock_state = %s\n",
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

static void print_thread_states(const char *str)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        print_thread(str, cs);
    }
}
#else
static void print_thread(const char *str, CPUState *cs)
{
}
static void print_thread_states(const char *str)
{
}
#endif

static void hex_k0_lock(CPUHexagonState *env)
{
    HEX_DEBUG_LOG("Before hex_k0_lock: %d\n", env->threadId);
    print_thread_states("\tThread");
    const bool exception_context = qemu_mutex_iothread_locked();
    if (!exception_context) qemu_mutex_lock_iothread();

    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    if (GET_SYSCFG_FIELD(SYSCFG_K0LOCK, syscfg)) {
        if (env->k0_lock_state == HEX_LOCK_OWNER) {
            HEX_DEBUG_LOG("Already the owner\n");
            if (!exception_context) qemu_mutex_unlock_iothread();
            return;
        }
        HEX_DEBUG_LOG("\tWaiting\n");
        env->k0_lock_state = HEX_LOCK_WAITING;
        cpu_exit(env_cpu(env));
    } else {
        HEX_DEBUG_LOG("\tAcquired\n");
        env->k0_lock_state = HEX_LOCK_OWNER;
        SET_SYSCFG_FIELD(env, SYSCFG_K0LOCK, 1);
    }

    if (!exception_context) qemu_mutex_unlock_iothread();
    HEX_DEBUG_LOG("After hex_k0_lock: %d\n", env->threadId);
    print_thread_states("\tThread");
}

static void hex_k0_unlock(CPUHexagonState *env)
{
    HEX_DEBUG_LOG("Before hex_k0_unlock: %d\n", env->threadId);
    print_thread_states("\tThread");
    qemu_mutex_lock_iothread();

    /* Nothing to do if the k0 isn't locked by this thread */
    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    if ((GET_SYSCFG_FIELD(SYSCFG_K0LOCK, syscfg) == 0) ||
         (env->k0_lock_state != HEX_LOCK_OWNER)) {
        qemu_log_mask(LOG_GUEST_ERROR,
            "thread %d attempted to unlock k0 without having the lock\n",
            env->threadId);
        qemu_mutex_unlock_iothread();
        return;
    }

    env->k0_lock_state = HEX_LOCK_UNLOCKED;
    SET_SYSCFG_FIELD(env, SYSCFG_K0LOCK, 0);

    /* Look for a thread to unlock */
    unsigned int this_threadId = env->threadId;
    CPUHexagonState *unlock_thread = NULL;
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread = &cpu->env;

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
        if (thread->k0_lock_state == HEX_LOCK_WAITING) {
            if (!unlock_thread) {
                unlock_thread = thread;
            } else if (unlock_thread->threadId > this_threadId) {
                if (this_threadId < thread->threadId &&
                    thread->threadId < unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            } else {
                if (thread->threadId > this_threadId) {
                    unlock_thread = thread;
                }
                if (thread->threadId < unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            }
        }
    }
    if (unlock_thread) {
        cs = env_cpu(unlock_thread);

        print_thread("\tWaiting thread found", cs);
        unlock_thread->k0_lock_state = HEX_LOCK_OWNER;
        SET_SYSCFG_FIELD(unlock_thread, SYSCFG_K0LOCK, 1);
        cpu_resume(cs);
    }

    qemu_mutex_unlock_iothread();
    HEX_DEBUG_LOG("After hex_k0_unlock: %d\n", env->threadId);
    print_thread_states("\tThread");
}
#endif

/* Helpful for printing intermediate values within instructions */
void HELPER(debug_value)(CPUHexagonState *env, int32_t value)
{
    HEX_DEBUG_LOG("value = 0x%x\n", value);
}

void HELPER(debug_value_i64)(CPUHexagonState *env, int64_t value)
{
    HEX_DEBUG_LOG("value_i64 = 0x%lx\n", value);
}

/* Histogram instructions */

void HELPER(vhist)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int lane = 0; lane < 8; lane++) {
        for (int i = 0; i < sizeof(MMVector) / 8; ++i) {
            unsigned char value = input->ub[(sizeof(MMVector) / 8) * lane + i];
            unsigned char regno = value >> 3;
            unsigned char element = value & 7;

            env->VRegs[regno].uh[(sizeof(MMVector) / 16) * lane + element]++;
        }
    }
}

void HELPER(vhistq)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int lane = 0; lane < 8; lane++) {
        for (int i = 0; i < sizeof(MMVector) / 8; ++i) {
            unsigned char value = input->ub[(sizeof(MMVector) / 8) * lane + i];
            unsigned char regno = value >> 3;
            unsigned char element = value & 7;

            if (fGETQBIT(env->qtmp, sizeof(MMVector) / 8 * lane + i)) {
                env->VRegs[regno].uh[
                    (sizeof(MMVector) / 16) * lane + element]++;
            }
        }
    }
}

void HELPER(vwhist256)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 0) & (~7)) | ((bucket >> 0) & 7);

        env->VRegs[vindex].uh[elindex] =
            env->VRegs[vindex].uh[elindex] + weight;
    }
}

void HELPER(vwhist256q)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 0) & (~7)) | ((bucket >> 0) & 7);

        if (fGETQBIT(env->qtmp, 2 * i)) {
            env->VRegs[vindex].uh[elindex] =
                env->VRegs[vindex].uh[elindex] + weight;
        }
    }
}

void HELPER(vwhist256_sat)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 0) & (~7)) | ((bucket >> 0) & 7);

        env->VRegs[vindex].uh[elindex] =
            fVSATUH(env->VRegs[vindex].uh[elindex] + weight);
    }
}

void HELPER(vwhist256q_sat)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 0) & (~7)) | ((bucket >> 0) & 7);

        if (fGETQBIT(env->qtmp, 2 * i)) {
            env->VRegs[vindex].uh[elindex] =
                fVSATUH(env->VRegs[vindex].uh[elindex] + weight);
        }
    }
}

void HELPER(vwhist128)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 1) & (~3)) | ((bucket >> 1) & 3);

        env->VRegs[vindex].uw[elindex] =
            env->VRegs[vindex].uw[elindex] + weight;
    }
}

void HELPER(vwhist128q)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 1) & (~3)) | ((bucket >> 1) & 3);

        if (fGETQBIT(env->qtmp, 2 * i)) {
            env->VRegs[vindex].uw[elindex] =
                env->VRegs[vindex].uw[elindex] + weight;
        }
    }
}

void HELPER(vwhist128m)(CPUHexagonState *env, int32_t uiV)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 1) & (~3)) | ((bucket >> 1) & 3);

        if ((bucket & 1) == uiV) {
            env->VRegs[vindex].uw[elindex] =
                env->VRegs[vindex].uw[elindex] + weight;
        }
    }
}

void HELPER(vwhist128qm)(CPUHexagonState *env, int32_t uiV)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (sizeof(MMVector) / 2); i++) {
        unsigned int bucket = fGETUBYTE(0, input->h[i]);
        unsigned int weight = fGETUBYTE(1, input->h[i]);
        unsigned int vindex = (bucket >> 3) & 0x1F;
        unsigned int elindex = ((i >> 1) & (~3)) | ((bucket >> 1) & 3);

        if (((bucket & 1) == uiV) && fGETQBIT(env->qtmp, 2 * i)) {
            env->VRegs[vindex].uw[elindex] =
                env->VRegs[vindex].uw[elindex] + weight;
        }
    }
}

static void cancel_slot(CPUHexagonState *env, uint32_t slot)
{
#ifdef CONFIG_USER_ONLY
    HEX_DEBUG_LOG("Slot %d cancelled\n", slot);
#else
    HEX_DEBUG_LOG("op_helper: slot_cancelled = %d: pc = 0x%x\n", slot, env->gpr[HEX_REG_PC]);
#endif
    env->slot_cancelled |= (1 << slot);
}

#ifndef CONFIG_USER_ONLY
void HELPER(fwait)(CPUHexagonState *env, uint32_t mask)

{
    /* mask is unsued */
    hexagon_wait_thread(env);
}

void HELPER(fresume)(CPUHexagonState *env, uint32_t mask)

{
    hexagon_resume_threads(env, mask);
}

void HELPER(fstart)(CPUHexagonState *env, uint32_t mask)

{
    hexagon_start_threads(env, mask);
}

void HELPER(clear_run_mode)(CPUHexagonState *env, uint32_t mask)

{
    /* mask is unused */
    hexagon_stop_thread(env);
}

void HELPER(iassignw)(CPUHexagonState *env, uint32_t src)

{
    HEX_DEBUG_LOG("%s: tid %d, src 0x%x\n",
        __FUNCTION__, env->threadId, src);
    uint32_t modectl =
        ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    CPUState *cpu = NULL;
    CPU_FOREACH (cpu) {
        CPUHexagonState *thread_env = &(HEXAGON_CPU(cpu)->env);
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if (thread_enabled_mask & thread_id_mask) {
            uint32_t imask = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_IMASK);
            //uint32_t imask_mask = GET_FIELD(IMASK_MASK, imask);
            uint32_t intbitpos = (src >> 16) & 0xF;
            fINSERT_BITS(imask, 1, intbitpos,
                (src >> thread_env->threadId) & 0x1);
            ARCH_SET_SYSTEM_REG(thread_env, HEX_SREG_IMASK, imask);
            HEX_DEBUG_LOG("%s: tid %d, read imask 0x%x, intbitpos %u, "
              "set bitval %u, new imask 0x%x\n",
              __FUNCTION__,
              thread_env->threadId, imask, intbitpos,
              (src >> thread_env->threadId) & 0x1,
              ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_IMASK));
        }
    }
}

uint32_t HELPER(iassignr)(CPUHexagonState *env, uint32_t src)

{
    uint32_t modectl =
        ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    /* src fields are in same position as modectl, but mean different things */
    uint32_t intbitpos = GET_FIELD(MODECTL_W, src);
    uint32_t dest_reg = 0;
    CPUState *cpu = NULL;
    CPU_FOREACH (cpu) {
        CPUHexagonState *thread_env = &(HEXAGON_CPU(cpu)->env);
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if (thread_enabled_mask & thread_id_mask) {
            uint32_t imask = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_IMASK);
            dest_reg |= ((imask >> intbitpos) & 0x1) << thread_env->threadId;
        }
    }
    return dest_reg;
}

static void hexagon_set_vid(CPUHexagonState *env, uint32_t offset, int val) {
    assert ((offset == L2VIC_VID_0) || (offset == L2VIC_VID_1));
    CPUState *cs = env_cpu(env);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    const hwaddr pend_mem = cpu->l2vic_base_addr + offset;
    cpu_physical_memory_write(pend_mem, &val, sizeof(val));
}

static void hexagon_clear_last_irq(CPUHexagonState *env, uint32_t offset) {
    /* currently only l2vic is the only attached it uses vid0, remove
     * the assert below if anther is added */
    hexagon_set_vid(env, offset, L2VIC_NO_PENDING);
}

static uint32_t hexagon_find_last_irq(CPUHexagonState *env, uint32_t vid)
{
    int offset = (vid ==  HEX_SREG_VID) ? L2VIC_VID_0 : L2VIC_VID_1;
    CPUState *cs = env_cpu(env);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    const hwaddr pend_mem = cpu->l2vic_base_addr + offset;
    uint32_t irq;
    cpu_physical_memory_read(pend_mem, &irq, sizeof(irq));
    return irq;
}

int hexagon_find_l2vic_pending(CPUHexagonState *env);
int hexagon_find_l2vic_pending(CPUHexagonState *env)
{
    int intnum = 0;
    CPUState *cs = env_cpu(env);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    const hwaddr pend = cpu->l2vic_base_addr + L2VIC_INT_STATUSn;
    uint64_t pending[L2VIC_INTERRUPT_MAX / (sizeof(uint64_t) * CHAR_BIT)];
    cpu_physical_memory_read(pend, pending, sizeof(pending));

    const hwaddr stat = cpu->l2vic_base_addr + L2VIC_INT_STATUSn;
    uint64_t status[L2VIC_INTERRUPT_MAX / (sizeof(uint64_t) * CHAR_BIT)];
    cpu_physical_memory_read(stat, status, sizeof(status));

    intnum = find_next_bit(pending, L2VIC_INTERRUPT_MAX, intnum);
    while (intnum < L2VIC_INTERRUPT_MAX) {
        /* Pending is set but status isn't the interrupt is pending */
        if (!test_bit(intnum, status)) {
            break;
        }
        intnum = find_next_bit(pending, L2VIC_INTERRUPT_MAX, intnum+1);
    }
    return (intnum < L2VIC_INTERRUPT_MAX) ? intnum : L2VIC_NO_PENDING;
}

void HELPER(ciad)(CPUHexagonState *env, uint32_t src)
{
    HEX_DEBUG_LOG("%s: tid %d, src 0x%x\n",
        __FUNCTION__, env->threadId, src);
    hexagon_clear_last_irq(env, L2VIC_VID_0);
    return;
}


static void hexagon_read_timer(CPUHexagonState *env, uint32_t *low, uint32_t *high)
{
    CPUState *cs = env_cpu(env);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    const hwaddr low_addr  = cpu->qtimer_base_addr + QCT_QTIMER_CNTPCT_LO;
    const hwaddr high_addr = cpu->qtimer_base_addr + QCT_QTIMER_CNTPCT_HI;

    cpu_physical_memory_read(low_addr, low, sizeof(*low));
    cpu_physical_memory_read(high_addr, high, sizeof(*high));
}

uint32_t HELPER(creg_read)(CPUHexagonState *env, uint32_t reg)
{
    uint32_t low, high;
    switch (reg) {
    case HEX_REG_PKTCNTLO:
        low = ARCH_GET_THREAD_REG(env, HEX_REG_QEMU_PKT_CNT);
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTLO, low);
        return low;
    case HEX_REG_PKTCNTHI:
        high = 0;
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTHI, high);
        return high;
    case HEX_REG_UPCYCLELO:
        return hexagon_get_sys_pcycle_count_low(env);
    case HEX_REG_UPCYCLEHI:
        return hexagon_get_sys_pcycle_count_high(env);
    case HEX_REG_UTIMERLO:
        hexagon_read_timer(env, &low, &high);
        return low;
    case HEX_REG_UTIMERHI:
        hexagon_read_timer(env, &low, &high);
        return high;
    default:
        g_assert_not_reached();
    }
    return 0;
}
uint64_t HELPER(creg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    uint32_t low = 0, high = 0;
    if (reg == HEX_REG_UPCYCLELO) {
        target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
        int ssr_ce = GET_SSR_FIELD(SSR_CE, ssr);
        return ssr_ce ? hexagon_get_sys_pcycle_count(env) : 0;
    } else if (reg == HEX_REG_PKTCNTLO) {
        low = ARCH_GET_THREAD_REG(env, HEX_REG_QEMU_PKT_CNT);
        high = 0;
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTLO, low);
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTHI, high);
        return low | (uint64_t)high << 32;
    } else if (reg == HEX_REG_UTIMERLO) {
        hexagon_read_timer(env, &low, &high);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERLO, low);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERHI, high);
        return low | (uint64_t)high << 32;
    } else {
        g_assert_not_reached();
    }
}

uint32_t HELPER(sreg_read)(CPUHexagonState *env, uint32_t reg)
{
    if ((reg == HEX_SREG_VID) || (reg == HEX_SREG_VID1)) {
        uint32_t vid = hexagon_find_last_irq(env, reg);
        ARCH_SET_SYSTEM_REG(env, reg, vid);
    } else if ((reg == HEX_SREG_TIMERLO) || (reg == HEX_SREG_TIMERHI)) {
        uint32_t low = 0;
        uint32_t high = 0;
        hexagon_read_timer(env, &low, &high);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERLO, low);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERHI, high);
    } else if (reg == HEX_SREG_PCYCLELO) {
        return hexagon_get_sys_pcycle_count_low(env);
    } else if (reg == HEX_SREG_PCYCLEHI) {
        return hexagon_get_sys_pcycle_count_high(env);
    } else if (reg == HEX_SREG_BADVA) {
        target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
        if (GET_SSR_FIELD(SSR_BVS, ssr)) {
            return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA1);
        }
        return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA0);
    }
    return ARCH_GET_SYSTEM_REG(env, reg);
}

uint64_t HELPER(sreg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    if (reg == HEX_SREG_TIMERLO) {
        uint32_t low = 0;
        uint32_t high = 0;
        hexagon_read_timer(env, &low, &high);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERLO, low);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERHI, high);
    } else if (reg == HEX_SREG_PCYCLELO) {
        return hexagon_get_sys_pcycle_count(env);
    }
    return   (uint64_t)ARCH_GET_SYSTEM_REG(env, reg) |
           (((uint64_t)ARCH_GET_SYSTEM_REG(env, reg+1)) << 32);
}

static void modify_syscfg(CPUHexagonState *env, uint32_t val)
{
    /* Check for change in MMU enable */
    target_ulong old_mmu_enable =
        GET_SYSCFG_FIELD(SYSCFG_MMUEN, env->g_sreg[HEX_SREG_SYSCFG]);
    target_ulong new_mmu_enable =
        GET_SYSCFG_FIELD(SYSCFG_MMUEN, val);
    if (new_mmu_enable && !old_mmu_enable) {
        hex_mmu_on(env);
    } else if (!new_mmu_enable && old_mmu_enable) {
        hex_mmu_off(env);
    }

    /* Changing pcycle enable from 0 to 1 resets the counters */
    uint32_t old = env->g_sreg[HEX_SREG_SYSCFG];
    uint8_t old_en = GET_SYSCFG_FIELD(SYSCFG_PCYCLEEN, old);
    uint8_t new_en = GET_SYSCFG_FIELD(SYSCFG_PCYCLEEN, val);
    CPUState *cs;
    if (old_en == 0 && new_en == 1) {
        CPU_FOREACH(cs) {
            HexagonCPU *cpu = HEXAGON_CPU(cs);
            CPUHexagonState *env = &cpu->env;
            env->t_packet_count = 0;
        }
    }

    /* See if global interrupts are turned on */
    uint8_t old_gie = GET_SYSCFG_FIELD(SYSCFG_GIE, old);
    uint8_t new_gie = GET_SYSCFG_FIELD(SYSCFG_GIE, val);
    if (!old_gie && new_gie) {
        qemu_log_mask(CPU_LOG_INT, "%s: global interrupts enabled\n", __func__);
        hex_interrupt_update(env);
    }
}

void HELPER(sreg_write)(CPUHexagonState *env, uint32_t reg, uint32_t val)

{
    if ((reg == HEX_SREG_VID) || (reg == HEX_SREG_VID1)) {
        hexagon_set_vid(env, (reg == HEX_SREG_VID) ? L2VIC_VID_0 : L2VIC_VID_1,
                         val);
        ARCH_SET_SYSTEM_REG(env, reg, val);
    } else if (reg == HEX_SREG_SYSCFG) {
        modify_syscfg(env, val);
        ARCH_SET_SYSTEM_REG(env, reg, val);
    } else if (reg == HEX_SREG_IMASK) {
        val = GET_FIELD(IMASK_MASK, val);
        ARCH_SET_SYSTEM_REG(env, reg, val);
    } else if (reg == HEX_SREG_PCYCLELO) {
        hexagon_set_sys_pcycle_count_low(env, val);
    } else if (reg == HEX_SREG_PCYCLEHI) {
        hexagon_set_sys_pcycle_count_high(env, val);
    } else {
        ARCH_SET_SYSTEM_REG(env, reg, val);
    }
}

void HELPER(sreg_write_pair)(CPUHexagonState *env, uint32_t reg, uint64_t val)

{
    ARCH_SET_SYSTEM_REG(env, reg, val & 0xFFFFFFFF);
    ARCH_SET_SYSTEM_REG(env, reg+1, val >> 32);
}

uint32_t HELPER(greg_read)(CPUHexagonState *env, uint32_t reg)

{
    return hexagon_greg_read(env, reg);
}

uint64_t HELPER(greg_read_pair)(CPUHexagonState *env, uint32_t reg)

{
    int off;

    if (reg == HEX_GREG_G0 || reg == HEX_GREG_G2) {
        return   (uint64_t)(env->greg[reg]) |
               (((uint64_t)(env->greg[reg+1])) << 32);
    }
    switch (reg) {
        case HEX_GREG_GPCYCLELO:
        {
            target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
            int ssr_ce = GET_SSR_FIELD(SSR_CE, ssr);
            return ssr_ce ? hexagon_get_sys_pcycle_count(env) : 0;
        }
        case HEX_GREG_GPMUCNT0:
        case HEX_GREG_GPMUCNT2:
            off = HEX_GREG_GPMUCNT3 - reg;
            return (uint64_t)ARCH_GET_SYSTEM_REG(env, HEX_SREG_PMUCNT0+off) |
                (uint64_t)(ARCH_GET_SYSTEM_REG(env, HEX_SREG_PMUCNT0+off+1))
                           << 32;
        default:
            return 0;
    }
}

uint32_t HELPER(getimask)(CPUHexagonState *env, uint32_t tid)

{
    HEX_DEBUG_LOG("%s: tid %u, for tid %u\n",
        __FUNCTION__, env->threadId, tid);
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *found_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *found_env = &found_cpu->env;
        if (found_env->threadId == tid) {
            target_ulong imask = ARCH_GET_SYSTEM_REG(found_env,HEX_SREG_IMASK);
            HEX_DEBUG_LOG("%s: tid %d, found it, imask = 0x%x\n",
                __FUNCTION__, env->threadId,
                (unsigned)GET_FIELD(IMASK_MASK, imask));
            return GET_FIELD(IMASK_MASK, imask);
        }
    }
    return 0;
}
void HELPER(setprio)(CPUHexagonState *env, uint32_t thread, uint32_t prio)

{
    thread = thread & (THREADS_MAX-1);
    HEX_DEBUG_LOG("%s: tid %u, setting thread 0x%x, prio 0x%x\n",
      __FUNCTION__, env->threadId, thread, prio);
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *found_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *found_env = &found_cpu->env;
        if (thread == found_env->threadId) {
            SET_SYSTEM_FIELD(found_env, HEX_SREG_STID, STID_PRIO, prio);
            HEX_DEBUG_LOG("%s: tid[%d].PRIO = 0x%x\n",
                __FUNCTION__, found_env->threadId, prio);
            hex_interrupt_update(env);
            return;
        }
    }
    g_assert_not_reached();
}

void HELPER(setimask)(CPUHexagonState *env, uint32_t pred, uint32_t imask)

{
    HEX_DEBUG_LOG("%s: tid %u, pred 0x%x, imask 0x%x\n",
      __FUNCTION__, env->threadId, pred, imask);
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *found_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *found_env = &found_cpu->env;

        if (pred & (0x1 << found_env->threadId)) {
            SET_SYSTEM_FIELD(found_env, HEX_SREG_IMASK, IMASK_MASK, imask);
            HEX_DEBUG_LOG("%s: tid %d, found it, imask 0x%x\n",
                __FUNCTION__, found_env->threadId, imask);
            hex_interrupt_update(env);
            return;
        }
    }
}

typedef struct {
    CPUState *cs;
    CPUHexagonState *env;
} thread_entry;


void HELPER(swi)(CPUHexagonState *env, uint32_t mask)
{
    hex_raise_interrupts(env, mask, CPU_INTERRUPT_SWI);
}

void HELPER(resched)(CPUHexagonState *env)
{
    qemu_log_mask(CPU_LOG_INT, "%s: check resched\n", __func__);
    const uint32_t schedcfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SCHEDCFG);
    const uint32_t schedcfg_en = GET_FIELD(SCHEDCFG_EN, schedcfg);
    const int int_number = GET_FIELD(SCHEDCFG_INTNO, schedcfg);
    CPUState *cs = env_cpu(env);
    if (!schedcfg_en) {
        return;
    }

    uint32_t lowest_th_prio = 0; /* 0 is highest prio */
    HexagonCPU *thread;
    CPUHexagonState *thread_env;
    CPU_FOREACH(cs) {
        thread = HEXAGON_CPU(cs);
        thread_env = &(thread->env);
        uint32_t th_prio = GET_FIELD(
            STID_PRIO, ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_STID));
        if (!hexagon_thread_is_enabled(thread_env)) {
            continue;
        }

        lowest_th_prio = (lowest_th_prio > th_prio)
            ? lowest_th_prio
            : th_prio;
    }

    uint32_t bestwait_reg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_BESTWAIT);
    uint32_t best_prio = GET_FIELD(BESTWAIT_PRIO, bestwait_reg);

    /*
     * If the lowest priority thread is lower priority than the
     * value in the BESTWAIT register, we must raise the reschedule
     * interrupt on the lowest priority thread.
     */
    if (lowest_th_prio > best_prio) {
        qemu_log_mask(CPU_LOG_INT, "%s: raising resched int %d, cur PC 0x%08x\n", __func__, int_number, ARCH_GET_THREAD_REG(env, HEX_REG_PC));
        SET_SYSTEM_FIELD(env, HEX_SREG_BESTWAIT, BESTWAIT_PRIO, 0x1ff);
        hex_raise_interrupts(env, 1 << int_number, CPU_INTERRUPT_SWI);
    }
}

void HELPER(nmi)(CPUHexagonState *env, uint32_t thread_mask)
{
    bool found = false;
    CPUState *cs = NULL;
    CPU_FOREACH (cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread_env = &cpu->env;
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if ((thread_mask & thread_id_mask) != 0) {
            found = true;
            cs->exception_index = HEX_EVENT_IMPRECISE;
            thread_env->cause_code = HEX_CAUSE_IMPRECISE_NMI;
            HEX_DEBUG_LOG("tid %d gets nmi\n", thread_env->threadId);
        }
    }
    if (found) {
        hex_interrupt_update(env);
    }
}

/*
 * Return the count of threads ready to run.
 */
static uint32_t get_ready_count(CPUHexagonState *env)
{
    uint32_t ready_count = 0;
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread_env = &cpu->env;
        const bool running = (get_exe_mode(thread_env) == HEX_EXE_MODE_RUN)
                          && (env->k0_lock_state != HEX_LOCK_WAITING)
                          && (env->tlb_lock_state != HEX_LOCK_WAITING);
        if (running) {
            ready_count += 1;
        }
    }
    return ready_count;
}

/*
 * Update the GCYCLE_XT register where X is the number of running threads
 */
void HELPER(inc_gcycle_xt)(CPUHexagonState *env)
{
    uint32_t num_threads = get_ready_count(env);
    if (1 <= num_threads && num_threads <= 6) {
        env->g_sreg[HEX_SREG_GCYCLE_1T + num_threads - 1]++;
    }
}

void HELPER(cpu_limit)(CPUHexagonState *env, target_ulong next_PC)
{
    uint32_t ready_count = get_ready_count(env);

    env->gpr[HEX_REG_QEMU_CPU_TB_CNT]++;

    if (ready_count > 1 &&
        env->gpr[HEX_REG_QEMU_CPU_TB_CNT] >= HEXAGON_TB_EXEC_PER_CPU_MAX) {
        env->gpr[HEX_REG_PC] = next_PC;
        HELPER(raise_exception)(env, EXCP_YIELD);
        env->gpr[HEX_REG_QEMU_CPU_TB_CNT] = 0;
    }
    env->last_cpu = env->threadId;
}

void HELPER(pending_interrupt)(CPUHexagonState *env)
{
    hex_interrupt_update(env);
}
#endif

#ifdef CONFIG_USER_ONLY
uint32_t HELPER(creg_read)(CPUHexagonState *env, uint32_t reg)
{
    return 0;
}

uint64_t HELPER(creg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    return 0;
}
#endif

void HELPER(commit_hmx)(CPUHexagonState *env)
{
    hmx_ext_commit_regs(env, 0);
    hmx_ext_commit_mem(env, 0, 0);
}

/* These macros can be referenced in the generated helper functions */
#define warn(...) /* Nothing */
#define fatal(...) g_assert_not_reached();
#define thread env
#define THREAD2STRUCT ((hmx_state_t*)env->processor_ptr->shared_extptr)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

/* Not supported and rejected by clang-11, possibly available in newer
 * clang releases:
 */
#if !defined(__clang_major__) || (defined(__clang_major__) && __clang_major__ < 11)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#define warn(...) /* Nothing */
#define fatal(...) g_assert_not_reached();

#define BOGUS_HELPER(tag) \
    printf("ERROR: bogus helper: " #tag "\n")

#include "mmvec/kvx_ieee.h"
#include "helper_funcs_generated.c.inc"

#pragma GCC diagnostic pop

