/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <hexagon_standalone.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "coproc_common.h"
#include "coproc_input_fp16_dat.h"
#include "coproc_ref_output_fp16_dat.h"

#include "vtcm_common.h"

int main(int argc, const char *argv[])
{
    SET_SSR_XE2();

    /* Pick some memory location for inputs and outputs in VTCM */
    uint8_t *vtcm_addr =
        (uint8_t *)setup_vtcm(VTCM_SIZE_KB / VTCM_PAGE_SIZE_MULT);

    /* Image Parameters */
    int32_t dx = atoi(argv[1]) & 0x7;
    int32_t dy = atoi(argv[2]) & 0x7;
    int32_t element_size = atoi(argv[3]); /* ch memory format */

    uint8_t *act_addr = (uint8_t *)vtcm_addr;
    uint8_t *wgt_addr = (uint8_t *)vtcm_addr + 64 * 1024;
    uint8_t *out_addr = (uint8_t *)vtcm_addr + 128 * 1024;
    uint8_t *bias_addr = (uint8_t *)vtcm_addr + 256 * 1024;
    int32_t x_tap = (dx & 0x7) + 1;
    int32_t y_tap = (dy & 0x7) + 1;
    int32_t weights_size =
        (x_tap) * (y_tap) * Z_TILE_SIZE_DEF * Z_TILE_SIZE_DEF * element_size;
    for (int32_t i = 0; i < 32; i++) {
        uint32_t *bias = (uint32_t *)bias_addr;
        bias[i] = (element_size == 1) ? INT8_SHIFT_SCALE : FP_BIAS;
    }
    int32_t in_size = 2 * X_TILE_SIZE_DEF * Y_TILE_SIZE_DEF * Z_TILE_SIZE_DEF;
    int32_t out_size = X_TILE_SIZE_DEF * Y_TILE_SIZE_DEF * Z_TILE_SIZE_DEF;

    uint8_t *tmp0 = (uint8_t *)malloc(sizeof(uint8_t) * in_size);
    uint8_t *tmp1 = (uint8_t *)malloc(sizeof(uint8_t) * weights_size);
    uint8_t *tmp2 = (uint8_t *)malloc(sizeof(uint8_t) * out_size);

    printf("Running coproc for input %lux%lux%lu filtered with %lux%lu.\n"
           "\tbias used: %lu element_size: (%ld) %s\n",
           (long)X_TILE_SIZE_DEF, (long)Y_TILE_SIZE_DEF, (long)Z_TILE_SIZE_DEF,
           (long)x_tap, (long)y_tap, (long)*((uint32_t *)bias_addr),
           element_size, element_size == 1 ? "int8" : "fp16");

    int32_t copy_size = sizeof(uint8_t) * in_size;
    memcpy(tmp0, &coproc_input_dat[0], copy_size);
    memcpy(tmp1, &coproc_input_dat[copy_size], sizeof(uint8_t) * weights_size);

    /* VTCM seems to fail with reading and writing with read/write */
    for (int i = 0; i < in_size; i++) {
        act_addr[i] = tmp0[i];
    }
    for (int i = 0; i < weights_size; i++) {
        wgt_addr[i] = tmp1[i];
    }

    mxmem_act_t mxmem_act;
    mxmem_act.ch_start(0);
    mxmem_act.ch_stop(31);
    mxmem_act.y0((uint32_t)act_addr >> 11);
    mxmem_act.dy((y_tap > 1) ? 1 : 0);
    mxmem_act.spatial_mask(Y_TILE_MASK << 3);
    mxmem_act.filter((dy << 3) | dx);

    mxmem_cvt_t mxmem_cvt;
    mxmem_cvt.y0((uint32_t)out_addr >> 11);
    mxmem_cvt.spatial_mask(Y_TILE_MASK << 3);

    uint32_t act_start = mxmem_act.start();
    uint32_t act_range = mxmem_act.range();

    uint32_t wgt_start = (uint32_t)wgt_addr;
    uint32_t wgt_range = weights_size - 1;

    uint32_t cvt_start = mxmem_cvt.start();
    uint32_t cvt_range = mxmem_cvt.range();

    printf("Inputs loaded and kicking off coproc.\n");

    asm("mxclracc;\n"
        "bias = mxmem(%0);\n"
        :
        : "r"((uint32_t)bias_addr));
    if (element_size == 2) {
        asm("{ activation.hf = mxmem(%0,%1); weight.hf = mxmem(%2,%3); }\n"
            "{ mxmem(%4,%5):after.hf = acc; }\n"
            :
            : "r"(act_start), "r"(act_range), "r"(wgt_start), "r"(wgt_range),
              "r"(cvt_start), "r"(cvt_range)
            : "r0");
    } else {
        asm("{ activation.ub = mxmem(%0,%1); weight.b = mxmem(%2,%3); }\n"
            "{ mxmem(%4,%5):after:sat.ub = acc; }\n"
            :
            : "r"(act_start), "r"(act_range), "r"(wgt_start), "r"(wgt_range),
              "r"(cvt_start), "r"(cvt_range)
            : "r0");
    }
    asm("{ r0 = memb(%0); }\n" : : "r"(cvt_start) : "r0");

    for (int i = 0; i < X_TILE_SIZE_DEF * Y_TILE_SIZE_DEF * Z_TILE_SIZE_DEF;
         i++) {
        tmp2[i] = out_addr[i];
    }

    int cmp = memcmp(tmp2, &coproc_ref_output_dat[0], CROUTON_SIZE);
    printf("%s: %s\n", argv[0], (cmp) ? "FAIL" : "PASS");

    free(tmp0);
    free(tmp1);
    free(tmp2);

    return (cmp == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
