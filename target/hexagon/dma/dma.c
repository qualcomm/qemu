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
 * User-DMA Engine.
 *
 * - This version implements "Hexagon V68 Architecture System-Level
 *   Specification - User DMA" which was released on July 28, 2018.
 *
 * - The specification is available from;
 *   https://sharepoint.qualcomm.com/qct/Modem-Tech/QDSP6/Shared%20Documents/
 *   QDSP6/QDSP6v68/Architecture/DMA/UserDMA.pdf
 *
 * TODO:
 * Should this code also use dma_decomposition.[ch]? to share code with the timing model.
 *
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

#ifndef ARCH_FUNCTION
#define ARCH_FUNCTION(func_name) func_name
#endif


#define DBG_TRACING 0
#ifdef DBG_TRACING
#define DMA_DEBUG_ONLY(X) X
#if 0
#define DMA_DEBUG(DMA, ...) printf( __VA_ARGS__);
#else
#define DMA_DEBUG(DMA, ...) \
{ \
    FILE * fp = dma_adapter_debug_log(DMA);\
    if (fp) {\
        fprintf(fp, __VA_ARGS__);\
        fflush(fp);\
    }\
}
#endif
#else
#define DMA_DEBUG(DMA, ...)
#define DMA_DEBUG_ONLY(...)
#endif

#ifndef MIN
#define MIN(A,B) (((A) < (B)) ? (A) : (B))
#endif

static inline uint32_t is_power_of_two(uint32_t x) {
    return (x & (x - 1)) == 0;
}

static inline uint32_t lower_power_of_two(unsigned long x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x>>1;
}

static inline uint16_t get_height(HEXAGON_DmaDescriptor2D_t *desc,
                                  dma_decoded_descriptor_t *desc_state) {
    if (desc_state->wide) {
        return (desc->height_hi << 8) | desc->height_lo;
    } else {
        return desc->height;
    }
}

static inline void set_height(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state, uint16_t val) {
    if (desc_state->wide) {
        desc->height_hi = val >> 8;
        desc->height_lo = val & 0xff;
    } else {
        desc->height = val;
    }
}

static inline uint32_t get_width(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state) {
    if (desc_state->wide) {
        return desc->width_wide;
    } else {
        return desc->width;
    }
}

static inline void set_width(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state, uint32_t val) {
    if (desc_state->wide) {
        desc->width_wide = val;
    } else {
        desc->width = (uint16_t)val;
    }
}

static inline uint32_t get_dst_stride(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state) {
    if (desc_state->wide) {
        return desc->dstStride_wide ? desc->dstStride_wide : 0x1000000;
    } else {
        return desc->dstStride ? desc->dstStride : 0x10000;
    }
}

static inline uint32_t get_src_stride(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state) {
    if (desc_state->wide) {
        return desc->srcStride_wide ? desc->srcStride_wide : 0x1000000;
    } else {
        return desc->srcStride ? desc->srcStride : 0x10000;
    }
}

static inline uint32_t get_dst_widthoffset(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state) {
    if (desc_state->wide) {
        return desc->width_offset;
    } else {
        return desc->dstWidthOffset;
    }
}

static inline uint32_t get_src_widthoffset(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state) {
    if (desc_state->wide) {
        return desc->width_offset;
    } else {
        return desc->srcWidthOffset;
    }
}

static inline void set_dst_widthoffset(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state, uint32_t val) {
    if (desc_state->wide) {
        desc->width_offset = val;
    } else {
        desc->dstWidthOffset = val;
    }
}

static inline void set_src_widthoffset(HEXAGON_DmaDescriptor2D_t *desc, dma_decoded_descriptor_t *desc_state, uint32_t val) {
    if (desc_state->wide) {
        desc->width_offset = val;
    } else {
        desc->srcWidthOffset = val;
    }
}



 const uint8_t dma_transfer_size_lut[256] = {
  // 0000_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0xFF,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 0001_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 0001_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 0010_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0x1F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 0011_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 0001_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 0100_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0x3F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 0101_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 0001_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 0110_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0x1F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 0111_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 0001_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1000_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0x7F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1001_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 0001_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1010_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0x1F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1011_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 0001_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1100_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0x3F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1101_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 0001_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1110_0000, 0000_0001, 0000_0010, 0000_0011, 0000_0100, 0000_0101, 0000_0110, 0000_0111, 0000_1000, 0000_1001, 0000_1010, 0000_1011, 0000_1100, 0000_1101, 0000_1110, 0000_1111,
          0x1F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,
  // 1111_0000, 0001_0001, 0001_0010, 0001_0011, 0001_0100, 0001_0101, 0001_0110, 0001_0111, 0001_1000, 0001_1001, 0001_1010, 0001_1011, 0001_1100, 0001_1101, 0001_1110, 1111_1111,
          0x0F,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00,      0x07,      0x00,      0x01,      0x00,      0x03,      0x00,      0x01,      0x00
    };
#ifdef DBG_TRACING
// If you want to dump a descriptor contents, use this function.
static void ARCH_FUNCTION(dump_dma_desc)(dma_t *dma, void* d, uint32_t va, uint64_t pa, const char* spot)
{
    char s[512];
    HEXAGON_DmaDescriptor_toStr(s, (HEXAGON_DmaDescriptor_t *)d);
    DMA_DEBUG(dma, "DMA %d: Tick %8d: desc=%08x pa=%llx  dump @%s: %s\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, va, (long long int)pa, spot, s);
}
static void ARCH_FUNCTION(dump_dma_status)(dma_t *dma, const char * buf, uint32_t val)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    DMA_DEBUG(dma, "DMA %d: Tick %8d: %s = %08x State Internal: ", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, buf, val);
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_IDLE))    { DMA_DEBUG(dma, "IDLE ");      }
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING)) { DMA_DEBUG(dma, "RUNNING ");   }
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED))  { DMA_DEBUG(dma, "PAUSED ");    }
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR))   { DMA_DEBUG(dma, "ERROR ");     }

    if (udma_ctx->exception_status)                       { DMA_DEBUG(dma, "EXCEPTION "); }

    DMA_DEBUG(dma, "\tExternal: ");
    if (udma_ctx->ext_status == DM0_STATUS_RUN)           { DMA_DEBUG(dma, "RUNNING\n");  }
    if (udma_ctx->ext_status == DM0_STATUS_IDLE)          { DMA_DEBUG(dma, "IDLE\n");     }
    if (udma_ctx->ext_status == DM0_STATUS_ERROR)         { DMA_DEBUG(dma, "ERROR\n");    }
}
#endif

static uint32_t ARCH_FUNCTION(dma_data_read)(dma_t *dma, uint64_t pa, uint32_t len, uint8_t *buffer)
{
#if defined(CONFIG_USER_ONLY)
    uint32_t bytes = len;
    uint8_t *p = buffer;
    uint64_t pa_cur = pa;
    while (bytes)
    {
        dma_adapter_memread(dma, 0, pa_cur, p++, 1);
        pa_cur++;
        bytes--;
    }
#else
    thread_t* thread = dma_adapter_retrieve_thread(dma);
    hexagon_read_memory_block(thread, pa, len, buffer);
#endif
    return 1;
}

static uint32_t ARCH_FUNCTION(dma_data_write)(dma_t *dma, uint64_t pa, uint32_t len, uint8_t *buffer)
{
#if defined(CONFIG_USER_ONLY)
    uint32_t bytes = len;
    uint8_t *p = buffer;
    uint64_t pa_cur = pa;
    while (bytes)
    {
        dma_adapter_memwrite(dma, 0, pa_cur, p++, 1);
        pa_cur++;
        bytes--;
    }
#else
    thread_t* thread = dma_adapter_retrieve_thread(dma);
    hexagon_write_memory_block(thread, pa, len, buffer);
#endif
    return 1;
}





static int ARCH_FUNCTION(set_dma_error)(dma_t *dma, uint32_t addr, uint32_t reason, const char * reason_str)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_ERROR);
    udma_ctx->ext_status = DM0_STATUS_ERROR;
    udma_ctx->error_state_reason_captured   = 1;
    udma_ctx->error_state_address           = addr;
    udma_ctx->error_state_reason            = reason;
    dma->error_state_reason = reason;       // Back to dma_adapter for debug help
    dma_adapter_set_dm5(dma, addr);
    if (udma_ctx->dm2.error_exception_enable)
    {
        udma_ctx->exception_status = 1;
        dma_adapter_register_error_exception(dma, addr);
    }
    DMA_DEBUG(dma, "DMA %d: Tick %8d: Entering Error State : %s syndrome code=%d badva=%x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, reason_str, reason, addr);
    return 0;
}
static inline uint32_t ARCH_FUNCTION(badva_report)(uint64_t va, uint32_t extended) {
    return (extended) ? (uint32_t) (( (uint64_t) va>>32) | (va & ~0xFFF)) : (uint32_t)va;
}

static uint32_t ARCH_FUNCTION(check_dma_address)(dma_t *dma, uint32_t addr, dma_memtype_t  * memtype, uint32_t result)
{
    if (memtype->invalid_dma) {
        ARCH_FUNCTION(set_dma_error)(dma, addr, DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_ADDRESS, "unsupported address for dma transaction");
        result = 0;
    }
    return result;
}

void ARCH_FUNCTION(dma_force_error)(dma_t *dma, uint32_t addr, uint32_t reason) {
    ARCH_FUNCTION(set_dma_error)(dma, addr, reason, "forced");
}
DMA_DEBUG_ONLY(static inline void ARCH_FUNCTION(verif_descriptor_peek)(dma_t *dma, uint32_t desc_va)
{
#ifdef VERIFICATION
    const uint32_t xlate_va = desc_va & 0xFFFFFFF0; // Force alignment to 16B for translation
    dma_memaccess_info_t dma_mem_access;
    uint64_t pa = 0;
    if (!dma_adapter_xlate_desc_va(dma, xlate_va, &pa, &dma_mem_access))
    {
        DMA_DEBUG(dma, "DMA %d: Tick %8d: verif_descriptor_peek desc=%08x translation error\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, xlate_va);
        return;
    }
    HEXAGON_DmaDescriptor_t desc;
    ARCH_FUNCTION(dma_data_read)(dma, pa, sizeof(HEXAGON_DmaDescriptor_t), (uint8_t *)&desc);
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_desc)(dma, (void*) &desc, xlate_va , pa,  "verif_descriptor_peek"););
    DMA_DEBUG(dma, "DMA %d: Tick %8d: desc=%08x raw word[0]: %08x ", dma->num,  ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, xlate_va, desc.word[0]);

    DMA_DEBUG(dma,  "word[1]: %08x ", desc.word[1]);
    DMA_DEBUG(dma,  "word[2]: %08x ", desc.word[2]);
    DMA_DEBUG(dma,  "word[3]: %08x ", desc.word[3]);

    if (desc.common.descSize == 1 ){
        DMA_DEBUG(dma,  "word[4]: %08x ", desc.word[4]);
        DMA_DEBUG(dma,  "word[5]: %08x ", desc.word[5]);
        DMA_DEBUG(dma,  "word[6]: %08x ", desc.word[6]);
        DMA_DEBUG(dma,  "word[7]: %08x ", desc.word[7]);
    }
    DMA_DEBUG(dma,  "\n");

#endif
}
)

void ARCH_FUNCTION(dma_target_desc)(dma_t *dma, uint32_t addr, uint32_t width, uint32_t height, uint32_t no_transfer) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    udma_ctx->target.va = addr;
    udma_ctx->target.valid = 1;
    udma_ctx->target.no_transfer  = no_transfer;
    udma_ctx->target.bytes_to_transfer = width;
    udma_ctx->target.lines_to_transfer = height;

    DMA_DEBUG(dma, "DMA %d: Tick %8d: Received pause target from TB desc=%08x width=%d height=%d (no transfers case=%d)\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, addr, width, height, no_transfer);
    DMA_DEBUG_ONLY(ARCH_FUNCTION(verif_descriptor_peek)(dma, addr););
}

int ARCH_FUNCTION(dma_check_verif_pause)(dma_t *dma);
int ARCH_FUNCTION(dma_check_verif_pause)(dma_t *dma)
{
#ifdef VERIFICATION
    if (!dma_test_gen_mode(dma))
    {
        udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
        dma_decoded_descriptor_t * current_desc = &udma_ctx->active;
        dma_decoded_descriptor_t * pause_desc  = &udma_ctx->target;
        int pause_enable = 0;
        if ( pause_desc->valid)
        {
            if (pause_desc->va == current_desc->va)
            {
                const uint32_t current_bytes = current_desc->bytes_to_transfer;
                const uint32_t final_bytes = pause_desc->bytes_to_transfer;
                if (pause_desc->no_transfer) {
                   // DMA_DEBUG(dma, "DMA %d: Tick %8d: Pause Decriptor Reached desc=%08x. Pausing before any transfer.\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, pause_desc->va);
                    pause_enable = 1;
                }
                else
                {
                    //DMA_DEBUG(dma, "DMA %d: Tick %8d: Pause Decriptor Reached desc=%08x. ", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, pause_desc->va);
                    if (current_desc->desc.common.descSize)
                    {
                        const uint32_t final_lines   = pause_desc->lines_to_transfer;
                        const uint32_t current_lines = current_desc->lines_to_transfer;
                        pause_enable = (current_lines <= final_lines) && (current_bytes <= final_bytes);
                        if (pause_enable) {
                            DMA_DEBUG(dma, "DMA %d: Tick %8d: desc=%08x Pause Condition %s (bytes remaining=%d pause at %d bytes. lines to transfer=%d pause at %d lines) \n",  dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, pause_desc->va, pause_enable ? "!!!HIT!!!" : "Not Ready!", current_bytes, final_bytes, current_lines, final_lines);
                        }
                    }
                    else
                    {
                        pause_enable = ( current_bytes <= final_bytes );
                        if (pause_enable) {
                            DMA_DEBUG(dma, "DMA %d: Tick %8d:  desc=%08x Pause Condition %s (bytes remaining=%d pause at %d bytes.) \n",  dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, pause_desc->va, pause_enable ? "!!!HIT!!!" : "Not Ready", current_bytes, final_bytes);
                        }
                    }

                }

                if (pause_enable) {
                    udma_ctx->target.no_transfer = 0;
                    udma_ctx->target.va = 0;
                    udma_ctx->target.valid = 0;
                    udma_ctx->pause_va = current_desc->va;
                    udma_ctx->pause_va_valid = 1;
                    return pause_enable;
                }
            }
        }
    }
#endif
    return 0;
}



void ARCH_FUNCTION(dma_stop)(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_IDLE);
    udma_ctx->ext_status = DM0_STATUS_IDLE;
    udma_ctx->active.va = 0;
    udma_ctx->active.valid = 0;
    udma_ctx->active.exception = DMA_DESC_NOT_DONE;
    udma_ctx->active.state = DMA_DESC_NOT_DONE;
    udma_ctx->target.va = 0;
    udma_ctx->target.valid = 0;
    DMA_DEBUG(dma, "DMA %d: Tick %8d: dma_stop\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count);
}

static void ARCH_FUNCTION(dma_word_store)(dma_t *dma, vaddr_t desc_va, vaddr_t pc, vaddr_t va, paddr_t pa) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    if (udma_ctx->timing_on) {
        ARCH_FUNCTION(uarch_dma_log_word_store)(dma, desc_va, pc, va, pa);
    }
}

static void ARCH_FUNCTION(update_descriptor)(dma_t *dma, dma_decoded_descriptor_t * entry)
{
    HEXAGON_DmaDescriptor_t *desc = (HEXAGON_DmaDescriptor_t*) &entry->desc;
    const uint64_t pa = entry->pa;
    const uint32_t partial_desc = (entry->pause || entry->exception);
    const uint32_t type1 = (desc->common.descSize == DESC_DESCTYPE_TYPE1);
    //DMA_DEBUG(dma, "DMA %d: Tick %8d: descriptor=%x update at pa=%llx pause=%d exception=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, entry->va, (long long int)pa, entry->pause, entry->exception);

    ARCH_FUNCTION(dma_data_write)(dma, pa + 4, 4, (uint8_t *)&desc->word[1]);  //ptr->dstate_order_bypass_comp_desctype_length
    ARCH_FUNCTION(dma_word_store)(dma, entry->va, entry->pc, entry->va, entry->pa);

    if (partial_desc && !entry->constant_fill) {
        ARCH_FUNCTION(dma_data_write)(dma, pa + 8,  4, (uint8_t *)&desc->common.srcAddress);
    }
    if (partial_desc && !entry->l2fetch) {
        ARCH_FUNCTION(dma_data_write)(dma, pa + 12, 4, (uint8_t *)&desc->common.dstAddress);
    }
    if (partial_desc && type1 && entry->extended_va) {
        uint8_t byte = desc->type1.srcUpperAddr;
        ARCH_FUNCTION(dma_data_write)(dma, pa + 17, 1, (uint8_t *)&byte);
    }
    if (partial_desc && type1 && entry->extended_va) {
        uint8_t byte = desc->type1.dstUpperAddr;
        ARCH_FUNCTION(dma_data_write)(dma, pa + 18, 1, (uint8_t *)&byte);
    }
    if (!partial_desc && type1) {
        if (entry->wide) {
            ARCH_FUNCTION(dma_data_write)(dma, pa + offsetof(HEXAGON_DmaDescriptor2D_t, width_wide_height_lo[0]), 3, (uint8_t *)&desc->type1.width_wide_height_lo[0]);
        } else {
            ARCH_FUNCTION(dma_data_write)(dma, pa + offsetof(HEXAGON_DmaDescriptor2D_t, width), 2, (uint8_t *)&desc->type1.width);
        }
    }
    if (type1) {
        if (entry->wide) {  // height bytes split
            ARCH_FUNCTION(dma_data_write)(dma, pa + offsetof(HEXAGON_DmaDescriptor2D_t, width_wide_height_lo[3]), 1, (uint8_t *)&desc->type1.width_wide_height_lo[3]);
            ARCH_FUNCTION(dma_data_write)(dma, pa + offsetof(HEXAGON_DmaDescriptor2D_t, height_hi_srcStride_wide[0]), 1, (uint8_t *)&desc->type1.height_hi_srcStride_wide[0]);
        } else {
            ARCH_FUNCTION(dma_data_write)(dma, pa + offsetof(HEXAGON_DmaDescriptor2D_t, height), 2, (uint8_t *)&desc->type1.height);
        }
    }
    if (type1 && !(partial_desc && entry->constant_fill)) {
        ARCH_FUNCTION(dma_data_write)(dma, pa + 28, 2, (uint8_t *)&desc->type1.srcWidthOffset);
    }
    if (type1 && !(partial_desc && entry->l2fetch)) {
        ARCH_FUNCTION(dma_data_write)(dma, pa + 30, 2, (uint8_t *)&desc->type1.dstWidthOffset);
    }
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_desc)(dma, (void*) desc, entry->va, pa, "updated descriptor in memory"););
    dma_adapter_descriptor_end(dma, entry->id, entry->va, (uint32_t*)desc, entry->pause, entry->exception);
}


static uint64_t ARCH_FUNCTION(retrieve_descriptor)(dma_t *dma, uint32_t desc_va, uint8_t *pdesc, uint32_t peek)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dma_decoded_descriptor_t *desc_state = (dma_decoded_descriptor_t *) &udma_ctx->active;
    uint32_t transfer_size = sizeof(dma_descriptor_type0_t);
    const uint32_t xlate_va = desc_va & 0xFFFFFFF0; // Force alignment to 16B for translation
    dma_memaccess_info_t dma_mem_access;
    uint64_t pa = 0;
    if (!dma_adapter_xlate_desc_va(dma, xlate_va, &pa, &dma_mem_access))
    {
        udma_ctx->exception_status = 1;
        if (ARCH_FUNCTION(check_dma_address)(dma, xlate_va, &dma_mem_access.memtype, 1) == 0) {
            DMA_DEBUG(dma, "DMA %d: Tick %8d: desc=%08x retrieve error memtype\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, xlate_va);
            return 0;
        }
        DMA_DEBUG(dma, "DMA %d: Tick %8d: desc=%08x retrieve error\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, xlate_va);
        return 0;
    }
    else
    {
        const HEXAGON_DmaDescriptor_t * desc = (HEXAGON_DmaDescriptor_t *) pdesc;
        ARCH_FUNCTION(dma_data_read)(dma, pa, transfer_size, pdesc);
        uint32_t type0 = desc->common.descSize == 0;
        uint32_t type1 = desc->common.descSize == 1;;

        // Valid Type check
        if (!type0 && !type1)  {
            uint32_t syndrome_addr = xlate_va;
            uint32_t syndrome_code = DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE;
            ARCH_FUNCTION(set_dma_error)(dma, syndrome_addr, syndrome_code, "invalid descriptor type");
            return 0;
        }
        // 2D alignment and load
        if (type1 && (pa & 0x1F)) {
            uint32_t syndrome_addr = xlate_va;
            uint32_t syndrome_code = DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE;
            ARCH_FUNCTION(set_dma_error)(dma, syndrome_addr, syndrome_code, "invalid 32-byte descriptor alignment");
            return 0;
        } else if (type1) {
            uint32_t remainder_size = sizeof(dma_descriptor_type1_t)-transfer_size;
            ARCH_FUNCTION(dma_data_read)(dma, pa+transfer_size, remainder_size, pdesc+transfer_size);
            DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_desc)(dma, (void*) pdesc, xlate_va , pa,  "read 32-byte 2d descriptor"););
        } else {
            DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_desc)(dma, (void*) pdesc, xlate_va , pa,  "read 16-byte linear descriptor"););
        }

        // Use ExtendVA when not supported
        desc_state->extended_va = type1 && ((desc->type1.type == DMA_DESC_38BIT_VA_TYPE)
                                || (desc->type1.type == DMA_DESC_38BIT_VA_GATHER_TYPE)
                                || (desc->type1.type == DMA_DESC_38BIT_VA_WIDE_TYPE));
                uint32_t syndrome_error = type1 && desc_state->extended_va && !dma_adapter_has_extended_tlb(dma);
        if (syndrome_error) {
            uint32_t syndrome_addr = (xlate_va & ~0xF) | 0x1;
            uint32_t syndrome_code = DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE;
            ARCH_FUNCTION(set_dma_error)(dma, syndrome_addr, syndrome_code , "38-bit va not supported");
            return 0;
        }

        // DLBC
        uint32_t dlbc_enabled = udma_ctx->dm2.dlbc_enable && (type0 || (type1 && ((desc->type1.type == DMA_DESC_32BIT_VA_TYPE)
                                || (desc->type1.type ==DMA_DESC_38BIT_VA_TYPE)
                                || (desc->type1.type ==DMA_DESC_32BIT_VA_WIDE_TYPE))));

        if (dlbc_enabled)
        {
            //DMA_DEBUG(dma, "DMA %d: Tick %8d: descriptor retrieve error alginment src=%d dst=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,
            //((desc->common.srcAddress & 255) == 0) , ((desc->common.dstAddress & 255) == 0));

            // If type1 set error
            syndrome_error |= type1 && (desc->common.dstDlbc||desc->common.srcDlbc);

            // Target must be bypass and DLBC
            syndrome_error |= !desc->common.srcBypass && desc->common.srcDlbc;
            syndrome_error |= !desc->common.dstBypass && desc->common.dstDlbc;

            // SRC or DST is DLBC and src/dst are not 256-byte aligned
            syndrome_error |= (desc->common.dstDlbc || desc->common.srcDlbc) && !(((desc->common.srcAddress & 255) == 0) && ((desc->common.dstAddress & 255) == 0));

            // SRC or DST is DLBC and length is not a multiple of 256
            syndrome_error |= type0 && (desc->common.dstDlbc || desc->common.srcDlbc) && ((desc->type0.length & 255) != 0);


            // Now redundant // syndrome_error |= (desc->common.srcDlbc || desc->common.dstDlbc) && ( (desc->common.srcAddress & 255) != (desc->common.dstAddress & 255) );
            // DST is DLBC and offsets do not match
            //syndrome_error |= type1 && desc->common.dstDlbc  &&  ((desc->type1.srcWidthOffset != desc->type1.dstWidthOffset) );

            // DST is DLBC and offsets do not match
            //syndrome_error |= type1 && (desc->common.dstDlbc||desc->common.srcDlbc) && (desc->type1.height > 1);

            if (syndrome_error) {
                uint32_t syndrome_addr = xlate_va;
                uint32_t syndrome_code = DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_COMP_MODE;
                ARCH_FUNCTION(set_dma_error)(dma, syndrome_addr, syndrome_code , "DLBC descriptor error");
                return 0;
            }
        }
        // ROI for src/dst stride vs list, only checks some type of descriptors
        uint32_t exclude_list = 0;
                desc_state->wide = type1 && ((desc->type1.type == DMA_DESC_32BIT_VA_WIDE_TYPE) || (desc->type1.type == DMA_DESC_38BIT_VA_WIDE_TYPE));
        exclude_list =  type1 && (desc->type1.type == DMA_DESC_32BIT_VA_CONSTANT_FILL_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_32BIT_VA_GATHER_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_38BIT_VA_GATHER_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_32BIT_VA_EXPANSION_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_32BIT_VA_COMPRESSION_TYPE);
        syndrome_error |= (type1 && !exclude_list) && (get_width((HEXAGON_DmaDescriptor2D_t *)(&desc->type1), desc_state) > get_src_stride((HEXAGON_DmaDescriptor2D_t *)(&desc->type1), desc_state));

        exclude_list =  type1 && (desc->type1.type == DMA_DESC_32BIT_VA_L2FETCH_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_32BIT_VA_GATHER_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_38BIT_VA_GATHER_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_32BIT_VA_EXPANSION_TYPE);
        exclude_list |= type1 && (desc->type1.type == DMA_DESC_32BIT_VA_COMPRESSION_TYPE);
        syndrome_error |= (type1 && !exclude_list) && (get_width((HEXAGON_DmaDescriptor2D_t *)(&desc->type1), desc_state) > get_dst_stride((HEXAGON_DmaDescriptor2D_t *)(&desc->type1), desc_state));

        // Padded source/dest not tested, not supported by RTL

        // check done bit
        syndrome_error |= desc->common.done;
        if (syndrome_error) {
            uint32_t syndrome_addr = xlate_va;
            uint32_t syndrome_code = DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_ROI_ERROR;
            ARCH_FUNCTION(set_dma_error)(dma, syndrome_addr, syndrome_code , "ROI Error");
           return 0;
        }
#ifdef VERIFICATION
        if (!peek) { // signal testbench that we're starting this desciptor
            dma_adapter_descriptor_start(dma, 0, desc_va, (uint32_t*)pdesc);
        }
#endif
    }
    return pa;
}




static uint32_t ARCH_FUNCTION(start_dma)(dma_t *dma, uint32_t new_dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    /* Deal with (fetching) a new descriptor that given by "new".
       Two paths: 1) from the user side via dmstart, dmlink and dmresume, or
       2) from DMA chaining via \transfer_done(). */
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->start_dma new", new_dma););
    dma_decoded_descriptor_t * desc_state = (dma_decoded_descriptor_t *) &udma_ctx->active;
    desc_state->pa = ARCH_FUNCTION(retrieve_descriptor)(dma, new_dma, (uint8_t*)&desc_state->desc, 0);
    udma_ctx->exception_va = 0;

    if (!desc_state->pa)
    {
        DMA_DEBUG(dma, "DMA %d: Tick %8d: <-start_dma exception desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, new_dma);
        udma_ctx->exception_status = 1;
        desc_state->va = new_dma;
        desc_state->valid = 1;
        udma_ctx->active.exception = 1;
        // Record is we took a recoverable or unrecoverable exception
        desc_state->state = (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR)) ? DMA_DESC_EXCEPT_ERROR : DMA_DESC_EXCEPT_RUNNING ;
        udma_ctx->exception_va = new_dma;
		dma_adapter_descriptor_end(dma, -1, new_dma, (uint32_t*)&desc_state->desc, 0, 1);
        return 0;
    }

    // The translation is ok; get the descriptor PA.
    HEXAGON_DmaDescriptor_t * desc = (HEXAGON_DmaDescriptor_t *) &desc_state->desc;
    desc_state->va = new_dma;
    desc_state->valid = 1;
    // Initialize condition

    // Dump the descriptor we are about to process
    DMA_DEBUG(dma, "DMA %d: Tick %8d: Start DMA desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va);

    desc_state->va = new_dma;
    desc_state->valid = 1;
    desc_state->state = DMA_DESC_NOT_DONE;
    desc_state->pause = 0;
    desc_state->exception = 0;
        /* set in retrieve_descriptor() */
        // desc_state->extended_va = 0;
        // desc_state->wide = 0;

    desc_state->l2fetch = 0;
    desc_state->gather = 0;
    desc_state->constant_fill = 0;
    desc_state->expansion   = 0;
    desc_state->compression = 0;

    desc_state->uwbc_a_state = 0;
    desc_state->desc.common.done = DESC_DSTATE_INCOMPLETE;

    if (desc->common.descSize == DESC_DESCTYPE_TYPE0) {
        desc_state->bytes_to_transfer = desc->type0.length;
        desc_state->lines_to_transfer = 0;
        desc_state->desc.type1.srcStride = desc->type0.srcAddress;  // This is a one-off hack for UWBC-A error checking
        desc_state->desc.type1.dstStride = desc->type0.dstAddress;  // Store initial addresses in unused field of struct, could check in the retrieve function instead ...
    }
    else if (desc_state->desc.common.descSize == DESC_DESCTYPE_TYPE1)
    {
        dma_adapter_pmu_increment(dma, DMA_PMU_PADDING_DESCRIPTOR, (desc->type1.transform != 0));

        /* set in retrieve_descriptor() */
        /* desc_state->extended_va = ((desc->type1.type == DMA_DESC_38BIT_VA_TYPE) */
        /*                                                   || (desc->type1.type == DMA_DESC_38BIT_VA_GATHER_TYPE) */
        /*                                                   || (desc->type1.type == DMA_DESC_38BIT_VA_WIDE_TYPE)); */

        /* set in retrieve_descriptor() */
        // desc_state->wide = (desc->type1.type == DMA_DESC_32BIT_VA_WIDE_TYPE) || (desc->type1.type == DMA_DESC_38BIT_VA_WIDE_TYPE);
        desc_state->gather      = (desc->type1.type==DMA_DESC_32BIT_VA_GATHER_TYPE) || (desc->type1.type==DMA_DESC_38BIT_VA_GATHER_TYPE) ;
        desc_state->l2fetch     = (desc->type1.type==DMA_DESC_32BIT_VA_L2FETCH_TYPE);
        desc_state->constant_fill = (desc->type1.type==DMA_DESC_32BIT_VA_CONSTANT_FILL_TYPE);
        desc_state->expansion   = (desc->type1.type==DMA_DESC_32BIT_VA_EXPANSION_TYPE);
        desc_state->compression = (desc->type1.type==DMA_DESC_32BIT_VA_COMPRESSION_TYPE);

        desc_state->src_roi_width      = get_width(&desc->type1, desc_state);
        desc_state->dst_roi_width      = get_width(&desc->type1, desc_state);
        desc_state->bytes_to_transfer  = get_width(&desc->type1, desc_state) - ((desc_state->constant_fill) ? get_dst_widthoffset((HEXAGON_DmaDescriptor2D_t *)(&desc->type1), desc_state) : get_src_widthoffset((HEXAGON_DmaDescriptor2D_t *)(&desc->type1), desc_state));
        desc_state->lines_to_transfer  = get_height(&desc->type1, desc_state);


        if (desc_state->expansion || desc_state->compression)  {
            uint32_t rd_block_size = desc->type1.blockSize + (desc_state->expansion ? 0 : desc->type1.blockDelta);
            uint32_t wr_block_size = desc->type1.blockSize + (desc_state->compression ? 0 : desc->type1.blockDelta);
            uint32_t blocks = (rd_block_size==0) ? 0 : (get_width(&desc->type1, desc_state))/rd_block_size;
            uint32_t remainder = get_width(&desc->type1, desc_state);
            if (blocks)
                remainder -= blocks*rd_block_size;

            desc_state->dst_roi_width         = remainder + blocks*wr_block_size;

            DMA_DEBUG(dma, "DMA %d: Tick %8d: Expansion/Compression DMA desc=%08x dst_roi_width=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va, desc_state->dst_roi_width);
        }


    }

    uint32_t src_alignment = (desc->type0.srcAddress & 0x255);
    uint32_t dst_alignment = (desc->type0.dstAddress & 0x255);
    uint32_t unaligned_desc = src_alignment != dst_alignment;
    dma_adapter_pmu_increment(dma, DMA_PMU_UNALIGNED_DESCRIPTOR, unaligned_desc);
    if (unaligned_desc) {
        uint32_t count = 0;
        while (src_alignment) {
           src_alignment += dma_transfer_size_lut[src_alignment & 0xFF] + 1;
           src_alignment &= 0xFF;
           count++;
        }
        if (desc_state->desc.common.descSize == DESC_DESCTYPE_TYPE1) count *= desc_state->lines_to_transfer;
        dma_adapter_pmu_increment(dma, DMA_PMU_UNALIGNED_RD, count);
        count = 0;
        while (dst_alignment) {
           dst_alignment += dma_transfer_size_lut[dst_alignment & 0xFF] + 1;
           dst_alignment &= 0xFF;
           count++;
        }
        if (desc_state->desc.common.descSize == DESC_DESCTYPE_TYPE1) count *= desc_state->lines_to_transfer;
        dma_adapter_pmu_increment(dma, DMA_PMU_UNALIGNED_WR, count);
    }

    if (udma_ctx->timing_on)
        udma_ctx->init_desc = udma_ctx->active;  // Copy for Timing model

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
    udma_ctx->ext_status = DM0_STATUS_RUN;

    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-start_dma",  desc_state->va););
    return 1;
}
static void ARCH_FUNCTION(descriptor_done_callback)(void * desc_void)
{
    desc_tracker_entry_t * entry = (desc_tracker_entry_t *) desc_void;

    // When we do the callback ....
    // Check if the descriptor was sent with a recoverable exception or not
    // If not, we assume it's done and we'll pop it from the work queue
    // On a recoverable exception, don't pop it from the queue in timing mode
    // Assume that the descriptor will restart on TLBW
    dma_t *dma = (dma_t *) entry->dma;
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    if (DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING) && (entry->desc.state == DMA_DESC_EXCEPT_RUNNING)) {
        DMA_DEBUG(dma, "DMA %d: Tick %8d: callback desc=%08x ended with exception\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, entry->desc.va);
    } else {
        entry->desc.state = DMA_DESC_DONE;
        DMA_DEBUG(dma, "DMA %d: Tick %8d: callback desc=%08x done pause=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, entry->desc.va, entry->desc.pause);
    }
}




void ARCH_FUNCTION(dma_update_descriptor_done)(dma_t *dma, dma_decoded_descriptor_t * entry)
{
    HEXAGON_DmaDescriptor_t *desc = (HEXAGON_DmaDescriptor_t*) &entry->desc;
    if ((entry->state == DMA_DESC_DONE) && !entry->exception && !entry->pause) {
        desc->common.done = DESC_DSTATE_COMPLETE;
        dma_adapter_pmu_increment(dma, DMA_PMU_DESC_DONE, 1);
    }
    DMA_DEBUG(dma, "DMA %d: Tick %8d: desc=%08x Done=%d Pause=%d Exception=%d and Update\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)entry->va, (int)entry->state, (int)entry->pause, (int)entry->exception);
    ARCH_FUNCTION(update_descriptor)(dma, entry);
}




// can't make function with bitfields
#define INC_64BIT_ADDRESS(VA, UPPER_VA, STRIDE, USE_EXTENDED)\
    { uint64_t _tmp0 = (uint64_t)VA + (USE_EXTENDED ? (((uint64_t)UPPER_VA) << 32) : 0);\
    uint64_t _tmp1 = _tmp0 + (uint64_t)STRIDE;\
    VA    = (uint32_t)_tmp1;\
    UPPER_VA = USE_EXTENDED ? (uint32_t)(_tmp1 >> 32) : UPPER_VA; \
    } \


#define DMA_XLATE(DMA,VA, PA, MEMACCESS, SIZE, TYPE, EXTENDED, EXCEPT_VTCM, IS_DLBC) \
    if (!dma_adapter_xlate_va(DMA, VA, &PA, &MEMACCESS, SIZE, TYPE, EXTENDED, EXCEPT_VTCM, IS_DLBC, ((udma_ctx_t *)DMA->udma_ctx)->active.desc.common.descSize == DESC_DESCTYPE_TYPE0 ? 0 : ((udma_ctx_t *)DMA->udma_ctx)->active.desc.type1.rd_alloc == DESC_RDALLOC_FORGET)) { \
    ((udma_ctx_t *)DMA->udma_ctx)->exception_status = 1;\
    if (MEMACCESS.memtype.invalid_dma) {\
        uint32_t syndrome_code = (EXCEPT_VTCM) ? DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_BYPASS_MODE : DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_ADDRESS;\
        ARCH_FUNCTION(set_dma_error)(DMA, ARCH_FUNCTION(badva_report)(VA,EXTENDED), syndrome_code, "unsuppored src/dst address");\
    }\
    return 0;\
}\

#define CHECK_UWBCA_STATE(DMA, DESC_STATE, NEW_STATE) \
if (DESC_STATE->uwbc_a_state==UWBCA_NEW) {\
    DESC_STATE->uwbc_a_state = NEW_STATE; /* upadate state, once we're in a new state we can't change it */\
} else if (DESC_STATE->uwbc_a_state != NEW_STATE) {\
    int32_t syndrome_address = DESC_STATE->va & ~0xF;\
    syndrome_address |= ((DESC_STATE->uwbc_a_state & 0xFF) != (NEW_STATE & 0xFF)) ? 5 : 13;\
    ARCH_FUNCTION(set_dma_error)(DMA, syndrome_address, DMA_CFG0_SYNDROME_CODE___UBWC_D, "uwbc aperature error");\
    return 0;\
}

#define CHECK_UWBC_16B(DMA, DESC_STATE, SRC_PA, DST_PA)\
if (((udma_ctx_t *)DMA->udma_ctx)->uwbc_a_en) {\
    dm3_reg_t dm3 = ((udma_ctx_t *)DMA->udma_ctx)->dm3;\
    uint32_t aperature_src = dm3.ubwc_support_enable && ( (dm3.ubwc_mask & (SRC_PA >> 32)) == (dm3.ubwc_mask & dm3.ubwc_range) );\
    uint32_t aperature_dst = dm3.ubwc_support_enable && ( (dm3.ubwc_mask & (DST_PA >> 32)) == (dm3.ubwc_mask & dm3.ubwc_range) );\
    uint32_t uwbca_state_new = (aperature_src ? UWBCA_IN : UWBCA_OUT) |  ((aperature_dst ? UWBCA_IN : UWBCA_OUT) << 8); \
    if (aperature_src || aperature_dst)  {\
        int32_t error = 0;\
        int32_t syndrome_address = DESC_STATE->va & ~0xF;\
        if ( aperature_src && DESC_STATE->desc.type0.srcDlbc) { \
            error = 1; \
            syndrome_address |= 3;\
        } else if ( aperature_dst && DESC_STATE->desc.type0.dstDlbc) { \
            error = 1; \
            syndrome_address |= 11;\
        } else if ( aperature_src && !DESC_STATE->desc.type0.srcBypass) { \
            error = 1; \
            syndrome_address |= 4;\
        } else if ( aperature_dst && !DESC_STATE->desc.type0.dstBypass) { \
            error = 1; \
            syndrome_address |= 12;\
        } else if ( (SRC_PA & 0xFF) != (DST_PA & 0xFF) ) { \
            error = 1; \
            syndrome_address |= aperature_src ? 1 : 9;\
        }  else if ( aperature_src && ((DESC_STATE->desc.type1.srcStride & 0xFF) != 0))  { /* reusing unused members for address */\
            error = 1; \
            syndrome_address |= 6;\
        } else if ( aperature_dst && ((DESC_STATE->desc.type1.dstStride & 0xFF) != 0))  { \
            error = 1; \
            syndrome_address |= 14;\
        }\
        if(error) {\
            ARCH_FUNCTION(set_dma_error)(DMA, syndrome_address, DMA_CFG0_SYNDROME_CODE___UBWC_D, "uwbc aperature error");\
            return 0;\
        }\
    } \
    CHECK_UWBCA_STATE(DMA, DESC_STATE, uwbca_state_new)\
}

#define CHECK_UWBC_32B(DMA, DESC_STATE, SRC_PA, DST_PA)\
if (((udma_ctx_t *)DMA->udma_ctx)->uwbc_a_en) {\
    dm3_reg_t dm3 = ((udma_ctx_t *)DMA->udma_ctx)->dm3;\
    uint32_t aperature_src = dm3.ubwc_support_enable && ( (dm3.ubwc_mask & (SRC_PA >> 32)) == (dm3.ubwc_mask & dm3.ubwc_range) );\
    uint32_t aperature_dst = dm3.ubwc_support_enable && ( (dm3.ubwc_mask & (DST_PA >> 32)) == (dm3.ubwc_mask & dm3.ubwc_range) );\
    uint32_t uwbca_state_new = (aperature_src ? UWBCA_IN : UWBCA_OUT) |  ((aperature_dst ? UWBCA_IN : UWBCA_OUT) << 8); \
    if (aperature_src || aperature_dst)  {\
        int32_t error = 0;\
        int32_t syndrome_address = DESC_STATE->va & ~0xF;\
        aperature_src = (DESC_STATE->desc.type1.type == 8) ? 0 :  aperature_src;\
        aperature_dst = (DESC_STATE->desc.type1.type == 3) ? 0 :  aperature_dst;\
        if ( aperature_src && DESC_STATE->desc.type1.srcDlbc) { \
            error = 1; \
            syndrome_address |= 3;\
        } else if ( aperature_dst && DESC_STATE->desc.type1.dstDlbc) { \
            error = 1; \
            syndrome_address |= 11;\
        } else if ( aperature_src && !DESC_STATE->desc.type1.srcBypass) { \
            error = 1; \
            syndrome_address |= 4;\
        } else if ( aperature_dst && !DESC_STATE->desc.type1.dstBypass) { \
            error = 1; \
            syndrome_address |= 12;\
        } else if (aperature_src && (DESC_STATE->desc.type1.type >= 3)) { \
            error = 1; \
            syndrome_address |= 0;\
        } else if (aperature_dst && (DESC_STATE->desc.type1.type >= 3)) { \
            error = 1; \
            syndrome_address |= 8;\
        } else if ( ((DESC_STATE->desc.type1.type != 3) && (DESC_STATE->desc.type1.type != 8))  && (((DESC_STATE->desc.type1.srcAddress & 0xFF) != (DESC_STATE->desc.type1.dstAddress & 0xFF)) || ((DESC_STATE->desc.type1.srcStride & 0xFF) != (DESC_STATE->desc.type1.dstStride & 0xFF))) ) { \
            error = 1; \
            syndrome_address |= aperature_src ? 1 : 9;\
        } else if ( DESC_STATE->desc.type1.transform != 0) { \
            error = 1; \
            syndrome_address |= aperature_src ? 2 : 10;\
        } else if ( aperature_src && ((DESC_STATE->desc.type1.srcAddress & 0xFF) != 0))  { \
            error = 1; \
            syndrome_address |= 6;\
        } else if ( aperature_dst && ((DESC_STATE->desc.type1.dstAddress & 0xFF) != 0))  { \
            error = 1; \
            syndrome_address |= 14;\
        }\
        if(error) {\
            ARCH_FUNCTION(set_dma_error)(DMA, syndrome_address, DMA_CFG0_SYNDROME_CODE___UBWC_D, "uwbc aperature error");\
            return 0;\
        }\
    }\
    CHECK_UWBCA_STATE(DMA, DESC_STATE, uwbca_state_new)\
}

#define ADD_38BIT_UPPER_VA(VA, OFFSET, EXTENDED)\
if (EXTENDED) { VA += ((uint64_t)OFFSET) << 32; }


#define DMA_BUFFER_FILL_LOAD(DMA,  VA, PA, LEN, BUFFER)\
ARCH_FUNCTION(dma_data_read)(DMA, PA, LEN, BUFFER); \
if (DMA->verbosity >= 5) {\
    DMA_DEBUG(DMA, "DMA %d: Tick %8d: rd transaction desc=%08x src va=%llx pa=%llx len=%u data=0x", DMA->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, ((udma_ctx_t *)dma->udma_ctx)->active.va, (long long int)VA, (long long int)PA, LEN);\
    for(int32_t i = LEN; i > 0; i--) { DMA_DEBUG(DMA, "%02x", (unsigned int)BUFFER[i-1]); }\
    DMA_DEBUG(DMA, "\n");}\

#define DMA_BUFFER_FILL_CONSTANT(DMA,  VA, PA, LEN, BUFFER)\
for(int32_t i = LEN; i > 0; i--) { BUFFER[i-1] = ((udma_ctx_t *)dma->udma_ctx)->active.desc.type1.fillValue; }

#define DMA_BUFFER_FILL_NONE(DMA, VA, PA, LEN, BUFFER)


#define DMA_BUFFER_WRITE(DMA, VA, PA, LEN, BUFFER)\
if (dma->verbosity >= 5) {\
    DMA_DEBUG(DMA, "DMA %d: Tick %8d: wr transaction desc=%08x dst va=%llx pa=%llx len=%u data=0x", DMA->num,  ((udma_ctx_t *)DMA->udma_ctx)->dma_tick_count, ((udma_ctx_t *)dma->udma_ctx)->active.va, (long long int)VA,  (long long int)PA, LEN);\
    for(int32_t i = LEN; i > 0; i--) { DMA_DEBUG(DMA, "%02x", (unsigned int)BUFFER[i-1]); }\
    DMA_DEBUG(DMA, "\n");}\
ARCH_FUNCTION(dma_data_write)(DMA, PA, LEN, BUFFER);

#define DMA_BUFFER_WRITE_NONE(DMA, VA, PA, LEN, BUFFER)


uint32_t ARCH_FUNCTION(step_linear_descriptor)(dma_t *dma);
uint32_t ARCH_FUNCTION(step_linear_descriptor)(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    const uint32_t dlbc_src = udma_ctx->dm2.dlbc_enable && udma_ctx->active.desc.common.srcDlbc;
    const uint32_t dlbc_dst = udma_ctx->dm2.dlbc_enable && udma_ctx->active.desc.common.dstDlbc;
    //DMA_DEBUG(dma, "DMA %d: Tick %8d: step_linear_descriptor for %x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->active.va);
    uint32_t desc_transfer_done = 0;
    while(!desc_transfer_done)
    {
        dma_decoded_descriptor_t * desc_state = (dma_decoded_descriptor_t *) &udma_ctx->active;
        HEXAGON_DmaDescriptorLinear_t * desc = (HEXAGON_DmaDescriptorLinear_t*) &desc_state->desc;
        if(ARCH_FUNCTION(dma_check_verif_pause)(dma)) {
            desc_state->pause = 1;
            return 1;
        }
        if (desc_state->bytes_to_transfer) {
            uint64_t src_va = (uint64_t)desc->srcAddress;
            uint64_t dst_va = (uint64_t)desc->dstAddress;
            uint32_t bytes_to_transfer = desc->length;
            uint32_t transfer_size = dma_transfer_size_lut[(dst_va|src_va) & 0xFF] + 1;
            transfer_size = transfer_size < bytes_to_transfer ? transfer_size : (is_power_of_two(bytes_to_transfer) ? bytes_to_transfer : lower_power_of_two(bytes_to_transfer));


            uint64_t src_pa = 0, dst_pa = 0;
            dma_memaccess_info_t dma_mem_access;
            DMA_XLATE(dma, src_va, src_pa, dma_mem_access, transfer_size, DMA_XLATE_TYPE_LOAD,  0, 0, dlbc_src);
            DMA_XLATE(dma, dst_va, dst_pa, dma_mem_access, transfer_size, DMA_XLATE_TYPE_STORE, 0, 0, dlbc_dst);
            CHECK_UWBC_16B(dma, desc_state, src_pa, dst_pa);

            uint8_t buffer[2*DMA_MAX_TRANSFER_SIZE];
            DMA_BUFFER_FILL_LOAD(dma, src_va, src_pa, transfer_size, buffer);
            DMA_BUFFER_WRITE(dma,     dst_va, dst_pa, transfer_size, buffer);

            desc_state->bytes_to_transfer  -= transfer_size;
            // Update a descriptor accordingly.
            desc->length -= transfer_size;
            desc->srcAddress += transfer_size;
            desc->dstAddress += transfer_size;

            desc_transfer_done = (desc_state->bytes_to_transfer == 0);

            if (!desc_transfer_done)
            {
                uint32_t syndrome_address = desc_state->va & ~0xF;
                if (desc->srcAddress < src_va) {
                    udma_ctx->exception_status = 1;
                    syndrome_address = (desc->srcDlbc) ? 0x4 : (syndrome_address | 0x2);
                    ARCH_FUNCTION(set_dma_error)(dma, syndrome_address, DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE, "src addresses rolled over");
                    return 0;
                }
                else
                {
                    if (desc->dstAddress < dst_va) {
                        udma_ctx->exception_status = 1;
                        syndrome_address |= 0x3;
                        ARCH_FUNCTION(set_dma_error)(dma, syndrome_address, DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE, "dst addresses rolled over");
                        return 0;
                    }
                }
            } else {
                return 1;
            }
        } else {
            return 1;
        }
    }

    return 1;
}

uint32_t ARCH_FUNCTION(step_2d_descriptor)(dma_t *dma);
uint32_t ARCH_FUNCTION(step_2d_descriptor)(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dma_decoded_descriptor_t * desc_state = (dma_decoded_descriptor_t *) &udma_ctx->active;
    //DMA_DEBUG(dma, "DMA %d: Tick %8d: step_2d_descriptor for %x udma_ctx->exception_status=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, desc_state->va, udma_ctx->exception_status);
    const uint32_t extended_va = desc_state->extended_va;
    const uint32_t gather = desc_state->gather;
    const uint32_t l2fetch = desc_state->l2fetch;
    const uint32_t constant_fill = desc_state->constant_fill;
    const uint32_t expansion = desc_state->expansion;
    const uint32_t compression = desc_state->compression;

    uint32_t new_gather_va = gather;
    uint64_t current_gather_list_va = 0;
    uint64_t current_gather_src_va = 0;
    uint32_t desc_transfer_done = 0;
    uint32_t transferred_blocks = 0;
    if (expansion||compression) {
        transferred_blocks = (desc_state->desc.type1.srcWidthOffset+desc_state->desc.type1.startBlockOffest) /  (  desc_state->desc.type1.blockSize +  ( (compression) ? desc_state->desc.type1.blockDelta : 0) );
    }
    while(!desc_transfer_done)
    {
        HEXAGON_DmaDescriptor2D_t * desc = (HEXAGON_DmaDescriptor2D_t*) &desc_state->desc;
        if(ARCH_FUNCTION(dma_check_verif_pause)(dma))
        {
            desc_state->pause = 1;
            return 1;
        }
        if (get_height(desc, desc_state))
        {
            const int32_t extended_src_va = extended_va;
            const int32_t extended_dst_va = extended_va && !gather; // Gather descriptors have only 32-bit destinations
            uint64_t src_va = (uint64_t)desc->srcAddress;
            uint64_t dst_va = (uint64_t)desc->dstAddress;

            if (gather)
            {
                uint64_t src_mask = ~0x1Fll;
                const uint64_t dst_mask = ~0x7Fll;
                if (new_gather_va)
                {   // We're going to over-ride the src address temporarily
                    dma_memaccess_info_t dma_mem_access;
                    current_gather_list_va = src_va;
                    uint64_t gather_addr_pa;
                    const uint32_t except_vtcm = 1;
                                        /* Don't consider rd_alloc == DESC_RDALLOC_FORGET for gather list */
                    if (dma_adapter_xlate_va(dma, current_gather_list_va, &gather_addr_pa, &dma_mem_access, desc->srcStride, DMA_XLATE_TYPE_LOAD, 0, except_vtcm, 0, 0))
                    {
                        current_gather_src_va = 0;
                        dma_data_read(dma, gather_addr_pa, desc->srcStride, (uint8_t *) &current_gather_src_va);
                        DMA_DEBUG(dma, "DMA %d: Tick %8d: gather address fetched for desc=%08x from src_va: %llx pa: %llx gather va=%llx\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, ((udma_ctx_t *)dma->udma_ctx)->active.va, (long long int)src_va, (long long int)gather_addr_pa, (long long int)current_gather_src_va);
                        new_gather_va = 0;
                    }
                    else
                    {
                        udma_ctx->exception_status = 1;
                        if (dma_mem_access.memtype.invalid_dma) {
                            src_mask = ~0x1Fll;
                            uint32_t syndrome_address = src_va & src_mask;
                            ARCH_FUNCTION(set_dma_error)(dma, syndrome_address, DMA_CFG0_SYNDROME_CODE___UNSUPPORTED_ADDRESS, "unsupported address for gather list");
                        } else {
                            src_mask = extended_va ? ~0x3Fll : ~0x1Fll;
                            dma_adapter_mask_badva(dma, (uint32_t) src_mask);  //HW fix for badva reporting
                        }
                        return 0;
                    }
                }
                src_va = current_gather_src_va & src_mask;
                dst_va &= dst_mask; // For 128B alignment of dest address
            }
            else
            {
                ADD_38BIT_UPPER_VA(src_va, desc->srcUpperAddr, extended_src_va);
            }
            ADD_38BIT_UPPER_VA(dst_va, desc->dstUpperAddr, extended_dst_va);


            uint32_t bytes_to_transfer = desc_state->bytes_to_transfer;
            src_va += get_src_widthoffset(desc, desc_state);
            dst_va += get_dst_widthoffset(desc, desc_state);
            uint32_t transfer_size = dma_transfer_size_lut[(dst_va|src_va) & 0xFF] + 1;
            if ( expansion ) {
                // Only transfer to blockSize
                bytes_to_transfer = get_width(desc, desc_state) - desc->srcWidthOffset;
                uint32_t max_size = ((transferred_blocks+1)*desc->blockSize) - desc->srcWidthOffset;
                max_size = is_power_of_two(max_size) ? max_size : lower_power_of_two(max_size);
                //DMA_DEBUG(dma, "DMA %d: Tick %8d: max_size=%d transfer_size=%d blocks=%d blockSize=%d srcWidthOffset=%d dstWidthOffset=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  max_size, transfer_size, transferred_blocks, desc->blockSize, desc->srcWidthOffset, desc->dstWidthOffset);
                transfer_size = max_size < transfer_size ? max_size : transfer_size;
            } else if (  compression) {
                // Only transfer to blockSize
                bytes_to_transfer = desc_state->dst_roi_width - get_dst_widthoffset(desc, desc_state);
                uint32_t max_size = ((transferred_blocks+1)*(desc->blockSize)) - get_dst_widthoffset(desc, desc_state);
                max_size = is_power_of_two(max_size) ? max_size : lower_power_of_two(max_size);
                //DMA_DEBUG(dma, "DMA %d: Tick %8d: max_size=%d transfer_size=%d blocks=%d blockSize=%d srcWidthOffset=%d dstWidthOffset=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  max_size, transfer_size, transferred_blocks, desc->blockSize, desc->srcWidthOffset, desc->dstWidthOffset);
                transfer_size = max_size < transfer_size ? max_size : transfer_size;
            }
            transfer_size = transfer_size < bytes_to_transfer ? transfer_size : (is_power_of_two(bytes_to_transfer) ? bytes_to_transfer : lower_power_of_two(bytes_to_transfer));

            //DMA_DEBUG(dma, "DMA %d: Tick %8d: transfer_size=%d bytes=%d lut=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  transfer_size, bytes_to_transfer, dma_transfer_size_lut[(dst_va|src_va) & 0xFF]);

            uint64_t src_pa = 0, dst_pa = 0;
            dma_memaccess_info_t dma_mem_access = {0};
            if (!constant_fill) {
                DMA_XLATE(dma, src_va, src_pa, dma_mem_access, transfer_size, DMA_XLATE_TYPE_LOAD,  extended_src_va, l2fetch, 0);
            }
            if (!l2fetch) {
                DMA_XLATE(dma, dst_va, dst_pa, dma_mem_access, transfer_size, DMA_XLATE_TYPE_STORE, extended_dst_va, 0, 0);
            }
            CHECK_UWBC_32B(dma, desc_state, src_pa, dst_pa);

            uint8_t buffer[DMA_MAX_TRANSFER_SIZE];
            if (!constant_fill) {
                DMA_BUFFER_FILL_LOAD(dma, src_va, src_pa, transfer_size, buffer);
            } else {
                DMA_BUFFER_FILL_CONSTANT(dma, src_va, src_pa, transfer_size, buffer);
            }
            if (!l2fetch) {
                DMA_BUFFER_WRITE(dma, dst_va, dst_pa, transfer_size, buffer);
            }

            desc_state->bytes_to_transfer  -= transfer_size;



            if (expansion) {
                desc->srcWidthOffset += transfer_size;
                transferred_blocks = (desc->srcWidthOffset+desc->startBlockOffest)/desc->blockSize;
                desc->dstWidthOffset = desc->srcWidthOffset + transferred_blocks*desc->blockDelta;
            } else if (compression) {

                uint32_t written_blocks = (desc->dstWidthOffset + desc->startBlockOffest + transfer_size)/(desc->blockSize);
                uint32_t remainder = (desc->dstWidthOffset + desc->startBlockOffest  + transfer_size) - written_blocks*(desc->blockSize);
                desc->srcWidthOffset = written_blocks*(desc->blockSize+desc->blockDelta)+remainder;
                transferred_blocks = (desc->srcWidthOffset+desc->startBlockOffest)/(desc->blockSize+desc->blockDelta);
                desc->dstWidthOffset = desc->srcWidthOffset - transferred_blocks*desc->blockDelta;

                desc_state->bytes_to_transfer = desc->width - desc->srcWidthOffset;
                //DMA_DEBUG(dma, "DMA %d: Tick %8d desc->dstWidthOffset=%x desc->srcWidthOffset=%x blockDelta=%d transferred_blocks=%x roi=%x %x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  desc->dstWidthOffset, desc->srcWidthOffset, desc->blockDelta, transferred_blocks, desc_state->src_roi_width, desc_state->dst_roi_width);

            } else {
                if (desc_state->wide) {
                    desc->width_offset += transfer_size;
                } else {
                    desc->srcWidthOffset += transfer_size;
                    desc->dstWidthOffset += transfer_size;
                }
            }

            uint32_t inc_src = (get_src_widthoffset(desc, desc_state) >= desc_state->src_roi_width);
            uint32_t inc_dst = (get_dst_widthoffset(desc, desc_state) >= desc_state->dst_roi_width);

            new_gather_va = gather && inc_dst;
            inc_src |= new_gather_va;

            inc_src &= !constant_fill;
            inc_dst &= !l2fetch;

            //inc_src |= inc_dst && compression; // hack
            inc_dst |= inc_src && (expansion|compression); // hack


            if (inc_src) {
                INC_64BIT_ADDRESS(desc->srcAddress, desc->srcUpperAddr, get_src_stride(desc, desc_state), extended_src_va);
            }
            if (inc_dst) {
                INC_64BIT_ADDRESS(desc->dstAddress, desc->dstUpperAddr, get_dst_stride(desc, desc_state), extended_dst_va);
            }

            if (inc_src || inc_dst) {
                //DMA_DEBUG(dma, "DMA %d: Tick %8d:inc_src=%d  src offset=%x roi=%x inc_dst=%d dst offset=%x roi=%x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, inc_src, desc->srcWidthOffset , desc_state->src_roi_width, inc_dst, desc->dstWidthOffset, desc_state->dst_roi_width);
                desc_state->bytes_to_transfer = desc_state->src_roi_width;
                desc_state->lines_to_transfer--;
                set_height(desc, desc_state, get_height(desc, desc_state) - 1);
                set_src_widthoffset(desc, desc_state, 0);
                set_dst_widthoffset(desc, desc_state, 0);
                transferred_blocks = 0;
            }

            desc_transfer_done = (get_height(desc, desc_state) == 0);
            if (desc_transfer_done)
            {
                set_src_widthoffset(desc, desc_state, 0);
                set_dst_widthoffset(desc, desc_state, 0);
                set_width(desc, desc_state, 0);
                return 1;
            }
            else
            {
                uint64_t base_addr = (uint64_t)desc->srcAddress + ( extended_src_va ? (((uint64_t)desc->srcUpperAddr)<<32) : 0);
                uint64_t addr_src_mask = (extended_src_va) ? 0x3FFFFFFFFFll : 0xFFFFFFFFll;
                if (gather)
                {
                    if (new_gather_va) {
                        src_va = current_gather_list_va; // old gather address
                        addr_src_mask = 0xFFFFFFFFll;
                    } else {
                        base_addr = current_gather_src_va;
                    }
                }
                uint64_t src_addr = base_addr + get_src_widthoffset(desc, desc_state);
                src_addr &= addr_src_mask;
                uint32_t syndrome_address = desc_state->va & ~0xF;
                if ((src_addr < src_va) && !constant_fill) {
                    udma_ctx->exception_status = 1;
                    DMA_DEBUG(dma, "DMA %d: Tick %8d: address rollover desc=%08x src addr cur=%llx next=%llx base=%llx offset=%x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  ((udma_ctx_t *)dma->udma_ctx)->active.va,  (long long int)src_va, (long long int)src_addr, (long long int)base_addr, get_src_widthoffset(desc, desc_state));
                    syndrome_address = (new_gather_va && gather ) ? 0x5 : (syndrome_address | 0x2);
                    ARCH_FUNCTION(set_dma_error)(dma, syndrome_address, DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE, "src addresses rolled over");
                    return 0;
                } else {
                    base_addr = (uint64_t)desc->dstAddress + (extended_dst_va ? (((uint64_t)desc->dstUpperAddr)<<32) : 0);
                    uint64_t addr_dst_mask = (extended_dst_va) ? 0x3FFFFFFFFFll : 0xFFFFFFFFll;
                    uint64_t dst_addr = base_addr + get_dst_widthoffset(desc, desc_state);
                    dst_addr &= addr_dst_mask;
                    if ((dst_addr < dst_va) && !l2fetch) {
                        udma_ctx->exception_status = 1;
                        syndrome_address |= 0x3;
                        DMA_DEBUG(dma, "DMA %d: Tick %8d: address rollover desc=%08x dst addr cur=%llx next=%llx base=%llx offset=%x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  ((udma_ctx_t *)dma->udma_ctx)->active.va, (long long int)dst_va, (long long int)dst_addr, (long long int)base_addr, get_dst_widthoffset(desc, desc_state));
                        ARCH_FUNCTION(set_dma_error)(dma, syndrome_address, DMA_CFG0_SYNDROME_CODE___DESCRIPTOR_INVALID_TYPE, "dst addresses rolled over");
                        return 0;
                    }
                }
            }
        } else {
            return 1;
        }
    }
    return 1;
}


static uint32_t ARCH_FUNCTION(dma_step_descriptor_chain)(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dma_decoded_descriptor_t * desc_state = (dma_decoded_descriptor_t *) &udma_ctx->active;
    HEXAGON_DmaDescriptor_t *desc = (HEXAGON_DmaDescriptor_t*) &desc_state->desc;

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
    udma_ctx->ext_status = DM0_STATUS_RUN;
    udma_ctx->pause_va_valid=0;
    desc_state->pause = 0;
    //DMA_DEBUG(dma, "DMA %d: Tick %8d: desc=%08x(from restart=%d)\n",   dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->desc_new, udma_ctx->desc_restart);

    while (udma_ctx->desc_new != 0)
    {
        if (ARCH_FUNCTION(start_dma)(dma, udma_ctx->desc_new) == 0)
        {
            udma_ctx->pause_va = udma_ctx->desc_new;
            udma_ctx->pause_va_valid = 1;
            return 0;
        }
        udma_ctx->desc_new = 0;
        uint32_t desc_transfer_done = (desc->common.descSize == 0) ? ARCH_FUNCTION(step_linear_descriptor)(dma) : ARCH_FUNCTION(step_2d_descriptor)(dma);

        if (udma_ctx->exception_status) {
            desc_transfer_done = 1;
            desc_state->exception = 1;
            desc_state->state = (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR)) ? DMA_DESC_EXCEPT_ERROR : DMA_DESC_EXCEPT_RUNNING;
            udma_ctx->exception_va = desc_state->va;
            DMA_DEBUG(dma, "DMA %d: Tick %8d: Logging exception desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)udma_ctx->exception_va );
            ARCH_FUNCTION(update_descriptor)(dma, &udma_ctx->active);
        } else if (desc_transfer_done) {
            desc_state->exception = 0;
            desc_state->state = DMA_DESC_NOT_DONE;
            DMA_DEBUG(dma, "DMA %d: Tick %8d: Logging done desc=%08x pause=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va, (unsigned int)desc_state->pause );
        }
        // This descriptor is done, put  it in the queue and log it to timing
        desc_tracker_entry_t * entry;
        if (udma_ctx->desc_restart) {
            udma_ctx->desc_restart = 0; // Restarted DMA already, don't need a new entry, just log it a
            entry = dma_adapter_peek_desc_queue_head(dma);
            entry->desc.exception = desc_state->exception;
            entry->desc.state = desc_state->state; // Update state, it may have finished or excepted again
            entry->desc.id =  entry->id;
            entry->desc.pause = desc_state->pause;
            DMA_DEBUG(dma, "DMA %d: Tick %8d: Setting callback for restart desc=%08x with pause=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va, entry->desc.pause );
            desc_state->id =  entry->id;
        } else {
            entry = dma_adapter_insert_desc_queue(dma, &udma_ctx->active );
            dma->idle = 0; // DMA is not idle so we can tick it, this gets rest when desc_queue goes empty, this is a speed optimization
            entry->desc.id =  entry->id;
            entry->desc.desc = desc_state->desc;
            entry->desc.pause = desc_state->pause;
            desc_state->id =  entry->id;
            entry->callback = ARCH_FUNCTION(descriptor_done_callback);
            DMA_DEBUG(dma, "DMA %d: Tick %8d: Setting callback for done desc=%08x with pause=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va, entry->desc.pause );
        }

#ifndef VERIFICATION
        dma_adapter_descriptor_start(dma, entry->desc.id, desc_state->va, (uint32_t*)desc);
#endif
        if (udma_ctx->timing_on )
        {
            if (!desc_state->gather && !udma_ctx->exception_status) {
                entry->uarch_desc = udma_ctx->init_desc;
                DMA_DEBUG(dma, "DMA %d: Tick %d: Logging desc=%08x ID=%lli to timing model state=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va, entry->id, entry->desc.state );
                // Log to timing
                ARCH_FUNCTION(uarch_dma_log_to_uarch)(dma, entry );
            } else {
                DMA_DEBUG(dma, "DMA %d: Tick %d: Gather Descriptor timing model not implemented! desc=%08x ID=%lli state=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va, entry->id, entry->desc.state );
                entry->callback((void*) entry); // Callback to indicate that the descriptor is done
            }
        } else {
            entry->callback((void*) entry); // Callback to indicate that the descriptor is done
        }

        if (!udma_ctx->exception_status && !udma_ctx->pause_va_valid)
        {   // Not in exception mode
            // Chaining of a next descriptor.
            uint32_t next_descriptor = desc->common.nextDescPointer;
            if (next_descriptor != 0)
            {
                desc_state->va = next_descriptor;
                desc_state->valid = 1;
                udma_ctx->desc_new = next_descriptor;
            } else {
               DMA_DEBUG(dma, "DMA %d: Tick %8d: End of chain with desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, (unsigned int)desc_state->va);
            }
        }
    }
    return 1;
}

void ARCH_FUNCTION(dma_arch_tlbw)(dma_t *dma, uint32_t jtlb_idx) {
    udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;
    ARCH_FUNCTION(uarch_dma_tlbw)(udma_ctx->uarch_dma_engine, jtlb_idx);
}
void ARCH_FUNCTION(dma_arch_tlbinvasid)(dma_t *dma) {
    udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;
    ARCH_FUNCTION(uarch_dma_tlbinvasid)(udma_ctx->uarch_dma_engine);
}
uint32_t ARCH_FUNCTION(dma_get_tick_count)(dma_t *dma) {
    return ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count;
}

uint32_t ARCH_FUNCTION(dma_retry_after_exception)(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    DMA_DEBUG(dma, "DMA %d: Tick %d: Trying restart after status idle=%d running=%d udma_ctx->exception_status=%d ", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_IDLE), DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING), udma_ctx->exception_status);

    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_IDLE)) {
        DMA_DEBUG(dma, "\n");
        return 0;
    }
    // Check if we are running and have already reported an exception
    if ( DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING) && (udma_ctx->exception_status == 0) )
    {
        // We should be in running mode and without exceptions
        desc_tracker_entry_t * entry = dma_adapter_peek_desc_queue_head(dma);
        // Only restart a DMA that was in exception,
        // If it was a good descriptor, it should have been flushed and done
        if (entry) {
            DMA_DEBUG(dma, "entry=%llx entry->desc.exception=%d state not done=%d new=%x ", (long long int)entry,  entry->desc.exception, (entry->desc.state != DMA_DESC_NOT_DONE), (int)(udma_ctx->desc_new) );
        }
        if (entry && entry->desc.exception && (entry->desc.state != DMA_DESC_NOT_DONE))
        {
            udma_ctx->desc_new = entry->desc.va;
            udma_ctx->desc_restart = 1;
            DMA_DEBUG(dma, "DMA %d: Tick %d: Trying restart after TLBW/Exception desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->desc_new );
#ifndef VERIFICATION
            ARCH_FUNCTION(dma_tick)(dma, 1);
        }
        else if ( (entry==NULL) && (udma_ctx->desc_new))
        {
            DMA_DEBUG(dma, "DMA %d: Tick %d: Trying restart after TLBW/Exception on descriptor fetch desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->desc_new );
            ARCH_FUNCTION(dma_tick)(dma, 1);
#endif
        }
    }
    DMA_DEBUG(dma, "\n");
    return 1;
}

uint32_t ARCH_FUNCTION(dma_tick)(dma_t *dma, uint32_t do_step)
{
    if (dma->idle)
        return 1;

    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    uint32_t stall_check_guest   = !udma_ctx->dm2.no_stall_guest && dma_adapter_in_guest_mode(dma);
    uint32_t stall_check_monitor = !udma_ctx->dm2.no_stall_monitor && dma_adapter_in_monitor_mode(dma);
    uint32_t stall_check_debug   = !udma_ctx->dm2.no_cont_debug && dma_adapter_in_debug_mode(dma);
    ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count  = dma_adapter_get_pcycle(dma);


    stall_check_guest = 0;
    stall_check_monitor = 0;
    stall_check_debug = 0;

    if ((DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING)) && (udma_ctx->exception_status == 0) && do_step)
    {
        if (stall_check_guest || stall_check_monitor || stall_check_debug) {
            DMA_DEBUG(dma, "Requesting Stall: DMA DM2=%08x ", udma_ctx->dm2.val);
            DMA_DEBUG(dma, "stall due to guest = %d monitor = %d debug = %d\n", stall_check_guest, stall_check_monitor, stall_check_debug);
        }
        else
        {
            ARCH_FUNCTION(dma_step_descriptor_chain)(dma);
        }
    }

    dma->idle = dma_adapter_pop_desc_queue_done(dma);

    if (udma_ctx->timing_on) {
        ARCH_FUNCTION(uarch_dma_engine_step)(udma_ctx->uarch_dma_engine);
    }

    return 1;
}

static uint32_t ARCH_FUNCTION(dma_cmd_insn_timer_checker)(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    if (--udma_ctx->insn_timer == 0)  {
        udma_ctx->insn_timer_active = INSN_TIMER_EXPIRED;
        dma_adapter_pmu_increment(dma, udma_ctx->insn_timer_pmu, 1);
        DMA_DEBUG(dma, "DMA %d: Tick %8d: insn latency expired\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count);
        return 1;
    }
    //DMA_DEBUG(dma, "DMA %d: Tick %8d: insn latency timer=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->insn_timer);
    return 0;
}

static inline uint32_t
ARCH_FUNCTION(dma_instruction_latency)(dma_t *dma, dma_cmd_report_t *report, uint32_t latency, uint32_t pmu_num) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    if (udma_ctx->timing_on == 1) {
        if (udma_ctx->insn_timer_active == INSN_TIMER_IDLE) {
            report->insn_checker = &ARCH_FUNCTION(dma_cmd_insn_timer_checker);
            udma_ctx->insn_timer_active = INSN_TIMER_ACTIVE;
            udma_ctx->insn_timer = dma_adapter_get_insn_latency(dma, latency); // get latency
            udma_ctx->insn_timer_pmu = pmu_num;
            DMA_DEBUG(dma, "DMA %d: Tick %8d: setting insn latency timer=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->insn_timer);
            return 1;
        } else if (udma_ctx->insn_timer_active == INSN_TIMER_EXPIRED) {
            return 0;
        } else {
            return 1;
        }
    }
    return 0;
}

#if !defined(CONFIG_USER_ONLY)
static void load_dma_descriptor_type0(thread_t *env, uint32_t desc_addr,
                                      dma_descriptor_type0_t *desc)
{
    uint8_t *store_ptr = (uint8_t *)desc;
    for (uint32_t i = 0; i < sizeof(dma_descriptor_type0_t); i += 4) {
        DEBUG_MEMORY_READ(desc_addr + i, 4, store_ptr + i);
    }
}

static void load_dma_descriptor_type1(thread_t *env, uint32_t desc_addr,
                                      dma_descriptor_type1_t *desc)
{
    uint8_t *store_ptr = (uint8_t *)desc;
    for (uint32_t i = 0; i < sizeof(dma_descriptor_type1_t); i += 4) {
        DEBUG_MEMORY_READ(desc_addr + i, 4, store_ptr + i);
    }
}

static void preload_buffers(dma_t *dma, uint32_t new)
{
    /* make sure tlb miss doesn't happen when dma actually in progress */
    thread_t* env = dma_adapter_retrieve_thread(dma);
    dma_descriptor_type0_t desc0;
    uint32_t desc_addr = new;
    load_dma_descriptor_type0(env, desc_addr, &desc0);
    uint32_t dma_type = get_dma_desc_desctype((void *)&desc0);
    if (dma_type == 0) {
        while (desc_addr) {
            load_dma_descriptor_type0(env, desc_addr, &desc0);
            uint32_t length = get_dma_desc_length(&desc0);
            uint32_t dst_va = (uint32_t)(desc0.dst);
            uint32_t src_va = (uint32_t)(desc0.src);
            hexagon_touch_memory(env, dst_va, length);
            hexagon_touch_memory(env, src_va, length);
            desc_addr = desc0.next;
        }
    } else if (dma_type == 1) {
        dma_descriptor_type1_t desc1;
        while (desc_addr) {
            load_dma_descriptor_type1(env, desc_addr, &desc1);
            uint32_t size = get_dma_desc_roiwidth(&desc1) *
                            get_dma_desc_roiheight(&desc1);
            uint32_t dst_va = (uint32_t)(desc1.dst);
            uint32_t src_va = (uint32_t)(desc1.src);
            hexagon_touch_memory(env, dst_va, size);
            hexagon_touch_memory(env, src_va, size);
            desc_addr = desc1.next;
        }

    } else {
        g_assert_not_reached();
    }
}
#endif

void ARCH_FUNCTION(dma_cmd_start)(dma_t *dma, uint32_t new_dma, dma_cmd_report_t *report)
{
    dma->idle = 0;
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_start new", new_dma););
    DMA_DEBUG_ONLY(ARCH_FUNCTION(verif_descriptor_peek)(dma, new_dma););
    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        return;
    }

    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING))
    {
        ARCH_FUNCTION(set_dma_error)(dma, new_dma, DMA_CFG0_SYNDROME_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "DMSTART_DMRESUME_IN_RUNSTATE");
        return;
    }

    if (new_dma == 0)
    {
        DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, " tried to start a null descriptor. nop. <-dma_cmd_start", 0););
        return;
    }
#if !defined(CONFIG_USER_ONLY)
    preload_buffers(dma, new_dma);
#endif

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
    udma_ctx->ext_status = DM0_STATUS_RUN;
    udma_ctx->active.va = new_dma;
    udma_ctx->active.valid = 1;
    udma_ctx->active.pc = dma->pc;
    udma_ctx->desc_new = new_dma;
    udma_ctx->desc_restart = 0;
    dma_adapter_pmu_increment(dma, DMA_PMU_CMD_START, 1);

#ifndef VERIFICATION
    ARCH_FUNCTION(dma_tick)(dma, 1);
#endif
   DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-dma_cmd_start", new_dma););
}

void ARCH_FUNCTION(dma_cmd_link)(dma_t *dma, uint32_t cur, uint32_t new_dma, uint64_t pa, uint32_t invalid_dmlink_addr, dma_cmd_report_t *report)
{
    dma->idle = 0;
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    /* Roles: chain a new descriptor, and start DMA if it can. */
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_link cur=", cur););
    //DMA_DEBUG_ONLY(ARCH_FUNCTION(verif_descriptor_peek)(dma, cur););
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_link new=", new_dma););
    DMA_DEBUG_ONLY(ARCH_FUNCTION(verif_descriptor_peek)(dma, new_dma););
    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        return;
    }

    if ( (new_dma == 0) && (cur == 0) && DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_IDLE))
    {
        DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, " tried to link a null descriptor with a null head. nop. <-dma_cmd_link", 0););
        return;
    }


    /* Try to chain or start. */
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING) || DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED))
    {
        {
            if(invalid_dmlink_addr) {
                udma_ctx->exception_status = 1;
                ARCH_FUNCTION(set_dma_error)(dma, 0, DMA_CFG0_SYNDROME_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "unsupported address for dmlink tail");
                return;
            }

            if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED)) {
                // If paused set to run
                //DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
                //udma_ctx->ext_status = DM0_STATUS_RUN;
                //DMA_DEBUG(dma, "DMA %d: Tick %8d: dma_cmd_link resuming dma\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count);
                if (new_dma != 0)
                {

                    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
                    udma_ctx->ext_status = DM0_STATUS_RUN;
                    udma_ctx->active.va = new_dma;
                    udma_ctx->active.valid = 1;
                    udma_ctx->desc_new = new_dma;
                    udma_ctx->desc_restart = 0;
                    DMA_DEBUG(dma, "DMA %d: Tick %8d: %s new != 0: active desc=%08x  desc_next=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, __FUNCTION__, (unsigned int)udma_ctx->active.va, (unsigned int)udma_ctx->desc_new );
#ifndef VERIFICATION
                    ARCH_FUNCTION(dma_tick)(dma, 1);
#endif
                }
            }
#ifndef VERIFICATION
            else if ( (new_dma != 0) && DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING)) {
                udma_ctx->active.va = new_dma;
                udma_ctx->active.valid = 1;
                udma_ctx->active.pc = dma->pc;
                udma_ctx->desc_new = new_dma;
                DMA_DEBUG(dma, "DMA %d: Tick %8d: %s new != 0: active desc=%08x  desc_next=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, __FUNCTION__, (unsigned int)udma_ctx->active.va, (unsigned int)udma_ctx->desc_new );
                ARCH_FUNCTION(dma_tick)(dma, 1);
            }
#endif
            ARCH_FUNCTION(dma_word_store)(dma, cur, dma->pc, cur, pa);   // Timing model only, send out a transaction to tail



            DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-dma_cmd_link", new_dma););
            dma_adapter_pmu_increment(dma, DMA_PMU_CMD_LINK, 1);
            return;
        }
    }
    /* If we reach here, this is our very first descriptor to process. */

    if (cur != 0) {
        if(invalid_dmlink_addr) {
            udma_ctx->exception_status = 1;
            ARCH_FUNCTION(set_dma_error)(dma, 0, DMA_CFG0_SYNDROME_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "unsupported address for dmlink tail");
            return;
        }
    }
    if (new_dma != 0)
    {
        DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
        udma_ctx->ext_status = DM0_STATUS_RUN;
        udma_ctx->active.va = new_dma;
        udma_ctx->active.valid = 1;
        udma_ctx->active.pc = dma->pc;
        udma_ctx->desc_new = new_dma;
        DMA_DEBUG(dma, "DMA %d: Tick %8d: %s new != 0: active desc=%08x  desc_next=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, __FUNCTION__, (unsigned int)udma_ctx->active.va, (unsigned int)udma_ctx->desc_new );

    }
    dma_adapter_pmu_increment(dma, DMA_PMU_CMD_LINK, 1);
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-dma_cmd_link", new_dma););
#ifndef VERIFICATION
    ARCH_FUNCTION(dma_tick)(dma, 1);
#endif
}




void ARCH_FUNCTION(dma_cmd_poll)(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_poll exception_status", udma_ctx->exception_status););
    /* If an exception happened already, we have to make an owner thread  take it. */

    if (ARCH_FUNCTION(dma_instruction_latency)(dma, report, DMA_INSN_LATENCY_DMPOLL, DMA_PMU_DMPOLL_CYCLES))
        return;

    uint32_t dm0 = dma_adapter_peek_desc_queue_head_va(dma);
    if (udma_ctx->exception_status)
    {
        /* Clear the DMA exception for a next ticking. */
        udma_ctx->exception_status = 0;
        dm0 = udma_ctx->exception_va;
        (*report).excpt = 1;
        DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_poll report exception", udma_ctx->exception_status););
    }
    /* Return value of dmpoll instruction. */

    (*ret) = dm0 | udma_ctx->ext_status;    // Need head of Queue
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-dma_cmd_poll dm0", (*ret)););
}

static uint32_t ARCH_FUNCTION(dma_cmd_wait_checker)(dma_t *dma)
{
    /* This function will be used as a stall-breaker for dmwait instruction.
       This function checks if DMA tasks of a current thread is done or not.
     While the DMA engine working, if an exception happened, we should stop
     stalling immediately so that DMWAIT or DMPOLL can be processed
     by the simulator's thread cycle. While this staller is ON,
     the thread cycle will be skipped, although the DMA cycle will keep
     working on. */
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;


    //DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dma_cmd_wait_checker: current_status=%d exception_status=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->ext_status, udma_ctx->exception_status);
    dma_adapter_pmu_increment(dma, DMA_PMU_DMWAIT_CYCLES, 1);
    return ((udma_ctx->ext_status == DM0_STATUS_IDLE) || (udma_ctx->exception_status));
}

void ARCH_FUNCTION(dma_cmd_wait)(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_wait", 0););

    uint32_t dm0 = 0;
#ifdef VERIFICATION
    ARCH_FUNCTION(dma_tick)(dma, 1);
#endif

    if (ARCH_FUNCTION(dma_instruction_latency)(dma, report, DMA_INSN_LATENCY_DMWAIT, DMA_PMU_DMWAIT_CYCLES))
        return;

    /* Take a pending exception. Because of this report, later we will
        revisit "dmwait" instruction again by replying. */
    if (udma_ctx->exception_status)
    {
        /* Clear the DMA exception to move forward. */

        DMA_DEBUG(dma, "DMA %d: Tick %8d: clear exception and set report and update descriptor on dmwait exception va=%x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->exception_va);
        udma_ctx->exception_status = 0;
        (*report).excpt = 1;
        dm0 = udma_ctx->exception_va;
    }
    else
    {
        /* When we reach here after an exception is resolved,
            we expect "RUNNING". */
        if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING))
        {
            /* We will wait for the entire task completed. This checker will see
                if the task is done, or an exception occurs. This will make
                the adapter automatically set up a staller on behalf of the engine.
            */
            (*report).insn_checker = &ARCH_FUNCTION(dma_cmd_wait_checker);

            DMA_DEBUG(dma, "DMA %d: Tick %8d: dmwait set up a staller\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count);
        } else if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR)) {
            if (udma_ctx->dm2.error_exception_enable)
            {
                (*report).excpt = 1;
            }
        }
    }

    if(udma_ctx->target.valid) {
        DMA_DEBUG(dma, "DMA %d: Tick %8d: clearing target descriptor=%x on wait\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->target.va);
        udma_ctx->target.va = 0;
        udma_ctx->target.valid = 0;
    }

    /* Return value of dmwait instruction. */
    (*ret) = dm0 | udma_ctx->ext_status;
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-dma_cmd_wait dm0", (*ret)););
}


uint32_t ARCH_FUNCTION(dma_in_error) (dma_t *dma) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    return  (udma_ctx->ext_status == DM0_STATUS_ERROR) || (udma_ctx->exception_status);
}
static uint32_t ARCH_FUNCTION(dma_cmd_pause_checker)(dma_t *dma)
{
    //DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dma_cmd_pause_checker\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count);
    dma_adapter_pmu_increment(dma, DMA_PMU_PAUSE_CYCLES, 1);
    return dma_adapter_desc_queue_empty(dma);
}


void ARCH_FUNCTION(dma_cmd_pause)(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_pause", 0););
    uint32_t active_desc_va = udma_ctx->active.va;
    uint32_t status = udma_ctx->ext_status;

    if (ARCH_FUNCTION(dma_instruction_latency)(dma, report, DMA_INSN_LATENCY_DMPAUSE, DMA_PMU_PAUSE_CYCLES))
        return;

    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        ARCH_FUNCTION(dma_stop)(dma);   // Clear error state;
        udma_ctx->exception_status = 0; // Clear exception status
    }

    /* If the engine is RUNNING, make it PAUSED. */
    if ( DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING) )
    {

#ifdef VERIFICATION
        ARCH_FUNCTION(dma_tick)(dma, 1);   // Tick until done or reached target descriptor
        active_desc_va = (udma_ctx->exception_status) ? udma_ctx->exception_va : udma_ctx->pause_va;   // Current active descriptor
#endif

        DMA_DEBUG(dma, "DMA %d: Tick %8d: udma_ctx->active.va=0x%08x desc_va=0x%08x (pause_va=0x%08x exception_va=%x exception_status=%d)\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->active.va, active_desc_va, udma_ctx->pause_va, udma_ctx->exception_va, udma_ctx->exception_status );
        DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_PAUSED);
        udma_ctx->ext_status = DM0_STATUS_IDLE;


#ifndef VERIFICATION
        if (udma_ctx->exception_status) {
            (*report).excpt = 1; //tmp, to signal when to flush chain
            desc_tracker_entry_t * entry = dma_adapter_peek_desc_queue_head(dma);
            if (entry) {
                DMA_DEBUG(dma, "DMA %d: Tick %d: pausing running state in exception for active desc: %x exception_va: %x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->active.va, udma_ctx->exception_va);
                active_desc_va = udma_ctx->exception_va;    // Report the descritor that took the exception, udma_ctx->active.va could have been overwritten by a dmlink
            }
        }
#endif


        udma_ctx->exception_status = 0; // Clear exception status
        // If not 0, it means that the new descriptor has not been loaded yet so no updating required
        // Flush buffers, short cutting dmpause. We're going to force everything immediately to completion in the timing model
        if (udma_ctx->timing_on) {
            ARCH_FUNCTION(uarch_dma_flush_timing)(dma);
            (*report).insn_checker = &ARCH_FUNCTION(dma_cmd_pause_checker);
            status = DM0_STATUS_IDLE;
        }
        if ((udma_ctx->pause_va_valid==0) && (udma_ctx->active.valid==0))
            status = 0;

        udma_ctx->pause_va = 0;
        udma_ctx->pause_va_valid = 0;
        udma_ctx->active.va = 0;
        udma_ctx->active.valid  = 0; // Transition to idle, no active descritor
        udma_ctx->desc_new = 0;
        udma_ctx->desc_restart = 0;

        if (udma_ctx->target.valid) {
            DMA_DEBUG(dma, "DMA %d: Tick %8d: clearing target descriptor on pause va=0x%08x)\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, udma_ctx->target.va );
            udma_ctx->target.valid = 0;
            udma_ctx->target.no_transfer  = 0;
            udma_ctx->target.bytes_to_transfer  = 0;
            udma_ctx->target.lines_to_transfer= 0;
        }

    }

    (*ret) =  active_desc_va  | status;
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-dma_cmd_pause ret", active_desc_va | status););
}

void ARCH_FUNCTION(dma_cmd_resume)(dma_t *dma, uint32_t ptr, dma_cmd_report_t *report)
{
    dma->idle = 0;
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "->dma_cmd_resume ptr", ptr););
    DMA_DEBUG_ONLY(ARCH_FUNCTION(verif_descriptor_peek)(dma, ptr););

    if (ARCH_FUNCTION(dma_instruction_latency)(dma, report, DMA_INSN_LATENCY_DMRESUME, DMA_PMU_NUM))
        return;

    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        return;
    }

    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING))
    {
        ARCH_FUNCTION(set_dma_error)(dma, ptr, DMA_CFG0_SYNDROME_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "DMSTART_DMRESUME_IN_RUNSTATE");
        return;
    }
    else if ((ptr & ~0x3)== 0)
    {
        DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, " tried to start a null descriptor. nop. <-dma_cmd_resume", 0););
        return;
    }
    else
    {
        /* We do not expect any exceptions from the monitor mode commands - pause
            and resume. */

        /* If the engine is PAUSED, clear it, and resume (restart). But don't
            take any exception. */
        if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED) || DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_IDLE))
        {
            if (ptr == 0)
            {
                ARCH_FUNCTION(dma_stop)(dma);
            }
            else
            {
                udma_ctx->desc_restart = 0;
                if((ptr & 0x3) == 0x2)
                {   // Resuming error state
                    udma_ctx->active.va = ptr & ~0xF;
                    udma_ctx->active.valid = 1;
                    udma_ctx->desc_new = ptr & ~0xF;
                    ARCH_FUNCTION(set_dma_error)(dma, 0, DMA_CFG0_SYNDROME_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "DMSTART_DMRESUME_IN_RUNSTATE");
                    DMA_DEBUG(dma, "DMA %d: Tick %8d: dma_cmd_resume: resuming error state desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, ptr & ~0xF);
                }
                else
                {
                    DMA_DEBUG(dma, "DMA %d: Tick %8d: dma_cmd_resume: setting desc=%08x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, ptr & ~0xF);
                    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
                    udma_ctx->ext_status = DM0_STATUS_RUN;
                    udma_ctx->active.va = ptr & ~0xF;
                    udma_ctx->active.valid = 1;
                    udma_ctx->desc_new = ptr & ~0xF;
#ifndef VERIFICATION
                    ARCH_FUNCTION(dma_tick)(dma, 1);
#endif
                }
            }
        }
        dma_adapter_pmu_increment(dma, DMA_PMU_CMD_RESUME, 1);
    }
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    DMA_DEBUG_ONLY(ARCH_FUNCTION(dump_dma_status)(dma, "<-dma_cmd_resume",0););
}

uint32_t ARCH_FUNCTION(dma_set_timing)(dma_t *dma, uint32_t timing_on)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    udma_ctx->timing_on = timing_on;
    return 1;
}

uint32_t ARCH_FUNCTION(dma_init)(dma_t *dma, uint32_t timing_on)
{
    udma_ctx_t *udma_ctx;
    if ((udma_ctx = (udma_ctx_t *) calloc(1, sizeof(udma_ctx_t))) == NULL)
    {
        return -1;
    }
    dma->udma_ctx = (void *)udma_ctx;
    udma_ctx->timing_on = timing_on;

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_IDLE);
    udma_ctx->ext_status = DM0_STATUS_IDLE;
    udma_ctx->exception_status = 0;
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;

    udma_ctx->target.va = 0;
    udma_ctx->target.valid = 0;
    //Initialize DM2
    udma_ctx->dm2.val = 0;
    udma_ctx->dm3.val = 0;
    udma_ctx->error_state_address = 0;

    udma_ctx->dm2.no_stall_guest = 0;
    udma_ctx->dm2.no_stall_monitor = 0;
    udma_ctx->dm2.no_cont_debug = 0;
    udma_ctx->dm2.priority = 1;                 // Inherits the thread?s priority
    udma_ctx->dm2.dlbc_enable = 1;
    udma_ctx->dm2.error_exception_enable = 1;
    udma_ctx->exception_status = 0;

    udma_ctx->uwbc_a_en = dma_adapter_uwbc_ap_en(dma);
    dma->verbosity = dma_adapter_debug_verbosity(dma);
    dma->idle = 1;
#ifdef VERIFICATION
    dma->verbosity = 9;

#endif

    //const unsigned int dma_dpu_prefetch_depth = 16;
    // WARNING: The timing model needs to be initialized regardless of timing_on
    // If running with --fastforward, timing_on may turn on later on during the run
    udma_ctx->uarch_dma_engine =  uarch_dma_engine_init(dma, dma_adapter_get_prefetch_depth(dma));
#if 0
    if (udma_ctx->uarch_dma_engine == NULL)
    {
        return -1;
    }
#endif

    return 1;
}

uint32_t ARCH_FUNCTION(dma_free)(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    uarch_dma_engine_inst_t * engine = udma_ctx->uarch_dma_engine;
    if (engine) {   // timing mode only
        uarch_dma_engine_free(engine);
    }

    if (dma->udma_ctx != NULL)
    {
        free(dma->udma_ctx);
        dma->udma_ctx = NULL;
    }

    return 1;
}

void ARCH_FUNCTION(dma_cmd_cfgrd)(dma_t *dma, uint32_t index, uint32_t *val, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dma_cmd_cfgrd: Index=%d\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, index);

    (*val) = 0;
    switch (index)
    {
        case 0:
            (*val) = udma_ctx->active.va | udma_ctx->ext_status;
            break;

        case 2:
            (*val) = udma_ctx->dm2.val;
            break;
        case 4:
            (*val) = udma_ctx->error_state_reason_captured |
                     dma->num << 4 |
                     udma_ctx->error_state_reason;
            break;

        case 5:
            (*val) = udma_ctx->error_state_address;
            break;

        case 1: // Reserved
            break;
        case 3: // Reserved
            (*val) = udma_ctx->dm3.val;
        default: // Out of range
            break;
    }
}

void ARCH_FUNCTION(dma_cmd_cfgwr)(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dma_cmd_cfgwr: Index=%d Val=%0x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, index, val);

    switch (index)
    {
        case 4:
            if ((val & 0x1) == 0)
            {
                udma_ctx->error_state_reason_captured   = 0;
                udma_ctx->error_state_reason            = 0;
                // udma_ctx->error_state_address           = 0;
            }
            break;

        case 2:
            udma_ctx->dm2.val = val;
            DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dm2 updated = %0x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  udma_ctx->dm2.val);
            break;

        case 0: // Not applicable
        case 1: // Reserved
            break;
        case 3:
            udma_ctx->dm3.val = val;
            DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dm3 updated = %0x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  udma_ctx->dm3.val)
            break;
        case 5:
             udma_ctx->error_state_address = val;
             DMA_DEBUG(dma, "DMA %d: Tick %8d: ->error_state_address updated = %0x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count,  udma_ctx->error_state_address);
        default: // Out of range
            break;
    }

}

void ARCH_FUNCTION(dma_cmd_syncht)(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report)
{
    DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dma_cmd_syncht: Index=%d Val=%0x\n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, index, val);
}
void ARCH_FUNCTION(dma_cmd_tlbsynch)(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report)
{
    DMA_DEBUG(dma, "DMA %d: Tick %8d: ->dma_cmd_tlbsynch: Index=%d Val=%0x \n", dma->num, ((udma_ctx_t *)dma->udma_ctx)->dma_tick_count, index, val);
}

