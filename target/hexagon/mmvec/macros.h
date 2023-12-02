/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_MMVEC_MACROS_H
#define HEXAGON_MMVEC_MACROS_H

#include "qemu/host-utils.h"
#include "mmvec/system_ext_mmvec.h"
#include "q6v_defines.h"

#ifndef QEMU_GENERATE
#define VdV      (*(mmvector_t *)(VdV_void))
#define VsV      (*(mmvector_t *)(VsV_void))
#define VuV      (*(mmvector_t *)(VuV_void))
#define VvV      (*(mmvector_t *)(VvV_void))
#define VwV      (*(mmvector_t *)(VwV_void))
#define VxV      (*(mmvector_t *)(VxV_void))
#define VyV      (*(mmvector_t *)(VyV_void))

#define VddV     (*(mmvector_pair_t *)(VddV_void))
#define VuuV     (*(mmvector_pair_t *)(VuuV_void))
#define VvvV     (*(mmvector_pair_t *)(VvvV_void))
#define VxxV     (*(mmvector_pair_t *)(VxxV_void))

#define QeV      (*(mmqreg_t *)(QeV_void))
#define QdV      (*(mmqreg_t *)(QdV_void))
#define QsV      (*(mmqreg_t *)(QsV_void))
#define QtV      (*(mmqreg_t *)(QtV_void))
#define QuV      (*(mmqreg_t *)(QuV_void))
#define QvV      (*(mmqreg_t *)(QvV_void))
#define QxV      (*(mmqreg_t *)(QxV_void))
#endif

#define LOG_VTCM_BYTE(VA, MASK, VAL, IDX) \
    do { \
        env->vtcm_log.data.ub[IDX] = (VAL); \
       if (MASK) { \
            set_bit((IDX), env->vtcm_log.mask); \
        } else { \
            clear_bit((IDX), env->vtcm_log.mask); \
        } \
        env->vtcm_log.va[IDX] = (VA); \
    } while (0)

/* VTCM Banks */
#ifdef CONFIG_USER_ONLY
#define LOG_VTCM_BANK(VAL, MASK, IDX)
#else
#define LOG_VTCM_BANK(VAL, MASK, IDX) \
    do { \
        env->vtcm_log.offsets.uh[IDX]  = (VAL & 0xFFF); \
        env->vtcm_log.offsets.uh[IDX] |= ((MASK & 0xF) << 12) ; \
    } while (0)
#endif

#define fVLOG_VTCM_WORD_INCREMENT(EA, OFFSET, INC, IDX, ALIGNMENT, LEN) \
    do { \
        int log_byte = 0; \
        vaddr_t va = EA; \
        vaddr_t va_high = EA + LEN; \
        for (int i0 = 0; i0 < 4; i0++) { \
            log_byte = (va + i0) <= va_high; \
            LOG_VTCM_BYTE(va + i0, log_byte, INC. ub[4 * IDX + i0], \
                          4 * IDX + i0); \
        } \
    } while (0)
#define fVLOG_VTCM_HALFWORD_INCREMENT(EA, OFFSET, INC, IDX, ALIGNMENT, LEN) \
    do { \
        int log_byte = 0; \
        target_ulong va = EA; \
        target_ulong va_high = EA + LEN; \
        for (int i0 = 0; i0 < 2; i0++) { \
            log_byte = (va + i0) <= va_high; \
            LOG_VTCM_BYTE(va + i0, log_byte, INC.ub[2 * IDX + i0], \
                          2 * IDX + i0); \
        } \
    } while (0)
#define fVLOG_VTCM_HALFWORD_INCREMENT_DV(EA, OFFSET, INC, IDX, IDX2, IDX_H, \
                                         ALIGNMENT, LEN) \
    do { \
        int log_byte = 0; \
        target_ulong va = EA; \
        target_ulong va_high = EA + LEN; \
        for (int i0 = 0; i0 < 2; i0++) { \
            log_byte = (va + i0) <= va_high; \
            LOG_VTCM_BYTE(va + i0, log_byte, INC.ub[2 * IDX + i0], \
                          2 * IDX + i0); \
        } \
    } while (0)
/* NOTE - Will this always be tmp_VRegs[0]; */
#define GATHER_FUNCTION(EA, OFFSET, IDX, LEN, ELEMENT_SIZE, BANK_IDX, QVAL) \
    do { \
        int i0; \
        target_ulong va = EA; \
        target_ulong va_high = EA + LEN; \
        uintptr_t ra = CPU_MEMOP_PC(env); \
        int log_bank = 0; \
        int log_byte = 0; \
        for (i0 = 0; i0 < ELEMENT_SIZE; i0++) { \
            log_byte = ((va + i0) <= va_high) && QVAL; \
            log_bank |= (log_byte << i0); \
            uint8_t B; \
            B = cpu_ldub_data_ra(env, EA + i0, ra); \
            env->tmp_VRegs[0].ub[ELEMENT_SIZE * IDX + i0] = B; \
            LOG_VTCM_BYTE(va + i0, log_byte, B, ELEMENT_SIZE * IDX + i0); \
        } \
        LOG_VTCM_BANK(va, log_bank, BANK_IDX); \
    } while (0)
#ifdef CONFIG_USER_ONLY
#define CLEAR_OFFSETS(IDX)
#else
#define CLEAR_OFFSETS(IDX) \
    do { \
        env->vtcm_log.offsets.ub[IDX] = 0; \
    } while (0)
#endif
#define SCATTER_OP_WRITE_TO_MEM(TYPE) \
    do { \
        uintptr_t ra = CPU_MEMOP_PC(env); \
        for (int i = 0; i < sizeof(MMVector); i += sizeof(TYPE)) { \
            if (test_bit(i, env->vtcm_log.mask)) { \
                TYPE dst = 0; \
                TYPE inc = 0; \
                for (int j = 0; j < sizeof(TYPE); j++) { \
                    uint8_t val; \
                    val = cpu_ldub_data_ra(env, env->vtcm_log.va[i + j], ra); \
                    dst |= val << (8 * j); \
                    inc |= env->vtcm_log.data.ub[j + i] << (8 * j); \
                    clear_bit(j + i, env->vtcm_log.mask); \
                    env->vtcm_log.data.ub[j + i] = 0; \
                    CLEAR_OFFSETS(j + i); \
                } \
                dst += inc; \
                for (int j = 0; j < sizeof(TYPE); j++) { \
                    cpu_stb_data_ra(env, env->vtcm_log.va[i + j], \
                                    (dst >> (8 * j)) & 0xFF, ra); \
                } \
            } \
        } \
    } while (0)
#define SCATTER_OP_PROBE_MEM(TYPE, MMU_IDX, RETADDR) \
    do { \
        for (int i = 0; i < sizeof(MMVector); i += sizeof(TYPE)) { \
            if (test_bit(i, env->vtcm_log.mask)) { \
                for (int j = 0; j < sizeof(TYPE); j++) { \
                    probe_read(env, env->vtcm_log.va[i + j], 1, \
                               MMU_IDX, RETADDR); \
                    probe_write(env, env->vtcm_log.va[i + j], 1, \
                                MMU_IDX, RETADDR); \
                } \
            } \
        } \
    } while (0)
#define SCATTER_FUNCTION(EA, OFFSET, IDX, LEN, ELEM_SIZE, BANK_IDX, QVAL, IN) \
    do { \
        int i0; \
        target_ulong va = EA; \
        target_ulong va_high = EA + LEN; \
        int log_bank = 0; \
        int log_byte = 0; \
        for (i0 = 0; i0 < ELEM_SIZE; i0++) { \
            log_byte = ((va + i0) <= va_high) && QVAL; \
            log_bank |= (log_byte << i0); \
            LOG_VTCM_BYTE(va + i0, log_byte, IN.ub[ELEM_SIZE * IDX + i0], \
                          ELEM_SIZE * IDX + i0); \
        } \
        LOG_VTCM_BANK(va, log_bank, BANK_IDX); \
    } while (0)

#endif
