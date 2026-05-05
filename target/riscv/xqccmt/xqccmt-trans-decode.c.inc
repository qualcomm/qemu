/*
 * RISC-V translation routines for the Xqccmt Extension.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define REQUIRE_XQCCMT(ctx) do {        \
    if (!ctx->cfg_ptr->ext_xqccmt) {    \
        return false;                   \
    }                                   \
} while (0)

/*
 * trans_qc_cm_jalt - translates both qc.cm.jt and qc.cm.jalt.
 *
 * qc.cm.jt  (index  0-31):  jump via JVT, no link register update.
 * qc.cm.jalt (index 32-255): jump via JVT; bit 0 of the raw table entry
 *   selects the link register at runtime:
 *     entry[0] == 0  ->  return address written to ra (x1)
 *     entry[0] == 1  ->  return address written to t0 (x5)
 *
 * The jump target always has bit 0 cleared: target = entry & ~1.
 */
static bool trans_qc_cm_jalt(DisasContext *ctx, arg_qc_cm_jalt *a)
{
    REQUIRE_XQCCMT(ctx);

    TCGv entry = tcg_temp_new();

    /*
     * Update pc to current so the non-unwinding exception that may come
     * from cpu_ld*_code() inside the helper has the right pc.
     */
    gen_update_pc(ctx, 0);
    gen_helper_cm_jalt(entry, tcg_env, tcg_constant_i32(a->index));

    /* qc.cm.jt (index < 32): no link register update. */
    if (a->index >= 32) {
        /* qc.cm.jalt: conditionally link to ra (x1) or t0 (x5). */
        TCGv return_addr = tcg_temp_new();
        gen_pc_plus_diff(return_addr, ctx, ctx->cur_insn_len);

        TCGv bit0 = tcg_temp_new();
        tcg_gen_andi_tl(bit0, entry, 1);

        TCGv zero = tcg_constant_tl(0);
        TCGv cur_ra = get_gpr(ctx, xRA, EXT_NONE);
        TCGv cur_t0 = get_gpr(ctx, xT0, EXT_NONE);
        TCGv new_ra = dest_gpr(ctx, xRA);
        TCGv new_t0 = dest_gpr(ctx, xT0);

        /* bit0 == 0: new_ra = return_addr, new_t0 = cur_t0 (unchanged) */
        tcg_gen_movcond_tl(TCG_COND_EQ, new_ra, bit0, zero,
                           return_addr, cur_ra);
        /* bit0 != 0: new_t0 = return_addr, new_ra = cur_ra (unchanged) */
        tcg_gen_movcond_tl(TCG_COND_NE, new_t0, bit0, zero,
                           return_addr, cur_t0);

        gen_set_gpr(ctx, xRA, new_ra);
        gen_set_gpr(ctx, xT0, new_t0);
    }

    /* Jump target: clear bit 0. */
    TCGv addr = tcg_temp_new();
    tcg_gen_andi_tl(addr, entry, ~(target_ulong)1);

#ifndef CONFIG_USER_ONLY
    if (ctx->cfg_ptr->ext_smctr || ctx->cfg_ptr->ext_ssctr) {
        TCGv type = tcg_constant_tl(a->index >= 32 ? CTRDATA_TYPE_DIRECT_CALL
                                                   : CTRDATA_TYPE_DIRECT_JUMP);
        gen_helper_ctr_add_entry(tcg_env, cpu_pc, addr, type);
    }
#endif

    tcg_gen_mov_tl(cpu_pc, addr);

    tcg_gen_lookup_and_goto_ptr();
    ctx->base.is_jmp = DISAS_NORETURN;
    return true;
}
