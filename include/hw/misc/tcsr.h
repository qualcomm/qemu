/*
 * TCSR (Top Control and Status Register) device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_TCSR_H
#define HW_MISC_TCSR_H

#include "hw/core/sysbus.h"

#define TYPE_TCSR "tcsr"
#define TCSR(obj) OBJECT_CHECK(TCSRState, (obj), TYPE_TCSR)

#endif /* HW_MISC_TCSR_H */
