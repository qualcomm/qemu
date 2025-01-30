/*
 *  Copyright(c) 2023-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include "dma.h"

#define SMALL_SIZE (1024 * 1024 * 1)
#define LARGE_SIZE (1024 * 1024 * 4)

unsigned char small_memory[SMALL_SIZE + ALIGN];
unsigned char large_memory[LARGE_SIZE + ALIGN];

unsigned char __attribute__((__aligned__(DESC_ALIGN))) desc_buf1[DESC_ALIGN * 2];
unsigned char __attribute__((__aligned__(DESC_ALIGN))) desc_buf2[DESC_ALIGN * 2];

#define HALF_SIZE(X) ((X) / 2)
#define QUARTER_SIZE(X) ((X) / 4)

void test(bool use_small, const char *err_msg)
{
    const char *ofname = "memory.dat";
    unsigned char *memory = use_small ? small_memory : large_memory;
    size_t alloc_size = use_small ? SMALL_SIZE : LARGE_SIZE;

    /* init data area */
    memory += ALIGN;
    memory = (unsigned char *)((uintptr_t)memory & (~(ALIGN - 1)));
    unsigned char *src1 = memory;
    unsigned char *src2 = memory + QUARTER_SIZE(alloc_size);
    memset(src1, 0xAA,
           DMA_XFER_SIZE(alloc_size)); /* fill source memory area 1 */
    memset(src2, 0xBB,
           DMA_XFER_SIZE(
               alloc_size)); /* fill source memory area 2 : different value */
    printf("memory at %p: src1 %p: src2 %p\n", memory, src1, src2);

    /* now init descriptors */
    hexagon_udma_descriptor_type0_t *desc0_1, *desc0_2;
    desc0_1 = (hexagon_udma_descriptor_type0_t *)desc_buf1;
    desc0_2 = (hexagon_udma_descriptor_type0_t *)desc_buf2;
    printf("aligned: desc0_1 at %p, desc0_2 at %p\n", desc0_1, desc0_2);

    unsigned char *dst1 = memory + HALF_SIZE(alloc_size);
    unsigned char *dst2 =
        memory + HALF_SIZE(alloc_size) + QUARTER_SIZE(alloc_size);
    printf("malloc memory at %p: dst1 %p: dst2 %p\n", memory, dst1, dst2);
    *desc0_1 = fill_descriptor0(src1, dst1, DMA_XFER_SIZE(alloc_size),
                                desc0_2); /* chain two descriptors together */
    *desc0_2 = fill_descriptor0(src2, dst2, DMA_XFER_SIZE(alloc_size),
                                NULL); /* end of chain */

    /* kick off dma */
    do_dmastart(desc0_1);

    /* validate transfer is correct */
    int fail = 0;
    if (memcmp(src1, dst1, DMA_XFER_SIZE(alloc_size)) != 0) {
        printf("first dma transfer failed\n");
        fail = 1;
    }
    if (memcmp(src2, dst2, DMA_XFER_SIZE(alloc_size)) != 0) {
        printf("second dma transfer failed\n");
        fail = 1;
    }
    if (fail) {
        printf("FAIL\n");
        printf("NOTE: %s\n", err_msg);
        exit(-3);
    }
}

int main(int argc, char **argv)

{
    test(true, "General DMA failure");
    test(false, "Preload of dst buffers probably missing");
    printf("PASS\n");

    exit(0);
}
