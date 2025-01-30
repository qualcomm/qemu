/*
 *  Copyright(c) 2024-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DMA_H
#define DMA_H

#include <hexagon_types.h>
#include <string.h>
#include <stdio.h>

/*
 * Helpers for DMA tests
 */

#define HEXAGON_UDMA_DM0_STATUS_IDLE 0x00000000
#define HEXAGON_UDMA_DM0_STATUS_RUN 0x00000001
#define HEXAGON_UDMA_DM0_STATUS_ERROR 0x00000002
#define HEXAGON_UDMA_DESC_DSTATE_INCOMPLETE 0
#define HEXAGON_UDMA_DESC_DSTATE_COMPLETE 1
#define HEXAGON_UDMA_DESC_ORDER_NOORDER 0
#define HEXAGON_UDMA_DESC_ORDER_ORDER 1
#define HEXAGON_UDMA_DESC_BYPASS_OFF 0
#define HEXAGON_UDMA_DESC_BYPASS_ON 1
#define HEXAGON_UDMA_DESC_COMP_NONE 0
#define HEXAGON_UDMA_DESC_COMP_DLBC 1
#define HEXAGON_UDMA_DESC_DESCTYPE_TYPE0 0
#define HEXAGON_UDMA_DESC_DESCTYPE_TYPE1 1

void do_dmastart(void *desc)
{
    asm volatile("r0 = %0\n"
                 "dmstart(r0)\n"
                 "r0 = dmwait\n"
                 :
                 : "r"(desc)
                 : "r0");
}

#define DMA_XFER_SIZE(X) ((X) / 8)

hexagon_udma_descriptor_type0_t
fill_descriptor0(void *src, void *dst, int length,
                 hexagon_udma_descriptor_type0_t *next)

{
    hexagon_udma_descriptor_type0_t desc0;

    memset(&desc0, 0, sizeof(hexagon_udma_descriptor_type0_t));
    desc0.next = next;
    desc0.order = HEXAGON_UDMA_DESC_ORDER_NOORDER;
    desc0.srcbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
    desc0.dstbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
    desc0.srccomp = HEXAGON_UDMA_DESC_COMP_NONE;
    desc0.dstcomp = HEXAGON_UDMA_DESC_COMP_NONE;
    desc0.desctype = HEXAGON_UDMA_DESC_DESCTYPE_TYPE0;
    desc0.length = length;
    desc0.src = src;
    desc0.dst = dst;
    printf("fill desc: src %p, dst %p, len %d, next %p\n", src, dst, length,
           next);

    return desc0;
}

hexagon_udma_descriptor_type1_t
fill_descriptor1(void *src, void *dst, int length, int roiheight, int roiwidth,
                 int src_stride, int dst_stride, int src_wo, int dst_wo,
                 hexagon_udma_descriptor_type1_t *next)
{
    hexagon_udma_descriptor_type1_t desc1;

    memset(&desc1, 0, sizeof(hexagon_udma_descriptor_type1_t));
    desc1.next = next;
    desc1.order = HEXAGON_UDMA_DESC_ORDER_NOORDER;
    desc1.srcbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
    desc1.dstbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
    desc1.srccomp = HEXAGON_UDMA_DESC_COMP_NONE;
    desc1.dstcomp = HEXAGON_UDMA_DESC_COMP_NONE;
    desc1.desctype = HEXAGON_UDMA_DESC_DESCTYPE_TYPE1;
    desc1.length = length;
    desc1.roiwidth = roiwidth;
    desc1.roiheight = roiheight;
    desc1.srcstride = src_stride;
    desc1.dststride = dst_stride;
    desc1.dstwidthoffset = dst_wo;
    desc1.srcwidthoffset = src_wo;
    desc1.src = src;
    desc1.dst = dst;
    printf("fill desc: src %p, dst %p, len %d, next %p\n", src, dst, length,
           next);

    return desc1;
}

#define ALIGN (1024 * 32)
#define DESC_ALIGN (1024 * 8)

#endif
