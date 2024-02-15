/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include "filename.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "dma.h"

#define SMALL_SIZE (1024 * 1024 * 1)
#define LARGE_SIZE (1024 * 1024 * 4)

#define HALF_SIZE(X) ((X) / 2)
#define QUARTER_SIZE(X) ((X) / 4)

void test(int do_file_op, const int alloc_size, const char *err_msg)

{
    const char *ofname = "memory.dat";
    unsigned char *memory;

    if (do_file_op) {
        (void)remove(ofname);
    }

    /* allocate, align and init data area */
    memory = malloc(alloc_size + ALIGN);
    if (!memory) {
        printf("FAIL : %s\n", __FILENAME__);
        printf("out of memory: data area\n");
        exit(-2);
    }
    memory += ALIGN;
    memory = (unsigned char *)((uintptr_t)memory & (~(ALIGN - 1)));
    unsigned char *src1 = memory;
    unsigned char *src2 = memory + QUARTER_SIZE(alloc_size);
    memset(src1, 0xAA,
           DMA_XFER_SIZE(alloc_size)); /* fill source memory area 1 */
    memset(src2, 0xBB,
           DMA_XFER_SIZE(
               alloc_size)); /* fill source memory area 2 : different value */
    printf("malloc memory at %p: src1 %p: src2 %p\n", memory, src1, src2);

    /* now allocate and init descriptors */
    hexagon_udma_descriptor_type0_t *desc0_1, *desc0_2;
    desc0_1 = alloc_descriptor();
    desc0_2 = alloc_descriptor();
    if (!desc0_1 || !desc0_2) {
        printf("FAIL : %s\n", __FILENAME__);
        printf("out of memory: descriptors\n");
        exit(-2);
    }
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
        printf("FAIL : %s\n", __FILENAME__);
        printf("NOTE: %s\n", err_msg);
        exit(-3);
    }

    /* write memory to output file */
    if (do_file_op) {
        int ofno;

        printf("writing to: %s\n", ofname);
        ofno = open(ofname, O_CREAT | O_TRUNC | O_WRONLY,
                    S_IRWXU | S_IRWXG | S_IRWXO);
        if (!ofno) {
            printf("FAIL : %s\n", __FILENAME__);
            printf("can't open file: %s\n", ofname);
            exit(-4);
        }

        if (write(ofno, memory, alloc_size) != alloc_size) {
            printf("FAIL : %s\n", __FILENAME__);
            printf("can't write file: %s\n", ofname);
            exit(-4);
        }
        close(ofno);
        printf("%s created successfully!\n", ofname);
    }

    free(memory);
    free(desc0_1);
    free(desc0_2);
}

int main(int argc, char **argv)

{
    int do_file_op = 0;

    puts("");
    if (argc == 2 && strcasecmp(argv[1], "-file") == 0) {
        do_file_op = 1;
    }

    test(do_file_op, SMALL_SIZE, "General DMA failure");
    test(do_file_op, LARGE_SIZE, "Preload of dst buffers probably missing");
    printf("PASS : %s\n", __FILENAME__);

    exit(0);
}
