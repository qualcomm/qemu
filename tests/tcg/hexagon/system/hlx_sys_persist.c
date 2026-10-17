/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * Test L-register state persistence across packets
 *
 * Writes patterns to VTCM via HLX, then in a later packet loads them
 * back and computes on them.  Verifies that gen_commit_hlx() correctly
 * commits L-register state at packet boundaries.
 */

#include <stdint.h>
#include <stdio.h>

#include <hexagon_standalone.h>
#include <hexagon_types.h>
#include <hlx_hexagon_protos.h>

#define HLX_HALF_ELEMS  1024
#define HLX_VEC_BYTES   2048
#define HLX_VSP_SIZE    (16 * HLX_VEC_BYTES)

static void *vtcm_base_ptr;
static size_t vtcm_offset;

static inline void init_vtcm(void)
{
    uint32_t base;
    asm volatile(
        "r1 = cfgbase\n"
        "r1 = asl(r1, #5)\n"
        "r2 = #0x38\n"
        "r1 = memw_phys(r2, r1)\n"
        "%0 = asl(r1, #16)\n"
        : "=r"(base) : : "r1", "r2"
    );
    vtcm_base_ptr = (void *)(uintptr_t)base;
    vtcm_offset = 0;

    uint64_t pa = (uint64_t)base;
    add_translation_fixed(1, vtcm_base_ptr, (void *)pa, 6, 0);
    add_translation_fixed(2, (char *)vtcm_base_ptr + 1024 * 1024,
                          (void *)(pa + 1024 * 1024), 6, 0);
}

static inline void *vtcm_alloc(size_t bytes)
{
    vtcm_offset = (vtcm_offset + (HLX_VEC_BYTES - 1))
                  & ~(size_t)(HLX_VEC_BYTES - 1);
    void *ptr = (char *)vtcm_base_ptr + vtcm_offset;
    vtcm_offset += bytes;
    return ptr;
}

static inline void enable_hlx(void)
{
    uint32_t tmp;
    asm volatile(
        "%0 = ccr\n"
        "%0 = setbit(%0, #28)\n"
        "ccr = %0\n"
        "isync\n"
        : "=&r"(tmp)
    );
}

static inline void init_hlx_vsp(void)
{
    void *vsp_area = vtcm_alloc(HLX_VSP_SIZE);
    uint32_t vsp_top = (uint32_t)(uintptr_t)vsp_area + HLX_VSP_SIZE;
    asm volatile("vsp = %0\n" : : "r"(vsp_top));
}

int main(void)
{
    init_vtcm();
    enable_hlx();

    uint16_t *buf_a = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    uint16_t *buf_b = vtcm_alloc(HLX_HALF_ELEMS * sizeof(uint16_t));
    init_hlx_vsp();

    const uint16_t pat_a = 0x1234;
    const uint16_t pat_b = 0x5678;

    /* Store patterns - each intrinsic generates a separate packet */
    HLX_Vector va = Q6_Lh_equals_Rh(pat_a);
    Q6_mem_Ku16_new_AL(buf_a, va);

    HLX_Vector vb = Q6_Lh_equals_Rh(pat_b);
    Q6_mem_Ku16_new_AL(buf_b, vb);

    /* Load back in later packets and compute */
    HLX_Vector la = Q6_L_tmp_mem_A(buf_a);
    HLX_Vector lb = Q6_L_tmp_mem_A(buf_b);
    HLX_Vector sum = Q6_Lh_add_LhLh(la, lb);
    Q6_mem_Ku16_new_AL(buf_a, sum);

    uint16_t expected = pat_a + pat_b;
    for (int i = 0; i < HLX_HALF_ELEMS; i++) {
        if (buf_a[i] != expected) {
            puts("FAIL");
            return 1;
        }
    }

    puts("PASS");
    return 0;
}
