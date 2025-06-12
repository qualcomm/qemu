#include "qemu/osdep.h"
#include "hw/qcom/graphics/kgsl.h"

uint64_t kgsl_read(void *gpu, hwaddr addr, unsigned size)
{
    return 0;
}

void kgsl_write(void *gpu, hwaddr addr, uint64_t val, unsigned size)
{}
