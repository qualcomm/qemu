#include "qemu/osdep.h"

#include "hw/qcom/graphics/cx_misc.h"

uint64_t cx_misc_read(void *gpu, hwaddr addr, unsigned size)
{
    return 0;
}

void cx_misc_write(void *gpu, hwaddr addr, uint64_t val, unsigned size)
{}
