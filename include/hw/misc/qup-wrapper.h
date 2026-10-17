/*
 * QUP wrapper (GENI SE QUP) device
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_QUP_WRAPPER_H
#define HW_MISC_QUP_WRAPPER_H

#include "hw/core/sysbus.h"

#define TYPE_QUP_WRAPPER "qup-wrapper"
#define QUP_WRAPPER(obj) OBJECT_CHECK(QupWrapperState, (obj), TYPE_QUP_WRAPPER)

#endif /* HW_MISC_QUP_WRAPPER_H */
