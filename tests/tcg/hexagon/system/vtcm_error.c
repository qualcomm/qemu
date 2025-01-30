/*
 *  Copyright(c) 2022-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#ifdef __hexagon__
#include <hexagon_types.h>
#ifndef __linux__
#include <hexagon_standalone.h>
#include <hexagon_sim_timer.h>
#endif
#endif
#include <hexagon_protos.h>

int err;

/* define the number of rows/cols in a square matrix */
#define MATRIX_SIZE 64

/* define the size of the scatter buffer */
#define SCATTER_BUFFER_SIZE (MATRIX_SIZE * MATRIX_SIZE)

/* fake vtcm - put buffers together and force alignment */
static struct {
    unsigned short vscatter16[SCATTER_BUFFER_SIZE];
    unsigned short vgather16[MATRIX_SIZE];
    unsigned int   vscatter32[SCATTER_BUFFER_SIZE];
    unsigned int   vgather32[MATRIX_SIZE];
    unsigned short vscatter16_32[SCATTER_BUFFER_SIZE];
    unsigned short vgather16_32[MATRIX_SIZE];
} vtcm __attribute__((aligned(0x10000)));

const uintptr_t VTCM_SCATTER16_ADDRESS		= 0;

const unsigned int region_len = 4*SCATTER_BUFFER_SIZE - 1;
unsigned short half_offsets[MATRIX_SIZE];
unsigned short half_values[MATRIX_SIZE];

#define FILL_CHAR       '.'

/* fill vtcm scratch with ee */
void prefill_vtcm_scratch(void)
{
    memset(&vtcm, FILL_CHAR, sizeof(vtcm));
}

int main()
{
    prefill_vtcm_scratch();

    HVX_Vector offsets = *(HVX_Vector *)half_offsets;
    HVX_Vector values  = *(HVX_Vector *)half_values;

    Q6_vscatter_RMVhV(VTCM_SCATTER16_ADDRESS, region_len, offsets, values);

    return 0;
}
