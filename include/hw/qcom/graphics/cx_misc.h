#pragma once

#include "exec/memory.h"

#define CX_MISC_MEMORY_BASE 0x9e000
#define CX_MISC_MEMORY_SIZE 0x2000

typedef struct CxMiscState CxMiscState;

struct CxMiscState {};

uint64_t cx_misc_read(void *gpu, hwaddr addr, unsigned size);

void cx_misc_write(void *gpu, hwaddr addr, uint64_t val, unsigned size);
