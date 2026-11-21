/*
 * QEMU RISC-V CLIC Interrupt Controller
 *
 * Copyright(c) Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/sysbus.h"
#include "hw/intc/riscv_clic.h"
#include "qapi/error.h"
#include "qemu/log.h"

static bool clic_source_implemented(unsigned int irq)
{
    return irq == 3 || irq == 7 || irq == 11 || irq == 12 || irq >= 16;
}

static void clic_update(RiscvCLICState *s)
{
    qemu_set_irq(s->output, riscv_clic_pending(s) >= 0);
}

static void clic_set_irq(void *opaque, int irq, int level)
{
    RiscvCLICState *s = opaque;

    if (irq < 0 || irq >= s->num_sources || !clic_source_implemented(irq)) {
        return;
    }
    s->input_level[irq] = !!level;
    s->pending[irq] = !!level || (irq < 16 && s->software_pending[irq]);
    clic_update(s);
}

static uint8_t clic_read_byte(RiscvCLICState *s, hwaddr offset)
{
    unsigned int irq;

    if (offset >= RISCV_CLICINTIP_OFFSET &&
        offset < RISCV_CLICINTIP_OFFSET + s->num_sources) {
        irq = offset - RISCV_CLICINTIP_OFFSET;
        return s->pending[irq];
    }
    if (offset >= RISCV_CLICINTIE_OFFSET &&
        offset < RISCV_CLICINTIE_OFFSET + s->num_sources) {
        irq = offset - RISCV_CLICINTIE_OFFSET;
        return s->enable[irq];
    }
    if (offset >= RISCV_CLICINTCFG_OFFSET &&
        offset < RISCV_CLICINTCFG_OFFSET + s->num_sources) {
        irq = offset - RISCV_CLICINTCFG_OFFSET;
        return clic_source_implemented(irq) ? (s->config[irq] << 4) | 0xf : 0;
    }
    if (offset == RISCV_CLICCFG_OFFSET) {
        return s->cliccfg;
    }
    if (offset == RISCV_CLIC_CONTROL_OFFSET) {
        return s->control;
    }
    return 0;
}

static void clic_write_byte(RiscvCLICState *s, hwaddr offset, uint8_t value)
{
    unsigned int irq;

    if (offset >= RISCV_CLICINTIP_OFFSET &&
        offset < RISCV_CLICINTIP_OFFSET + s->num_sources) {
        irq = offset - RISCV_CLICINTIP_OFFSET;
        /* External pending state is cleared at the source/conditioner. */
        if (irq == 3 || irq == 12) {
            s->software_pending[irq] = value & 1;
            s->pending[irq] = s->software_pending[irq] | s->input_level[irq];
        }
    } else if (offset >= RISCV_CLICINTIE_OFFSET &&
               offset < RISCV_CLICINTIE_OFFSET + s->num_sources) {
        irq = offset - RISCV_CLICINTIE_OFFSET;
        if (clic_source_implemented(irq)) {
            s->enable[irq] = value & 1;
        }
    } else if (offset >= RISCV_CLICINTCFG_OFFSET &&
               offset < RISCV_CLICINTCFG_OFFSET + s->num_sources) {
        irq = offset - RISCV_CLICINTCFG_OFFSET;
        if (clic_source_implemented(irq)) {
            s->config[irq] = value >> 4;
        }
    } else if (offset == RISCV_CLICCFG_OFFSET) {
        s->cliccfg = value & 0x7f;
    } else if (offset == RISCV_CLIC_CONTROL_OFFSET) {
        s->control = value & 1;
    }
}

static uint64_t clic_read(void *opaque, hwaddr offset, unsigned size)
{
    RiscvCLICState *s = opaque;
    uint64_t value = 0;

    for (unsigned int i = 0; i < size; i++) {
        value |= (uint64_t)clic_read_byte(s, offset + i) << (8 * i);
    }
    return value;
}

static void clic_write(void *opaque, hwaddr offset, uint64_t value,
                       unsigned size)
{
    RiscvCLICState *s = opaque;

    for (unsigned int i = 0; i < size; i++) {
        clic_write_byte(s, offset + i, value >> (8 * i));
    }
    clic_update(s);
}

static uint64_t clic_control_read(void *opaque, hwaddr offset, unsigned size)
{
    return clic_read(opaque, RISCV_CLIC_CONTROL_OFFSET + offset, size);
}

static void clic_control_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned size)
{
    clic_write(opaque, RISCV_CLIC_CONTROL_OFFSET + offset, value, size);
}

static uint64_t clic_interrupt_read(void *opaque, hwaddr offset, unsigned size)
{
    return clic_read(opaque, RISCV_CLICINTIP_OFFSET + offset, size);
}

static void clic_interrupt_write(void *opaque, hwaddr offset, uint64_t value,
                                 unsigned size)
{
    clic_write(opaque, RISCV_CLICINTIP_OFFSET + offset, value, size);
}

static const MemoryRegionOps clic_control_ops = {
    .read = clic_control_read,
    .write = clic_control_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 1, .max_access_size = 4 },
    .impl = { .min_access_size = 1, .max_access_size = 4 },
};

static const MemoryRegionOps clic_interrupt_ops = {
    .read = clic_interrupt_read,
    .write = clic_interrupt_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 1, .max_access_size = 4 },
    .impl = { .min_access_size = 1, .max_access_size = 4 },
};

static void clic_reset_state(RiscvCLICState *s)
{
    memset(s->pending, 0, sizeof(s->pending));
    memset(s->software_pending, 0, sizeof(s->software_pending));
    memset(s->enable, 0, sizeof(s->enable));
    memset(s->config, 0, sizeof(s->config));
    s->cliccfg = 0;
    s->control = 1;
    for (unsigned int i = 0; i < s->num_sources; i++) {
        s->pending[i] = s->input_level[i];
    }
    clic_update(s);
}

static void clic_reset(DeviceState *dev)
{
    clic_reset_state(RISCV_CLIC(dev));
}

static const Property clic_props[] = {
    DEFINE_PROP_UINT32("num-sources", RiscvCLICState, num_sources, 32),
    DEFINE_PROP_UINT32("aperture-size", RiscvCLICState, aperture_size,
                       RISCV_CLIC_MMIO_SIZE),
};

static void clic_realize(DeviceState *dev, Error **errp)
{
    RiscvCLICState *s = RISCV_CLIC(dev);

    if (!s->num_sources || s->num_sources > RISCV_CLIC_MAX_SOURCES) {
        error_setg(errp, "num-sources must be between 1 and %u",
                   RISCV_CLIC_MAX_SOURCES);
        return;
    }
    if (s->aperture_size < RISCV_CLICCFG_OFFSET + 4) {
        error_setg(errp, "aperture-size does not cover the CLIC registers");
        return;
    }
    memory_region_init_io(&s->control_mmio, OBJECT(dev), &clic_control_ops, s,
                          "clic-control", RISCV_CLIC_WINDOW_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->control_mmio);
    memory_region_init_io(&s->interrupt_mmio, OBJECT(dev), &clic_interrupt_ops, s,
                          "clic-interrupts", RISCV_CLIC_WINDOW_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->interrupt_mmio);
    qdev_init_gpio_in(dev, clic_set_irq, s->num_sources);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->output);
    clic_reset(dev);
}

static void clic_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = clic_realize;
    device_class_set_legacy_reset(dc, clic_reset);
    device_class_set_props(dc, clic_props);
}

static const TypeInfo clic_info[] = {
    {
        .name = TYPE_RISCV_CLIC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(RiscvCLICState),
        .class_init = clic_class_init,
    },
};

DEFINE_TYPES(clic_info)
