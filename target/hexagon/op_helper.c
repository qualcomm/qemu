/*
 *  Copyright(c) 2019-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "qemu/timer.h"
#include "accel/tcg/cpu-ldst.h"
#include "accel/tcg/cpu-loop.h"
#include "accel/tcg/probe.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/page-protection.h"
#include "fpu/softfloat.h"
#include "internal.h"
#include "int16_emu.h"
#include "macros.h"
#include "hw/hexagon/hexagon_tlb.h"
#include "sys_macros.h"
#include "arch.h"
#include "hex_arch_types.h"
#include "op_helper.h"
#include "fma_emu.h"
#include "mmvec/mmvec.h"
#include "hw/intc/l2vic.h"
#include "mmvec/macros_auto.h"
#include "mmvec/mmvec_qfloat.h"
#include "arch_options_calc.h"
#include "system.h"
#include "dma_adapter.h"
#ifndef CONFIG_USER_ONLY
#include "system/cpus.h"
#include "hw/core/boards.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hex_mmu.h"
#include "hw/intc/l2vic.h"
#include "hw/timer/qct-qtimer.h"
#include "hex_interrupts.h"
#include "hexswi.h"
#include "pmu.h"
#endif
#include "mmvec/macros.h"
#include "translate.h"
#include "coproc.h"
#include "trace.h"
#include "genptr.h"

#define SF_BIAS        127
#define SF_MANTBITS    23

/* Exceptions processing helpers */
G_NORETURN
void do_raise_exception(CPUHexagonState *env, uint32_t exception,
                        target_ulong PC, uintptr_t retaddr)
{
    CPUState *cs = env_cpu(env);
#ifdef CONFIG_USER_ONLY
    qemu_log_mask(CPU_LOG_INT, "%s: 0x%08x\n", __func__, exception);
#else
    qemu_log_mask(CPU_LOG_INT, "%s: 0x%08x, @ %08" PRIx32 ", tbl = %d\n",
                  __func__, exception, PC,
                  env->exec_ctr_tb);

#endif

    env->gpr[HEX_REG_PC] = PC;
    cs->exception_index = exception;
    cpu_loop_exit_restore(cs, retaddr);
}

G_NORETURN void hexagon_raise_exception_err(CPUHexagonState *env,
                                            uint32_t exception,
                                            uintptr_t pc)
{
    do_raise_exception(env, exception, pc, 0);
}

G_NORETURN void HELPER(raise_exception)(CPUHexagonState *env, uint32_t excp,
                                        target_ulong PC)
{
    hexagon_raise_exception_err(env, excp, PC);
}

void log_store32(CPUHexagonState *env, target_ulong addr,
                 target_ulong val, uint32_t width, int slot)
{
    env->mem_log_stores[slot].va = addr;
    env->mem_log_stores[slot].width = width;
    env->mem_log_stores[slot].data32 = val;
}

void log_store64(CPUHexagonState *env, target_ulong addr,
                 int64_t val, uint32_t width, int slot)
{
    env->mem_log_stores[slot].va = addr;
    env->mem_log_stores[slot].width = width;
    env->mem_log_stores[slot].data64 = val;
}

static void commit_store(CPUHexagonState *env, int slot_num, uintptr_t ra)
{
    uint32_t width = env->mem_log_stores[slot_num].width;
    target_ulong va = env->mem_log_stores[slot_num].va;

    switch (width) {
    case 1:
        cpu_stb_data_ra(env, va, env->mem_log_stores[slot_num].data32, ra);
        break;
    case 2:
        cpu_stw_le_data_ra(env, va, env->mem_log_stores[slot_num].data32, ra);
        break;
    case 4:
        cpu_stl_le_data_ra(env, va, env->mem_log_stores[slot_num].data32, ra);
        break;
    case 8:
        cpu_stq_le_data_ra(env, va, env->mem_log_stores[slot_num].data64, ra);
        break;
    default:
        g_assert_not_reached();
    }
}

void HELPER(commit_store)(CPUHexagonState *env, int slot_num)
{
    uintptr_t ra = GETPC();
    commit_store(env, slot_num, ra);
}

void HELPER(gather_store)(CPUHexagonState *env, uint32_t addr, int slot)
{
    mem_gather_store(env, addr, slot);
}


static void *probe_contiguous(CPUHexagonState *env, target_ulong addr, uint32_t nb,
                              MMUAccessType access_type, int mmu_idx,
                              uintptr_t raddr)
{
    void *host1, *host2;
    uint32_t nb_pg1, nb_pg2;

    nb_pg1 = -(addr | TARGET_PAGE_MASK);
    if (likely(nb <= nb_pg1)) {
        /* The entire operation is on a single page.  */
        return probe_access(env, addr, nb, access_type, mmu_idx, raddr);
    }

    /* The operation spans two pages.  */
    nb_pg2 = nb - nb_pg1;
    host1 = probe_access(env, addr, nb_pg1, access_type, mmu_idx, raddr);
    addr += nb_pg1;
    host2 = probe_access(env, addr, nb_pg2, access_type, mmu_idx, raddr);

    /* If the two host pages are contiguous, optimize.  */
    if (host2 == host1 + nb_pg1) {
        return host1;
    }
    return NULL;
}

void HELPER(commit_hvx_stores)(CPUHexagonState *env)
{
    HexagonCPU *cpu = env_archcpu(env);
    CPU_MEMOP_PC_SET(env);
    uintptr_t ra = CPU_MEMOP_PC(env);

    /* Normal (possibly masked) vector store */
    for (int i = 0; i < VSTORES_MAX; i++) {
        if (env->vstore_pending[i]) {
            env->vstore_pending[i] = 0;
            target_ulong va = env->vstore[i].va;
            if (cpu->paranoid_commit_state && va == PARANOID_VALUE) {
                CPUState *cs = env_cpu(env);
                cpu_abort(cs, "Invalid HVX store found at PC 0x%x\n",
                          env->gpr[HEX_REG_PC]);
            }
            int size = env->vstore[i].size;

            uint8_t *host = probe_contiguous(env, va, size, MMU_DATA_STORE,
                                              CPU_MMU_INDEX(env), ra);
            if (likely(host)) {
                for (int j = 0; j < size; j++) {
                    if (test_bit(j, env->vstore[i].mask)) {
                        *(host + j) = env->vstore[i].data.ub[j];
                    }
                }
            } else {
                for (int j = 0; j < size; j++) {
                    if (test_bit(j, env->vstore[i].mask)) {
                        cpu_stb_data_ra(env, va + j, env->vstore[i].data.ub[j], ra);
                    }
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
            for (int i = 0; i < VECTOR_SIZE_BYTE; i++) {
                if (test_bit(i, env->vtcm_log.mask)) {
                    cpu_stb_data_ra(env, env->vtcm_log.va[i],
                                    env->vtcm_log.data.ub[i], ra);
                    clear_bit(i, env->vtcm_log.mask);
                    env->vtcm_log.data.ub[i] = 0;
#ifndef CONFIG_USER_ONLY
                    fVARRAY_ELEMENT_ACCESS(env->vtcm_log.offsets, ub, i) = 0;
#endif
                }

            }
        }
    }
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
uint64_t HELPER(sfrecipa)(CPUHexagonState *env, float32 RsV, float32 RtV,
                          uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return ((uint64_t)RdV << 32) | PeV;
}

uint64_t HELPER(sfinvsqrta)(CPUHexagonState *env, float32 RsV,
                            uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return ((uint64_t)RdV << 32) | PeV;
}

int64_t HELPER(vacsh_val)(CPUHexagonState *env,
                           int64_t RxxV, int64_t RssV, int64_t RttV,
                           uint32_t pkt_need_commit)
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
                           int slot, int mmu_idx, target_ulong PC)
{
    if (hexagon_cpu_mmu_enabled(env)) {
        hwaddr phys;
        int prot;
        uint64_t size;
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
                    do_raise_exception(env, cs->exception_index, PC, retaddr);
                }
            }
        } else {
            /* If no TLB match found, raise a TLB miss exception */
            CPUState *cs = env_cpu(env);
            uintptr_t retaddr = GETPC();
            raise_tlbmiss_exception(cs, RsV, slot, MMU_DATA_LOAD);
            do_raise_exception(env, cs->exception_index, PC, retaddr);
        }
    }
}

void HELPER(insn_cache_op)(CPUHexagonState *env, target_ulong RsV,
                           int slot, int mmu_idx, target_ulong PC)
{
    if (hexagon_cpu_mmu_enabled(env)) {
        hwaddr phys;
        int prot;
        uint64_t size;
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
            do_raise_exception(env, cs->exception_index, PC, retaddr);
        }
    }
}

void HELPER(swi)(CPUHexagonState *env, uint32_t mask)
{
    BQL_LOCK_GUARD();
    hex_raise_interrupts(env, mask, CPU_INTERRUPT_SWI);
}

void HELPER(cswi)(CPUHexagonState *env, uint32_t mask)
{
    BQL_LOCK_GUARD();
    hex_clear_interrupts(env, mask, CPU_INTERRUPT_SWI);
}


/*
 * ciad - clear interrupt auto disable
 *  - When taking an interrupt the hardware will set ipend.iad to
 *    prevent another thread from servicing the interrupt.  At the completion
 *    of service software uses ciad to clear the bit indicating the
 *    interrupt can be accepted again.
 *  - ciad also handshakes with the l2vic allowing a new vid on the vector
 *    port.
 */
void HELPER(ciad)(CPUHexagonState *env, uint32_t mask)
{
    uint32_t iad;

    BQL_LOCK_GUARD();
    iad = arch_get_system_reg(env, HEX_SREG_IAD) & ~(mask);
    arch_set_system_reg(env, HEX_SREG_IAD, iad);

    /* Clear interrupt using L2VIC interface */
    CPUState *cs = env_cpu(env);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    if (cpu->l2vic) {
        l2vic_clear_interrupt(cpu->l2vic);
    }

    hex_interrupt_update(env);
}

void HELPER(siad)(CPUHexagonState *env, uint32_t mask)
{
    uint32_t iad;

    BQL_LOCK_GUARD();
    iad = arch_get_system_reg(env, HEX_SREG_IAD) | mask;
    arch_set_system_reg(env, HEX_SREG_IAD, iad);
    hex_interrupt_update(env);
}

void HELPER(wait)(CPUHexagonState *env, target_ulong PC)
{
    BQL_LOCK_GUARD();

    if (!fIN_DEBUG_MODE(fGET_TNUM())) {
        hexagon_wait_thread(env, PC);
     }
}

void HELPER(resume)(CPUHexagonState *env, uint32_t mask)
{
    BQL_LOCK_GUARD();
    hexagon_resume_threads(env, mask);
}
#endif

int64_t HELPER(cabacdecbin_val)(int64_t RssV, int64_t RttV)
{
    int64_t RddV = 0;
    size4u_t state;
    size4u_t valMPS;
    size4u_t bitpos;
    size4u_t range;
    size4u_t offset;
    size4u_t rLPS;
    size4u_t rMPS;

    state =  fEXTRACTU_RANGE(fGETWORD(1, RttV), 5, 0);
    valMPS = fEXTRACTU_RANGE(fGETWORD(1, RttV), 8, 8);
    bitpos = fEXTRACTU_RANGE(fGETWORD(0, RttV), 4, 0);
    range =  fGETWORD(0, RssV);
    offset = fGETWORD(1, RssV);

    /* calculate rLPS */
    range <<= bitpos;
    offset <<= bitpos;
    rLPS = rLPS_table_64x4[state][(range >> 29) & 3];
    rLPS  = rLPS << 23;   /* left aligned */

    /* calculate rMPS */
    rMPS = (range & 0xff800000) - rLPS;

    /* most probable region */
    if (offset < rMPS) {
        RddV = AC_next_state_MPS_64[state];
        fINSERT_RANGE(RddV, 8, 8, valMPS);
        fINSERT_RANGE(RddV, 31, 23, (rMPS >> 23));
        fSETWORD(1, RddV, offset);
    }
    /* least probable region */
    else {
        RddV = AC_next_state_LPS_64[state];
        fINSERT_RANGE(RddV, 8, 8, ((!state) ? (1 - valMPS) : (valMPS)));
        fINSERT_RANGE(RddV, 31, 23, (rLPS >> 23));
        fSETWORD(1, RddV, (offset - rMPS));
    }
    return RddV;
}

int32_t HELPER(cabacdecbin_pred)(int64_t RssV, int64_t RttV)
{
    int32_t p0 = 0;
    size4u_t state;
    size4u_t valMPS;
    size4u_t bitpos;
    size4u_t range;
    size4u_t offset;
    size4u_t rLPS;
    size4u_t rMPS;

    state =  fEXTRACTU_RANGE(fGETWORD(1, RttV), 5, 0);
    valMPS = fEXTRACTU_RANGE(fGETWORD(1, RttV), 8, 8);
    bitpos = fEXTRACTU_RANGE(fGETWORD(0, RttV), 4, 0);
    range =  fGETWORD(0, RssV);
    offset = fGETWORD(1, RssV);

    /* calculate rLPS */
    range <<= bitpos;
    offset <<= bitpos;
    rLPS = rLPS_table_64x4[state][(range >> 29) & 3];
    rLPS  = rLPS << 23;   /* left aligned */

    /* calculate rMPS */
    rMPS = (range & 0xff800000) - rLPS;

    /* most probable region */
    if (offset < rMPS) {
        p0 = valMPS;

    }
    /* least probable region */
    else {
        p0 = valMPS ^ 1;
    }
    return p0;
}

static void probe_store(CPUHexagonState *env, int slot, int mmu_idx,
                        bool is_predicated, uintptr_t retaddr)
{
    if (!is_predicated || !(env->slot_cancelled & (1 << slot))) {
        uint32_t width = env->mem_log_stores[slot].width;
        target_ulong va = env->mem_log_stores[slot].va;
        probe_write(env, va, width, mmu_idx, retaddr);
    }
}

/*
 * Called from a mem_noshuf packet to make sure the load doesn't
 * raise an exception
 */
void HELPER(probe_noshuf_load)(CPUHexagonState *env, target_ulong va,
                               int size, int mmu_idx)
{
    uintptr_t retaddr = GETPC();
    probe_read(env, va, size, mmu_idx, retaddr);
}

/* Called during packet commit when there are two scalar stores */
void HELPER(probe_pkt_scalar_store_s0)(CPUHexagonState *env, int args)
{
    int mmu_idx = FIELD_EX32(args, PROBE_PKT_SCALAR_STORE_S0, MMU_IDX);
    bool is_predicated =
        FIELD_EX32(args, PROBE_PKT_SCALAR_STORE_S0, IS_PREDICATED);
    uintptr_t ra = GETPC();
    probe_store(env, 0, mmu_idx, is_predicated, ra);
}

static void probe_hvx_stores(CPUHexagonState *env, int mmu_idx,
                                    uintptr_t retaddr)
{
    /* Normal (possibly masked) vector store */
    for (int i = 0; i < VSTORES_MAX; i++) {
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
            for (int i = 0; i < VECTOR_SIZE_BYTE; i++) {
                if (test_bit(i, env->vtcm_log.mask)) {
                    probe_write(env, env->vtcm_log.va[i], 1, mmu_idx, retaddr);
                }

            }
        }
    }
}

void HELPER(probe_hvx_stores)(CPUHexagonState *env, int mmu_idx)
{
    uintptr_t retaddr = GETPC();
    probe_hvx_stores(env, mmu_idx, retaddr);
}

void HELPER(probe_pkt_scalar_hvx_stores)(CPUHexagonState *env, int mask)
{
    bool has_st0 = FIELD_EX32(mask, PROBE_PKT_SCALAR_HVX_STORES, HAS_ST0);
    bool has_st1 = FIELD_EX32(mask, PROBE_PKT_SCALAR_HVX_STORES, HAS_ST1);
    bool has_hvx_stores =
        FIELD_EX32(mask, PROBE_PKT_SCALAR_HVX_STORES, HAS_HVX_STORES);
    bool s0_is_pred = FIELD_EX32(mask, PROBE_PKT_SCALAR_HVX_STORES, S0_IS_PRED);
    bool s1_is_pred = FIELD_EX32(mask, PROBE_PKT_SCALAR_HVX_STORES, S1_IS_PRED);
    int mmu_idx = FIELD_EX32(mask, PROBE_PKT_SCALAR_HVX_STORES, MMU_IDX);
    uintptr_t ra = GETPC();

    if (has_st0) {
        probe_store(env, 0, mmu_idx, s0_is_pred, ra);
    }
    if (has_st1) {
        probe_store(env, 1, mmu_idx, s1_is_pred, ra);
    }
    if (has_hvx_stores) {
        probe_hvx_stores(env, mmu_idx, ra);
    }
}

void HELPER(assert_store_valid)(CPUHexagonState *env, int slot)
{
    if (env->mem_log_stores[slot].va == PARANOID_VALUE) {
        CPUState *cs = env_cpu(env);
        cpu_abort(cs, "Invalid store found at PC 0x%x\n", env->gpr[HEX_REG_PC]);
    }
}

/*
 * mem_noshuf
 * Section 5.5 of the Hexagon V67 Programmer's Reference Manual
 *
 * If the load is in slot 0 and there is a store in slot1 (that
 * wasn't cancelled), we have to do the store first.
 */
void check_noshuf(CPUHexagonState *env, bool pkt_has_scalar_store_s1,
                  uint32_t slot, target_ulong vaddr, int size,
                  uintptr_t ra)
{
    if (slot == 0 && pkt_has_scalar_store_s1 &&
        ((env->slot_cancelled & (1 << 1)) == 0)) {
        probe_read(env, vaddr, size, MMU_USER_IDX, ra);
        commit_store(env, 1, ra);
    }
}

/* Floating point */
float64 HELPER(conv_sf2df)(CPUHexagonState *env, float32 RsV,
                           uint32_t pkt_need_commit)
{
    float64 out_f64;
    arch_fpop_start(env);
    out_f64 = float32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return out_f64;
}

float32 HELPER(conv_df2sf)(CPUHexagonState *env, float64 RssV,
                           uint32_t pkt_need_commit)
{
    float32 out_f32;
    arch_fpop_start(env);
    out_f32 = float64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return out_f32;
}

float32 HELPER(conv_uw2sf)(CPUHexagonState *env, int32_t RsV,
                           uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = uint32_to_float32(RsV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float64 HELPER(conv_uw2df)(CPUHexagonState *env, int32_t RsV,
                           uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = uint32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float32 HELPER(conv_w2sf)(CPUHexagonState *env, int32_t RsV,
                          uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = int32_to_float32(RsV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float64 HELPER(conv_w2df)(CPUHexagonState *env, int32_t RsV,
                          uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = int32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float32 HELPER(conv_ud2sf)(CPUHexagonState *env, int64_t RssV,
                           uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = uint64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float64 HELPER(conv_ud2df)(CPUHexagonState *env, int64_t RssV,
                           uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = uint64_to_float64(RssV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float32 HELPER(conv_d2sf)(CPUHexagonState *env, int64_t RssV,
                          uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = int64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float64 HELPER(conv_d2df)(CPUHexagonState *env, int64_t RssV,
                          uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = int64_to_float64(RssV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

uint32_t HELPER(conv_sf2uw)(CPUHexagonState *env, float32 RsV,
                            uint32_t pkt_need_commit)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV) && !float32_is_zero(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float32_to_uint32(RsV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

int32_t HELPER(conv_sf2w)(CPUHexagonState *env, float32 RsV,
                          uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

uint64_t HELPER(conv_sf2ud)(CPUHexagonState *env, float32 RsV,
                            uint32_t pkt_need_commit)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV) && !float32_is_zero(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float32_to_uint64(RsV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

int64_t HELPER(conv_sf2d)(CPUHexagonState *env, float32 RsV,
                          uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

uint32_t HELPER(conv_df2uw)(CPUHexagonState *env, float64 RssV,
                            uint32_t pkt_need_commit)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV) && !float64_is_zero(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float64_to_uint32(RssV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

int32_t HELPER(conv_df2w)(CPUHexagonState *env, float64 RssV,
                          uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

uint64_t HELPER(conv_df2ud)(CPUHexagonState *env, float64 RssV,
                            uint32_t pkt_need_commit)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV) && !float64_is_zero(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float64_to_uint64(RssV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

int64_t HELPER(conv_df2d)(CPUHexagonState *env, float64 RssV,
                          uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

uint32_t HELPER(conv_sf2uw_chop)(CPUHexagonState *env, float32 RsV,
                                 uint32_t pkt_need_commit)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV) && !float32_is_zero(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float32_to_uint32_round_to_zero(RsV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

int32_t HELPER(conv_sf2w_chop)(CPUHexagonState *env, float32 RsV,
                               uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

uint64_t HELPER(conv_sf2ud_chop)(CPUHexagonState *env, float32 RsV,
                                 uint32_t pkt_need_commit)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float32_is_neg(RsV) && !float32_is_any_nan(RsV) && !float32_is_zero(RsV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float32_to_uint64_round_to_zero(RsV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

int64_t HELPER(conv_sf2d_chop)(CPUHexagonState *env, float32 RsV,
                               uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

uint32_t HELPER(conv_df2uw_chop)(CPUHexagonState *env, float64 RssV,
                                 uint32_t pkt_need_commit)
{
    uint32_t RdV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV) && !float64_is_zero(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RdV = 0;
    } else {
        RdV = float64_to_uint32_round_to_zero(RssV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

int32_t HELPER(conv_df2w_chop)(CPUHexagonState *env, float64 RssV,
                               uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

uint64_t HELPER(conv_df2ud_chop)(CPUHexagonState *env, float64 RssV,
                                 uint32_t pkt_need_commit)
{
    uint64_t RddV;
    arch_fpop_start(env);
    /* Hexagon checks the sign before rounding */
    if (float64_is_neg(RssV) && !float64_is_any_nan(RssV) && !float64_is_zero(RssV)) {
        float_raise(float_flag_invalid, &env->fp_status);
        RddV = 0;
    } else {
        RddV = float64_to_uint64_round_to_zero(RssV, &env->fp_status);
    }
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

int64_t HELPER(conv_df2d_chop)(CPUHexagonState *env, float64 RssV,
                               uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float32 HELPER(sfadd)(CPUHexagonState *env, float32 RsV, float32 RtV,
                      uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_add(RsV, RtV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float32 HELPER(sfsub)(CPUHexagonState *env, float32 RsV, float32 RtV,
                      uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_sub(RsV, RtV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

int32_t HELPER(sfcmpeq)(CPUHexagonState *env, float32 RsV, float32 RtV,
                        uint32_t pkt_need_commit)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float32_eq_quiet(RsV, RtV, &env->fp_status));
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

int32_t HELPER(sfcmpgt)(CPUHexagonState *env, float32 RsV, float32 RtV,
                        uint32_t pkt_need_commit)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float32_compare_quiet(RsV, RtV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater);
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

int32_t HELPER(sfcmpge)(CPUHexagonState *env, float32 RsV, float32 RtV,
                        uint32_t pkt_need_commit)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float32_compare_quiet(RsV, RtV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater ||
                   cmp == float_relation_equal);
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

int32_t HELPER(sfcmpuo)(CPUHexagonState *env, float32 RsV, float32 RtV,
                        uint32_t pkt_need_commit)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float32_unordered_quiet(RsV, RtV, &env->fp_status));
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

float32 HELPER(sfmax)(CPUHexagonState *env, float32 RsV, float32 RtV,
                      uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_maximum_number(RsV, RtV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float32 HELPER(sfmin)(CPUHexagonState *env, float32 RsV, float32 RtV,
                      uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_minimum_number(RsV, RtV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

int32_t HELPER(sfclass)(CPUHexagonState *env, float32 RsV, int32_t uiV,
                        uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

float32 HELPER(sffixupn)(CPUHexagonState *env, float32 RsV, float32 RtV,
                         uint32_t pkt_need_commit)
{
    float32 RdV = 0;
    int adjust;
    arch_fpop_start(env);
    arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status);
    RdV = RsV;
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float32 HELPER(sffixupd)(CPUHexagonState *env, float32 RsV, float32 RtV,
                         uint32_t pkt_need_commit)
{
    float32 RdV = 0;
    int adjust;
    arch_fpop_start(env);
    arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status);
    RdV = RtV;
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float32 HELPER(sffixupr)(CPUHexagonState *env, float32 RsV,
                         uint32_t pkt_need_commit)
{
    float32 RdV = 0;
    int adjust;
    arch_fpop_start(env);
    arch_sf_invsqrt_common(&RsV, &RdV, &adjust, &env->fp_status);
    RdV = RsV;
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float64 HELPER(dfadd)(CPUHexagonState *env, float64 RssV, float64 RttV,
                      uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_add(RssV, RttV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float64 HELPER(dfsub)(CPUHexagonState *env, float64 RssV, float64 RttV,
                      uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_sub(RssV, RttV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float64 HELPER(dfmax)(CPUHexagonState *env, float64 RssV, float64 RttV,
                      uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_maximum_number(RssV, RttV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float64 HELPER(dfmin)(CPUHexagonState *env, float64 RssV, float64 RttV,
                      uint32_t pkt_need_commit)
{
    float64 RddV;
    arch_fpop_start(env);
    RddV = float64_minimum_number(RssV, RttV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

int32_t HELPER(dfcmpeq)(CPUHexagonState *env, float64 RssV, float64 RttV,
                        uint32_t pkt_need_commit)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float64_eq_quiet(RssV, RttV, &env->fp_status));
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

int32_t HELPER(dfcmpgt)(CPUHexagonState *env, float64 RssV, float64 RttV,
                        uint32_t pkt_need_commit)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float64_compare_quiet(RssV, RttV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater);
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

int32_t HELPER(dfcmpge)(CPUHexagonState *env, float64 RssV, float64 RttV,
                        uint32_t pkt_need_commit)
{
    int cmp;
    int32_t PdV;
    arch_fpop_start(env);
    cmp = float64_compare_quiet(RssV, RttV, &env->fp_status);
    PdV = f8BITSOF(cmp == float_relation_greater ||
                   cmp == float_relation_equal);
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

int32_t HELPER(dfcmpuo)(CPUHexagonState *env, float64 RssV, float64 RttV,
                        uint32_t pkt_need_commit)
{
    int32_t PdV;
    arch_fpop_start(env);
    PdV = f8BITSOF(float64_unordered_quiet(RssV, RttV, &env->fp_status));
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

int32_t HELPER(dfclass)(CPUHexagonState *env, float64 RssV, int32_t uiV,
                        uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return PdV;
}

float32 HELPER(sfmpy)(CPUHexagonState *env, float32 RsV, float32 RtV,
                      uint32_t pkt_need_commit)
{
    float32 RdV;
    arch_fpop_start(env);
    RdV = float32_mul(RsV, RtV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RdV;
}

float32 HELPER(sffma)(CPUHexagonState *env, float32 RxV,
                      float32 RsV, float32 RtV,
                      uint32_t pkt_need_commit)
{
    arch_fpop_start(env);
    RxV = float32_muladd(RsV, RtV, RxV, 0, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RxV;
}

float32 HELPER(sffma_sc)(CPUHexagonState *env, float32 RxV,
                         float32 RsV, float32 RtV, float32 PuV,
                         uint32_t pkt_need_commit)
{
    arch_fpop_start(env);
    RxV = float32_muladd_scalbn(RsV, RtV, RxV, fSXTN(8, 64, PuV),
                                float_muladd_suppress_add_product_zero,
                                &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RxV;
}

float32 HELPER(sffms)(CPUHexagonState *env, float32 RxV,
                      float32 RsV, float32 RtV, uint32_t pkt_need_commit)
{
    arch_fpop_start(env);
    RxV = float32_muladd(RsV, RtV, RxV, float_muladd_negate_product,
                         &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RxV;
}

static float32 do_sffma_lib(CPUHexagonState *env, float32 RxV,
                            float32 RsV, float32 RtV, int negate,
                            uint32_t pkt_need_commit)
{
    int flags;

    arch_fpop_start(env);

    set_float_rounding_mode(float_round_nearest_even_max, &env->fp_status);
    RxV = float32_muladd(RsV, RtV, RxV,
                         negate | float_muladd_suppress_add_product_zero,
                         &env->fp_status);

    flags = get_float_exception_flags(&env->fp_status);
    if (flags) {
        /* Flags are suppressed by this instruction. */
        set_float_exception_flags(0, &env->fp_status);

        /* Return 0 for Inf - Inf. */
        if (flags & float_flag_invalid_isi) {
            RxV = 0;
        }
    }

    arch_fpop_end(env, pkt_need_commit);
    return RxV;
}

float32 HELPER(sffma_lib)(CPUHexagonState *env, float32 RxV,
                          float32 RsV, float32 RtV, uint32_t pkt_need_commit)
{
    return do_sffma_lib(env, RxV, RsV, RtV, 0, pkt_need_commit);
}

float32 HELPER(sffms_lib)(CPUHexagonState *env, float32 RxV,
                          float32 RsV, float32 RtV, uint32_t pkt_need_commit)
{
    return do_sffma_lib(env, RxV, RsV, RtV, float_muladd_negate_product,
                        pkt_need_commit);
}

float64 HELPER(dfmpyfix)(CPUHexagonState *env, float64 RssV, float64 RttV,
                         uint32_t pkt_need_commit)
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
    arch_fpop_end(env, pkt_need_commit);
    return RddV;
}

float64 HELPER(dfmpyhh)(CPUHexagonState *env, float64 RxxV,
                        float64 RssV, float64 RttV, uint32_t pkt_need_commit)
{
    arch_fpop_start(env);
    RxxV = internal_mpyhh(RssV, RttV, RxxV, &env->fp_status);
    arch_fpop_end(env, pkt_need_commit);
    return RxxV;
}

#ifndef CONFIG_USER_ONLY
void HELPER(raise_stack_overflow)(CPUHexagonState *env, uint32_t slot,
                                  target_ulong badva)
{
    /*
     * Per section 7.3.1 of the V67 Programmer's Reference,
     * stack limit exception isn't raised in monitor mode.
     */
    if (sys_in_monitor_mode(env)) {
        return;
    }

    CPUState *cs = env_cpu(env);
    cs->exception_index = HEX_EVENT_PRECISE;
    env->cause_code = HEX_CAUSE_STACK_LIMIT;

    if (slot == 0) {
        arch_set_system_reg(env, HEX_SREG_BADVA0, badva);
        SET_SSR_FIELD(env, SSR_V0, 1);
        SET_SSR_FIELD(env, SSR_V1, 0);
        SET_SSR_FIELD(env, SSR_BVS, 0);
    } else if (slot == 1) {
        arch_set_system_reg(env, HEX_SREG_BADVA1, badva);
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
void HELPER(modify_ssr)(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    BQL_LOCK_GUARD();
    hexagon_modify_ssr(env, new, old);
}

static void hex_k0_lock(CPUHexagonState *env)
{
    trace_hexagon_k0_lock_info(env->threadId, "Before hex_k0_lock");
    BQL_LOCK_GUARD();
    trace_hexagon_k0_lock(env->threadId, env->next_PC, env->k0_lock_count);
    g_assert((env->k0_lock_count == 0) || (env->k0_lock_count == 1));

    uint32_t syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);
    if (GET_SYSCFG_FIELD(SYSCFG_K0LOCK, syscfg)) {
        if (env->k0_lock_state == HEX_LOCK_QUEUED) {
            env->next_PC += 4;
            env->k0_lock_count++;
            env->k0_lock_state = HEX_LOCK_OWNER;
            SET_SYSCFG_FIELD(env, SYSCFG_K0LOCK, 1);
            trace_hexagon_k0_lock_info(env->threadId,
                                       "queued thread waiting gets lock\n");
            return;
        }
        if (env->k0_lock_state == HEX_LOCK_OWNER) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "Double k0lock at PC: 0x%x, thread may hang\n",
                          env->next_PC);
            env->next_PC += 4;
            CPUState *cs = env_cpu(env);
            cpu_interrupt(cs, CPU_INTERRUPT_HALT);
            return;
        }
        trace_hexagon_k0_lock_info(env->threadId, "Waiting for k0lock");
        env->k0_lock_state = HEX_LOCK_WAITING;
        CPUState *cs = env_cpu(env);
        cpu_interrupt(cs, CPU_INTERRUPT_HALT);
    } else {
        trace_hexagon_k0_lock_info(env->threadId, "Acquired k0lock");
        env->next_PC += 4;
        env->k0_lock_count++;
        env->k0_lock_state = HEX_LOCK_OWNER;
        SET_SYSCFG_FIELD(env, SYSCFG_K0LOCK, 1);
    }

    trace_hexagon_k0_lock_info(env->threadId, "After hex_k0_lock\n");
}

static void hex_k0_unlock(CPUHexagonState *env)
{
    trace_hexagon_k0_lock_info(env->threadId, "Before hex_k0_unlock");
    BQL_LOCK_GUARD();
    trace_hexagon_k0_lock(env->threadId, env->next_PC, env->k0_lock_count);
    g_assert((env->k0_lock_count == 0) || (env->k0_lock_count == 1));

    /* Nothing to do if the k0 isn't locked by this thread */
    uint32_t syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);
    if ((GET_SYSCFG_FIELD(SYSCFG_K0LOCK, syscfg) == 0) ||
        (env->k0_lock_state != HEX_LOCK_OWNER)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "thread %d attempted to unlock k0 without having the "
                      "lock, k0_lock state = %d, syscfg:k0 = %d\n",
                      env->threadId, env->k0_lock_state,
                      GET_SYSCFG_FIELD(SYSCFG_K0LOCK, syscfg));
        g_assert(env->k0_lock_state != HEX_LOCK_WAITING);
        return;
    }

    trace_hexagon_k0_lock_info(env->threadId, "Unlocking k0lock");
    env->k0_lock_count--;
    env->k0_lock_state = HEX_LOCK_UNLOCKED;
    SET_SYSCFG_FIELD(env, SYSCFG_K0LOCK, 0);

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
        trace_hexagon_k0_lock_info(unlock_thread->threadId, "Will get the next k0lock");
        unlock_thread->k0_lock_state = HEX_LOCK_QUEUED;
        SET_SYSCFG_FIELD(unlock_thread, SYSCFG_K0LOCK, 1);
        cpu_interrupt(cs, CPU_INTERRUPT_K0_UNLOCK);
    }

    trace_hexagon_k0_lock_info(env->threadId, "After hex_k0_unlock\n");
}
#endif

/* Histogram instructions */

void HELPER(vhist)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int lane = 0; lane < 8; lane++) {
        for (int i = 0; i < VECTOR_SIZE_BYTE / 8; ++i) {
            unsigned char value = input->ub[(VECTOR_SIZE_BYTE / 8) * lane + i];
            unsigned char regno = value >> 3;
            unsigned char element = value & 7;

            env->VRegs[regno].uh[(VECTOR_SIZE_BYTE / 16) * lane + element]++;
        }
    }
}

void HELPER(vhistq)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int lane = 0; lane < 8; lane++) {
        for (int i = 0; i < VECTOR_SIZE_BYTE / 8; ++i) {
            unsigned char value = input->ub[(VECTOR_SIZE_BYTE / 8) * lane + i];
            unsigned char regno = value >> 3;
            unsigned char element = value & 7;

            if (fGETQBIT(env->qtmp, VECTOR_SIZE_BYTE / 8 * lane + i)) {
                env->VRegs[regno].uh[
                    (VECTOR_SIZE_BYTE / 16) * lane + element]++;
            }
        }
    }
}

void HELPER(vwhist256)(CPUHexagonState *env)
{
    MMVector *input = &env->tmp_VRegs[0];

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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

    for (int i = 0; i < (VECTOR_SIZE_BYTE / 2); i++) {
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
    env->slot_cancelled |= (1 << slot);
}

#ifndef CONFIG_USER_ONLY
void HELPER(iassignw)(CPUHexagonState *env, uint32_t src)
{
    uint32_t modectl;
    uint32_t thread_enabled_mask;
    CPUState *cpu;

    BQL_LOCK_GUARD();
    modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
    thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);

    CPU_FOREACH (cpu) {
        CPUHexagonState *thread_env = &(HEXAGON_CPU(cpu)->env);
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if (thread_enabled_mask & thread_id_mask) {
            uint32_t imask = arch_get_system_reg(thread_env, HEX_SREG_IMASK);
            uint32_t intbitpos = (src >> 16) & 0xF;
            uint32_t val = (src >> thread_env->threadId) & 0x1;
            imask = deposit32(imask, intbitpos, 1, val);
            arch_set_system_reg(thread_env, HEX_SREG_IMASK, imask);

            qemu_log_mask(CPU_LOG_INT, "%s: thread %d, new imask 0x%x\n",
                          __func__, thread_env->threadId, imask);
        }
    }
    hex_interrupt_update(env);
}

uint32_t HELPER(iassignr)(CPUHexagonState *env, uint32_t src)

{
    uint32_t modectl;
    uint32_t thread_enabled_mask;
    uint32_t intbitpos;
    uint32_t dest_reg;
    CPUState *cpu;

    BQL_LOCK_GUARD();
    modectl = arch_get_system_reg(env, HEX_SREG_MODECTL);
    thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    /* src fields are in same position as modectl, but mean different things */
    intbitpos = GET_FIELD(MODECTL_W, src);
    dest_reg = 0;
    CPU_FOREACH (cpu) {
        CPUHexagonState *thread_env = &(HEXAGON_CPU(cpu)->env);
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if (thread_enabled_mask & thread_id_mask) {
            uint32_t imask = arch_get_system_reg(thread_env, HEX_SREG_IMASK);
            dest_reg |= ((imask >> intbitpos) & 0x1) << thread_env->threadId;
        }
    }

    return dest_reg;
}


static inline bool ssr_ce_enabled(CPUHexagonState *env)
{
    target_ulong ssr = arch_get_system_reg(env, HEX_SREG_SSR);
    return GET_SSR_FIELD(SSR_CE, ssr);
}

static uint32_t creg_read(CPUHexagonState *env, uint32_t reg)
{
    HexagonCPU *cpu = env_archcpu(env);
    uint32_t low, high;
    if (IS_PMU_CREG(reg)) {
        target_ulong ssr = arch_get_system_reg(env, HEX_SREG_SSR);
        int ssr_pe = GET_SSR_FIELD(SSR_PE, ssr);
        return ssr_pe ? hexagon_get_pmu_counter(env, reg - HEX_REG_UPMUCNT0) : 0;
    }
    switch (reg) {
    case HEX_REG_PKTCNTLO:
        low = env->exec_ctr_pkt;
        arch_set_thread_reg(env, HEX_REG_PKTCNTLO, low);
        return low;
    case HEX_REG_PKTCNTHI:
        high = 0;
        arch_set_thread_reg(env, HEX_REG_PKTCNTHI, high);
        return high;
    case HEX_REG_UPCYCLELO:
        return ssr_ce_enabled(env) ? hexagon_get_sys_pcycle_count_low(env) : 0;
    case HEX_REG_UPCYCLEHI:
        return ssr_ce_enabled(env) ? hexagon_get_sys_pcycle_count_high(env) : 0;
    case HEX_REG_UTIMERLO:
        return hexagon_globalreg_read(cpu->globalregs, HEX_SREG_TIMERLO);
    case HEX_REG_UTIMERHI:
        return hexagon_globalreg_read(cpu->globalregs, HEX_SREG_TIMERHI);
    default:
        return env->gpr[reg];
    }
}

uint32_t HELPER(creg_read)(CPUHexagonState *env, uint32_t reg)
{
    return creg_read(env, reg);
}

uint64_t HELPER(creg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    return  (uint64_t)creg_read(env, reg) |
           (((uint64_t)creg_read(env, reg + 1)) << 32);
}

static inline QEMU_ALWAYS_INLINE uint32_t sreg_read(CPUHexagonState *env,
                                                    uint32_t reg)
{
    g_assert(bql_locked());
    if (reg == HEX_SREG_BADVA) {
        target_ulong ssr = arch_get_system_reg(env, HEX_SREG_SSR);
        if (GET_SSR_FIELD(SSR_BVS, ssr)) {
            return arch_get_system_reg(env, HEX_SREG_BADVA1);
        }
        return arch_get_system_reg(env, HEX_SREG_BADVA0);
    } else if (IS_PMU_SREG(reg)) {
        return hexagon_get_pmu_counter(env, pmu_index_from_sreg(reg));
    } else if (reg == HEX_SREG_IPENDAD) {
        return (arch_get_system_reg(env, HEX_SREG_IPEND) & 0xffff) |
            ((arch_get_system_reg(env, HEX_SREG_IAD) & 0xffff) << 16);
    }
    return arch_get_system_reg(env, reg);
}

uint32_t HELPER(sreg_read)(CPUHexagonState *env, uint32_t reg)
{
    BQL_LOCK_GUARD();
    return sreg_read(env, reg);
}

uint32_t hexagon_sreg_read(CPUHexagonState *env, uint32_t reg)
{
    return sreg_read(env, reg);
}

uint64_t HELPER(sreg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    BQL_LOCK_GUARD();
    if (reg == HEX_SREG_PCYCLELO) {
        return hexagon_get_sys_pcycle_count(env);
    }
    return   (uint64_t)sreg_read(env, reg) |
           (((uint64_t)sreg_read(env, reg + 1)) << 32);
}

#define DECL_PMU_EVENT(name, val) case name:
static bool pmu_event_implemented(int event)
{
    switch (event) {
    HEX_PMU_EVENTS
        return true;
    default:
        return false;
    }
}
#undef DECL_PMU_EVENT

static inline void log_if_unimp_pmu_event(int event)
{
    if (qemu_loglevel_mask(LOG_UNIMP) && !pmu_event_implemented(event)) {
        qemu_log("PMU event %d (0x%x) not implemented\n", event, event);
    }
}

static void check_all_pmu_events(CPUHexagonState *env)
{
    if (qemu_loglevel_mask(LOG_UNIMP)) {
        for (int i = 0; i < NUM_PMU_CTRS; i++) {
            log_if_unimp_pmu_event(env->pmu.g_events[i]);
        }
    }
}

static void modify_syscfg(CPUHexagonState *env, uint32_t val)
{
    g_assert(bql_locked());

    uint32_t old;
    uint32_t syscfg_read_only_mask = 0x80001c00;
    uint32_t syscfg = arch_get_system_reg(env, HEX_SREG_SYSCFG);

    /* clear read-only bits if they are set in the new value. */
    val &= ~syscfg_read_only_mask;
    /* if read-only are currently set in syscfg keep them set. */
    val |= (syscfg & syscfg_read_only_mask);

    uint32_t tmp = val;
    old = arch_get_system_reg(env, HEX_SREG_SYSCFG);
    arch_set_system_reg(env, HEX_SREG_SYSCFG, tmp);

    /* Check for change in MMU enable */
    target_ulong old_mmu_enable = GET_SYSCFG_FIELD(SYSCFG_MMUEN, old);
    uint8_t old_en = GET_SYSCFG_FIELD(SYSCFG_PCYCLEEN, old);
    uint8_t old_gie = GET_SYSCFG_FIELD(SYSCFG_GIE, old);
    uint8_t old_pm = GET_SYSCFG_FIELD(SYSCFG_PM, old);
    target_ulong new_mmu_enable =
        GET_SYSCFG_FIELD(SYSCFG_MMUEN, val);
    if (new_mmu_enable && !old_mmu_enable) {
        hex_mmu_on(env);
    } else if (!new_mmu_enable && old_mmu_enable) {
        hex_mmu_off(env);
    }

    /* Changing pcycle enable from 0 to 1 resets the counters */
    uint8_t new_en = GET_SYSCFG_FIELD(SYSCFG_PCYCLEEN, val);
    CPUState *cs;
    if (old_en == 0 && new_en == 1) {
        CPU_FOREACH(cs) {
            CPUHexagonState *_env = cpu_env(cs);
            _env->t_cycle_count = 0;
        }
    }

    /* See if global interrupts are turned on */
    uint8_t new_gie = GET_SYSCFG_FIELD(SYSCFG_GIE, val);
    if (!old_gie && new_gie) {
        qemu_log_mask(CPU_LOG_INT, "%s: global interrupts enabled\n", __func__);
        hex_interrupt_update(env);
    }

    if (qemu_loglevel_mask(LOG_UNIMP)) {
        int new_v2x = GET_SYSCFG_FIELD(SYSCFG_V2X, val);
        if (!new_v2x) {
            qemu_log("HVX: 64 bits vector length is unsupported\n");
        }
    }

    uint8_t new_pm = GET_SYSCFG_FIELD(SYSCFG_PM, val);
    if (!old_pm && new_pm) {
        check_all_pmu_events(env);
    }
}

static void set_pmu_event(CPUHexagonState *env, unsigned int index,
                          uint16_t event)
{
    g_assert(index < NUM_PMU_CTRS);

    pmu_lock();
    uint16_t old_event = env->pmu.g_events[index];
    env->pmu.g_events[index] = event;

    bool pmu_enabled =
        GET_SYSCFG_FIELD(SYSCFG_PM, arch_get_system_reg(env, HEX_SREG_SYSCFG));

    if (event != old_event) {
        if (pmu_enabled) {
            log_if_unimp_pmu_event(event);
        }
        /*
         * As we are changing event, accumulate the current event's stats into
         * the counter offset, so that we don't lose this value. Also reset
         * the new event stats.
         */
        env->pmu.g_ctrs_off[index] += hexagon_get_pmu_event_stats(old_event);
        hexagon_reset_pmu_event_stats(event);
    }
    pmu_unlock();
}

static bool handle_pmu_sreg_write(CPUHexagonState *env, uint32_t reg,
                                  uint32_t val)
{
    uint32_t old = arch_get_system_reg(env, reg);
    uint32_t new = val;

    g_assert(bql_locked());
    if (reg == HEX_SREG_PMUSTID0 || reg == HEX_SREG_PMUSTID1) {
        if (old != new) {
            qemu_log_mask(LOG_UNIMP, "PMUSTID settings not implemented.");
        }
        arch_set_system_reg(env, reg, val);
        return true;
    } else if (reg == HEX_SREG_PMUCFG) {
        int old_thmask = GET_FIELD(PMUCFG_THMASK, old);
        int new_thmask = GET_FIELD(PMUCFG_THMASK, val);
        if (old_thmask != new_thmask && new_thmask) {
            qemu_log_mask(LOG_UNIMP,
                          "Only PMUCFG thread mask 0 is implemented.");
        }
        pmu_lock();
        for (int i = 0; i < NUM_PMU_CTRS; i++) {
            uint16_t new_bits = GET_FIELD(PMUCFG_CNT0_MSB + i, val);
            set_pmu_event(env, i,
                          deposit16(env->pmu.g_events[i], 8, 2, new_bits));
        }
        pmu_unlock();
        arch_set_system_reg(env, reg, val);
        return true;
    } else if (reg == HEX_SREG_PMUEVTCFG || reg == HEX_SREG_PMUEVTCFG1) {
        int half_pmu_ctrs = NUM_PMU_CTRS / 2;
        pmu_lock();
        for (int i = 0; i < half_pmu_ctrs; i++) {
            int index = i + (reg == HEX_SREG_PMUEVTCFG1 ? half_pmu_ctrs : 0);
            uint16_t new_bits = GET_FIELD(PMUEVTCFG_CNT0_LSB + i, val);
            set_pmu_event(env, index,
                          deposit16(env->pmu.g_events[index], 0, 8, new_bits));
        }
        pmu_unlock();
        arch_set_system_reg(env, reg, val);
        return true;
    } else if (IS_PMU_SREG(reg)) {
        hexagon_set_pmu_counter(env, reg, val);
        return true;
    }
    return false;
}

static inline QEMU_ALWAYS_INLINE void sreg_write(CPUHexagonState *env,
                                                 uint32_t reg, uint32_t val)

{
    g_assert(bql_locked());
    if (reg == HEX_SREG_SYSCFG) {
        modify_syscfg(env, val);
    } else if (reg == HEX_SREG_IMASK) {
        val = GET_FIELD(IMASK_MASK, val);
        arch_set_system_reg(env, reg, val);
    } else if (reg == HEX_SREG_PCYCLELO) {
        hexagon_set_sys_pcycle_count_low(env, val);
    } else if (reg == HEX_SREG_PCYCLEHI) {
        hexagon_set_sys_pcycle_count_high(env, val);
    } else if (!handle_pmu_sreg_write(env, reg, val)) {
        arch_set_system_reg(env, reg, val);
    }
}

static inline QEMU_ALWAYS_INLINE void
sreg_write_masked(CPUHexagonState *env, uint32_t reg, uint32_t val)

{
    g_assert(bql_locked());
    if (reg == HEX_SREG_SYSCFG) {
        modify_syscfg(env, val);
    } else if (reg == HEX_SREG_IMASK) {
        val = GET_FIELD(IMASK_MASK, val);
        arch_set_system_reg_masked(env, reg, val);
    } else if (reg == HEX_SREG_PCYCLELO) {
        hexagon_set_sys_pcycle_count_low(env, val);
    } else if (reg == HEX_SREG_PCYCLEHI) {
        hexagon_set_sys_pcycle_count_high(env, val);
    } else if (!handle_pmu_sreg_write(env, reg, val)) {
        arch_set_system_reg_masked(env, reg, val);
    }
}


void HELPER(sreg_write)(CPUHexagonState *env, uint32_t reg, uint32_t val)
{
    BQL_LOCK_GUARD();
    sreg_write(env, reg, val);
}

void HELPER(sreg_write_masked)(CPUHexagonState *env, uint32_t reg, uint32_t val)
{
    BQL_LOCK_GUARD();
    sreg_write_masked(env, reg, val);
}

void hexagon_gdb_sreg_write(CPUHexagonState *env, uint32_t reg, uint32_t val)
{
    BQL_LOCK_GUARD();
    sreg_write(env, reg, val);
    /*
     * The above is needed to run special logic for regs like syscfg, but it
     * won't set read-only bits. This will:
     */
    arch_set_system_reg(env, reg, val);
}

void HELPER(sreg_write_pair)(CPUHexagonState *env, uint32_t reg, uint64_t val)

{
    BQL_LOCK_GUARD();
    sreg_write(env, reg, val & 0xFFFFFFFF);
    sreg_write(env, reg + 1, val >> 32);
}

void HELPER(sreg_write_pair_masked)(CPUHexagonState *env, uint32_t reg,
                                    uint64_t val)

{
    BQL_LOCK_GUARD();
    sreg_write_masked(env, reg, val & 0xFFFFFFFF);
    sreg_write_masked(env, reg + 1, val >> 32);
}

uint32_t HELPER(greg_read)(CPUHexagonState *env, uint32_t reg)

{
    return hexagon_greg_read(env, reg);
}

uint64_t HELPER(greg_read_pair)(CPUHexagonState *env, uint32_t reg)

{
    if (reg == HEX_GREG_G0 || reg == HEX_GREG_G2) {
        return (uint64_t)(env->greg[reg]) |
               (((uint64_t)(env->greg[reg + 1])) << 32);
    }
    switch (reg) {
    case HEX_GREG_GPCYCLELO: {
        target_ulong ssr = arch_get_system_reg(env, HEX_SREG_SSR);
        int ssr_ce = GET_SSR_FIELD(SSR_CE, ssr);
        return ssr_ce ? hexagon_get_sys_pcycle_count(env) : 0;
    }
    default:
        return (uint64_t)hexagon_greg_read(env, reg) |
               ((uint64_t)(hexagon_greg_read(env, reg + 1)) << 32);
    }
}

uint32_t HELPER(getimask)(CPUHexagonState *env, uint32_t tid)

{
    CPUState *cs;
    CPU_FOREACH(cs) {
        CPUHexagonState *found_env = cpu_env(cs);
        if (found_env->threadId == tid) {
            target_ulong imask = arch_get_system_reg(found_env, HEX_SREG_IMASK);
            qemu_log_mask(CPU_LOG_INT, "%s: tid %d imask = 0x%x\n",
                          __func__, env->threadId,
                          (unsigned)GET_FIELD(IMASK_MASK, imask));
            return GET_FIELD(IMASK_MASK, imask);
        }
    }
    return 0;
}

static void resched(CPUHexagonState *env);

void HELPER(setprio)(CPUHexagonState *env, uint32_t thread, uint32_t prio)
{
    CPUState *cs;

    g_assert(env->processor_ptr->thread_system_mask != 0);

    BQL_LOCK_GUARD();
    thread &= env->processor_ptr->thread_system_mask;
    CPU_FOREACH(cs) {
        CPUHexagonState *found_env = cpu_env(cs);
        if (thread == found_env->threadId) {
            SET_SYSTEM_FIELD(found_env, HEX_SREG_STID, STID_PRIO, prio);
            qemu_log_mask(CPU_LOG_INT, "%s: tid %d prio = 0x%x\n",
                          __func__, found_env->threadId, prio);
            resched(env);
            return;
        }
    }
    g_assert_not_reached();
}

void HELPER(setimask)(CPUHexagonState *env, uint32_t pred, uint32_t imask)
{
    CPUState *cs;

    g_assert(env->processor_ptr->thread_system_mask != 0);

    BQL_LOCK_GUARD();
    pred &= env->processor_ptr->thread_system_mask;
    CPU_FOREACH(cs) {
        CPUHexagonState *found_env = cpu_env(cs);

        if (pred == found_env->threadId) {
            SET_SYSTEM_FIELD(found_env, HEX_SREG_IMASK, IMASK_MASK, imask);
            qemu_log_mask(CPU_LOG_INT, "%s: tid %d imask 0x%x\n",
                          __func__, found_env->threadId, imask);
            hex_interrupt_update(env);
            return;
        }
    }
    hex_interrupt_update(env);
}

void HELPER(start)(CPUHexagonState *env, uint32_t imask)
{
    hexagon_start_threads(env, imask);
}

void HELPER(stop)(CPUHexagonState *env)
{
    hexagon_stop_thread(env);
}

static inline QEMU_ALWAYS_INLINE void resched(CPUHexagonState *env)
{
    uint32_t schedcfg;
    uint32_t schedcfg_en;
    int int_number;
    CPUState *cs;
    uint32_t lowest_th_prio = 0; /* 0 is highest prio */
    uint32_t bestwait_reg;
    uint32_t best_prio;

    BQL_LOCK_GUARD();
    qemu_log_mask(CPU_LOG_INT, "%s: check resched\n", __func__);
    schedcfg = arch_get_system_reg(env, HEX_SREG_SCHEDCFG);
    schedcfg_en = GET_FIELD(SCHEDCFG_EN, schedcfg);
    int_number = GET_FIELD(SCHEDCFG_INTNO, schedcfg);

    if (!schedcfg_en) {
        return;
    }

    CPU_FOREACH(cs) {
        HexagonCPU *thread = HEXAGON_CPU(cs);
        CPUHexagonState *thread_env = &(thread->env);
        uint32_t th_prio = GET_FIELD(
            STID_PRIO, arch_get_system_reg(thread_env, HEX_SREG_STID));
        if (!hexagon_thread_is_enabled(thread_env)) {
            continue;
        }

        lowest_th_prio = (lowest_th_prio > th_prio)
            ? lowest_th_prio
            : th_prio;
    }

    bestwait_reg = arch_get_system_reg(env, HEX_SREG_BESTWAIT);
    best_prio = GET_FIELD(BESTWAIT_PRIO, bestwait_reg);

    /*
     * If the lowest priority thread is lower priority than the
     * value in the BESTWAIT register, we must raise the reschedule
     * interrupt on the lowest priority thread.
     */
    if (lowest_th_prio > best_prio) {
        qemu_log_mask(CPU_LOG_INT,
                      "%s: raising resched int %d, cur PC 0x%08x\n", __func__,
                      int_number, arch_get_thread_reg(env, HEX_REG_PC));
        SET_SYSTEM_FIELD(env, HEX_SREG_BESTWAIT, BESTWAIT_PRIO, ~0);
        hex_raise_interrupts(env, 1 << int_number, CPU_INTERRUPT_SWI);
    }
}

void HELPER(resched)(CPUHexagonState *env)
{
    resched(env);
}

void HELPER(nmi)(CPUHexagonState *env, uint32_t thread_mask)
{
    bool found = false;
    CPUState *cs = NULL;

    trace_hexagon_critical("NMI (Non Maskable Interrupt)", env->threadId,
        arch_get_thread_reg(env, HEX_REG_PC),
        arch_get_thread_reg(env, HEX_REG_R29),
        arch_get_system_reg(env, HEX_SREG_SSR),
        arch_get_system_reg(env, HEX_SREG_BADVA),
        arch_get_system_reg(env, HEX_SREG_ELR),
        arch_get_system_reg(env, HEX_SREG_DIAG));

    BQL_LOCK_GUARD();
    CPU_FOREACH (cs) {
        CPUHexagonState *thread_env = cpu_env(cs);
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if ((thread_mask & thread_id_mask) != 0) {
            found = true;
            qatomic_set(&cs->exception_index, HEX_EVENT_IMPRECISE);
            thread_env->cause_code = HEX_CAUSE_IMPRECISE_NMI;
            /*
             * Break the receiving CPU out of any TB chain so it returns to
             * cpu_handle_exception and dispatches the imprecise NMI.
             * Pending exception_index now satisfies hexagon_cpu_has_work,
             * so a halted thread also wakes.
             */
            qemu_cpu_kick(cs);
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

    g_assert(bql_locked());
    CPU_FOREACH(cs) {
        CPUHexagonState *thread_env = cpu_env(cs);
        const bool running =
            (get_exe_mode(thread_env) == HEX_EXE_MODE_RUN) &&
            (env->k0_lock_state != HEX_LOCK_WAITING) &&
            (env->tlb_lock_state != HEX_LOCK_WAITING);
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
    BQL_LOCK_GUARD();
    uint32_t num_threads = get_ready_count(env);
    if (1 <= num_threads && num_threads <= 6) {
        env->g_gcycle[num_threads - 1]++;
    }
}

void HELPER(cpu_limit)(CPUHexagonState *env, target_ulong PC,
                       target_ulong next_PC)
{
    BQL_LOCK_GUARD();
    uint32_t ready_count = get_ready_count(env);

    env->exec_ctr_tb++;

    if (ready_count > 1 &&
        env->exec_ctr_tb >= HEXAGON_TB_EXEC_PER_CPU_MAX) {
        env->gpr[HEX_REG_PC] = next_PC;
        env->exec_ctr_tb = 0;
        hexagon_raise_exception_err(env, EXCP_YIELD, next_PC);
    }
    env->last_cpu = env->threadId;
}

void HELPER(pending_interrupt)(CPUHexagonState *env)
{
    BQL_LOCK_GUARD();
    hex_interrupt_update(env);
}
#endif

#ifdef CONFIG_USER_ONLY
uint32_t HELPER(creg_read)(CPUHexagonState *env, uint32_t reg)
{
    /* These are handled directly by gen_read_ctrl_reg(). */
    g_assert(reg != HEX_REG_UPCYCLELO && reg != HEX_REG_UPCYCLEHI);

    if (reg == HEX_REG_UTIMERHI) {
        return cpu_get_host_ticks() >> 32;
    } else if (reg == HEX_REG_UTIMERLO) {
        return extract32(cpu_get_host_ticks(), 0, 32);
    }
    return 0;
}

uint64_t HELPER(creg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    if (reg == HEX_REG_UPCYCLELO) {
        /* Pretend SSR[CE] is always set. */
#ifndef CONFIG_USER_ONLY
        return hexagon_get_sys_pcycle_count(env);
#else
        return env->t_cycle_count;
#endif
    }
    if (reg == HEX_REG_UTIMERLO) {
        return cpu_get_host_ticks();
    }
    return 0;
}
#endif

uint32_t HELPER(read_pcyclelo)(CPUHexagonState *env)
{
#ifndef CONFIG_USER_ONLY
    return hexagon_get_sys_pcycle_count_low(env);
#else
    return extract64(env->t_cycle_count, 0, 32);
#endif
}

uint32_t HELPER(read_pcyclehi)(CPUHexagonState *env)
{
#ifndef CONFIG_USER_ONLY
    return hexagon_get_sys_pcycle_count_high(env);
#else
    return extract64(env->t_cycle_count, 32, 32);
#endif
}

void HELPER(commit_coproc)(CPUHexagonState *env)
{
    CoprocArgs args = {0};

    args.opcode = COPROC_COMMIT;
    coproc(&args);
}

void HELPER(check_vtcm_memcpy)(CPUHexagonState *env, uint32_t dst, uint32_t src,
                               uint32_t cp_chunks, uint32_t slot)
{
    /*
     * TODO: there are other exception triggers to be implemented:
     * - Source or destination base address in illegal space
     * - Source or destination buffer crosses a page boundary
     * - Source base address is NOT in AXI space
     */
    for (uint32_t i = 0; i < cp_chunks; i++) {
        if (!in_vtcm_space(env, dst + (i * 4))) {
            register_coproc_ldst_exception(env, slot, dst);
        }
    }
}

/* These macros can be referenced in the generated helper functions */
#define warn(...) /* Nothing */
#define fatal(...) g_assert_not_reached();
#define thread env
#define BOGUS_HELPER(tag) \
    printf("ERROR: bogus helper: " #tag "\n")

#ifdef CONFIG_USER_ONLY
/*
 * The external coproc in user mode has no page table
 */
static G_GNUC_UNUSED bool hex_tlb_find_match(CPUHexagonState *env, target_ulong VA,
                               MMUAccessType access_type,
                               hwaddr *PA, int *prot, uint64_t *size,
                               int32_t *excp, int mmu_idx) {
    return true;
}
#endif

uint32_t hexagon_creg_read_debug(CPUHexagonState *env, uint32_t reg)
{
    g_assert(reg >= HEX_REG_CREGS_START && reg < TOTAL_PER_THREAD_REGS);
#ifndef CONFIG_USER_ONLY
    return creg_read(env, reg);
#else
    if (reg == HEX_REG_UPCYCLELO) {
        return extract64(env->t_cycle_count, 0, 32);
    } else if (reg == HEX_REG_UPCYCLEHI) {
        return extract64(env->t_cycle_count, 32, 32);
    } else if (reg == HEX_REG_UTIMERHI) {
        return cpu_get_host_ticks() >> 32;
    } else if (reg == HEX_REG_UTIMERLO) {
        return extract32(cpu_get_host_ticks(), 0, 32);
    } else {
        return 0;
    }
#endif
}

#include "mmvec/kvx_ieee.h"
#include "mmvec/kvx_fp8.h"
#include "helper_funcs_generated.c.inc"
