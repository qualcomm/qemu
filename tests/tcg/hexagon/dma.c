/*
 * Test the Hexagon user-DMA engine: dmstart/dmlink/dmpoll/dmwait/
 * dmpause/dmresume and the descriptor types it supports.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <hexagon_protos.h>

int err;

#include "hex_test.h"

#define DM0_STATUS_IDLE   0x00000000
#define DM0_STATUS_ERROR  0x00000002

#define DESC_DESCTYPE_TYPE0  (0u << 24)
#define DESC_DESCTYPE_TYPE1  (1u << 24)
#define DESC_ORDER            (1u << 30)
#define DESC_BYPASSSRC         (1u << 29)
#define DESC_BYPASSDST         (1u << 28)
#define DESC_SRCCOMP          (1u << 27)

/* 32-byte descriptor's DescriptorType byte. */
#define DESC_TYPE_UNSUPPORTED   0x7fu
#define DESC_TYPE_CONSTANT_FILL 8u

/* desc[0]=next desc[1]=ctrl desc[2]=src desc[3]=dst */
typedef uint32_t type0_desc_t[4] __attribute__((aligned(16)));
/* + desc[4]=alloc/padding desc[5]=roi desc[6]=stride */
typedef uint32_t type1_desc_t[8] __attribute__((aligned(32)));

static sigjmp_buf fault_jmp;

static void sigsegv_handler(int sig)
{
    siglongjmp(fault_jmp, sig);
}

static inline void dmstart(uint32_t desc_va)
{
    Q6_dmstart_A((void *)(uintptr_t)desc_va);
}

static inline void dmresume(uint32_t desc_va)
{
    Q6_dmresume_A((void *)(uintptr_t)desc_va);
}

/* dmlink(Rs, Rt): append the descriptor at new_va after tail_va. */
static inline void dmlink(uint32_t tail_va, uint32_t new_va)
{
    Q6_dmlink_AA((void *)(uintptr_t)tail_va, (void *)(uintptr_t)new_va);
}

static inline uint32_t dmpoll(void)
{
    return Q6_R_dmpoll();
}

static inline uint32_t dmwait(void)
{
    return Q6_R_dmwait();
}

static inline uint32_t dmpause(void)
{
    return Q6_R_dmpause();
}

static void test_type0_single(void)
{
    static type0_desc_t desc;
    static uint8_t src[64], dst[64];

    for (int i = 0; i < sizeof(src); i++) {
        src[i] = i + 1;
    }
    memset(dst, 0, sizeof(dst));

    desc[0] = 0; /* next */
    desc[1] = DESC_DESCTYPE_TYPE0 | sizeof(src);
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;

    dmstart((uint32_t)(uintptr_t)desc);

    check32(dmpoll(), DM0_STATUS_IDLE);
    check32(dmwait(), DM0_STATUS_IDLE);
    for (int i = 0; i < sizeof(dst); i++) {
        check32(dst[i], src[i]);
    }
}

static void test_type0_chain(void)
{
    static type0_desc_t desc0, desc1;
    static uint8_t src0[32], src1[32], dst0[32], dst1[32];

    for (int i = 0; i < sizeof(src0); i++) {
        src0[i] = i + 1;
        src1[i] = 0x80 + i;
    }
    memset(dst0, 0, sizeof(dst0));
    memset(dst1, 0, sizeof(dst1));

    desc0[0] = 0; /* next, patched by dmlink below */
    desc0[1] = DESC_DESCTYPE_TYPE0 | sizeof(src0);
    desc0[2] = (uint32_t)(uintptr_t)src0;
    desc0[3] = (uint32_t)(uintptr_t)dst0;

    desc1[0] = 0;
    desc1[1] = DESC_DESCTYPE_TYPE0 | sizeof(src1);
    desc1[2] = (uint32_t)(uintptr_t)src1;
    desc1[3] = (uint32_t)(uintptr_t)dst1;

    /* The tail runs alone; linking to an idle engine starts the new one. */
    dmstart((uint32_t)(uintptr_t)desc0);
    check32(dmpoll(), DM0_STATUS_IDLE);
    for (int i = 0; i < sizeof(dst0); i++) {
        check32(dst0[i], src0[i]);
        check32(dst1[i], 0);
    }

    dmlink((uint32_t)(uintptr_t)desc0, (uint32_t)(uintptr_t)desc1);
    check32(desc0[0], (uint32_t)(uintptr_t)desc1);
    check32(dmpoll(), DM0_STATUS_IDLE);
    for (int i = 0; i < sizeof(dst1); i++) {
        check32(dst1[i], src1[i]);
    }

    /* The linked chain now runs from its head to its new tail. */
    memset(dst0, 0, sizeof(dst0));
    memset(dst1, 0, sizeof(dst1));
    dmstart((uint32_t)(uintptr_t)desc0);

    check32(dmpoll(), DM0_STATUS_IDLE);
    for (int i = 0; i < sizeof(dst0); i++) {
        check32(dst0[i], src0[i]);
    }
    for (int i = 0; i < sizeof(dst1); i++) {
        check32(dst1[i], src1[i]);
    }
}

static void test_type1_box(void)
{
    static type1_desc_t desc;
    static uint8_t src[8 * 16], dst[8 * 16];
    const uint32_t width = 5, height = 4;
    const uint32_t srcstride = 16, dststride = 16;

    for (int i = 0; i < sizeof(src); i++) {
        src[i] = i + 1;
    }
    memset(dst, 0, sizeof(dst));

    desc[0] = 0;
    desc[1] = DESC_DESCTYPE_TYPE1;
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;
    desc[4] = 0;
    desc[5] = (height << 16) | width;
    desc[6] = (dststride << 16) | srcstride;
    desc[7] = 0; /* dstwidthoffset:srcwidthoffset */

    dmstart((uint32_t)(uintptr_t)desc);

    check32(dmpoll(), DM0_STATUS_IDLE);
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            check32(dst[row * dststride + col], src[row * srcstride + col]);
        }
        for (uint32_t col = width; col < dststride; col++) {
            check32(dst[row * dststride + col], 0);
        }
    }
}

static void test_type1_zero_stride(void)
{
    static type1_desc_t desc;
    static uint8_t src[UINT16_MAX + 2];
    static uint8_t dst[UINT16_MAX + 2];

    memset(src, 0, sizeof(src));
    memset(dst, 0, sizeof(dst));
    src[0] = 0x5a;
    src[UINT16_MAX + 1] = 0xa5;

    desc[0] = 0;
    desc[1] = DESC_DESCTYPE_TYPE1;
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;
    desc[4] = 0;
    desc[5] = (2u << 16) | 1;
    desc[6] = 0; /* Version 2+: source and destination stride = 0x10000. */
    desc[7] = 0;

    dmstart((uint32_t)(uintptr_t)desc);

    check32(dmpoll(), DM0_STATUS_IDLE);
    check32(dst[0], src[0]);
    check32(dst[UINT16_MAX + 1], src[UINT16_MAX + 1]);
}

static void test_constant_fill(void)
{
    static type1_desc_t desc;
    static uint8_t dst[4 * 8];
    const uint32_t width = 5, height = 3, dststride = 8;
    const uint8_t fill = 0x5a;

    memset(dst, 0, sizeof(dst));

    desc[0] = 0;
    desc[1] = DESC_DESCTYPE_TYPE1;
    desc[2] = 0; /* Source fields are ignored for constant fill. */
    desc[3] = (uint32_t)(uintptr_t)dst;
    desc[4] = (fill << 8) | DESC_TYPE_CONSTANT_FILL;
    desc[5] = (height << 16) | width;
    desc[6] = dststride << 16;
    desc[7] = 0;

    dmstart((uint32_t)(uintptr_t)desc);

    check32(dmpoll(), DM0_STATUS_IDLE);
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            check32(dst[row * dststride + col], fill);
        }
        for (uint32_t col = width; col < dststride; col++) {
            check32(dst[row * dststride + col], 0);
        }
    }
}

static void test_dmpause_after_completion(void)
{
    static type0_desc_t desc;
    static uint8_t src[16], dst[16];

    memset(src, 0x5a, sizeof(src));
    memset(dst, 0, sizeof(dst));

    desc[0] = 0;
    desc[1] = DESC_DESCTYPE_TYPE0 | sizeof(src);
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;

    dmstart((uint32_t)(uintptr_t)desc);
    check32(dmpause(), DM0_STATUS_IDLE);

    /* dmresume restarts the same descriptor; the copy is idempotent. */
    dmresume((uint32_t)(uintptr_t)desc);
    check32(dmwait(), DM0_STATUS_IDLE);
    for (int i = 0; i < sizeof(dst); i++) {
        check32(dst[i], src[i]);
    }
}

static void test_misaligned_descriptor(void)
{
    static uint8_t buf[32] __attribute__((aligned(16)));

    /* buf + 1 is guaranteed misaligned relative to the 16-byte requirement. */
    dmstart((uint32_t)(uintptr_t)(buf + 1));
    check32(dmpoll(), DM0_STATUS_ERROR);
    check32(dmpause(), DM0_STATUS_ERROR);
}

static void test_misaligned_type1_descriptor(void)
{
    static uint8_t buf[48] __attribute__((aligned(32)));
    type1_desc_t *desc = (type1_desc_t *)(buf + 16);

    memset(desc, 0, sizeof(*desc));
    (*desc)[1] = DESC_DESCTYPE_TYPE1;
    dmstart((uint32_t)(uintptr_t)desc);
    check32(dmpoll(), DM0_STATUS_ERROR);
    check32(dmpause(), DM0_STATUS_ERROR);
}

static void test_unsupported_control(void)
{
    static type0_desc_t desc;
    static uint8_t src[16], dst[16];

    memset(src, 0x5a, sizeof(src));
    memset(dst, 0, sizeof(dst));
    desc[0] = 0;
    desc[1] = DESC_SRCCOMP | sizeof(src);
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;

    dmstart((uint32_t)(uintptr_t)desc);
    check32(dmpoll(), DM0_STATUS_ERROR);
    for (int i = 0; i < sizeof(dst); i++) {
        check32(dst[i], 0);
    }
    check32(dmpause(), DM0_STATUS_ERROR);
}

static void test_unsupported_width_offset(void)
{
    static type1_desc_t desc;
    static uint8_t src[16], dst[16];

    memset(src, 0x5a, sizeof(src));
    memset(dst, 0, sizeof(dst));
    desc[0] = 0;
    desc[1] = DESC_DESCTYPE_TYPE1;
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;
    desc[4] = 0;
    desc[5] = (1u << 16) | sizeof(src);
    desc[6] = (sizeof(src) << 16) | sizeof(src);
    desc[7] = 1;

    dmstart((uint32_t)(uintptr_t)desc);
    check32(dmpoll(), DM0_STATUS_ERROR);
    for (int i = 0; i < sizeof(dst); i++) {
        check32(dst[i], 0);
    }
    check32(dmpause(), DM0_STATUS_ERROR);
}

static void test_error_state_commands(void)
{
    static type0_desc_t bad, good;
    static uint8_t src[16], dst[16];

    memset(src, 0x5a, sizeof(src));
    memset(dst, 0, sizeof(dst));
    bad[0] = 0;
    bad[1] = DESC_SRCCOMP | sizeof(src);
    bad[2] = (uint32_t)(uintptr_t)src;
    bad[3] = (uint32_t)(uintptr_t)dst;
    good[0] = 0;
    good[1] = sizeof(src);
    good[2] = (uint32_t)(uintptr_t)src;
    good[3] = (uint32_t)(uintptr_t)dst;

    dmstart((uint32_t)(uintptr_t)bad);
    check32(dmpoll(), DM0_STATUS_ERROR);
    dmstart((uint32_t)(uintptr_t)good);
    check32(dmpoll(), DM0_STATUS_ERROR);
    dmlink((uint32_t)(uintptr_t)bad, (uint32_t)(uintptr_t)good);
    check32(bad[0], 0);
    check32(dmpause(), DM0_STATUS_ERROR);
    check32(dmpoll(), DM0_STATUS_IDLE);

    dmstart((uint32_t)(uintptr_t)good);
    check32(dmpoll(), DM0_STATUS_IDLE);
    for (int i = 0; i < sizeof(dst); i++) {
        check32(dst[i], src[i]);
    }
}

static void test_cyclic_chain(void)
{
    static type0_desc_t desc;

    desc[0] = (uint32_t)(uintptr_t)desc;
    desc[1] = DESC_DESCTYPE_TYPE0; /* A zero-length transfer is sufficient. */
    desc[2] = 0;
    desc[3] = 0;

    dmstart((uint32_t)(uintptr_t)desc);
    check32(dmpoll(), DM0_STATUS_ERROR);
    check32(dmpause(), DM0_STATUS_ERROR);
}

/*
 * Order and Bypass are legal, commonly-set descriptor bits: Order requests
 * chain synchronization and Bypass requests the DSP-cache-bypass memory
 * path.  Neither bit is backed by distinguishable behavior in this
 * synchronous model, but setting them must not be treated as an unsupported
 * descriptor.
 */
static void test_order_and_bypass_accepted(void)
{
    static type0_desc_t desc;
    static uint8_t src[16], dst[16];

    for (int i = 0; i < sizeof(src); i++) {
        src[i] = i + 1;
    }
    memset(dst, 0, sizeof(dst));

    desc[0] = 0;
    desc[1] = DESC_ORDER | DESC_BYPASSSRC | DESC_BYPASSDST |
              DESC_DESCTYPE_TYPE0 | sizeof(src);
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;

    dmstart((uint32_t)(uintptr_t)desc);

    check32(dmpoll(), DM0_STATUS_IDLE);
    for (int i = 0; i < sizeof(dst); i++) {
        check32(dst[i], src[i]);
    }
}

/*
 * A 32-byte descriptor whose DescriptorType byte names an unimplemented
 * type must be rejected outright, not misdecoded as the plain 2D type and
 * run with garbage width/height/stride.
 */
static void test_unsupported_descriptor_type(void)
{
    static type1_desc_t desc;
    static uint8_t src[16], dst[16];

    memset(src, 0x5a, sizeof(src));
    memset(dst, 0, sizeof(dst));

    desc[0] = 0;
    desc[1] = DESC_DESCTYPE_TYPE1;
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = (uint32_t)(uintptr_t)dst;
    desc[4] = DESC_TYPE_UNSUPPORTED; /* DescriptorType byte */
    desc[5] = (1u << 16) | sizeof(src);
    desc[6] = (sizeof(src) << 16) | sizeof(src);
    desc[7] = 0;

    dmstart((uint32_t)(uintptr_t)desc);
    check32(dmpoll(), DM0_STATUS_ERROR);
    for (int i = 0; i < sizeof(dst); i++) {
        check32(dst[i], 0);
    }
    check32(dmpause(), DM0_STATUS_ERROR);
}

static void test_address_space_overflow(void)
{
    static type1_desc_t desc;
    static uint8_t src[64];

    memset(src, 0x5a, sizeof(src));
    desc[0] = 0;
    desc[1] = DESC_DESCTYPE_TYPE1;
    desc[2] = (uint32_t)(uintptr_t)src;
    desc[3] = 0xfffffff0; /* The second row would wrap past the top. */
    desc[4] = 0;
    desc[5] = (2u << 16) | 4;
    desc[6] = (64u << 16) | 16;
    desc[7] = 0;

    dmstart((uint32_t)(uintptr_t)desc);
    check32(dmpoll(), DM0_STATUS_ERROR);
    check32(dmpause(), DM0_STATUS_ERROR);
}

static void test_descriptor_fault_status(void)
{
    struct sigaction act = { .sa_handler = sigsegv_handler };

    sigemptyset(&act.sa_mask);
    if (sigaction(SIGSEGV, &act, NULL) != 0) {
        err++;
        return;
    }
    if (sigsetjmp(fault_jmp, 1) == 0) {
        /* Aligned but unmapped, so descriptor probing raises SIGSEGV. */
        dmstart(0x10);
        err++;
    }
    check32(dmpoll(), DM0_STATUS_ERROR);
    signal(SIGSEGV, SIG_DFL);
}

int main(void)
{
    test_type0_single();
    test_type0_chain();
    test_type1_box();
    test_type1_zero_stride();
    test_constant_fill();
    test_dmpause_after_completion();
    test_misaligned_descriptor();
    test_misaligned_type1_descriptor();
    test_unsupported_control();
    test_unsupported_width_offset();
    test_error_state_commands();
    test_cyclic_chain();
    test_order_and_bypass_accepted();
    test_unsupported_descriptor_type();
    test_address_space_overflow();
    test_descriptor_fault_status();

    puts(err ? "FAIL" : "PASS");
    return err;
}
