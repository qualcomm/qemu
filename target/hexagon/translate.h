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

#ifndef HEXAGON_TRANSLATE_H
#define HEXAGON_TRANSLATE_H

#include "qemu/bitmap.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/translator.h"
#include "tcg/tcg-op.h"
#include "internal.h"

typedef struct DisasContext {
    DisasContextBase base;
    Packet *pkt;
    uint32_t next_PC;
    uint32_t mem_idx;
    uint32_t num_packets;
    uint32_t num_insns;
    uint32_t num_hvx_insns;
    uint32_t num_hmx_insns;
    int reg_log[REG_WRITES_MAX];
    int reg_log_idx;
    DECLARE_BITMAP(regs_written, TOTAL_PER_THREAD_REGS);
#ifndef CONFIG_USER_ONLY
    int greg_log[GREG_WRITES_MAX];
    int greg_log_idx;
    int sreg_log[SREG_WRITES_MAX];
    int sreg_log_idx;
    bool need_cpu_limit;
    bool resched_required;
    bool intcheck_required;
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
    bool vreg_is_predicated[NUM_VREGS];
    int vreg_log_idx;
    DECLARE_BITMAP(vregs_updated_tmp, NUM_VREGS);
    DECLARE_BITMAP(vregs_updated, NUM_VREGS);
    DECLARE_BITMAP(vregs_select, NUM_VREGS);
    int qreg_log[NUM_QREGS];
    bool qreg_is_predicated[NUM_QREGS];
    int qreg_log_idx;
    bool pre_commit;
    bool has_single_direct_branch;
    TCGv branch_cond;
    target_ulong branch_dest;
    bool is_tight_loop;
    bool hvx_check_emitted;
    bool hmx_check_emitted;
    bool pcycle_enabled;
    TCGv zero;
    TCGv_i64 zero64;
    TCGv ones;
    TCGv_i64 ones64;
    bool gen_cacheop_exceptions;
    bool should_advance_pc;
    bool ss_active;
    bool ss_pending;
} DisasContext;

static inline void ctx_log_reg_write(DisasContext *ctx, int rnum)
{
    if (test_bit(rnum, ctx->regs_written)) {
        HEX_DEBUG_LOG("WARNING: Multiple writes to r%d\n", rnum);
        return;
    }
    ctx->reg_log[ctx->reg_log_idx] = rnum;
    ctx->reg_log_idx++;
    set_bit(rnum, ctx->regs_written);
}

static inline void ctx_log_reg_write_pair(DisasContext *ctx, int rnum)
{
    ctx_log_reg_write(ctx, rnum);
    ctx_log_reg_write(ctx, rnum + 1);
}

#ifndef CONFIG_USER_ONLY
static inline void ctx_log_greg_write(DisasContext *ctx, int rnum)
{
    ctx->greg_log[ctx->greg_log_idx] = rnum;
    ctx->greg_log_idx++;
}

static inline void ctx_log_sreg_write(DisasContext *ctx, int rnum)
{
    if (rnum < HEX_SREG_GLB_START) {
        ctx->sreg_log[ctx->sreg_log_idx] = rnum;
        ctx->sreg_log_idx++;
    }
    if ((rnum == HEX_SREG_STID)     ||
        (rnum == HEX_SREG_SSR)      ||
        (rnum == HEX_SREG_IMASK)    ||
        (rnum == HEX_SREG_BESTWAIT) ||
        (rnum == HEX_SREG_SCHEDCFG) ||
        (rnum == HEX_SREG_IPENDAD)) {
        ctx->resched_required = true;
        ctx->intcheck_required = true;
        ctx->base.is_jmp = DISAS_NORETURN;
    }
}
#endif

static inline void ctx_log_pred_write(DisasContext *ctx, int pnum)
{
    ctx->preg_log[ctx->preg_log_idx] = pnum;
    ctx->preg_log_idx++;
    set_bit(pnum, ctx->pregs_written);
}

static inline bool is_preloaded(DisasContext *ctx, int num)
{
    return test_bit(num, ctx->regs_written);
}

intptr_t ctx_future_vreg_off(DisasContext *ctx, int regnum,
                             int num, bool alloc_ok);
intptr_t ctx_tmp_vreg_off(DisasContext *ctx, int regnum,
                          int num, bool alloc_ok);

static inline void ctx_log_vreg_write(DisasContext *ctx,
                                      int rnum, VRegWriteType type,
                                      bool is_predicated)
{
    if (type != EXT_TMP) {
        ctx->vreg_log[ctx->vreg_log_idx] = rnum;
        ctx->vreg_is_predicated[ctx->vreg_log_idx] = is_predicated;
        ctx->vreg_log_idx++;

        set_bit(rnum, ctx->vregs_updated);
    }
    if (type == EXT_NEW) {
        set_bit(rnum, ctx->vregs_select);
    }
    if (type == EXT_TMP) {
        set_bit(rnum, ctx->vregs_updated_tmp);
    }
}

static inline void ctx_log_vreg_write_pair(DisasContext *ctx,
                                           int rnum, VRegWriteType type,
                                           bool is_predicated)
{
    ctx_log_vreg_write(ctx, rnum ^ 0, type, is_predicated);
    ctx_log_vreg_write(ctx, rnum ^ 1, type, is_predicated);
}

static inline void ctx_log_qreg_write(DisasContext *ctx,
                                      int rnum, bool is_predicated)
{
    ctx->qreg_log[ctx->qreg_log_idx] = rnum;
    ctx->qreg_is_predicated[ctx->qreg_log_idx] = is_predicated;
    ctx->qreg_log_idx++;
}

extern TCGv hex_gpr[TOTAL_PER_THREAD_REGS];
extern TCGv hex_pred[NUM_PREGS];
extern TCGv hex_next_PC;
extern TCGv hex_this_PC;
extern TCGv hex_slot_cancelled;
extern TCGv hex_branch_taken;
extern TCGv hex_new_value[TOTAL_PER_THREAD_REGS];
extern TCGv hex_reg_written[TOTAL_PER_THREAD_REGS];
extern TCGv hex_new_pred_value[NUM_PREGS];
extern TCGv hex_pred_written;
extern TCGv hex_store_addr[STORES_MAX];
extern TCGv hex_store_width[STORES_MAX];
extern TCGv hex_store_val32[STORES_MAX];
extern TCGv_i64 hex_store_val64[STORES_MAX];
extern TCGv hex_dczero_addr;
extern TCGv hex_llsc_addr;
extern TCGv hex_llsc_val;
extern TCGv_i64 hex_llsc_val_i64;
extern TCGv hex_VRegs_updated;
extern TCGv hex_QRegs_updated;
#ifndef CONFIG_USER_ONLY
extern TCGv hex_greg[NUM_GREGS];
extern TCGv hex_greg_new_value[NUM_GREGS];
extern TCGv hex_greg_written[NUM_GREGS];
extern TCGv hex_t_sreg[NUM_SREGS];
extern TCGv hex_t_sreg_new_value[NUM_SREGS];
extern TCGv hex_t_sreg_written[NUM_SREGS];
extern TCGv_i64 hex_packet_count;
#endif
extern TCGv hex_vstore_addr[VSTORES_MAX];
extern TCGv hex_vstore_size[VSTORES_MAX];
extern TCGv hex_vstore_pending[VSTORES_MAX];
#ifndef CONFIG_USER_ONLY
extern TCGv hex_slot;
extern TCGv hex_imprecise_exception;
#endif

void gen_debug_exception(int excp, target_ulong PC);
void gen_exception(int excp);
void gen_exception_end_tb(DisasContext *ctx, int excp);
bool is_gather_store_insn(Insn *insn, Packet *pkt);
void process_store(DisasContext *ctx, Packet *pkt, int slot_num);
#endif
