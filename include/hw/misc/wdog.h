/*
 * Watchdog (WDOG) device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_WDOG_H
#define HW_MISC_WDOG_H

#include "hw/core/sysbus.h"

#define TYPE_WDOG "wdog"
#define WDOG(obj) OBJECT_CHECK(WdogState, (obj), TYPE_WDOG)

#endif /* HW_MISC_WDOG_H */
