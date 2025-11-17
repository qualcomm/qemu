/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEX_COPROC_H
#define HEX_COPROC_H

#include "exec/hwaddr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COPROC_SUCCESS 0
#define COPROC_ERROR -1

typedef enum {
#define OPCODE(IID) COPROC_ ##IID
#include "coproc_opcodes_generated.h.inc"
#undef OPCODE
} Coproc_opcode;

#define COPROC_INIT -1
#define COPROC_RESET -2
#define COPROC_COMMIT -3

typedef struct {
    int32_t opcode;
    hwaddr vtcm_base;
    uint32_t vtcm_size;
    uint8_t minver;
    uint8_t unit;
    uint16_t spare;
    uint32_t reg_usr;
    uint32_t subsystem_id;
    uint64_t page_size;
    int32_t arg1;
    int32_t arg2;
} CoprocArgs;

void coproc(const CoprocArgs *args);
int coproc_init(const char *coproc_location_user, int hex_rev);
void coproc_trace_op(int opcode);

#ifdef __cplusplus
}
#endif

#endif
