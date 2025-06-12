#include "qemu/osdep.h"

#include "hw/qcom/graphics/rscc.h"

uint64_t rscc_read(void *gpu, hwaddr addr, unsigned size)
{
    return 0;
}

void rscc_write(void *gpu, hwaddr addr, uint64_t val, unsigned size)
{}
