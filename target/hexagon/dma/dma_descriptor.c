/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

/*
 * User-DMA Descriptor Services.
 *
 */

#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#include "dma.h"
#include "dma_adapter.h"

// DMA Descriptor getter/setter ------------------------------------------------
uint32_t get_dma_desc_next(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->next) & DESC_NEXT_MASK) >> DESC_NEXT_SHIFT);
}

void set_dma_desc_next(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->next) &= ~DESC_NEXT_MASK;
    (((dma_descriptor_type0_t*)d)->next) |= ((v << DESC_NEXT_SHIFT) & DESC_NEXT_MASK);
}

uint32_t get_dma_desc_dstate(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_DSTATE_MASK) >> DESC_DSTATE_SHIFT);
}

void set_dma_desc_dstate(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_DSTATE_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_DSTATE_SHIFT) & DESC_DSTATE_MASK);
}

uint32_t get_dma_desc_order(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_ORDER_MASK) >> DESC_ORDER_SHIFT);
}

void set_dma_desc_order(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_ORDER_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_ORDER_SHIFT) & DESC_ORDER_MASK);
}

uint32_t get_dma_desc_bypasssrc(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_BYPASSSRC_MASK) >> DESC_BYPASSSRC_SHIFT);
}

void set_dma_desc_bypasssrc(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_BYPASSSRC_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_BYPASSSRC_SHIFT) & DESC_BYPASSSRC_MASK);
}

uint32_t get_dma_desc_bypassdst(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_BYPASSDST_MASK) >> DESC_BYPASSDST_SHIFT);
}

void set_dma_desc_bypassdst(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_BYPASSDST_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_BYPASSDST_SHIFT) & DESC_BYPASSDST_MASK);
}

uint32_t get_dma_desc_desctype(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_DESCTYPE_MASK) >> DESC_DESCTYPE_SHIFT);
}

void set_dma_desc_desctype(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_DESCTYPE_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_DESCTYPE_SHIFT) & DESC_DESCTYPE_MASK);
}

uint32_t get_dma_desc_srccomp(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_SRCCOMP_MASK) >> DESC_SRCCOMP_SHIFT);
}

void set_dma_desc_srccomp(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_SRCCOMP_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_SRCCOMP_SHIFT) & DESC_SRCCOMP_MASK);
}

uint32_t get_dma_desc_dstcomp(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_DSTCOMP_MASK) >> DESC_DSTCOMP_SHIFT);
}

void set_dma_desc_dstcomp(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_DSTCOMP_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_DSTCOMP_SHIFT) & DESC_DSTCOMP_MASK);
}

uint32_t get_dma_desc_length(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) & DESC_LENGTH_MASK) >> DESC_LENGTH_SHIFT);
}

void set_dma_desc_length(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) &= ~DESC_LENGTH_MASK;
    (((dma_descriptor_type0_t*)d)->dstate_order_bypass_comp_desctype_length) |= ((v << DESC_LENGTH_SHIFT) & DESC_LENGTH_MASK);
}

uint32_t get_dma_desc_src(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->src) & DESC_SRC_MASK) >> DESC_SRC_SHIFT);
}

void set_dma_desc_src(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->src) &= ~DESC_SRC_MASK;
    (((dma_descriptor_type0_t*)d)->src) |= ((v << DESC_SRC_SHIFT) & DESC_SRC_MASK);
}

uint32_t get_dma_desc_dst(void *d)
{
    return (((((dma_descriptor_type0_t*)d)->dst) & DESC_DST_MASK) >> DESC_DST_SHIFT);
}

void set_dma_desc_dst(void *d, uint32_t v)
{
    (((dma_descriptor_type0_t*)d)->dst) &= ~DESC_DST_MASK;
    (((dma_descriptor_type0_t*)d)->dst) |= ((v << DESC_DST_SHIFT) & DESC_DST_MASK);
}

uint32_t get_dma_desc_roiwidth(void *d)
{
    return (((((dma_descriptor_type1_t*)d)->roiheight_roiwidth) & DESC_ROIWIDTH_MASK) >> DESC_ROIWIDTH_SHIFT);
}

void set_dma_desc_roiwidth(void *d, uint32_t v)
{
    (((dma_descriptor_type1_t*)d)->roiheight_roiwidth) &= ~DESC_ROIWIDTH_MASK;
    (((dma_descriptor_type1_t*)d)->roiheight_roiwidth) |= ((v << DESC_ROIWIDTH_SHIFT) & DESC_ROIWIDTH_MASK);
}

uint32_t get_dma_desc_roiheight(void *d)
{
    return (((((dma_descriptor_type1_t*)d)->roiheight_roiwidth) & DESC_ROIHEIGHT_MASK) >> DESC_ROIHEIGHT_SHIFT);
}

void set_dma_desc_roiheight(void *d, uint32_t v)
{
    (((dma_descriptor_type1_t*)d)->roiheight_roiwidth) &= ~DESC_ROIHEIGHT_MASK;
    (((dma_descriptor_type1_t*)d)->roiheight_roiwidth) |= ((v << DESC_ROIHEIGHT_SHIFT) & DESC_ROIHEIGHT_MASK);
}

uint32_t get_dma_desc_srcstride(void *d)
{
    return (((((dma_descriptor_type1_t*)d)->dststride_srcstride) & DESC_SRCSTRIDE_MASK) >> DESC_SRCSTRIDE_SHIFT);
}

void set_dma_desc_srcstride(void *d, uint32_t v)
{
    (((dma_descriptor_type1_t*)d)->dststride_srcstride) &= ~DESC_SRCSTRIDE_MASK;
    (((dma_descriptor_type1_t*)d)->dststride_srcstride) |= ((v << DESC_SRCSTRIDE_SHIFT) & DESC_SRCSTRIDE_MASK);
}

uint32_t get_dma_desc_dststride(void *d)
{
    return (((((dma_descriptor_type1_t*)d)->dststride_srcstride) & DESC_DSTSTRIDE_MASK) >> DESC_DSTSTRIDE_SHIFT);
}

void set_dma_desc_dststride(void *d, uint32_t v)
{
    (((dma_descriptor_type1_t*)d)->dststride_srcstride) &= ~DESC_DSTSTRIDE_MASK;
    (((dma_descriptor_type1_t*)d)->dststride_srcstride) |= ((v << DESC_DSTSTRIDE_SHIFT) & DESC_DSTSTRIDE_MASK);
}

uint32_t get_dma_desc_srcwidthoffset(void *d)
{
    return (((((dma_descriptor_type1_t*)d)->dstwidthoffset_srcwidthoffset) & DESC_SRCWIDTHOFFSET_MASK) >> DESC_SRCWIDTHOFFSET_SHIFT);
}

void set_dma_desc_srcwidthoffset(void *d, uint32_t v)
{
    (((dma_descriptor_type1_t*)d)->dstwidthoffset_srcwidthoffset) &= ~DESC_SRCWIDTHOFFSET_MASK;
    (((dma_descriptor_type1_t*)d)->dstwidthoffset_srcwidthoffset) |= ((v << DESC_SRCWIDTHOFFSET_SHIFT) & DESC_SRCWIDTHOFFSET_MASK);
}

uint32_t get_dma_desc_dstwidthoffset(void *d)
{
    return (((((dma_descriptor_type1_t*)d)->dstwidthoffset_srcwidthoffset) & DESC_DSTWIDTHOFFSET_MASK) >> DESC_DSTWIDTHOFFSET_SHIFT);
}

void set_dma_desc_dstwidthoffset(void *d, uint32_t v)
{
    (((dma_descriptor_type1_t*)d)->dstwidthoffset_srcwidthoffset) &= ~DESC_DSTWIDTHOFFSET_MASK;
    (((dma_descriptor_type1_t*)d)->dstwidthoffset_srcwidthoffset) |= ((v << DESC_DSTWIDTHOFFSET_SHIFT) & DESC_DSTWIDTHOFFSET_MASK);
}

uint32_t get_dma_desc_padding(void *d)
{
    return (((((dma_descriptor_type1_t*)d)->allocation_padding) & DESC_PADDING_MASK) >> DESC_PADDING_SHIFT);
}

void set_dma_desc_padding(void *d, uint32_t v)
{
    (((dma_descriptor_type1_t*)d)->allocation_padding) &= ~DESC_PADDING_MASK;
    (((dma_descriptor_type1_t*)d)->allocation_padding) |= ((v << DESC_PADDING_SHIFT) & DESC_PADDING_MASK);
}


uint32_t get_dma_dm2_cfg_guest_mode_stall(uint32_t cfg)
{
    return (cfg & DM2CFG_GUEST_MODE_STALL_MASK) >> DM2CFG_GUEST_MODE_STALL_SHIFT;
}

void set_dma_dm2_cfg_guest_mode_stall(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_GUEST_MODE_STALL_MASK;
    *cfg |= ((v << DM2CFG_GUEST_MODE_STALL_SHIFT) & DM2CFG_GUEST_MODE_STALL_MASK);
}

uint32_t get_dma_dm2_cfg_monitor_mode_stall(uint32_t cfg)
{
    return (cfg & DM2CFG_MONITOR_MODE_STALL_MASK) >> DM2CFG_MONITOR_MODE_STALL_SHIFT;
}

void set_dma_dm2_cfg_monitor_mode_stall(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_MONITOR_MODE_STALL_MASK;
    *cfg |= ((v << DM2CFG_MONITOR_MODE_STALL_SHIFT) & DM2CFG_MONITOR_MODE_STALL_MASK);
}

uint32_t get_dma_dm2_cfg_exception_stall(uint32_t cfg)
{
    return (cfg & DM2CFG_EXCEPTION_MODE_STALL_MASK) >> DM2CFG_EXCEPTION_MODE_STALL_SHIFT;
}

void set_dma_dm2_cfg_exception_stall(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_EXCEPTION_MODE_STALL_MASK;
    *cfg |= ((v << DM2CFG_EXCEPTION_MODE_STALL_SHIFT) & DM2CFG_EXCEPTION_MODE_STALL_MASK);
}

uint32_t get_dma_dm2_cfg_debug_stall(uint32_t cfg)
{
    return (cfg & DM2CFG_DEBUG_MODE_STALL_MASK) >> DM2CFG_DEBUG_MODE_STALL_SHIFT;
}

void set_dma_dm2_cfg_debug_stall(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_DEBUG_MODE_STALL_MASK;
    *cfg |= ((v << DM2CFG_DEBUG_MODE_STALL_SHIFT) & DM2CFG_DEBUG_MODE_STALL_MASK);
}

uint32_t get_dma_dm2_cfg_dlbc_enable(uint32_t cfg)
{
    return (cfg & DM2CFG_DLBC_ENABLE_MASK) >> DM2CFG_DLBC_ENABLE_SHIFT;
}

void set_dma_dm2_cfg_dlbc_enable(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_DLBC_ENABLE_MASK;
    *cfg |= ((v << DM2CFG_DLBC_ENABLE_SHIFT) & DM2CFG_DLBC_ENABLE_MASK);
}

uint32_t get_dma_dm2_cfg_error_exception_ctrl(uint32_t cfg)
{
    return (cfg & DM2CFG_ERROR_EXCEPTION_MASK) >> DM2CFG_ERROR_EXCEPTION_SHIFT;
}

void set_dma_dm2_cfg_error_exception_ctrl(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_ERROR_EXCEPTION_MASK;
    *cfg |= ((v << DM2CFG_ERROR_EXCEPTION_SHIFT) & DM2CFG_ERROR_EXCEPTION_MASK);
}

uint32_t get_dma_dm2_cfg_ooo_write_ctrl(uint32_t cfg)
{
    return (cfg & DM2CFG_OOO_WRITE_MASK) >> DM2CFG_OOO_WRITE_SHIFT;
}

void set_dma_dm2_cfg_ooo_write_ctrl(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_OOO_WRITE_MASK;
    *cfg |= ((v << DM2CFG_OOO_WRITE_SHIFT) & DM2CFG_OOO_WRITE_MASK);
}

uint32_t get_dma_dm2_cfg_outstanding_transactions_read(uint32_t cfg)
{
    return (cfg & DM2CFG_OUTSTANDING_READ_MASK) >> DM2CFG_OUTSTANDING_READ_SHIFT;
}

void set_dma_dm2_cfg_outstanding_transactions_read(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_OUTSTANDING_READ_MASK;
    *cfg |= ((v << DM2CFG_OUTSTANDING_READ_SHIFT) & DM2CFG_OUTSTANDING_READ_MASK);
}

uint32_t get_dma_dm2_cfg_outstanding_transactions_write(uint32_t cfg)
{
    return (cfg & DM2CFG_OUTSTANDING_WRITE_MASK) >> DM2CFG_OUTSTANDING_WRITE_SHIFT;
}

void set_dma_dm2_cfg_outstanding_transactions_write(uint32_t *cfg, uint32_t v)
{
    *cfg &= ~DM2CFG_OUTSTANDING_WRITE_MASK;
    *cfg |= ((v << DM2CFG_OUTSTANDING_WRITE_SHIFT) & DM2CFG_OUTSTANDING_WRITE_MASK);
}

//==============================================================================

uint32_t HEXAGON_DmaDescriptorSize(uint32_t descSize) {
	const uint32_t desc_size_by_type[] = {16, 32};
	return( desc_size_by_type[descSize] );
}

void HEXAGON_DmaDescriptor_printList(const char *prefix, HEXAGON_DmaDescriptor_t *descP) {
	char str[16 * 1024];
	va_t nextDescPointer;
	do {
		HEXAGON_DmaDescriptor_toStr(str, descP);
		printf("%s0x%p:%s\n", prefix, descP, str);
		nextDescPointer = descP->common.nextDescPointer;
		uint64_t nextDescPointer_64 = nextDescPointer;
		descP = (HEXAGON_DmaDescriptor_t *)nextDescPointer_64;
	} while( nextDescPointer != 0 );
}

int HEXAGON_DmaDescriptor_toStr(char *s, const HEXAGON_DmaDescriptor_t *descP) {
	int nChars = 0;
	switch (descP->common.descSize) {
		case 0:		// 16B - linear
			nChars = HEXAGON_DmaDescriptorLinear_toStr( s, (HEXAGON_DmaDescriptorLinear_t *)descP );
			break;
		case 1:		// 32B - 2D
			nChars = HEXAGON_DmaDescriptor2D_toStr( s, (HEXAGON_DmaDescriptor2D_t *)descP );
			break;
		default:
			*s = '\0';
			//assert(descP->common.descSize <= 1);
	}
    return(nChars);
}

int HEXAGON_DmaDescriptorLinear_toStr(char *s, const HEXAGON_DmaDescriptorLinear_t *p) {
	int nChars = sprintf(s,
		"SRC: 0x%" VA_FMT " -> DST: 0x%" VA_FMT " %uB  src: bypass=%u dlbc=%u  dst: bypass=%u dlbc=%u  order=%u done=%u nextP=0x%" VA_FMT,
		p->srcAddress, p->dstAddress, p->length,
		p->srcBypass, p->srcDlbc,
		p->dstBypass, p->dstDlbc,
		p->order, p->done, p->nextDescPointer
		);
	return(nChars);
}

static inline uint32_t decode_stride2(uint32_t stride ) {
    return (stride) ? stride : 65536;
}

int HEXAGON_DmaDescriptor2D_toStr(char *s, const HEXAGON_DmaDescriptor2D_t *p) {
    int nChars = 0;
    long long int src = 0;
    long long int dst = 0;
    switch (p->type)
    {
        case 0:  // default
                nChars = sprintf(s,
                "2D Desc 32-bit: SRC: 0x%08x -> DST: 0x%08x %u x %u  src: bypass=%u stride=%u widthOffset=%u  dst: bypass=%u stride=%u widthOffset=%u wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                p->srcAddress, p->dstAddress, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset, p->dstBypass, decode_stride2(p->dstStride), p->dstWidthOffset, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;
        case 2:
                src = (long long int)p->srcAddress | ( (long long int)p->srcUpperAddr <<32);
                dst = (long long int)p->dstAddress | ( (long long int)p->dstUpperAddr <<32);
                nChars = sprintf(s,
                "2D Desc 38-bit: SRC: 0x%010llx -> DST: 0x%010llx  %u x %u  src: bypass=%u stride=%u widthOffset=%u  dst: bypass=%u stride=%u widthOffset=%u  wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                src, dst, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset,  p->dstBypass, decode_stride2(p->dstStride), p->dstWidthOffset, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;
        case 3:
            nChars = sprintf(s,
                "2D Desc L2 Fetch: SRC: 0x%08x %u x %u  src: bypass=%u stride=%u widthOffset=%u  wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                p->srcAddress, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;
        case 4:
            nChars = sprintf(s,
                "2D Desc 32-bit Gather: SRC: 0x%08x -> DST: 0x%08x %u x %u  src: bypass=%u stride=%u widthOffset=%u  dst: bypass=%u stride=%u widthOffset=%u wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                p->srcAddress, p->dstAddress, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset, p->dstBypass, decode_stride2(p->dstStride), p->dstWidthOffset, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;
        case 5:
                src = (long long int)p->srcAddress | ( (long long int)p->srcUpperAddr <<32);
                dst = (long long int)p->dstAddress | ( (long long int)p->dstUpperAddr <<32);
                nChars = sprintf(s,
                "2D Desc 38-bit Gather: SRC: 0x%010llx -> DST: 0x%010llx  %u x %u  src: bypass=%u stride=%u widthOffset=%u  dst: bypass=%u stride=%u widthOffset=%u  wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                src, dst, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset, p->dstBypass, decode_stride2(p->dstStride), p->dstWidthOffset, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;
        case 6:
                nChars = sprintf(s,
                "2D Desc 32-bit Expansion: SRC: 0x%08x -> DST: 0x%08x %u x %u  src: bypass=%u stride=%u widthOffset=%u  dst: bypass=%u stride=%u widthOffset=%u blockSize=%u curBlockOffset=%u startBlockOffest=%u wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                p->srcAddress, p->dstAddress, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset, p->dstBypass, decode_stride2(p->dstStride), p->dstWidthOffset, p->blockSize, p->curBlockOffset, p->startBlockOffest, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;
        case 7:
                nChars = sprintf(s,
                "2D Desc 32-bit Compression: SRC: 0x%08x -> DST: 0x%08x %u x %u  src: bypass=%u stride=%u widthOffset=%u  dst: bypass=%u stride=%u widthOffset=%u blockSize=%u curBlockOffset=%u startBlockOffest=%u wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                p->srcAddress, p->dstAddress, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset, p->dstBypass, decode_stride2(p->dstStride), p->dstWidthOffset, p->blockSize, p->curBlockOffset, p->startBlockOffest, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;

        case 8:
                nChars = sprintf(s,
                "2D Desc 32-bit Constant Fill: DST: 0x%08x %u x %u  src: bypass=%u stride=%u widthOffset=%u  dst: bypass=%u stride=%u widthOffset=%u fillValue=%u wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                p->dstAddress, p->width, p->height, p->srcBypass, decode_stride2(p->srcStride), p->srcWidthOffset, p->dstBypass, decode_stride2(p->dstStride), p->dstWidthOffset, p->fillValue, p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;

        case 9:
                nChars = sprintf(s,
                "2D Desc 32-bit Wide 2D: SRC: 0x%08x -> DST: 0x%08x %u x %u  src: bypass=%u stride=%u dst: bypass=%u stride=%u wr_allocation=%u rd_allocation=%u order=%u done=%u nextP=0x%08x",
                p->srcAddress, p->dstAddress, p->width_wide, p->height, p->srcBypass, decode_stride2(p->srcStride_wide), p->dstBypass, decode_stride2(p->dstStride_wide), p->wr_alloc, p->rd_alloc, p->order, p->done, p->nextDescPointer);
                break;


        default:
            nChars = sprintf(s, "Unknown Type");
    };




	return(nChars);
}

