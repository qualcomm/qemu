#pragma once

#include "exec/memory.h"

#define CX_DBGC_MEMORY_BASE 0x61000
#define CX_DBGC_MEMORY_SIZE 0x3000

typedef struct CxDbgcState CxDbgcState;

struct CxDbgcState {};

uint64_t cx_dbgc_read(void *gpu, hwaddr addr, unsigned size);

void cx_dbgc_write(void *gpu, hwaddr addr, uint64_t val, unsigned size);
