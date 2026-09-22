/*
 * Hexagon HMX TCG Code Generation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Most HMX operations don't fit idef-parser's normal per-GPR codegen
 * model (they read/write hmx_state.h's HmxState via env->hmx_state,
 * bulk-load memory, or need helper functions that operate on more
 * than idef-parser's register/immediate operands). Those get an
 * fGEN_TCG_<tag>() override here, following the same mechanism HVX
 * uses in gen_tcg_hvx.h for its own non-idef-parser instructions.
 */

#ifndef HEXAGON_GEN_TCG_HMX_H
#define HEXAGON_GEN_TCG_HMX_H

#endif /* HEXAGON_GEN_TCG_HMX_H */
