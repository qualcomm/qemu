/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Hexagon HLX (eLement-wise eXtensions) context pool QOM object.
 *
 * Copyright(c) 2026 Qualcomm Innovation Center, Inc. All Rights Reserved.
 */

#include "qemu/osdep.h"
#include "qemu/host-utils.h"       /* ctpop32 */
#include "hw/core/qdev-properties.h"
#include "hw/hexagon/hexagon_hlx.h"
#include "migration/vmstate.h"
#include "qapi/error.h"

/*
 * xa3 is the raw value read from CCR.XA3 (a 1-bit field today, may
 * widen to 2 bits when CCR[23:22] are unreserved).  contexts_bitmap
 * is the HLX_CONTEXTS bitmap from cfgtable 0x84.  Returns the flat
 * index in contexts[] of the selected context, or -1 if invalid.
 */
static int hlx_xa3_to_pool_idx(uint32_t contexts_bitmap, uint32_t xa3)
{
    if (!((contexts_bitmap >> xa3) & 1u)) {
        return -1;
    }
    uint32_t below_mask = (xa3 == 0) ? 0u : ((1u << xa3) - 1u);
    return ctpop32(contexts_bitmap & below_mask);
}

HexagonHLXContext *hexagon_hlx_get_context(HexagonHLXState *s, uint32_t xa3)
{
    int idx = hlx_xa3_to_pool_idx(s->contexts_bitmap, xa3);
    if (idx < 0 || idx >= s->num_contexts) {
        return NULL;
    }
    return &s->contexts[idx];
}

void hexagon_hlx_reset_all(HexagonHLXState *s)
{
    if (!s->contexts) {
        return;
    }
    memset(s->contexts, 0, sizeof(HexagonHLXContext) * (size_t)s->num_contexts);
}

static void hexagon_hlx_realize(DeviceState *dev, Error **errp)
{
    HexagonHLXState *s = HEXAGON_HLX(dev);

    if (s->hlx_size_bytes == 0) {
        s->hlx_size_bytes = HLX_VEC_SIZE_BYTES;
    }
    if (s->hlx_size_bytes != HLX_VEC_SIZE_BYTES) {
        /* Only 2048-byte HLX vectors are supported. */
        error_setg(errp,
                   "HLX_SIZE=%u from cfgtable differs from compile-time "
                   "HLX_VEC_SIZE_BYTES=%u; widening requires additional work",
                   s->hlx_size_bytes, HLX_VEC_SIZE_BYTES);
        return;
    }

    s->num_contexts = (int32_t)ctpop32(s->contexts_bitmap);
    if (s->num_contexts == 0) {
        /* HLX not enabled in this config; zero-context state. */
        s->contexts = NULL;
        return;
    }
    s->contexts = g_new0(HexagonHLXContext, (size_t)s->num_contexts);
}

static void hexagon_hlx_unrealize(DeviceState *dev)
{
    HexagonHLXState *s = HEXAGON_HLX(dev);
    g_clear_pointer(&s->contexts, g_free);
    s->num_contexts = 0;
}

static const Property hexagon_hlx_properties[] = {
    DEFINE_PROP_UINT32("contexts-bitmap", HexagonHLXState, contexts_bitmap, 0),
    DEFINE_PROP_UINT32("hlx-size-bytes",  HexagonHLXState, hlx_size_bytes, 0),
};

/*
 * VMState: variable-length array of HexagonHLXContext driven by num_contexts.
 *
 * Serialize the entire HexagonHLXContext as a flat byte buffer.
 * sizeof(HexagonHLXContext) = NUM_LREGS * HLX_VEC_SIZE_BYTES = 32 KiB.
 * VMSTATE_BUFFER_UNSAFE_INFO_TEST avoids the type_check_array constraint
 * that VMSTATE_BUFFER imposes (it expects a uint8_t array field).
 */
static const VMStateDescription vmstate_hexagon_hlx_context = {
    .name = "hexagon-hlx-context",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_BUFFER_UNSAFE_INFO_TEST(LRegs, HexagonHLXContext, NULL,
                                        0, vmstate_info_buffer,
                                        sizeof(((HexagonHLXContext *)0)->LRegs)),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_hexagon_hlx = {
    .name = TYPE_HEXAGON_HLX,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(contexts_bitmap, HexagonHLXState),
        VMSTATE_UINT32(hlx_size_bytes,  HexagonHLXState),
        VMSTATE_INT32(num_contexts,     HexagonHLXState),
        VMSTATE_STRUCT_VARRAY_ALLOC(contexts, HexagonHLXState,
                                    num_contexts, 1,
                                    vmstate_hexagon_hlx_context,
                                    HexagonHLXContext),
        VMSTATE_END_OF_LIST()
    }
};

static void hexagon_hlx_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize   = hexagon_hlx_realize;
    dc->unrealize = hexagon_hlx_unrealize;
    dc->vmsd      = &vmstate_hexagon_hlx;
    device_class_set_props(dc, hexagon_hlx_properties);
}

static const TypeInfo hexagon_hlx_info = {
    .name          = TYPE_HEXAGON_HLX,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HexagonHLXState),
    .class_init    = hexagon_hlx_class_init,
};

static void hexagon_hlx_register_types(void)
{
    type_register_static(&hexagon_hlx_info);
}

type_init(hexagon_hlx_register_types)
