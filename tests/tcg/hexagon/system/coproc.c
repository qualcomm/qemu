/*
 *  Copyright(c) 2022-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __HVXDBL__ 1
#if !defined(__linux__)
#include "hexagon_standalone.h"
#endif
#include "filename.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "hexagon_protos.h"
#pragma clang diagnostic pop

uint8_t activations[2048] __attribute__((aligned(2048)));
uint8_t output[2048] __attribute__((aligned(2048)));
int32_t bias[64] __attribute__((aligned(256)));
int8_t weights[128] __attribute__((aligned(128)));

uint8_t *vtcm;
uint8_t *va_vtcm = (uint8_t *)0xf0000000;

uint8_t *get_vtcm_base()

{
    unsigned char *vtcm_base = NULL;
#if !defined(__linux__)
    asm volatile("r1 = cfgbase\n"
                 "r1 = asl(r1, #5)\n"
                 "r2 = #0x38\n"
                 "r1 = memw_phys(r2, r1)\n"
                 "%0 = asl(r1, #16)\n"
                 : "=r"(vtcm_base)
                 :
                 : "r1", "r2");
#else
    abort();
#endif

    return vtcm_base;
}

#define asm_mxclracc() __asm__ __volatile__("    mxclracc\n"  : : : );
void do_mxclracc()

{
    asm_mxclracc();
}

#define asm_bias_mxmem2()                          \
    __asm__ __volatile__("    r0 = %0\n"           \
                         "    bias = mxmem2(r0)\n" \
                         :                         \
                         : "r"(bias_vtcm)          \
                         : "r0");
void do_bias_mxmem2(uintptr_t bias_vtcm)

{
    asm_bias_mxmem2();
}

#define asm_activation_weight()                                           \
    __asm__ __volatile__("{    activation.ub = mxmem(%0,%1):cm\n"         \
                         "    weight.b = mxmem(%2,%3)\n}"                 \
                         :                                                \
                         : "r"(activations_vtcm), "r"(activations_range), \
                           "r"(weights_vtcm), "r"(weights_range));
void do_activation_weight(uintptr_t activations_vtcm,
                          unsigned activations_range, uintptr_t weights_vtcm,
                          unsigned weights_range)

{
    asm_activation_weight();
}

#define asm_mxmem_after_cm_sat_ub()                                 \
    __asm__ __volatile__("    r0 = %0\n"                            \
                         "    r1 = %1\n"                            \
                         "    mxmem(r0,r1):after:cm:sat.ub = acc\n" \
                         :                                          \
                         : "r"(output_vtcm), "r"(spatialMask));
void do_mxmem_after_cm_sat_ub(uintptr_t output_vtcm, unsigned spatialMask)

{
    asm_mxmem_after_cm_sat_ub();
}

int main()
{
    assert((uintptr_t)activations % 2048 == 0);
    assert((uintptr_t)output % 2048 == 0);
    memset(activations, 0, sizeof(activations));
    activations[0] = 10;

    assert((uintptr_t)weights % 128 == 0);
    memset(weights, 0, sizeof(weights));
    weights[0] = 10;

    assert((uintptr_t)bias % 256 == 0);
    memset(bias, 0, sizeof(bias));
    bias[0] = 24 << 10;

    unsigned dY = 0;
    unsigned dW = 0;
    unsigned channelStop = 3;
    unsigned spatialMask = 0xe0;
    unsigned activations_range = dY | spatialMask | channelStop;
    unsigned weights_range = dW;
#if !defined(__linux__)
    unsigned vtcmPageSize = 4 * 1024 * 1024;
    unsigned pageSizeEnum = 32;
    unsigned perms = 7;
    unsigned cachability = 6;
    unsigned asid = 0;
    unsigned aa = 0;
    unsigned vg = 3;
#endif

    vtcm = get_vtcm_base();
#if !defined(__linux__)
    add_translation_extended(1, va_vtcm, (uint64_t)vtcm, pageSizeEnum, perms,
                             cachability, asid, aa, vg);
    add_translation_extended(2, va_vtcm + vtcmPageSize,
                             (uint64_t)(vtcm + vtcmPageSize), pageSizeEnum,
                             perms, cachability, asid, aa, vg);
#endif
    printf("vtcm at  %p\n", vtcm);

    /* acquire coproc */
    asm volatile("R6=SSR\n"
                 "R6=setbit(R6, #26)\n"
                 "SSR = R6\n"
                 "{ nop; }\n"
                 "{ nop; }\n"
                 "isync;\n"
                 :
                 :
                 : "r6");

    uint8_t *activations_vtcm = va_vtcm;
    uint8_t *output_vtcm = va_vtcm + sizeof(activations);
    uint8_t *bias_vtcm = va_vtcm + sizeof(output);
    uint8_t *weights_vtcm = va_vtcm + sizeof(bias);

    assert((uintptr_t)activations_vtcm % 2048 == 0);
    assert((uintptr_t)output_vtcm % 2048 == 0);
    assert((uintptr_t)weights_vtcm % 128 == 0);
    assert((uintptr_t)bias_vtcm % 256 == 0);

    memcpy(activations_vtcm, activations, sizeof(activations));
    memcpy(weights_vtcm, weights, sizeof(weights));
    memcpy(bias_vtcm, bias, sizeof(bias));

    do_mxclracc();
    do_bias_mxmem2((uintptr_t)bias_vtcm);
    do_activation_weight((uintptr_t)activations_vtcm, activations_range,
                         (uintptr_t)weights_vtcm, weights_range);
    do_mxmem_after_cm_sat_ub((uintptr_t)output_vtcm, spatialMask);

    memcpy(output, output_vtcm, sizeof(output));

    printf("output = %hhd, expected %hhd\n", output[0], (uint8_t)100);
    if (output[0] == (uint8_t)100) {
        printf("PASS : %s\n", __FILENAME__);
    } else {
        printf("FAIL : %s\n", __FILENAME__);
    }

    return output[0] != 100;
}
