#include "qemu/osdep.h"
#include "hw/qcom/graphics/gmu.h"

uint64_t gmu_read(void *gpu, hwaddr addr, unsigned size)
{
    return 0;
}

void gmu_write(void *gpu, hwaddr addr, uint64_t val, unsigned size)
{}
