/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_SYS_MACROS_H
#define HEXAGON_SYS_MACROS_H

/*
 * Macro definitions for Hexagon system mode
 */

#ifndef CONFIG_USER_ONLY


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
        uint32_t regval = arch_get_system_reg(ENV, REG); \
        fINSERT_BITS(regval, reg_field_info[FIELD].width, \
                     reg_field_info[FIELD].offset, (VAL)); \
        arch_set_system_reg(ENV, REG, regval); \
    } while (0)
#define SET_SSR_FIELD(ENV, FIELD, VAL) \
    SET_SYSTEM_FIELD(ENV, HEX_SREG_SSR, FIELD, VAL)
#define SET_SYSCFG_FIELD(ENV, FIELD, VAL) \
    SET_SYSTEM_FIELD(ENV, HEX_SREG_SYSCFG, FIELD, VAL)

#define CCR_FIELD_SET(ENV, FIELD) \
    (!!GET_FIELD(FIELD, arch_get_system_reg(ENV, HEX_SREG_CCR)))

#endif

#define fLOAD_PHYS(NUM, SIZE, SIGN, SRC1, SRC2, DST) { \
  const uintptr_t rs = ((unsigned long)(unsigned)(SRC1)) & 0x7ff; \
  const uintptr_t rt = ((unsigned long)(unsigned)(SRC2)) << 11; \
  const uintptr_t addr = rs + rt;         \
  physical_memory_read(addr, &DST, sizeof(uint32_t)); \
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
#define fFRAMECHECK(ADDR, EA) gen_framecheck(ctx, ADDR, EA)
#endif

#define fTRAP(TRAPTYPE, IMM) \
    register_trap_exception(env, TRAPTYPE, IMM, PC)

/*
 * Virtual instruction macros for trap1-based guest operations.
 * These implement the Hexagon virtualization ISA extensions.
 */

/* VMRTE (trap1 #1): return from guest event handler */
#define fVIRTINSN_RTE(IMM) \
    hexagon_vmrte(env)

/*
 * VMSETIE (trap1 #3): set/get CCR.GIE atomically.
 * VMGETIE (trap1 #4): read CCR.GIE.
 * VMSPSWAP (trap1 #6): swap SP with GOSP if GSR.UM=1.
 *
 * These virtual instructions return normally (unlike VMRTE which
 * calls cpu_loop_exit).  We must advance PC to next_PC so the
 * generated TB exit doesn't loop back to the same trap1 insn.
 * next_PC is a parameter in the generated helper for J2_trap1.
 */
#define fVIRTINSN_SETIE(IMM, REG) \
    do { \
        uint32_t _old_gie = CCR_FIELD_SET(env, CCR_GIE); \
        SET_SYSTEM_FIELD(env, HEX_SREG_CCR, CCR_GIE, (REG) & 1); \
        (REG) = _old_gie; \
        env->gpr[HEX_REG_PC] = next_PC; \
    } while (0)

/* VMGETIE (trap1 #4): read CCR.GIE */
#define fVIRTINSN_GETIE(IMM, REG) \
    do { \
        (REG) = CCR_FIELD_SET(env, CCR_GIE); \
        env->gpr[HEX_REG_PC] = next_PC; \
    } while (0)

/* VMSPSWAP (trap1 #6): swap SP with GOSP if GSR.UM=1 */
#define fVIRTINSN_SPSWAP(IMM, REG) \
    do { \
        uint32_t _gsr = env->greg[HEX_GREG_GSR]; \
        if (extract32(_gsr, 31, 1)) { \
            uint32_t _t = (REG); \
            (REG) = env->greg[HEX_GREG_GOSP]; \
            env->greg[HEX_GREG_GOSP] = _t; \
        } \
        env->gpr[HEX_REG_PC] = next_PC; \
    } while (0)

#define fGRE_ENABLED() \
    GET_FIELD(CCR_GRE, arch_get_system_reg(env, HEX_SREG_CCR))
#define fTRAP1_VIRTINSN(IMM) \
    (sys_in_guest_mode(env) && fGRE_ENABLED() && \
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
        g_assert_not_reached()

#define fDCINVIDX(REG)

#define fSET_TLB_LOCK()       hex_tlb_lock(env);
#define fCLEAR_TLB_LOCK()     hex_tlb_unlock(env);

#define fSET_K0_LOCK()        hex_k0_lock(env);
#define fCLEAR_K0_LOCK()      hex_k0_unlock(env);

#define fTLB_IDXMASK(INDEX) \
    ((INDEX) & (fPOW2_ROUNDUP(\
        fCAST4u(hexagon_tlb_get_num_entries(env_archcpu(env)->tlb))) - 1))
#define fDMATLB_IDXMASK(INDEX) \
    ((INDEX) & \
     (fPOW2_ROUNDUP(\
        fCAST4u(hexagon_tlb_get_dma_entries(env_archcpu(env)->tlb))) - 1))

#define fTLB_NONPOW2WRAP(INDEX) ({ \
    uint32_t _wrapped_idx = (INDEX); \
    uint32_t _num_entries = \
        hexagon_tlb_get_num_entries(env_archcpu(env)->tlb); \
    if ((INDEX) >= _num_entries) { \
        qemu_log_mask(LOG_GUEST_ERROR, \
                      "TLB index beyond limit, wrapping around. PC: 0x%x\n", \
                      env->gpr[HEX_REG_PC]); \
        _wrapped_idx = ((INDEX) - _num_entries); \
    } \
    _wrapped_idx; \
})

#define fTLBW(INDEX, VALUE) \
    hex_tlbw(env, (INDEX), (VALUE))
#define fTLBW_EXTENDED(INDEX, VALUE) \
    hex_tlbw(env, (INDEX), (VALUE))
#define fTLB_ENTRY_OVERLAP(VALUE, INDEX) \
    (hex_tlb_check_overlap(env, VALUE, INDEX) != -2)
#define fTLB_ENTRY_OVERLAP_IDX(VALUE, INDEX) \
    hex_tlb_check_overlap(env, VALUE, INDEX)
#define TLB_WRAP_INDEX(INDEX) \
    (((INDEX >= DMA_TLB_OFFSET) && \
      (hexagon_tlb_get_dma_entries(env_archcpu(env)->tlb) > 0)) \
     ? fTLB_NONPOW2WRAP(fDMATLB_IDXMASK(INDEX - DMA_TLB_OFFSET)) + \
       DMA_TLB_OFFSET \
     : fTLB_NONPOW2WRAP(fTLB_IDXMASK(INDEX)))
#define fTLBR(INDEX) \
    hex_tlb_read(env, INDEX)
#define fTLBR_EXTENDED(INDEX) \
    fTLBR(INDEX)

#define fTLBP(TLBHI) \
    hex_tlb_lookup(env, ((TLBHI) >> 12), ((TLBHI) << 12))
#define fTLBPP(TLBHI) \
    hex_tlb_lookup_extended(env, ((TLBHI) << 8), ((TLBHI) & 0xfffffffffffff000))

#define fIN_DEBUG_MODE(TNUM) \
    0    /* FIXME */
#define fIN_DEBUG_MODE_NO_ISDB(TNUM) \
    0    /* FIXME */

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

#define NUM_TLB_REGS(x) hexagon_tlb_get_total_entries(env_archcpu(env)->tlb)

#endif
