#include "qemu/osdep.h"

#include "hw/qcom/graphics/cx_dbgc.h"

uint64_t cx_dbgc_read(void *gpu, hwaddr addr, unsigned size)
{
    return 0;
}

void cx_dbgc_write(void *gpu, hwaddr addr, uint64_t val, unsigned size)
{}
