/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_HMX_CONFIG_H
#define HEXAGON_HMX_CONFIG_H

#include "cpu-qom.h"

/*
 * mx_fp_rate: FP MAC reduction-group size.
 * hmx_fp_uses_xfp: selects the bit-exact XFP accumulator model (v81) over
 *     the SoftFloat one (see hmx_state.h's HmxAccFp).
 */
typedef struct HmxConfig {
    uint32_t mx_rows;
    uint32_t mx_cols;
    uint32_t mx_fp_cols;
    uint32_t mx_parallel_grps;
    uint32_t mx_fp_rate;
    uint32_t mx_fp_acc_exp;
    uint32_t mx_fp_acc_frac;
    uint32_t mx_fp_acc_int;
    uint32_t mx_fp_acc_norm;
    uint32_t xfp_cvt_int;
    uint32_t xfp_cvt_frac;
    uint32_t xfp_cvt_exp;
    uint32_t xfp_inexact_enable;
    bool mx_fp8_en;
    bool hmx_fp_uses_xfp;
} HmxConfig;

void hmx_init_config(HexagonCPU *cpu);

#endif /* HEXAGON_HMX_CONFIG_H */
