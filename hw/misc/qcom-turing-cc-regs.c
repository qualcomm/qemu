/*
 * Turing Clock Controller (TURING_CC) — QEMU sysbus device
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/misc/qcom-turing-cc-regs.h"

#define R(off) ((off) / 4)

static uint64_t turing_cc_read(void *opaque, hwaddr addr, unsigned size)
{
    TuringCcState *s = opaque;

    if (size != 4 || addr >= TURING_CC_MMIO_SIZE || (addr & 3)) {
        return 0;
    }

    switch (addr) {
    case 0x000:
    case 0x040:
    case 0x26C:
    case 0x31C:
    case 0x42C:
        return s->regs[R(addr)];

    case 0x00C:
    case 0x018:
    case 0x01C:
    case 0x0C0:
    case 0x0C4:
    case 0x0C8:
    case 0x0CC:
    case 0x0D0:
    case 0x118:
    case 0x150:
    case 0x184:
    case 0x194:
    case 0x1B4:
    case 0x1C4:
    case 0x240:
    case 0x244:
    case 0x248:
    case 0x24C:
    case 0x250:
    case 0x254:
    case 0x2E0:
    case 0x338:
    case 0x408:
    case 0x414:
    case 0x418:
    case 0x41C:
    case 0x420:
    case 0x424:
    case 0x428:
    case 0x434:
    case 0x438:
        return s->regs[R(addr)] & ~(1u << 31);

    default:
        return s->regs[R(addr)];
    }
}

static void turing_cc_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    TuringCcState *s = opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4 || addr >= TURING_CC_MMIO_SIZE || (addr & 3)) {
        return;
    }

    switch (addr) {
    case 0x000:
    case 0x040:
    case 0x26C:
    case 0x31C:
    case 0x42C:
        s->regs[R(addr)] = v;
        if (v & 0x1u) {
            s->regs[R(addr)] &= ~((1u << 31) | 0x1u);
        }
        break;

    default:
        s->regs[R(addr)] = v;
        break;
    }
}

static const MemoryRegionOps turing_cc_ops = {
    .read = turing_cc_read,
    .write = turing_cc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

static void turing_cc_reset_hold(Object *obj, ResetType type)
{
    TuringCcState *s = TURING_CC(obj);
    static const struct { uint16_t off; uint32_t val; } reset[] = {
        { 0x004, 0x00100000 }, { 0x00C, 0x08000001 }, { 0x018, 0x08000001 },
        { 0x01C, 0x08000001 }, { 0x044, 0x00100000 }, { 0x0C0, 0x88000000 },
        { 0x0C4, 0x88000000 }, { 0x0C8, 0x88000000 }, { 0x0CC, 0x88000000 },
        { 0x0D0, 0x88000000 }, { 0x118, 0x88000000 }, { 0x13C, 0x02110001 },
        { 0x150, 0x88000000 }, { 0x184, 0x88000000 }, { 0x194, 0x88000000 },
        { 0x1B4, 0x88000000 }, { 0x1C4, 0x88000000 }, { 0x240, 0x88000000 },
        { 0x244, 0x88000000 }, { 0x248, 0x88000000 }, { 0x24C, 0x88000000 },
        { 0x250, 0x88000000 }, { 0x254, 0x88000000 }, { 0x270, 0x00100000 },
        { 0x280, 0x00000002 }, { 0x2B8, 0x00000002 }, { 0x2E0, 0x88000000 },
        { 0x320, 0x00100000 }, { 0x338, 0x88000000 }, { 0x408, 0x08000001 },
        { 0x414, 0x88000000 }, { 0x418, 0x88000000 }, { 0x41C, 0x88000000 },
        { 0x420, 0x88000000 }, { 0x424, 0x88000000 }, { 0x428, 0x88000000 },
        { 0x42C, 0x80000000 }, { 0x430, 0x00100000 }, { 0x434, 0x88000000 },
        { 0x438, 0x88000000 },
    };

    memset(s->regs, 0, sizeof(s->regs));
    for (size_t i = 0; i < ARRAY_SIZE(reset); i++) {
        s->regs[R(reset[i].off)] = reset[i].val;
    }
}

static void turing_cc_realize(DeviceState *dev, Error **errp)
{
    TuringCcState *s = TURING_CC(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &turing_cc_ops, s,
                          TYPE_TURING_CC, TURING_CC_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void turing_cc_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = turing_cc_realize;
    rc->phases.hold = turing_cc_reset_hold;
    dc->desc = "Turing Clock Controller (TURING_CC)";
}

static const TypeInfo turing_cc_info = {
    .name = TYPE_TURING_CC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringCcState),
    .class_init = turing_cc_class_init,
};

static void turing_cc_register_types(void)
{
    type_register_static(&turing_cc_info);
}

type_init(turing_cc_register_types)
