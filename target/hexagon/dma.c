/*
 * Hexagon User-DMA Engine
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 * This models the "User DMA" chapter of the Hexagon system-level
 * specification: the 16-byte linear and 32-byte Type 0 2D box-copy
 * descriptors, plus the dmstart/dmlink/dmpoll/dmwait/dmpause/dmresume
 * instructions.  The monitor-mode commands are not implemented.
 *
 * The engine is synchronous: a chain runs to completion inside the
 * instruction that starts it, so dmpoll/dmwait never observe a running
 * engine.  To keep a malformed chain from wedging QEMU, one chain is limited
 * to DMA_MAX_CHAIN_DESCRIPTORS descriptors and DMA_MAX_CHAIN_WORK units of
 * work (one per byte or row); larger chains end in the error state.
 *
 * Guest memory faults raised while accessing a descriptor or its payload are
 * delivered to the guest as ordinary exceptions and leave DM0 in the error
 * state.  They are not captured in DM4/DM5, which only record the errors the
 * engine itself detects.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "dma.h"
#include "accel/tcg/cpu-ldst.h"
#include "accel/tcg/probe.h"
#include "qemu/log.h"
#include "trace.h"

/* Error syndrome codes: the subset of DM4's encoding this model produces. */
enum {
    DMA_SYNDROME_INVALID_ALIGNMENT = 1,
    DMA_SYNDROME_INVALID_TYPE = 2,
    DMA_SYNDROME_UNSUPPORTED = 3,
    DMA_SYNDROME_CHAIN_LIMIT = 4,
    DMA_SYNDROME_MEMORY_ACCESS = 5,
};

#define DMA_MAX_CHAIN_DESCRIPTORS   1024
#define DMA_MAX_CHAIN_WORK          (UINT64_C(64) << 20)

/*
 * Descriptor byte layout.  Guest memory is never aliased through a host
 * struct: every field is read/written with the normal cpu_ld/st*_data_ra()
 * accessors at these offsets, so ordinary guest MMU/fault behavior applies
 * to descriptor and payload access alike.
 */
#define DESC_OFF_NEXT               0
#define DESC_OFF_CTRL               4
#define DESC_OFF_SRC                8
#define DESC_OFF_DST                12
#define DESC_TYPE0_SIZE             16

#define DESC_OFF_TYPE               16   /* low byte: DescriptorType */
#define DESC_OFF_ROI                20   /* roiheight:roiwidth */
#define DESC_OFF_STRIDE             24   /* dststride:srcstride */
#define DESC_OFF_WIDTHOFFSET        28   /* dstwidthoffset:srcwidthoffset */
#define DESC_TYPE1_SIZE             32

#define DESC_ALIGNMENT              16

/* CTRL word (offset DESC_OFF_CTRL) */
#define DESC_DSTATE_MASK            0x80000000
#define DESC_SRCCOMP_MASK           0x08000000
#define DESC_DSTCOMP_MASK           0x04000000
/*
 * This is the descriptor's "DescriptorSize" field (16-byte vs 32-byte
 * layout), not its "DescriptorType" byte (DESC_OFF_TYPE).  A 32-byte
 * descriptor still needs its DescriptorType byte checked to know which
 * of the many 32-byte descriptor shapes it actually is.
 */
#define DESC_DESCTYPE_MASK          0x03000000
#define DESC_DESCTYPE_SHIFT         24
#define DESC_DESCTYPE_TYPE0         0
#define DESC_DESCTYPE_TYPE1         1
#define DESC_LENGTH_MASK            0x00FFFFFF
/*
 * The Order and Bypass bits are legal and commonly set.  Order is a no-op
 * because the engine is synchronous, so every earlier descriptor in the
 * chain is already complete.  Bypass is a no-op because this model has no
 * separate cached/bypass memory path.  Compression (SrcComp/DstComp) is the
 * only control pair that is not implemented.
 */
#define DESC_UNSUPPORTED_CTRL_MASK  (DESC_SRCCOMP_MASK | DESC_DSTCOMP_MASK)

/* DescriptorType byte (offset DESC_OFF_TYPE), 32-byte descriptors only. */
#define DESC_TYPE_FIELD_MASK        0x000000FF
enum {
    DESC_TYPE_2D = 0,
};

/* ROI word (offset DESC_OFF_ROI), type1 only */
#define DESC_ROIWIDTH_MASK          0x0000FFFF
#define DESC_ROIHEIGHT_MASK         0xFFFF0000
#define DESC_ROIHEIGHT_SHIFT        16

/* Stride word (offset DESC_OFF_STRIDE), type1 only */
#define DESC_SRCSTRIDE_MASK         0x0000FFFF
#define DESC_DSTSTRIDE_MASK         0xFFFF0000
#define DESC_DSTSTRIDE_SHIFT        16

typedef struct HexagonDMAMemory {
    CPUHexagonState *env;
    int mmu_idx;
    uintptr_t ra;
} HexagonDMAMemory;

typedef enum DMAKind {
    DMA_COPY,
} DMAKind;

/* A decoded 2D transfer: height rows of width bytes. */
typedef struct DMATransfer {
    DMAKind kind;
    target_ulong src;
    target_ulong dst;
    uint32_t width;
    uint32_t height;
    uint32_t srcstride;
    uint32_t dststride;
} DMATransfer;

void hexagon_dma_reset(HexagonDMAState *dma)
{
    dma->status = DM0_STATUS_IDLE;
    dma->syndrome = 0;
    dma->desc_ptr = 0;
}

uint32_t hexagon_dma_htid(HexagonCPU *cpu)
{
#ifdef CONFIG_USER_ONLY
    return 0;
#else
    return cpu->env.threadId;
#endif
}

uint32_t hexagon_dma_version(HexagonCPU *cpu)
{
    return cpu->dma_version;
}

#ifdef CONFIG_USER_ONLY
uint32_t hexagon_dma_model_version(HexagonCPU *cpu)
{
    HexagonVersion cpu_version = cpu->cfg.hex_def->hex_version;

    if (cpu_version >= HEX_VER_V81) {
        return 8;
    } else if (cpu_version >= HEX_VER_V79) {
        return 6;
    } else if (cpu_version >= HEX_VER_V75) {
        return 4;
    } else if (cpu_version >= HEX_VER_V73) {
        return 2;
    } else if (cpu_version >= HEX_VER_V68) {
        return 1;
    }
    return 0;
}
#endif

static uint32_t dma_ldl(const HexagonDMAMemory *mem, target_ulong addr)
{
    return cpu_ldl_le_data_ra(mem->env, addr, mem->ra);
}

static void dma_stl(const HexagonDMAMemory *mem, target_ulong addr,
                    uint32_t value)
{
    cpu_stl_le_data_ra(mem->env, addr, value, mem->ra);
}

static uint32_t dma_page_left(target_ulong addr)
{
    return (1u << TARGET_PAGE_BITS) - (addr & ((1u << TARGET_PAGE_BITS) - 1));
}

/* Copy length bytes from src to dst, a page at a time. */
static void dma_copy(const HexagonDMAMemory *mem, target_ulong dst,
                     target_ulong src, uint32_t length)
{
    while (length != 0) {
        uint32_t chunk = MIN(length, MIN(dma_page_left(src),
                                         dma_page_left(dst)));
        void *src_host = probe_read(mem->env, src, chunk, mem->mmu_idx,
                                    mem->ra);
        void *dst_host = probe_write(mem->env, dst, chunk, mem->mmu_idx,
                                     mem->ra);

        if (src_host && dst_host) {
            memmove(dst_host, src_host, chunk);
        } else {
            for (uint32_t i = 0; i < chunk; i++) {
                uint8_t byte = cpu_ldub_data_ra(mem->env, src + i, mem->ra);
                cpu_stb_data_ra(mem->env, dst + i, byte, mem->ra);
            }
        }
        src += chunk;
        dst += chunk;
        length -= chunk;
    }
}

/* Raise the guest fault, if any, for accessing [addr, addr + length). */
static void dma_probe_range(const HexagonDMAMemory *mem, target_ulong addr,
                            uint32_t length, MMUAccessType access_type)
{
    while (length != 0) {
        uint32_t chunk = MIN(length, dma_page_left(addr));

        probe_access(mem->env, addr, chunk, access_type, mem->mmu_idx,
                     mem->ra);
        addr += chunk;
        length -= chunk;
    }
}

/* Does [addr, addr + length) wrap around the end of the address space? */
static bool dma_range_wraps(uint64_t addr, uint64_t length)
{
    return length != 0 && addr + length - 1 > UINT32_MAX;
}

/* Do rows rows of width bytes, stride apart, stay inside the address space? */
static bool dma_span_ok(target_ulong base, uint32_t rows, uint32_t stride,
                        uint32_t width)
{
    if (rows == 0 || width == 0) {
        return true;
    }
    return !dma_range_wraps(base, (uint64_t)(rows - 1) * stride + width);
}

/*
 * Leave a terminal state behind in case a guest memory fault unwinds out of
 * the engine before the next descriptor step completes.  This is local to
 * the engine: the shared DM4/DM5 registers record only detected errors.
 */
static void dma_stage_fault(HexagonDMAState *dma, target_ulong desc_va)
{
    dma->status = DM0_STATUS_ERROR;
    dma->syndrome = DMA_SYNDROME_MEMORY_ACCESS;
    dma->desc_ptr = desc_va;
}

static void dma_set_error(HexagonCPU *cpu, target_ulong desc_va,
                          uint32_t syndrome)
{
    HexagonDMAState *dma = &cpu->dma;
    uint32_t htid = hexagon_dma_htid(cpu);

    dma->status = DM0_STATUS_ERROR;
    dma->syndrome = syndrome;
    dma->desc_ptr = desc_va;
    trace_hexagon_dma_error(htid, desc_va, syndrome);
}

/* Fetch a zero-means-64K stride field (DMA version 2 and later). */
static uint32_t dma_stride16(uint32_t field, bool zero_is_max)
{
    return (field == 0 && zero_is_max) ? UINT16_MAX + 1 : field;
}

/* Type 0 2D box copy: a roiheight x roiwidth block. */
static uint32_t dma_decode_2d(HexagonCPU *cpu, const HexagonDMAMemory *mem,
                              target_ulong desc_va, DMATransfer *xfer)
{
    uint32_t roi = dma_ldl(mem, desc_va + DESC_OFF_ROI);
    uint32_t stride = dma_ldl(mem, desc_va + DESC_OFF_STRIDE);
    bool v2 = hexagon_dma_version(cpu) >= 2;

    xfer->kind = DMA_COPY;
    xfer->src = dma_ldl(mem, desc_va + DESC_OFF_SRC);
    xfer->width = roi & DESC_ROIWIDTH_MASK;
    xfer->height = (roi & DESC_ROIHEIGHT_MASK) >> DESC_ROIHEIGHT_SHIFT;
    xfer->srcstride = dma_stride16(stride & DESC_SRCSTRIDE_MASK, v2);
    xfer->dststride = dma_stride16((stride & DESC_DSTSTRIDE_MASK) >>
                                   DESC_DSTSTRIDE_SHIFT, v2);
    return 0;
}

/*
 * Decode a 32-byte descriptor into a transfer.  Returns 0, or the syndrome
 * for a descriptor the engine rejects.
 */
static uint32_t dma_decode_type1(HexagonCPU *cpu, const HexagonDMAMemory *mem,
                                 target_ulong desc_va, uint32_t ctrl,
                                 DMATransfer *xfer)
{
    uint32_t desc_type;

    if (desc_va % DESC_TYPE1_SIZE != 0) {
        return DMA_SYNDROME_INVALID_ALIGNMENT;
    }
    dma_probe_range(mem, desc_va, DESC_TYPE1_SIZE, MMU_DATA_LOAD);
    if (dma_ldl(mem, desc_va + DESC_OFF_WIDTHOFFSET) != 0) {
        return DMA_SYNDROME_UNSUPPORTED;
    }

    xfer->dst = dma_ldl(mem, desc_va + DESC_OFF_DST);
    desc_type = dma_ldl(mem, desc_va + DESC_OFF_TYPE) & DESC_TYPE_FIELD_MASK;
    switch (desc_type) {
    case DESC_TYPE_2D:
        return dma_decode_2d(cpu, mem, desc_va, xfer);
    default:
        return DMA_SYNDROME_UNSUPPORTED;
    }
}

/*
 * Reject a transfer whose rows would run off the end of the address space.
 * Gather reads one list entry per row from src, so the bound applies to the
 * list rather than to the rows it names.
 */
static bool dma_xfer_in_range(const DMATransfer *xfer)
{
    switch (xfer->kind) {
    case DMA_COPY:
        return dma_span_ok(xfer->src, xfer->height, xfer->srcstride,
                           xfer->width) &&
               dma_span_ok(xfer->dst, xfer->height, xfer->dststride,
                           xfer->width);
    default:
        g_assert_not_reached();
    }
}

static void dma_execute(const HexagonDMAMemory *mem, const DMATransfer *xfer)
{
    for (uint32_t row = 0; row < xfer->height; row++) {
        target_ulong srow = xfer->src + (target_ulong)row * xfer->srcstride;
        target_ulong drow = xfer->dst + (target_ulong)row * xfer->dststride;

        switch (xfer->kind) {
        case DMA_COPY:
            dma_copy(mem, drow, srow, xfer->width);
            break;
        default:
            g_assert_not_reached();
        }
    }
}

void hexagon_dma_run_chain(HexagonCPU *cpu, target_ulong desc_va,
                           uintptr_t ra)
{
    HexagonDMAState *dma = &cpu->dma;
    CPUHexagonState *env = &cpu->env;
    HexagonDMAMemory mem = {
        .env = env,
        .mmu_idx = cpu_mmu_index(env_cpu(env), false),
        .ra = ra,
    };
    uint32_t htid = hexagon_dma_htid(cpu);
    uint32_t desc_count = 0;
    uint64_t work = 0;

    /* DMStart and DMResume only act on an idle engine. */
    if (hexagon_dma_version(cpu) == 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "hexagon: user DMA is not present on this core\n");
        return;
    }
    if (dma->status != DM0_STATUS_IDLE) {
        return;
    }
    dma->status = DM0_STATUS_RUN;

    while (desc_va != 0) {
        DMATransfer xfer = { };
        uint32_t ctrl, desctype, syndrome;
        uint64_t xfer_work;

        if (desc_va % DESC_ALIGNMENT != 0) {
            dma_set_error(cpu, desc_va, DMA_SYNDROME_INVALID_ALIGNMENT);
            return;
        }
        if (desc_count++ == DMA_MAX_CHAIN_DESCRIPTORS) {
            dma_set_error(cpu, desc_va, DMA_SYNDROME_CHAIN_LIMIT);
            return;
        }
        if (dma_range_wraps(desc_va, DESC_TYPE0_SIZE)) {
            dma_set_error(cpu, desc_va, DMA_SYNDROME_MEMORY_ACCESS);
            return;
        }

        /* Leave a terminal state behind if probing faults. */
        dma_stage_fault(dma, desc_va);
        dma_probe_range(&mem, desc_va, DESC_TYPE0_SIZE, MMU_DATA_LOAD);
        dma_probe_range(&mem, desc_va + DESC_OFF_CTRL, sizeof(ctrl),
                        MMU_DATA_STORE);

        dma->desc_ptr = desc_va;
        ctrl = dma_ldl(&mem, desc_va + DESC_OFF_CTRL);
        desctype = (ctrl & DESC_DESCTYPE_MASK) >> DESC_DESCTYPE_SHIFT;

        if (ctrl & DESC_UNSUPPORTED_CTRL_MASK) {
            dma_set_error(cpu, desc_va, DMA_SYNDROME_UNSUPPORTED);
            return;
        }

        switch (desctype) {
        case DESC_DESCTYPE_TYPE0:
            xfer.kind = DMA_COPY;
            xfer.src = dma_ldl(&mem, desc_va + DESC_OFF_SRC);
            xfer.dst = dma_ldl(&mem, desc_va + DESC_OFF_DST);
            xfer.width = ctrl & DESC_LENGTH_MASK;
            xfer.height = 1;
            break;
        case DESC_DESCTYPE_TYPE1:
            if (dma_range_wraps(desc_va, DESC_TYPE1_SIZE)) {
                dma_set_error(cpu, desc_va, DMA_SYNDROME_MEMORY_ACCESS);
                return;
            }
            syndrome = dma_decode_type1(cpu, &mem, desc_va, ctrl, &xfer);
            if (syndrome) {
                dma_set_error(cpu, desc_va, syndrome);
                return;
            }
            break;
        default:
            dma_set_error(cpu, desc_va, DMA_SYNDROME_INVALID_TYPE);
            return;
        }

        /* Every row costs at least one unit, even a zero-width one. */
        xfer_work = (uint64_t)xfer.width * xfer.height + xfer.height;
        if (work + xfer_work > DMA_MAX_CHAIN_WORK) {
            dma_set_error(cpu, desc_va, DMA_SYNDROME_CHAIN_LIMIT);
            return;
        }
        if (!dma_xfer_in_range(&xfer)) {
            dma_set_error(cpu, desc_va, DMA_SYNDROME_UNSUPPORTED);
            return;
        }
        work += xfer_work;

        dma_execute(&mem, &xfer);

        /* Mark the descriptor complete before following the chain. */
        dma_stl(&mem, desc_va + DESC_OFF_CTRL, ctrl | DESC_DSTATE_MASK);

        desc_va = dma_ldl(&mem, desc_va + DESC_OFF_NEXT);
        trace_hexagon_dma_desc(htid, dma->desc_ptr, desctype, desc_va);
    }

    dma->status = DM0_STATUS_IDLE;
    dma->syndrome = 0;
    trace_hexagon_dma_done(htid, dma->desc_ptr);
}

void hexagon_dma_link(HexagonCPU *cpu, target_ulong tail_va,
                      target_ulong new_va, uintptr_t ra)
{
    HexagonDMAState *dma = &cpu->dma;
    CPUHexagonState *env = &cpu->env;
    HexagonDMAMemory mem = {
        .env = env,
        .mmu_idx = cpu_mmu_index(env_cpu(env), false),
        .ra = ra,
    };

    trace_hexagon_dma_link(hexagon_dma_htid(cpu), new_va, tail_va);

    if (hexagon_dma_version(cpu) == 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "hexagon: user DMA is not present on this core\n");
        return;
    }

    /* DMLink cannot recover an Error engine. */
    if (dma->status == DM0_STATUS_ERROR) {
        return;
    }

    if (tail_va % DESC_ALIGNMENT != 0) {
        dma_set_error(cpu, tail_va, DMA_SYNDROME_INVALID_ALIGNMENT);
        return;
    }
    if (dma_range_wraps(tail_va, DESC_TYPE0_SIZE)) {
        dma_set_error(cpu, tail_va, DMA_SYNDROME_MEMORY_ACCESS);
        return;
    }

    dma_stage_fault(dma, tail_va);
    dma_probe_range(&mem, tail_va + DESC_OFF_NEXT, sizeof(uint32_t),
                    MMU_DATA_STORE);
    dma_stl(&mem, tail_va + DESC_OFF_NEXT, new_va);
    dma->status = DM0_STATUS_IDLE;
}

uint32_t hexagon_dma_pause(HexagonCPU *cpu)
{
    HexagonDMAState *dma = &cpu->dma;
    uint32_t status = dma->status;

    dma->status = DM0_STATUS_IDLE;
    dma->desc_ptr = 0;
    return status;
}
