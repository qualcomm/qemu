/*
 * CDSP0 Clock Controller SWI (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/misc/qcom-qdsp6-cc-swi.h"

#define R(off) ((off) / 4)

static uint64_t cdsp0_clkctl_read(void *opaque, hwaddr addr, unsigned size)
{
    Cdsp0ClkctlState *s = opaque;

    if (size != 4 || addr >= CDSP0_CLKCTL_MMIO_SIZE || (addr & 3)) {
        return 0;
    }

    switch (addr) {
    /* CMD_RCGR registers: the update bit self-clears when read */
    case 0x000:
    case 0x098:
    case 0x0B0:
    case 0x0C8:
    case 0x0E0:
    case 0x0FC:
    case 0x114:
    case 0x180:
    case 0x41C:
    case 0x428:
        if (s->regs[R(addr)] & 0x1) {
            s->regs[R(addr)] &= ~0x1;
        }
        break;
    default:
        break;
    }

    return s->regs[R(addr)];
}

static void cdsp0_clkctl_write(void *opaque, hwaddr addr, uint64_t value,
                               unsigned size)
{
    Cdsp0ClkctlState *s = opaque;

    if (size != 4 || addr >= CDSP0_CLKCTL_MMIO_SIZE || (addr & 3)) {
        return;
    }

    s->regs[R(addr)] = (uint32_t)value;
}

static const MemoryRegionOps cdsp0_clkctl_ops = {
    .read = cdsp0_clkctl_read,
    .write = cdsp0_clkctl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static void cdsp0_clkctl_reset(Object *obj, ResetType type)
{
    Cdsp0ClkctlState *s = CDSP0_CLKCTL(obj);
    static const struct { uint16_t off; uint32_t val; } reset[] = {
        { 0x004, 0x00100000 }, { 0x008, 0x00000021 }, { 0x00C, 0x00000001 },
        { 0x010, 0x00000001 }, { 0x014, 0x00000001 }, { 0x018, 0x00000001 },
        { 0x040, 0x80000000 }, { 0x044, 0x80000000 }, { 0x048, 0x80000000 },
        { 0x04C, 0x80000000 }, { 0x050, 0x00000001 }, { 0x054, 0x00000001 },
        { 0x058, 0x00000001 },
        { 0x080, 0x80226001 }, { 0x084, 0x00098400 }, { 0x088, 0x0002022A },
        { 0x08C, 0x02F00000 }, { 0x090, 0x00222222 },
        { 0x098, 0x80000000 }, { 0x09C, 0x00100000 }, { 0x0A0, 0x00000021 },
        { 0x0A4, 0x80000220 }, { 0x0A8, 0x00014000 },
        { 0x0B0, 0x80000000 }, { 0x0B4, 0x00100000 }, { 0x0B8, 0x00000021 },
        { 0x0BC, 0x80000220 }, { 0x0C0, 0x00014000 },
        { 0x0C8, 0x80000000 }, { 0x0CC, 0x00100000 }, { 0x0D0, 0x00000021 },
        { 0x0D4, 0x80000220 }, { 0x0D8, 0x00014000 },
        { 0x0E0, 0x80000000 }, { 0x0E4, 0x00100000 }, { 0x0E8, 0x00000021 },
        { 0x0EC, 0x80000220 }, { 0x0F0, 0x00014000 }, { 0x0F8, 0x80000000 },
        { 0x0FC, 0x80000000 }, { 0x100, 0x00100000 }, { 0x104, 0x00000021 },
        { 0x108, 0x80000220 }, { 0x10C, 0x00014000 },
        { 0x114, 0x80000000 }, { 0x118, 0x00100000 }, { 0x11C, 0x00000021 },
        { 0x120, 0x80000220 }, { 0x124, 0x00014000 }, { 0x12C, 0x80000000 },
        { 0x130, 0x80000000 }, { 0x134, 0x80000000 }, { 0x138, 0x80000000 },
        { 0x13C, 0x80000000 }, { 0x140, 0x80000000 }, { 0x144, 0x80000000 },
        { 0x150, 0x80000000 }, { 0x154, 0x80000000 },
        { 0x180, 0x80000000 }, { 0x184, 0x00100000 }, { 0x18C, 0x80000220 },
        { 0x190, 0x00014000 }, { 0x198, 0x80000000 },
        { 0x200, 0x80226000 }, { 0x204, 0x00088400 }, { 0x208, 0x0002022A },
        { 0x20C, 0x02F00000 }, { 0x210, 0x00222222 },
        { 0x218, 0x80000000 }, { 0x21C, 0x80000000 }, { 0x220, 0x80000220 },
        { 0x224, 0x00014000 }, { 0x22C, 0x80000220 }, { 0x230, 0x00014000 },
        { 0x238, 0x80000000 }, { 0x23C, 0x80000000 }, { 0x240, 0x80000000 },
        { 0x404, 0x80000000 }, { 0x40C, 0x80000000 }, { 0x410, 0x80000000 },
        { 0x414, 0x80000000 }, { 0x418, 0x80000000 }, { 0x41C, 0x80000000 },
        { 0x424, 0x80000000 }, { 0x428, 0x80000000 }, { 0x430, 0x80000000 },
        { 0x434, 0x80000000 }, { 0x438, 0x80000000 }, { 0x43C, 0x80000000 },
    };

    memset(s->regs, 0, sizeof(s->regs));
    for (size_t i = 0; i < ARRAY_SIZE(reset); i++) {
        s->regs[R(reset[i].off)] = reset[i].val;
    }
}

static void cdsp0_clkctl_realize(DeviceState *dev, Error **errp)
{
    Cdsp0ClkctlState *s = CDSP0_CLKCTL(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &cdsp0_clkctl_ops, s,
                          TYPE_CDSP0_CLKCTL, CDSP0_CLKCTL_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void cdsp0_clkctl_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = cdsp0_clkctl_realize;
    rc->phases.hold = cdsp0_clkctl_reset;
    dc->desc = "QDSP6 Clock Software Interface (SWI)";
}

static const TypeInfo cdsp0_clkctl_info = {
    .name = TYPE_CDSP0_CLKCTL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Cdsp0ClkctlState),
    .class_init = cdsp0_clkctl_class_init,
};

static void cdsp0_clkctl_register_types(void)
{
    type_register_static(&cdsp0_clkctl_info);
}

type_init(cdsp0_clkctl_register_types)
