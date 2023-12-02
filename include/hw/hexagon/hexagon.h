/*
 * Hexagon Baseboard System emulation.
 *
 * Copyright (c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#ifndef HW_HEXAGON_H
#define HW_HEXAGON_H

#include "exec/memory.h"

struct hexagon_board_boot_info {
    uint64_t ram_size;
    const char *kernel_filename;
    uint32_t kernel_elf_flags;
};

enum {
    HEXAGON_LPDDR,
    HEXAGON_IOMEM,
    HEXAGON_CONFIG_TABLE,
    HEXAGON_VTCM,
    HEXAGON_L2CFG,
    HEXAGON_FASTL2VIC,
    HEXAGON_TCM,
    HEXAGON_L2VIC,
    HEXAGON_CSR1,
    HEXAGON_QTMR_RG0,
    HEXAGON_QTMR_RG1,
    HEXAGON_CSR2,
    HEXAGON_QTMR2,
};

typedef enum {
  HEXAGON_COPROC_HVX = 0x01,
  HEXAGON_COPROC_RESERVED = 0x02,
} HexagonCoprocPresent;

typedef enum {
  HEXAGON_HVX_VEC_LEN_V2X_1 = 0x01,
  HEXAGON_HVX_VEC_LEN_V2X_2 = 0x02,
} HexagonVecLenSupported;

typedef enum {
  HEXAGON_L1_WRITE_THROUGH = 0x01,
  HEXAGON_L1_WRITE_BACK    = 0x02,
} HexagonL1WritePolicy;

typedef enum {
    v66_rev = 0xa666,
    v67_rev = 0x2667,
    v68_rev = 0x8d68,
    v69_rev = 0x8c69,
    v71_rev = 0x8c71,
    v73_rev = 0x8c73,
} Rev_t;

/* Config table address bases represent bits [35:16].
 */
#define HEXAGON_CFG_ADDR_BASE(addr) ((addr >> 16) & 0x0fffff)

#define HEXAGON_DEFAULT_L2_TAG_SIZE (1024)
#define HEXAGON_DEFAULT_TLB_ENTRIES (128)
#define HEXAGON_DEFAULT_HVX_CONTEXTS (4)

#define HEXAGON_V65_L2_LINE_SIZE_BYTES (0x40)
#define HEXAGON_V66_L2_LINE_SIZE_BYTES (0x40)
#define HEXAGON_V67h_3072_L2_LINE_SIZE_BYTES (0x40)
#define HEXAGON_V67_L2_LINE_SIZE_BYTES (HEXAGON_V67h_3072_L2_LINE_SIZE_BYTES)
#define HEXAGON_V68n_1024_L2_LINE_SIZE_BYTES (0x80)

#define HEXAGON_DEFAULT_L1D_SIZE_KB (10)
#define HEXAGON_DEFAULT_L1I_SIZE_KB (20)
#define HEXAGON_DEFAULT_L1D_WRITE_POLICY (HEXAGON_L1_WRITE_BACK)
#define HEXAGON_CFGSPACE_ENTRIES (128)

/* TODO: make this default per-arch?
 */
#define HEXAGON_HVX_DEFAULT_VEC_LOG_LEN_BYTES (7)

typedef  union {
  struct {
    uint32_t l2tcm_base; /* Base address of L2TCM space */
    uint32_t reserved; /* Reserved */
    uint32_t subsystem_base; /* Base address of subsystem space */
    uint32_t etm_base; /* Base address of ETM space */
    uint32_t l2cfg_base; /* Base address of L2 configuration space */
    uint32_t reserved2;
    uint32_t l1s0_base; /* Base address of L1S */
    uint32_t axi2_lowaddr; /* Base address of AXI2 */
    uint32_t streamer_base; /* Base address of streamer base */
    uint32_t clade_base; /* Base address of Clade */
    uint32_t fastl2vic_base; /* Base address of fast L2VIC */
    uint32_t jtlb_size_entries; /* Number of entries in JTLB */
    uint32_t coproc_present; /* Coprocessor type */
    uint32_t ext_contexts; /* Number of extension execution contexts available */
    uint32_t vtcm_base; /* Base address of Hexagon Vector Tightly Coupled Memory (VTCM) */
    uint32_t vtcm_size_kb; /* Size of VTCM (in KB) */
    uint32_t l2tag_size; /* L2 tag size */
    uint32_t l2ecomem_size; /* Amount of physical L2 memory in released version */
    uint32_t thread_enable_mask; /* Hardware threads available on the core */
    uint32_t eccreg_base; /* Base address of the ECC registers */
    uint32_t l2line_size; /* L2 line size */
    uint32_t tiny_core; /* Small Core processor (also implies audio extension) */
    uint32_t l2itcm_size; /* Size of L2TCM */
    uint32_t l2itcm_base; /* Base address of L2-ITCM */
    uint32_t clade2_base; /* Base address of Clade2 */
    uint32_t dtm_present; /* DTM is present */
    uint32_t dma_version; /* Version of the DMA */
    uint32_t hvx_vec_log_length; /* Native HVX vector length in log of bytes */
    uint32_t core_id; /* Core ID of the multi-core */
    uint32_t core_count; /* Number of multi-core cores */
    uint32_t coproc2_reg0;
    uint32_t coproc2_reg1;
    uint32_t v2x_mode; /* Supported HVX vector length, see HexagonVecLenSupported */
    uint32_t coproc2_reg2;
    uint32_t coproc2_reg3;
    uint32_t coproc2_reg4;
    uint32_t coproc2_reg5;
    uint32_t coproc2_reg6;
    uint32_t coproc2_reg7;
    uint32_t acd_preset; /* Voltage droop mitigation technique parameter */
    uint32_t mnd_preset; /* Voltage droop mitigation technique parameter */
    uint32_t l1d_size_kb; /* L1 data cache size (in KB) */
    uint32_t l1i_size_kb; /* L1 instruction cache size in (KB) */
    uint32_t l1d_write_policy; /* L1 data cache write policy: see HexagonL1WritePolicy */
    uint32_t vtcm_bank_width; /* VTCM bank width  */
    uint32_t reserved3;
    uint32_t reserved4;
    uint32_t reserved5;
    uint32_t coproc2_cvt_mpy_size;
    uint32_t consistency_domain;
    uint32_t capacity_domain;
    uint32_t axi3_lowaddr;
    uint32_t coproc2_int8_subcolumns;
    uint32_t corecfg_present;
    uint32_t coproc2_fp16_acc_exp;
    uint32_t AXIM2_secondary_base;
  };
  uint32_t raw[HEXAGON_CFGSPACE_ENTRIES];
} hexagon_config_table;

typedef struct {
    uint32_t cfgbase; /* Base address of config table */
    uint32_t cfgtable_size; /* Size of config table */
    uint32_t l2tcm_size; /* Size of L2 TCM */
    uint32_t l2vic_base; /* Base address of L2VIC */
    uint32_t l2vic_size; /* Size of L2VIC region */
    uint32_t csr_base; /* QTimer csr base */
    uint32_t qtmr_rg0;
    uint32_t qtmr_rg1;
} hexagon_config_extensions;

#endif
