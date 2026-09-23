/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "decode.h"
#include "hmx_config.h"
#include "hmx_state.h"

void hmx_init_config(HexagonCPU *cpu)
{
    HexagonVersion ver = cpu->cfg.hex_def->hex_version;
    HmxConfig *hmx_cfg = &cpu->hmx_cfg;

    /* Default dimensions, shared by v75, v79, and v81 and later. */
    hmx_cfg->mx_rows                = HMX_SPATIAL_DIM_FXP;
    hmx_cfg->mx_cols                = HMX_OUTPUT_CHANNELS;
    hmx_cfg->mx_fp_cols             = HMX_OUTPUT_CHANNELS;
    hmx_cfg->mx_parallel_grps       = 2;
    hmx_cfg->mx_fp_rate             = 8;
    /*
     * FP accumulator: a signed int8.frac22 significand plus an exponent.
     * With v75/v79's 7-bit exponent that is the 37-bit accumulator the
     * HMX PRM (80-N2040-62) describes; v81 widens the exponent (below).
     */
    hmx_cfg->mx_fp_acc_exp          = 7;
    hmx_cfg->mx_fp_acc_frac         = 22;
    hmx_cfg->mx_fp_acc_int          = 8;
    hmx_cfg->mx_fp_acc_norm         = 3;
    hmx_cfg->xfp_cvt_int            = 3;
    hmx_cfg->xfp_cvt_frac           = 13;
    hmx_cfg->xfp_cvt_exp            = 8;
    hmx_cfg->xfp_inexact_enable     = 1;
    hmx_cfg->mx_fp8_en              = false;
    /*
     * v75/v79 have no XFP MAC/convert path at all (see hmx_xfp.c's
     * header comment) -- they use the SoftFloat FP path.
     */
    hmx_cfg->hmx_fp_uses_xfp        = false;

    if (ver >= HEX_VER_V81) {
        /*
         * 9-bit exponent (a 39-bit accumulator), needed to cover BF16's
         * 8-bit exponent range. The v81 HMX PRM documents FP16 only and
         * still gives the 37-bit v75/v79 accumulator width.
         */
        hmx_cfg->mx_fp_acc_exp      = 9;
        hmx_cfg->mx_fp8_en          = true;
        /*
         * v81 uses the bit-exact XFP MAC/convert path
         * (hmx_matmul_fp_xfp()/hmx_fp_convert_xfp()) instead of the
         * SoftFloat path every other version uses.
         */
        hmx_cfg->hmx_fp_uses_xfp    = true;
    }

    g_assert(hmx_cfg->mx_rows != 0 && hmx_cfg->mx_rows <= HMX_SPATIAL_DIM_FXP);
    g_assert(hmx_cfg->mx_cols != 0 && hmx_cfg->mx_cols <= HMX_OUTPUT_CHANNELS);
    g_assert(hmx_cfg->mx_fp_cols != 0 &&
             hmx_cfg->mx_fp_cols <= HMX_OUTPUT_CHANNELS);

    /*
     * The F8 helpers rely on mx_fp8_en, but it is tag_rev_info.c.inc that
     * makes the F8 instructions illegal where FP8 is absent, so the two
     * must agree. Only the XFP path implements F8.
     */
    g_assert(hmx_cfg->mx_fp8_en ==
             opcode_supported(M8_cvt_rs_f8, cpu->cfg.hex_def));
    g_assert(!hmx_cfg->mx_fp8_en || hmx_cfg->hmx_fp_uses_xfp);
}
