/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "hmx_config.h"
#include "hmx_state.h"

void hmx_init_config(HexagonCPU *cpu)
{
    HexagonVersion ver = HEXAGON_CPU_GET_CLASS(cpu)->hex_def->hex_version;
    HmxConfig *hmx_cfg = &cpu->hmx_cfg;

    /* Default dimensions, shared by v75, v79, and v81. */
    hmx_cfg->mx_rows                = HMX_SPATIAL_DIM_FXP;
    hmx_cfg->mx_cols                = HMX_OUTPUT_CHANNELS;
    hmx_cfg->mx_input_channels      = HMX_INPUT_CHANNELS;
    hmx_cfg->mx_fp_rows             = HMX_SPATIAL_DIM_FP;
    hmx_cfg->mx_fp_cols             = HMX_OUTPUT_CHANNELS;
    hmx_cfg->mx_redundant_sub_cols  = 2;
    hmx_cfg->mx_num_bias_grps       = HMX_NUM_BIAS_SETS;
    hmx_cfg->mx_parallel_grps       = 2;
    hmx_cfg->mx_cvt_width           = 12;
    hmx_cfg->mx_rate                = 16;
    hmx_cfg->mx_fp_rate             = 8;
    hmx_cfg->mx_fp_acc_exp          = 7;
    hmx_cfg->mx_fp_acc_frac         = 22;
    hmx_cfg->mx_fp_acc_int          = 8;
    hmx_cfg->mx_fp_acc_norm         = 3;
    hmx_cfg->xfp_cvt_int            = 3;
    hmx_cfg->xfp_cvt_frac           = 13;
    hmx_cfg->xfp_cvt_exp            = 8;
    hmx_cfg->xfp_inexact_enable     = 1;
    hmx_cfg->mx_fp_present          = true;
    hmx_cfg->mx_fp8_en              = false;
    hmx_cfg->mx_bthenc              = false;
    hmx_cfg->hmx_fp_uses_xfp        = false;
    hmx_cfg->hmx_present            = (ver >= HEX_VER_V75);

    if (ver == HEX_VER_V81) {
        hmx_cfg->mx_fp_acc_exp      = 9;
        hmx_cfg->mx_fp8_en          = true;
        hmx_cfg->mx_bthenc          = true;
        hmx_cfg->hmx_fp_uses_xfp    = true;
    }

    g_assert(hmx_cfg->mx_rows <= HMX_SPATIAL_DIM_FXP);
    g_assert(hmx_cfg->mx_cols <= HMX_OUTPUT_CHANNELS);
    g_assert(hmx_cfg->mx_fp_rows <= HMX_SPATIAL_DIM_FP);
    g_assert(hmx_cfg->mx_fp_cols <= HMX_OUTPUT_CHANNELS);
    g_assert(hmx_cfg->mx_input_channels <= HMX_INPUT_CHANNELS);
    g_assert(hmx_cfg->mx_num_bias_grps <= HMX_NUM_BIAS_SETS);

    if (hmx_cfg->hmx_present) {
        g_assert(hmx_cfg->mx_rows != 0);
        g_assert(hmx_cfg->mx_cols != 0);
        g_assert(hmx_cfg->mx_input_channels != 0);
        g_assert(hmx_cfg->mx_redundant_sub_cols >= 1);
        if (hmx_cfg->mx_fp_present) {
            g_assert(hmx_cfg->mx_fp_rows != 0);
            g_assert(hmx_cfg->mx_fp_cols != 0);
        }
    }
}
