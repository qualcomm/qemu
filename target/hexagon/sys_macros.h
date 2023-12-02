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

#ifndef HEXAGON_SYS_MACROS_H
#define HEXAGON_SYS_MACROS_H

/*
 * Macro definitions for Hexagon system mode
 */

#ifndef CONFIG_USER_ONLY

#define READ_SREG(NUM) ARCH_GET_SYSTEM_REG(env, NUM)
#define READ_SGP0()    ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP0)
#define READ_SGP1()    ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP1)
#define READ_SGP10()   ((uint64_t)ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP0) | \
    ((uint64_t)ARCH_GET_SYSTEM_REG(env, HEX_SREG_SGP1) << 32))

#define WRITE_SREG(NUM, VAL)      log_sreg_write(env, NUM, VAL, slot)
#define WRITE_SGP0(VAL)           log_sreg_write(env, HEX_SREG_SGP0, VAL, slot)
#define WRITE_SGP1(VAL)           log_sreg_write(env, HEX_SREG_SGP1, VAL, slot)
#define WRITE_SGP10(VAL) \
    do { \
        log_sreg_write(env, HEX_SREG_SGP0, (VAL) & 0xFFFFFFFF, slot); \
        log_sreg_write(env, HEX_SREG_SGP1, (VAL) >> 32, slot); \
    } while (0)

#ifdef QEMU_GENERATE
#define GET_SSR_FIELD(RES, FIELD) \
    GET_FIELD(RES, FIELD, hex_t_sreg[HEX_SREG_SSR])
#else

#define GET_SSR_FIELD(FIELD, REGIN) \
    (uint32_t)GET_FIELD(FIELD, REGIN)
#define GET_SYSCFG_FIELD(FIELD, REGIN) \
    (uint32_t)GET_FIELD(FIELD, REGIN)
#define SET_SYSTEM_FIELD(ENV, REG, FIELD, VAL) \
    do { \
        uint32_t regval = ARCH_GET_SYSTEM_REG(ENV, REG); \
        fINSERT_BITS(regval, reg_field_info[FIELD].width, \
                     reg_field_info[FIELD].offset, (VAL)); \
        ARCH_SET_SYSTEM_REG(ENV, REG, regval); \
    } while (0)
#define SET_SSR_FIELD(ENV, FIELD, VAL) \
    SET_SYSTEM_FIELD(ENV, HEX_SREG_SSR, FIELD, VAL)
#define SET_SYSCFG_FIELD(ENV, FIELD, VAL) \
    SET_SYSTEM_FIELD(ENV, HEX_SREG_SYSCFG, FIELD, VAL)

#define CCR_FIELD_SET(ENV, FIELD) \
    (!!GET_FIELD(FIELD, ARCH_GET_SYSTEM_REG(ENV, HEX_SREG_CCR)))

/*
 * Direct-to-guest is not implemented yet, continuing would cause unexpected
 * behavior, so we abort.
 */
#define ASSERT_DIRECT_TO_GUEST_UNSET(ENV, EXCP) \
    do { \
        switch (EXCP) { \
        case HEX_EVENT_TRAP0: \
            g_assert(!CCR_FIELD_SET(ENV, CCR_GTE)); \
            break; \
        case HEX_EVENT_IMPRECISE: \
        case HEX_EVENT_PRECISE: \
        case HEX_EVENT_FPTRAP: \
            g_assert(!CCR_FIELD_SET(ENV, CCR_GEE)); \
            break; \
        default: \
            if ((EXCP) >= HEX_EVENT_INT0) { \
                g_assert(!CCR_FIELD_SET(ENV, CCR_GIE)); \
            } \
            break; \
        } \
    } while (0)
#endif

#define fREAD_ELR() (READ_SREG(HEX_SREG_ELR))

#define fLOAD_PHYS(NUM, SIZE, SIGN, SRC1, SRC2, DST) { \
  const uintptr_t rs = ((unsigned long)(unsigned)(SRC1)) & 0x7ff; \
  const uintptr_t rt = ((unsigned long)(unsigned)(SRC2)) << 11; \
  const uintptr_t addr = rs + rt;         \
  cpu_physical_memory_read(addr, &DST, sizeof(uint32_t)); \
}

#define fPOW2_HELP_ROUNDUP(VAL) \
    ((VAL) | \
     ((VAL) >> 1) | \
     ((VAL) >> 2) | \
     ((VAL) >> 4) | \
     ((VAL) >> 8) | \
     ((VAL) >> 16))
#define fPOW2_ROUNDUP(VAL) (fPOW2_HELP_ROUNDUP((VAL) - 1) + 1)

#ifdef QEMU_GENERATE
#ifdef FIXME
/* FIXME - Need to enable this check */
#define fFRAMECHECK(ADDR, EA) \
    do { \
        TCGLabel *ok = gen_new_label(); \
        tcg_gen_brcond_tl(TCG_COND_GEU, ADDR, hex_gpr[HEX_REG_FRAMELIMIT], \
                          ok); \
        gen_helper_raise_stack_overflow(tcg_env, \
                                        tcg_constant_i32(insn->slot), EA); \
        gen_set_label(ok); \
    } while (0)
#else
#define fFRAMECHECK(ADDR, EA)
#endif
#endif

#define fTRAP(TRAPTYPE, IMM) \
    register_trap_exception(env, TRAPTYPE, IMM, PC)

/*
 * FIXME - Fix the implementation of trap1
 *
 * The fVIRTINSN_* macros should do the right thing
 *
 * The fTRAP1_VIRTINSN macro should be
 *     ((fIN_GUEST_MODE())
 *        && (fGRE_ENABLED())
 *        && (   ((IMM) == 1)
 *            || ((IMM) == 3)
 *            || ((IMM) == 4)
 *            || ((IMM) == 6)))
 * The fIN_GUEST_MODE() check is missing
 */
#define fVIRTINSN_SPSWAP(IMM, REG)
#define fVIRTINSN_GETIE(IMM, REG) { REG = 0xdeafbeef; }
#define fVIRTINSN_SETIE(IMM, REG)
#define fVIRTINSN_RTE(IMM, REG)
#define fGRE_ENABLED() GET_FIELD(CCR_GRE, READ_SREG(HEX_SREG_CCR))
#define fTRAP1_VIRTINSN(IMM) \
    (fGRE_ENABLED() && \
        (((IMM) == 1) || ((IMM) == 3) || ((IMM) == 4) || ((IMM) == 6)))

#define fICINVIDX(REG)
#define fICKILL()
#define fDCKILL()
#define fL2KILL()
#define fL2UNLOCK()
#define fL2CLEAN()
#define fL2CLEANINV()
#define fL2CLEANPA(REG)
#define fL2CLEANINVPA(REG)
#define fL2CLEANINVIDX(REG)
#define fL2CLEANIDX(REG)
#define fL2INVIDX(REG)
#define fL2TAGR(INDEX, DST, DSTREG)
#define fL2UNLOCKA(VA)
#define fL2TAGW(INDEX, PART2)
#define fDCCLEANIDX(REG)
#define fDCCLEANINVIDX(REG)

/* Always succeed: */
#define fL2LOCKA(EA, PDV, PDN) (PDV = 0xFF)
#define fCLEAR_RTE_EX() \
    do { \
        uint32_t tmp = 0; \
        tmp = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR); \
        fINSERT_BITS(tmp, reg_field_info[SSR_EX].width, \
                     reg_field_info[SSR_EX].offset, 0); \
        log_sreg_write(env, HEX_SREG_SSR, tmp, slot); \
    } while (0)

#define fDCINVIDX(REG)
#define fDCINVA(REG) do { REG = REG; } while (0) /* Nothing to do in qemu */

#define fSET_TLB_LOCK()                             \
    do {                                            \
        hex_tlb_lock(env);                          \
        switch (ATOMIC_LOAD(env->tlb_lock_state)) { \
        case HEX_LOCK_OWNER:                        \
            env->gpr[HEX_REG_PC] = PC + 4;          \
            break;                                  \
        case HEX_LOCK_WAITING:                      \
            env->gpr[HEX_REG_PC] = PC;              \
            break;                                  \
        case HEX_LOCK_UNLOCKED:                     \
            g_assert_not_reached();                 \
            break;                                  \
        default:                                    \
            g_assert_not_reached();                 \
            break;                                  \
        }                                           \
    } while (0)
#define fCLEAR_TLB_LOCK()     hex_tlb_unlock(env);

#define fSET_K0_LOCK()                             \
    do {                                           \
        hex_k0_lock(env);                          \
        switch (ATOMIC_LOAD(env->k0_lock_state)) { \
        case HEX_LOCK_OWNER:                       \
            env->gpr[HEX_REG_PC] = PC + 4;         \
            break;                                 \
        case HEX_LOCK_WAITING:                     \
            env->gpr[HEX_REG_PC] = PC;             \
            break;                                 \
        case HEX_LOCK_UNLOCKED:                    \
            g_assert_not_reached();                \
            break;                                 \
        default:                                   \
            g_assert_not_reached();                \
            break;                                 \
        }                                          \
    } while (0)
#define fCLEAR_K0_LOCK()      hex_k0_unlock(env);

#define fGET_TNUM()               thread->threadId
#define fSTART(REG)               hexagon_start_threads(env, REG)
#define fRESUME(REG)              hexagon_resume_threads(env, REG)
#define fCLEAR_RUN_MODE(x)        hexagon_stop_thread(env)
#define READ_IMASK(TID)           getimask(env, TID)
#define WRITE_IMASK(PRED, MASK)   setimask(env, PRED, MASK)
#define WRITE_PRIO(TH, PRIO)      setprio(env, TH, PRIO)

#define fTLB_IDXMASK(INDEX) \
    ((INDEX) & (fPOW2_ROUNDUP(fCAST4u(NUM_TLB_ENTRIES)) - 1))

#define fTLB_NONPOW2WRAP(INDEX) \
    (((INDEX) >= NUM_TLB_ENTRIES) ? ((INDEX) - NUM_TLB_ENTRIES) : (INDEX))

#define fTLBW(INDEX, VALUE) \
    hex_tlbw(env, (INDEX), (VALUE))
#define fTLBW_EXTENDED(INDEX, VALUE) \
    hex_tlbw(env, (INDEX), (VALUE))
#define fTLB_ENTRY_OVERLAP(VALUE, INDEX) \
    (hex_tlb_check_overlap(env, VALUE, INDEX) != -2)
#define fTLB_ENTRY_OVERLAP_IDX(VALUE, INDEX) \
    hex_tlb_check_overlap(env, VALUE, INDEX)
#define fTLBR(INDEX) \
    (env->hex_tlb->entries[fTLB_NONPOW2WRAP(fTLB_IDXMASK(INDEX))])
#define fTLBR_EXTENDED(INDEX) \
    (env->hex_tlb->entries[fTLB_NONPOW2WRAP(fTLB_IDXMASK(INDEX))])
#define fTLBP(TLBHI) \
    hex_tlb_lookup(env, ((TLBHI) >> 12), ((TLBHI) << 12))

#define fIN_DEBUG_MODE(TNUM) \
    0    /* FIXME */
#define fIN_DEBUG_MODE_NO_ISDB(TNUM) \
    0    /* FIXME */
#define fIN_DEBUG_MODE_WARN(TNUM)

#ifdef QEMU_GENERATE

/*
 * Read tags back as zero for now:
 *
 * tag value in RD[31:10] for 32k, RD[31:9] for 16k
 */
#define fICTAGR(RS, RD, RD2) \
    do { \
        RD = ctx->zero; \
    } while (0)
#define fICTAGW(RS, RD)
#define fICDATAR(RS, RD) \
    do { \
        RD = ctx->zero; \
    } while (0)
#define fICDATAW(RS, RD)

#define fDCTAGW(RS, RT)
/* tag: RD[23:0], state: RD[30:29] */
#define fDCTAGR(INDEX, DST, DST_REG_NUM) \
    do { \
        DST = ctx->zero; \
    } while (0)
#else

/*
 * Read tags back as zero for now:
 *
 * tag value in RD[31:10] for 32k, RD[31:9] for 16k
 */
#define fICTAGR(RS, RD, RD2) \
    do { \
        RD = 0x00; \
    } while (0)
#define fICTAGW(RS, RD)
#define fICDATAR(RS, RD) \
    do { \
        RD = 0x00; \
    } while (0)
#define fICDATAW(RS, RD)

#define fDCTAGW(RS, RT)
/* tag: RD[23:0], state: RD[30:29] */
#define fDCTAGR(INDEX, DST, DST_REG_NUM) \
    do { \
        DST = HEX_DC_STATE_INVALID | 0x00; \
    } while (0)
#endif

#endif

#endif
