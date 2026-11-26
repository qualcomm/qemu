/*
 * pbl_init: memory that publishes its second generation of writes only
 * once every chunk has been written.
 *
 * The device covers [addr, addr + size) of the system address space,
 * above anything else mapped there (typically RAM), and splits it into
 * chunks of chunk-size bytes. Every chunk goes through these states:
 *
 * - initial: reads return 0.
 * - first-written: the first write access to the chunk is stored and
 *   reads return the result. Later writes are kept on the side: they
 *   are not visible to reads.
 *
 * When the first write to the last remaining chunk happens, the writes
 * kept on the side become visible, and from then on the whole region
 * behaves like plain memory, until the next system reset.
 *
 * Usage:
 *   -device pbl_init,addr=0x40800000,size=0x200000,chunk-size=0x1000
 *   -trace 'pbl_init_*'
 *
 * Copyright (c) 2026 Pierrick Bouvier
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitmap.h"
#include "qemu/bswap.h"
#include "qapi/error.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/resettable.h"
#include "migration/vmstate.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/reset.h"
#include "trace.h"

#define TYPE_PBL_INIT "pbl_init"
OBJECT_DECLARE_SIMPLE_TYPE(PblInitState, PBL_INIT)

struct PblInitState {
    DeviceState parent_obj;

    MemoryRegion mmio;
    /* What reads return. */
    uint8_t *data;
    /* Every write, including the ones not visible yet. */
    uint8_t *pending;
    /* Chunks that have received their first write. */
    unsigned long *written;
    uint64_t nr_chunks;
    uint64_t nr_written;
    /* All chunks have been written: plain memory from now on. */
    bool committed;

    /* Properties */
    uint64_t addr;
    uint64_t size;
    uint64_t chunk_size;
};

static const char *pbl_init_chunk_state(PblInitState *s, uint64_t chunk)
{
    if (s->committed) {
        return "normal";
    }
    return test_bit(chunk, s->written) ? "first-written" : "initial";
}

static uint64_t pbl_init_read(void *opaque, hwaddr offset, unsigned size)
{
    PblInitState *s = opaque;
    uint64_t chunk = offset / s->chunk_size;
    uint64_t val = ldn_le_p(s->data + offset, size);

    if (s->committed) {
        trace_pbl_init_read(offset, size, val, chunk, "normal");
    } else {
        uint64_t new_val = ldn_le_p(s->pending + offset, size);

        if (new_val != val) {
            trace_pbl_init_read_stale(offset, size, val, chunk,
                                      pbl_init_chunk_state(s, chunk),
                                      new_val);
        } else {
            trace_pbl_init_read(offset, size, val, chunk,
                                pbl_init_chunk_state(s, chunk));
        }
    }
    return val;
}

static void pbl_init_commit(PblInitState *s)
{
    memcpy(s->data, s->pending, s->size);
    s->committed = true;
    trace_pbl_init_commit(s->nr_chunks);
}

static void pbl_init_write(void *opaque, hwaddr offset, uint64_t val,
                           unsigned size)
{
    PblInitState *s = opaque;
    uint8_t buf[8];
    hwaddr pos, end = offset + size;

    stn_le_p(buf, size, val);

    if (s->committed) {
        trace_pbl_init_write(offset, size, val, offset / s->chunk_size);
        memcpy(s->data + offset, buf, size);
        return;
    }

    /* An unaligned access may straddle two chunks: handle each part. */
    for (pos = offset; pos < end; ) {
        uint64_t chunk = pos / s->chunk_size;
        hwaddr part_end = MIN(end, (chunk + 1) * s->chunk_size);
        size_t len = part_end - pos;
        const uint8_t *src = buf + (pos - offset);

        memcpy(s->pending + pos, src, len);
        if (!test_bit(chunk, s->written)) {
            memcpy(s->data + pos, src, len);
            set_bit(chunk, s->written);
            s->nr_written++;
            trace_pbl_init_write_first(offset, size, val, chunk,
                                       s->nr_chunks - s->nr_written);
        } else {
            trace_pbl_init_write_deferred(offset, size, val, chunk,
                                          s->nr_chunks - s->nr_written);
        }
        pos = part_end;
    }

    if (s->nr_written == s->nr_chunks) {
        pbl_init_commit(s);
    }
}

static const MemoryRegionOps pbl_init_ops = {
    .read = pbl_init_read,
    .write = pbl_init_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = true,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = true,
    },
};

static void pbl_init_reset_hold(Object *obj, ResetType type)
{
    PblInitState *s = PBL_INIT(obj);

    memset(s->data, 0, s->size);
    memset(s->pending, 0, s->size);
    bitmap_zero(s->written, s->nr_chunks);
    s->nr_written = 0;
    s->committed = false;
    trace_pbl_init_reset(s->addr, s->size, s->chunk_size, s->nr_chunks);
}

static void pbl_init_realize(DeviceState *dev, Error **errp)
{
    PblInitState *s = PBL_INIT(dev);

    if (!s->size) {
        error_setg(errp, "property 'size' must be set and non zero");
        return;
    }
    if (!s->chunk_size) {
        error_setg(errp, "property 'chunk-size' must be non zero");
        return;
    }
    if (s->size % s->chunk_size) {
        error_setg(errp, "'size' (0x%" PRIx64 ") must be a multiple of "
                   "'chunk-size' (0x%" PRIx64 ")", s->size, s->chunk_size);
        return;
    }
    if (s->addr + s->size - 1 < s->addr) {
        error_setg(errp, "'addr' + 'size' overflows the address space");
        return;
    }

    s->nr_chunks = s->size / s->chunk_size;
    s->data = g_malloc0(s->size);
    s->pending = g_malloc0(s->size);
    s->written = bitmap_new(s->nr_chunks);

    memory_region_init_io(&s->mmio, OBJECT(dev), &pbl_init_ops, s,
                          TYPE_PBL_INIT, s->size);
    /* Above RAM or anything else already mapped there. */
    memory_region_add_subregion_overlap(get_system_memory(), s->addr,
                                        &s->mmio, 1);

    /*
     * Bus-less devices are not part of the reset tree: register with the
     * system reset ourselves, like generic-loader.
     */
    qemu_register_resettable(OBJECT(dev));
    pbl_init_reset_hold(OBJECT(dev), RESET_TYPE_COLD);
}

static void pbl_init_unrealize(DeviceState *dev)
{
    PblInitState *s = PBL_INIT(dev);

    qemu_unregister_resettable(OBJECT(dev));
    memory_region_del_subregion(get_system_memory(), &s->mmio);
    object_unparent(OBJECT(&s->mmio));
    g_free(s->data);
    g_free(s->pending);
    g_free(s->written);
}

static const Property pbl_init_props[] = {
    DEFINE_PROP_UINT64("addr", PblInitState, addr, 0),
    DEFINE_PROP_UINT64("size", PblInitState, size, 0),
    DEFINE_PROP_UINT64("chunk-size", PblInitState, chunk_size, 4096),
};

static const VMStateDescription vmstate_pbl_init = {
    .name = TYPE_PBL_INIT,
    .unmigratable = 1,
};

static void pbl_init_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->desc = "Memory publishing its later writes once fully written";
    dc->realize = pbl_init_realize;
    dc->unrealize = pbl_init_unrealize;
    dc->vmsd = &vmstate_pbl_init;
    rc->phases.hold = pbl_init_reset_hold;
    device_class_set_props(dc, pbl_init_props);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo pbl_init_info = {
    .name = TYPE_PBL_INIT,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(PblInitState),
    .class_init = pbl_init_class_init,
};

static void pbl_init_register_types(void)
{
    type_register_static(&pbl_init_info);
}

type_init(pbl_init_register_types)
