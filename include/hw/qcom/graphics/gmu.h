#pragma once

#include "hw/sysbus.h"
#include "qom/object.h"

#include "kgsl.h"
#include "rscc.h"
#include "cx_dbgc.h"
#include "cx_misc.h"

#define QCOM_GMU_MEMORY_BASE 0x3D00000
#define QCOM_GMU_MEMORY_SIZE 0x100000

uint64_t gmu_read(void *gpu, hwaddr addr, unsigned size);

void gmu_write(void *gpu, hwaddr addr, uint64_t val, unsigned size);
