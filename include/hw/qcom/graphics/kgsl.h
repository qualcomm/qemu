#pragma once

#include "exec/memory.h"

#define QCOM_GRAPHICS_KGSL_OFFSET       0x0000000
#define QCOM_GRAPHICS_KGSL_SIZE         0x0040000

#define QCOM_GRAPHICS_RSCC_OFFSET       0x0050000
#define QCOM_GRAPHICS_RSCC_SIZE         0x0010000

#define QCOM_GRAPHICS_CX_DBGC_OFFSET    0x0061000
#define QCOM_GRAPHICS_CX_DBGC_SIZE      0x0003000

#define QCOM_GRAPHICS_CX_MISC_OFFSET    0x009e000
#define QCOM_GRAPHICS_CX_MISC_SIZE      0x0002000


typedef struct KgslState KgslState;

struct KgslState {};

uint64_t kgsl_read(void *gpu, hwaddr addr, unsigned size);

void kgsl_write(void *gpu, hwaddr addr, uint64_t val, unsigned size);
