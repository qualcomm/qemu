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

#ifndef HEXAGON_TRANSLATE_H
#define HEXAGON_TRANSLATE_H

#include "qemu/bitmap.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/translator.h"
#include "tcg/tcg-op.h"
#include "insn.h"
#include "internal.h"

typedef struct HmxPacketCtx {
    bool act_valid;      /* true after activation insn is translated */
    int format_offset;   /* 0 (DM) or 2 (SM) */
    int act_mod;         /* HMX_ACT_BLK/NOBLK/BLK_U/BLK_S/BLK_D */
    int act_type;        /* HMX_ACT_UB/HF/F8 */
} HmxPacketCtx;

typedef struct DisasContext {
    DisasContextBase base;
    Packet pkt;
    Insn *insn;
    const HexagonCPUDef *hex_def;
    uint32_t next_PC;
    uint32_t mem_idx;
    uint32_t hvx_vec_len;
    uint32_t num_packets;
    uint32_t num_insns;
    uint32_t num_hvx_insns;
    int reg_log[REG_WRITES_MAX];
    int reg_log_idx;
    DECLARE_BITMAP(regs_written, TOTAL_PER_THREAD_REGS);
    DECLARE_BITMAP(predicated_regs, TOTAL_PER_THREAD_REGS);
    bool pkt_has_uncond_mult_reg_write;
    DECLARE_BITMAP(wreg_mult_gprs, NUM_GPREGS);
    DECLARE_BITMAP(uncond_wreg_gprs, NUM_GPREGS);
    bool implicit_usr_write;
#ifndef CONFIG_USER_ONLY
    int greg_log[GREG_WRITES_MAX];
    int greg_log_idx;
    int sreg_log[SREG_WRITES_MAX];
    int sreg_log_idx;
    TCGv t_sreg_new_value[NUM_SREGS];
    TCGv greg_new_value[NUM_GREGS];
#endif
    int preg_log[PRED_WRITES_MAX];
    int preg_log_idx;
    DECLARE_BITMAP(pregs_written, NUM_PREGS);
    uint8_t store_width[STORES_MAX];
    bool s1_store_processed;
    int future_vregs_idx;
    int future_vregs_num[VECTOR_TEMPS_MAX];
    int tmp_vregs_idx;
    int tmp_vregs_num[VECTOR_TEMPS_MAX];
    int vreg_log[NUM_VREGS];
    int vreg_log_idx;
    DECLARE_BITMAP(vregs_written, NUM_VREGS);
    DECLARE_BITMAP(insn_vregs_written, NUM_VREGS);
    DECLARE_BITMAP(vregs_updated_tmp, NUM_VREGS);
    DECLARE_BITMAP(vregs_updated, NUM_VREGS);
    DECLARE_BITMAP(vregs_select, NUM_VREGS);
    DECLARE_BITMAP(predicated_future_vregs, NUM_VREGS);
    DECLARE_BITMAP(predicated_tmp_vregs, NUM_VREGS);
    DECLARE_BITMAP(insn_vregs_read, NUM_VREGS);
    int qreg_log[NUM_QREGS];
    int qreg_log_idx;
    DECLARE_BITMAP(qregs_written, NUM_QREGS);
    DECLARE_BITMAP(insn_qregs_written, NUM_QREGS);
    DECLARE_BITMAP(insn_qregs_read, NUM_QREGS);
    bool pre_commit;
    bool need_commit;
    TCGCond branch_cond;
    target_ulong branch_dest;
    bool is_tight_loop;
    bool short_circuit;
    HmxPacketCtx hmx_pkt;
    bool read_after_write;
    bool has_hvx_overlap;
    TCGv new_value[TOTAL_PER_THREAD_REGS];
    TCGv new_pred_value[NUM_PREGS];
    TCGv branch_taken;
    TCGv dczero_addr;
    TCGv gpreg_written;
    TCGv mult_reg_written;
    bool pcycle_enabled;
    bool hvx_coproc_enabled;
    bool hmx_coproc_enabled;
    bool hvx_64b_mode;
    bool ss_active;
    bool ss_pending;
    bool pkt_ends_tb;
    bool need_next_pc;
    uint32_t num_cycles;
#ifndef CONFIG_USER_ONLY
    bool need_cpu_limit;
    bool gen_cacheop_exceptions;
    bool pmu_enabled;
    uint32_t pmu_num_packets;
    uint32_t pmu_hvx_packets;
#endif
} DisasContext;

bool is_gather_store_insn(DisasContext *ctx);

#ifndef CONFIG_USER_ONLY
static inline void ctx_log_greg_write(DisasContext *ctx, int rnum)
{
    if (rnum <= HEX_GREG_G3) {
        ctx->greg_log[ctx->greg_log_idx] = rnum;
        ctx->greg_log_idx++;
    }
}

static inline void ctx_log_greg_write_pair(DisasContext *ctx, int rnum)
{
    ctx_log_greg_write(ctx, rnum);
    ctx_log_greg_write(ctx, rnum + 1);
}

static inline void ctx_log_sreg_write(DisasContext *ctx, int rnum)
{
    ctx->sreg_log[ctx->sreg_log_idx] = rnum;
    ctx->sreg_log_idx++;
}

static inline void ctx_log_sreg_write_pair(DisasContext *ctx, int rnum)
{
    ctx_log_sreg_write(ctx, rnum);
    ctx_log_sreg_write(ctx, rnum + 1);
}
#endif

static inline void ctx_log_pred_write(DisasContext *ctx, int pnum)
{
    if (!test_bit(pnum, ctx->pregs_written)) {
        ctx->preg_log[ctx->preg_log_idx] = pnum;
        ctx->preg_log_idx++;
        set_bit(pnum, ctx->pregs_written);
    }
}

static inline void ctx_log_pred_read(DisasContext *ctx, int pnum)
{
    if (test_bit(pnum, ctx->pregs_written)) {
        ctx->read_after_write = true;
    }
}

static inline void ctx_log_pred_read_new(DisasContext *ctx, int pnum)
{
    g_assert(test_bit(pnum, ctx->pregs_written));
}

static inline void ctx_log_reg_write(DisasContext *ctx, int rnum,
                                     bool is_predicated)
{
    if (rnum == HEX_REG_P3_0_ALIASED) {
        for (int i = 0; i < NUM_PREGS; i++) {
            ctx_log_pred_write(ctx, i);
        }
    } else {
        if (!test_bit(rnum, ctx->regs_written)) {
            ctx->reg_log[ctx->reg_log_idx] = rnum;
            ctx->reg_log_idx++;
            set_bit(rnum, ctx->regs_written);
        } else if (rnum < NUM_GPREGS) {
            set_bit(rnum, ctx->wreg_mult_gprs);
        }
        if (is_predicated) {
            set_bit(rnum, ctx->predicated_regs);
        } else if (rnum < NUM_GPREGS) {
            bool uncond_set =
                test_and_set_bit(rnum, ctx->uncond_wreg_gprs);
            ctx->pkt_has_uncond_mult_reg_write |= uncond_set;
        }
    }
}

static inline void ctx_log_reg_write_pair(DisasContext *ctx, int rnum,
                                          bool is_predicated)
{
    ctx_log_reg_write(ctx, rnum, is_predicated);
    ctx_log_reg_write(ctx, rnum + 1, is_predicated);
}

static inline void ctx_log_reg_read(DisasContext *ctx, int rnum)
{
    if (test_bit(rnum, ctx->regs_written)) {
        ctx->read_after_write = true;
    }
}

static inline void ctx_log_reg_read_new(DisasContext *ctx, int rnum)
{
    g_assert(test_bit(rnum, ctx->regs_written));
}

static inline void ctx_log_reg_read_pair(DisasContext *ctx, int rnum)
{
    ctx_log_reg_read(ctx, rnum);
    ctx_log_reg_read(ctx, rnum + 1);
}

intptr_t ctx_future_vreg_off(DisasContext *ctx, int regnum,
                             int num, bool alloc_ok);
intptr_t ctx_tmp_vreg_off(DisasContext *ctx, int regnum,
                          int num, bool alloc_ok);

static inline void ctx_start_hvx_insn(DisasContext *ctx)
{
    bitmap_zero(ctx->insn_vregs_written, NUM_VREGS);
    bitmap_zero(ctx->insn_vregs_read, NUM_VREGS);
    bitmap_zero(ctx->insn_qregs_written, NUM_QREGS);
    bitmap_zero(ctx->insn_qregs_read, NUM_QREGS);
}

static inline void ctx_log_vreg_write(DisasContext *ctx,
                                      int rnum, VRegWriteType type,
                                      bool is_predicated, bool has_helper)
{
    if (has_helper) {
        set_bit(rnum, ctx->insn_vregs_written);
        if (test_bit(rnum, ctx->insn_vregs_read)) {
            ctx->has_hvx_overlap = true;
        }
    }
    set_bit(rnum, ctx->vregs_written);
    if (type != EXT_TMP) {
        if (!test_bit(rnum, ctx->vregs_updated)) {
            ctx->vreg_log[ctx->vreg_log_idx] = rnum;
            ctx->vreg_log_idx++;
            set_bit(rnum, ctx->vregs_updated);
        }

        set_bit(rnum, ctx->vregs_updated);
        if (is_predicated) {
            set_bit(rnum, ctx->predicated_future_vregs);
        }
    }
    if (type == EXT_NEW) {
        set_bit(rnum, ctx->vregs_select);
    }
    if (type == EXT_TMP) {
        set_bit(rnum, ctx->vregs_updated_tmp);
        if (is_predicated) {
            set_bit(rnum, ctx->predicated_tmp_vregs);
        }
    }
}

static inline void ctx_log_vreg_write_pair(DisasContext *ctx,
                                           int rnum, VRegWriteType type,
                                           bool is_predicated, bool has_helper)
{
    ctx_log_vreg_write(ctx, rnum ^ 0, type, is_predicated, has_helper);
    ctx_log_vreg_write(ctx, rnum ^ 1, type, is_predicated, has_helper);
}

static inline void ctx_log_vreg_read(DisasContext *ctx, int rnum,
                                     bool has_helper)
{
    if (has_helper) {
        set_bit(rnum, ctx->insn_vregs_read);
        if (test_bit(rnum, ctx->insn_vregs_written)) {
            ctx->has_hvx_overlap = true;
        }
    }
    if (test_bit(rnum, ctx->vregs_written)) {
        ctx->read_after_write = true;
    }
}

static inline void ctx_log_vreg_read_new(DisasContext *ctx, int rnum,
                                         bool has_helper)
{
    g_assert(is_gather_store_insn(ctx) ||
             test_bit(rnum, ctx->vregs_updated) ||
             test_bit(rnum, ctx->vregs_select) ||
             test_bit(rnum, ctx->vregs_updated_tmp));
    if (has_helper) {
        set_bit(rnum, ctx->insn_vregs_read);
        if (test_bit(rnum, ctx->insn_vregs_written)) {
            ctx->has_hvx_overlap = true;
        }
    }
    if (is_gather_store_insn(ctx)) {
        ctx->read_after_write = true;
    }
}

static inline void ctx_log_vreg_read_pair(DisasContext *ctx, int rnum,
                                          bool has_helper)
{
    ctx_log_vreg_read(ctx, rnum ^ 0, has_helper);
    ctx_log_vreg_read(ctx, rnum ^ 1, has_helper);
}

static inline void ctx_log_qreg_write(DisasContext *ctx,
                                      int rnum, bool has_helper)
{
    if (has_helper) {
        set_bit(rnum, ctx->insn_qregs_written);
        if (test_bit(rnum, ctx->insn_qregs_read)) {
            ctx->has_hvx_overlap = true;
        }
    }
    set_bit(rnum, ctx->qregs_written);
    ctx->qreg_log[ctx->qreg_log_idx] = rnum;
    ctx->qreg_log_idx++;
}

static inline void ctx_log_qreg_read(DisasContext *ctx,
                                     int qnum, bool has_helper)
{
    if (has_helper) {
        set_bit(qnum, ctx->insn_qregs_read);
        if (test_bit(qnum, ctx->insn_qregs_written)) {
            ctx->has_hvx_overlap = true;
        }
    }
    if (test_bit(qnum, ctx->qregs_written)) {
        ctx->read_after_write = true;
    }
}

extern TCGv hex_gpr[TOTAL_PER_THREAD_REGS];
extern TCGv hex_next_PC;
extern TCGv hex_pred[NUM_PREGS];
extern TCGv hex_slot_cancelled;
extern TCGv hex_new_value_usr;
extern TCGv hex_store_addr[STORES_MAX];
extern TCGv_i32 hex_store_width[STORES_MAX];
extern TCGv hex_store_val32[STORES_MAX];
extern TCGv_i64 hex_store_val64[STORES_MAX];
extern TCGv hex_llsc_addr;
extern TCGv hex_llsc_val;
extern TCGv_i64 hex_llsc_val_i64;
extern TCGv hex_vstore_addr[VSTORES_MAX];
extern TCGv hex_vstore_size[VSTORES_MAX];
extern TCGv hex_vstore_pending[VSTORES_MAX];
#ifndef CONFIG_USER_ONLY
extern TCGv hex_greg[NUM_GREGS];
extern TCGv hex_t_sreg[NUM_SREGS];
extern TCGv hex_slot;
extern TCGv hex_imprecise_exception;
extern TCGv hex_cause_code;
extern TCGv hex_ss_pending;
extern TCGv hex_exec_ctr_tb;
extern TCGv hex_last_cpu;
extern TCGv hex_thread_id;
extern TCGv hex_pmu_num_packets;
extern TCGv hex_pmu_hvx_packets;
#endif
extern TCGv hex_exec_ctr_pkt;
extern TCGv hex_exec_ctr_insn;
extern TCGv hex_exec_ctr_hvx;


void hex_gen_exception_end_tb(DisasContext *ctx, int excp);

void process_store(DisasContext *ctx, int slot_num);

FIELD(PROBE_PKT_SCALAR_STORE_S0, MMU_IDX,       0, 2)
FIELD(PROBE_PKT_SCALAR_STORE_S0, IS_PREDICATED, 2, 1)

FIELD(PROBE_PKT_SCALAR_HVX_STORES, HAS_ST0,        0, 1)
FIELD(PROBE_PKT_SCALAR_HVX_STORES, HAS_ST1,        1, 1)
FIELD(PROBE_PKT_SCALAR_HVX_STORES, HAS_HVX_STORES, 2, 1)
FIELD(PROBE_PKT_SCALAR_HVX_STORES, S0_IS_PRED,     3, 1)
FIELD(PROBE_PKT_SCALAR_HVX_STORES, S1_IS_PRED,     4, 1)
FIELD(PROBE_PKT_SCALAR_HVX_STORES, MMU_IDX,        5, 2)

#endif
