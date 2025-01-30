/*
 *  Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MMVEC_QFLOAT_COMPLIANCE_H
#define MMVEC_QFLOAT_COMPLIANCE_H 1

#include <stdbool.h>
#include "mmvec_qfloat.h"

void check_cmpgt_compliance(f_type type, uint32_t ain, uint32_t bin, int result);

void check_cmpeq_compliance(f_type type, uint32_t ain, uint32_t bin, int result);

void check_minmax_compliance(f_type type, bool max, uint32_t ain, uint32_t bin, uint32_t result);

void check_vconv_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext, bool daz_mode);

void check_vneg_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext, bool daz_mode);

void check_vabs_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext, bool daz_mode);

void check_vilog2_compliance(f_type atype, uint32_t ain, uint8_t a_ext, int rin);

void check_narrowing_vconv_compliance(f_type atype, f_type rtype, uint32_t ain, uint8_t a_ext, uint32_t rin, uint8_t r_ext);


#endif
