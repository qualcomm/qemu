/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

/*
 * User-DMA Engine.
 *
 * - This version implements "Hexagon V68 Architecture System-Level
 *   Specification - User DMA" which was released on July 28, 2018.
 *
 * - The specification is available from;
 *   https://sharepoint.qualcomm.com/qct/Modem-Tech/QDSP6/Shared%20Documents/
 *   QDSP6/QDSP6v68/Architecture/DMA/UserDMA.pdf
 */

#ifndef _DMA_H_
#define _DMA_H_

#include "dma_descriptor.h"
/* Function pointer type to let us know if an instruction finishes its job. */
typedef struct dma_state dma_t;	// forward declaration
typedef uint32_t (*dma_insn_checker_ptr)(struct dma_state *dma);
#include "uarch/uarch_dma.h"

#define DMA_STATUS_INT_IDLE       0x0000
#define DMA_STATUS_INT_RUNNING    0x0001
#define DMA_STATUS_INT_PAUSED     0x0002
#define DMA_STATUS_INT_ERROR      0x0004

#define DMA_STATUS_INT(DMA, STATE) (((DMA)->int_status) == STATE)
#define DMA_STATUS_INT_SET(DMA, STATE) (((DMA)->int_status = STATE))

#define DMA_MAX_TRANSFER_SIZE       256
#ifdef VERIFICATION
#define DMA_BUF_SIZE                2
#else
#define DMA_BUF_SIZE                128*256
#endif

#define DMA_MAX_DESC_SIZE           32

#define DMA_CFG0_SYNDROME_CODE___DMSTART_DMRESUME_IN_RUNSTATE   0
#define DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_ALIGNMENT   1
#define DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE        2
#define DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_ADDRESS            3
#define DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_BYPASS_MODE        4
#define DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_COMP_MODE          5
#define DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_ROI_ERROR           6
#define DMA_CFG0_SYNDROME_CODE___INVALID_ACCESS_RIGHTS          102
#define DMA_CFG0_SYNDROME_CODE___DATA_TIMEOUT                   103
#define DMA_CFG0_SYNDROME_CODE___DATA_ABORT                     104
#define DMA_CFG0_SYNDROME_CODE___UBWC_D                         12

#define INSN_TIMER_IDLE             0
#define INSN_TIMER_ACTIVE           1
#define INSN_TIMER_EXPIRED          2
#define INSN_TIMER_NOTUSED          3

#define INSN_TIMER_LATENCY_DMA_CMD_POLL     20
#define INSN_TIMER_LATENCY_DMA_CMD_WAIT     24
#define INSN_TIMER_LATENCY_DMA_CMD_START    20
#define INSN_TIMER_LATENCY_DMA_CMD_LINK     20
#define INSN_TIMER_LATENCY_DMA_CMD_PAUSE    20
#define INSN_TIMER_LATENCY_DMA_CMD_RESUME   20

#define DMA_MEM_TYPE_L2             0x00000000
#define DMA_MEM_TYPE_VTCM           0x00000001

#define DMA_MEM_PERMISSION_USER     0x00000001
#define DMA_MEM_PERMISSION_WRITE    0x00000002
#define DMA_MEM_PERMISSION_READ     0x00000004
#define DMA_MEM_PERMISSION_EXECUTE  0x00000008

#define DMA_XLATE_TYPE_LOAD         0x00000000
#define DMA_XLATE_TYPE_STORE        0x00000001

#define DMA_DESC_32BIT_VA_TYPE 0
#define DMA_DESC_38BIT_VA_TYPE 2
#define DMA_DESC_32BIT_VA_L2FETCH_TYPE 3
#define DMA_DESC_32BIT_VA_GATHER_TYPE 4
#define DMA_DESC_38BIT_VA_GATHER_TYPE 5
#define DMA_DESC_32BIT_VA_EXPANSION_TYPE 6
#define DMA_DESC_32BIT_VA_COMPRESSION_TYPE 7
#define DMA_DESC_32BIT_VA_CONSTANT_FILL_TYPE 8
#define DMA_DESC_32BIT_VA_WIDE_TYPE 9
#define DMA_DESC_38BIT_VA_WIDE_TYPE 10

#define DMA_CACHE_ALLOCATION_NONE 0
#define DMA_CACHE_ALLOCATION_WR_ONLY 1
#define DMA_CACHE_ALLOCATION_RD_ONLY 2
#define DMA_CACHE_ALLOCATION_RD_WR 3

#define DMA_MAX_REG 13

typedef struct dma_state {
    int num;                          // DMA instance identification index (num).
    int error_state_reason;
    uint32_t pc;                      /* current PC for debug */
    void *udma_ctx;                   // User DMA Context Structure
    void * owner;
	int verbosity;					// 0-9;
  int idle;
} dma_t;

typedef struct dm2_reg_t {
  union {
    struct {
      uint32_t no_stall_guest:1;
      uint32_t no_stall_monitor:1;
      uint32_t reserved_0:1;
      uint32_t no_cont_except:1;
      uint32_t no_cont_debug:1;
      uint32_t priority:2;
      uint32_t dlbc_enable:1;
      uint32_t ooo_disable:1;
      uint32_t error_exception_enable:1;
      uint32_t reserved_1:2;
      uint32_t prefetch_fifo_depth:2;
      uint32_t descriptor_ot_fifo_depth:2;
      uint32_t max_rd_tx:8;
      uint32_t max_wr_tx:8;
    };
    uint32_t val;
  };

} dm2_reg_t;

typedef struct dm3_reg_t {
  union {
    struct {
      uint32_t unaligned_desc_max_part_size:4;
      uint32_t read_request_micro_part_size:2;
      uint32_t reserved:2;
      uint32_t ubwc_mask:8;
      uint32_t ubwc_range:8;
      uint32_t ubwc_support_enable:1;
      uint32_t reserved2:7;
    };
    uint32_t val;
  };

} dm3_reg_t;


typedef struct udma_ctx_t {
    int32_t num;                        // DMA instance identification index (num).
    uint32_t int_status;                // Internal State Machine
    uint32_t ext_status;                // External (M0) State machine status.
    uint32_t dma_tick_count;

    uint32_t desc_new;                  // New descriptor to be processed
    // Timing mode support
    uint32_t timing_on;                 // for timing, 0 for non-timing
    int16_t insn_timer;                 // Instruction timer : 0xFFFF means there is no
                                        // timing model working. 0 means the timer expires.
    uint32_t insn_timer_active;
    uint32_t insn_timer_pmu;
    // Error information
    uint32_t error_state_reason_captured;
    uint32_t error_state_reason;
    uint32_t error_state_address;
    uint32_t exception_status;
    uint32_t exception_va;
    uint32_t desc_restart;
    uint32_t pause_va;
    uint32_t pause_va_valid;
    uint32_t uwbc_a_en;
    // DM2 Cfg
    dm2_reg_t dm2;
    dm3_reg_t dm3;

    dma_decoded_descriptor_t init_desc;
    dma_decoded_descriptor_t active;
    dma_decoded_descriptor_t target;  // For verif - precise pause


    uint32_t pc; // archsim debug/tracing

	// timing model
	uarch_dma_engine_inst_t * uarch_dma_engine;				// Timing model instance (Data Prefetch Unit)

} udma_ctx_t;




#ifndef ARCH_FUNCTION_DECL
#define ARCH_FUNCTION_DECL(rval, fname, ...) \
rval fname(__VA_ARGS__);\
rval fname##_##debug(__VA_ARGS__);
#endif


#define VERIF_DMASTEP_DESC_DONE 0
#define VERIF_DMASTEP_DESC_SET 1
#define VERIF_DMASTEP_DESC_IDLE 2
#define VERIF_DMASTEP_DESC_FINISHED 3

/* Information which will be returned from a DMA instruction implementation.
   The engine should return these values to make our simulation scheme.
   The adapter will assume these values as a kind of contract. */
typedef struct {
  uint32_t excpt;                    /* 1 for exception report, otherwise 0 */
  dma_insn_checker_ptr insn_checker; /* Instruction completeness checker */
} dma_cmd_report_t;

ARCH_FUNCTION_DECL(void,     dma_arch_tlbw,          dma_t *dma, uint32_t jtlb_idx);
ARCH_FUNCTION_DECL(void,     dma_arch_tlbinvasid,    dma_t *dma);

ARCH_FUNCTION_DECL(uint32_t, dma_tick,               dma_t *dma, uint32_t do_step);
ARCH_FUNCTION_DECL(uint32_t, dma_set_timing,         dma_t *dma, uint32_t timing_on);
ARCH_FUNCTION_DECL(void,     dma_cmd_start,          dma_t *dma, uint32_t new_desc,  dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_link,           dma_t *dma, uint32_t cur,   uint32_t new_desc, uint64_t pa, uint32_t invalid_dmlink_addr, dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_poll,           dma_t *dma, uint32_t *ret,  dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_wait,           dma_t *dma, uint32_t *ret,  dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_pause,          dma_t *dma, uint32_t *ret,  dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_resume,         dma_t *dma, uint32_t ptr,   dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_syncht,         dma_t *dma, uint32_t index, uint32_t  val, dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_tlbsynch,       dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_cfgrd,          dma_t *dma, uint32_t index, uint32_t *val, dma_cmd_report_t *report);
ARCH_FUNCTION_DECL(void,     dma_cmd_cfgwr,          dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report);

ARCH_FUNCTION_DECL(uint32_t, dma_init,               dma_t *dma, uint32_t timing_on);
ARCH_FUNCTION_DECL(uint32_t, dma_free,               dma_t *dma);
ARCH_FUNCTION_DECL(void,     dma_stop,               dma_t *dma);
ARCH_FUNCTION_DECL(uint32_t, dma_in_error,           dma_t *dma);

ARCH_FUNCTION_DECL(void,     mem_trans_commit,       dma_t *dma);
ARCH_FUNCTION_DECL(void,     dma_tlb_entry_invalidated, dma_t *dma, uint32_t asid);

ARCH_FUNCTION_DECL(uint32_t, dma_get_tick_count,         dma_t *dma);
ARCH_FUNCTION_DECL(void,     dma_force_error,            dma_t *dma, uint32_t addr, uint32_t reason);
ARCH_FUNCTION_DECL(void,     dma_target_desc,            dma_t *dma, uint32_t addr, uint32_t width, uint32_t height, uint32_t no_transfer);
ARCH_FUNCTION_DECL(void,     dma_update_descriptor_done, dma_t *dma, dma_decoded_descriptor_t * entry);
ARCH_FUNCTION_DECL(uint32_t, dma_retry_after_exception,  dma_t *dma);

#endif
