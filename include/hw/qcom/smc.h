/*
 * SCM handler
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 */

#ifndef QEMU_QCOM_SMC_H
#define QEMU_QCOM_SMC_H

#include "qemu/osdep.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/multiprocessing.h"
#include "target/arm/gtimer.h"

void qcom_smc_handler(ARMCPU* cpu);

#endif
