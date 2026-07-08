/*
 * Hexagon H2 hypervisor angel semihosting mailbox stub
 *
 * The H2 hypervisor's "booter" program (roms/hexagon-hypervisor) services
 * semihosting requests through its own H2K_trap_angel handler rather than
 * a direct trap0, by writing a request into a per-hardware-thread mailbox
 * slot at a fixed virtual address (ANGEL_VA), then polling a busy/ready
 * flag in that slot until it is cleared by the observing agent (normally
 * hexagon-sim).  QEMU does not implement that mailbox protocol, so
 * without this device the guest spins forever.  See where this device
 * is mapped (hw/hexagon/hexagon_dsp.c) for why it sits at csr_base
 * rather than at the guest's intended physical address for ANGEL_VA.
 *
 * This device does not implement the protocol; it only unblocks the
 * guest's poll loop by always reading back a cleared busy flag, so the
 * actual semihosting request content is discarded.  See
 * docs/system/hexagon/booter.rst.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/hexagon/hexagon-angel-mbox.h"
#include "qemu/module.h"

/* From roms/hexagon-hypervisor/kernel/util/max/max.h: ANGEL_PG_SIZE */
#define ANGEL_MBOX_MEM_SIZE 0x1000

static uint64_t hexagon_angel_mbox_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    return 0;
}

static void hexagon_angel_mbox_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
    /* Discarded: the request content is never serviced. */
}

static const MemoryRegionOps hexagon_angel_mbox_ops = {
    .read = hexagon_angel_mbox_read,
    .write = hexagon_angel_mbox_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void hexagon_angel_mbox_realize(DeviceState *dev, Error **errp)
{
    HexagonAngelMboxState *s = HEXAGON_ANGEL_MBOX(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &hexagon_angel_mbox_ops, s,
                          "hexagon-angel-mbox", ANGEL_MBOX_MEM_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void hexagon_angel_mbox_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = hexagon_angel_mbox_realize;
}

static const TypeInfo hexagon_angel_mbox_info = {
    .name = TYPE_HEXAGON_ANGEL_MBOX,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HexagonAngelMboxState),
    .class_init = hexagon_angel_mbox_class_init,
};

static void hexagon_angel_mbox_register_types(void)
{
    type_register_static(&hexagon_angel_mbox_info);
}

type_init(hexagon_angel_mbox_register_types)
