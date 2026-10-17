/*
 * Hexagon HLX (eLement-wise eXtensions) context pool QOM object.
 *
 * Copyright(c) 2026 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_HEXAGON_HEXAGON_HLX_H
#define HW_HEXAGON_HEXAGON_HLX_H

#include "hw/core/qdev.h"
#include "hw/core/sysbus.h"
#include "qom/object.h"
#include "target/hexagon/hlx/hlx.h"   /* HLXVector, NUM_LREGS */

#define TYPE_HEXAGON_HLX "hexagon-hlx"
OBJECT_DECLARE_SIMPLE_TYPE(HexagonHLXState, HEXAGON_HLX)

/* Word index in hexagon_config_table.raw[] for HLX_SIZE (byte offset 0x100). */
#define HLX_SIZE_CFG_INDEX 64

/*
 * Per-context state.  Access the register file through ctx->LRegs[n];
 * do not cast HexagonHLXContext * to HLXVector *.
 */
typedef struct HexagonHLXContext {
    HLXVector LRegs[NUM_LREGS] QEMU_ALIGNED(16);
} HexagonHLXContext;

struct HexagonHLXState {
    SysBusDevice parent_obj;

    /*
     * Bitmap of enabled HLX contexts as reported by cfgtable offset
     * 0x84 (HLX_CONTEXTS).  Populated from a qdev property at
     * realize time.
     */
    uint32_t contexts_bitmap;

    /*
     * Raw per-register size in bytes (cfgtable offset 0x100, HLX_SIZE).
     * Populated from a qdev property (default 2048).  Used only for a
     * realize-time sanity check against HLX_VEC_SIZE_BYTES; the
     * compile-time constant is still authoritative at runtime.
     */
    uint32_t hlx_size_bytes;

    int32_t num_contexts;   /* popcount(contexts_bitmap), derived at realize */

    /*
     * Flat pool: contexts[0..num_contexts-1].  The index into this
     * pool is the POPCOUNT-INDEX of CCR.XA3 within contexts_bitmap:
     * count the number of set bits in contexts_bitmap below position xa3.
     */
    HexagonHLXContext *contexts;
};

/*
 * Resolve an HLX context pointer given the current CCR.XA3 value.
 * Returns NULL if XA3 selects a bit that is not set in contexts_bitmap
 * (i.e. the guest selected a non-existent context: treated as disabled).
 * Safe to call with out-of-range XA3.
 */
HexagonHLXContext *hexagon_hlx_get_context(HexagonHLXState *s, uint32_t xa3);

/*
 * Zero the register file of every context.  Called from
 * machine-wide reset to ensure a clean initial HLX register state.
 */
void hexagon_hlx_reset_all(HexagonHLXState *s);

#endif /* HW_HEXAGON_HEXAGON_HLX_H */
