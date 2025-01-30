/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <hexagon_standalone.h>
#include "vtcm_common.h"
#include "cfgtable.h"
#define NO_DEFAULT_EVENT_HANDLES
#include "mmu.h"

#define INIT_VAL 0x11111111
#define HEX_CAUSE_COPROC_LDST 0x26

static uint32_t l2line_size;

void my_err_handler_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    if (cause < 64) {
        *my_exceptions |= 1LL << cause;
    } else {
        *my_exceptions = cause;
    }

    switch (cause) {
    case HEX_CAUSE_COPROC_LDST:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(4);
        break;
    default:
        do_coredump();
        break;
    }
}

MAKE_ERR_HANDLER(my_err_handler, my_err_handler_helper)

uint32_t round_up(uint32_t num, uint32_t alignment)
{
    uint32_t remainder = num % alignment;
    return remainder ? num + alignment - remainder : num;
}

typedef enum { EXPECT_SUCCESS, EXPECT_FAILURE } expect_mode;

static void test_memcpy(uint32_t *dst, uint32_t *src, uint32_t src_bytes,
                        uint32_t cp_bytes, expect_mode mode)
{
    uint32_t real_cp_bytes = round_up(cp_bytes + 1, l2line_size);
    fprintf(stderr, "Testing size %"PRIu32"\n", cp_bytes);

    assert(src_bytes % sizeof(*src) == 0);
    assert(real_cp_bytes % sizeof(*src) == 0);
    uint32_t src_size = src_bytes / sizeof(*src);
    uint32_t real_cp_size = real_cp_bytes / sizeof(*src);

    for (int i = 0; i < src_size; i++) {
        src[i] = i;
        dst[i] = INIT_VAL;
    }

    asm volatile(
        "m0 = %2\n"
        "memcpy(%0, %1, m0)\n"
        : : "r"(dst), "r"(src), "r"(cp_bytes)
        : "m0", "memory"
    );

    for (int i = 0; i < src_size; i++) {
        uint32_t expect;
        if (mode == EXPECT_SUCCESS) {
            expect = i < real_cp_size ? src[i] : INIT_VAL;
        } else {
            expect = INIT_VAL;
        }
        if (dst[i] != expect) {
            fprintf(stderr, "ERROR: at %d got %"PRIu32", exp %"PRIu32"\n",
                    i, dst[i], expect);
            err |= 1;
        }
    }
}

static void *alloc_aligned(size_t alignment, size_t size)
{
    void *buf;
    if (posix_memalign(&buf, alignment, size)) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(1);
    }
    return buf;
}

static void test_invalid_vtcm(uint32_t *src, uint32_t src_bytes)
{
    void *dst = alloc_aligned(l2line_size, src_bytes);
    INSTALL_ERR_HANDLER(my_err_handler);
    test_memcpy(dst, src, src_bytes, src_bytes, EXPECT_FAILURE);
    check64(*my_exceptions, (uint64_t)1 << HEX_CAUSE_COPROC_LDST);
    free(dst);
}

int main()
{
    uint32_t src_bytes;
    uint32_t *src, *dst = setup_default_vtcm();

    l2line_size = read_cfgtable_field(0x50); /* L2LINE_SZ */
    src_bytes = l2line_size * 3;
    src = alloc_aligned(l2line_size, src_bytes);

    /*
     * Test some key sizes. See vtcm_memcpy implementation note at
     * target/hexagon/genptr.c for more details.
     */
    test_memcpy(dst, src, src_bytes, 0, EXPECT_SUCCESS);
    test_memcpy(dst, src, src_bytes, l2line_size - 1, EXPECT_SUCCESS);
    test_memcpy(dst, src, src_bytes, l2line_size, EXPECT_SUCCESS);
    test_memcpy(dst, src, src_bytes, 2 * l2line_size - 1, EXPECT_SUCCESS);
    test_memcpy(dst, src, src_bytes, 2 * l2line_size, EXPECT_SUCCESS);

    test_invalid_vtcm(src, src_bytes);

    free(src);
    puts(err ? "FAIL\n" : "PASS\n");
    return err;
}
