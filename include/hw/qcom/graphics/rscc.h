#pragma once

#include "exec/memory.h"

#define RSCC_MEMORY_BASE 0x50000
#define RSCC_MEMORY_SIZE 0x10000

typedef struct RsccState RsccState;

struct RsccState {};

uint64_t rscc_read(void *gpu, hwaddr addr, unsigned size);

void rscc_write(void *gpu, hwaddr addr, uint64_t val, unsigned size);
