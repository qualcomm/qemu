#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "hw/core/irq.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "hw/intc/riscv_clic.h"

static void clic_update(RiscvCLICState *s) { bool active = false;
    for (uint32_t i = 0; i < s->num_sources; i++) if (s->pending[i] && s->enable[i]) { active = true; break; }
    qemu_set_irq(s->output, active); }
static void clic_set_irq(void *opaque, int irq, int level) { RiscvCLICState *s = opaque;
    if (irq >= 0 && (uint32_t)irq < s->num_sources) { s->pending[irq] = !!level; clic_update(s); } }
static uint64_t clic_read(void *opaque, hwaddr o, unsigned size) { RiscvCLICState *s = opaque; uint32_t v = 0;
    if (size != 4 || (o & 3)) return 0;
    uint32_t bank = (o >= RISCV_CLICINTIP_OFFSET && o < RISCV_CLICINTIP_OFFSET + 0x400) ? 1 :
                    (o >= RISCV_CLICINTIE_OFFSET && o < RISCV_CLICINTIE_OFFSET + 0x400) ? 2 :
                    (o >= RISCV_CLICINTCFG_OFFSET && o < RISCV_CLICINTCFG_OFFSET + 0x400) ? 3 : 0;
    if (bank) { uint32_t w = (o - (bank == 1 ? RISCV_CLICINTIP_OFFSET : bank == 2 ? RISCV_CLICINTIE_OFFSET : RISCV_CLICINTCFG_OFFSET)) / 4;
        for (uint32_t lane = 0; lane < 4; lane++) { uint32_t irq = w * 4 + lane; if (irq >= s->num_sources) continue;
            if (bank == 1 && s->pending[irq]) v |= 1U << (lane * 8);
            if (bank == 2 && s->enable[irq]) v |= 1U << (lane * 8);
            if (bank == 3) v |= (uint32_t)s->config[irq] << (lane * 8 + 4); } }
    else if (o == RISCV_CLICCFG_OFFSET) v = s->cliccfg; else if (o == RISCV_CLIC_CONTROL_OFFSET) v = s->control;
    return v; }
static void clic_write(void *opaque, hwaddr o, uint64_t data, unsigned size) { RiscvCLICState *s = opaque; uint32_t v = data;
    if (size != 4 || (o & 3)) return;
    uint32_t bank = (o >= RISCV_CLICINTIP_OFFSET && o < RISCV_CLICINTIP_OFFSET + 0x400) ? 1 :
                    (o >= RISCV_CLICINTIE_OFFSET && o < RISCV_CLICINTIE_OFFSET + 0x400) ? 2 :
                    (o >= RISCV_CLICINTCFG_OFFSET && o < RISCV_CLICINTCFG_OFFSET + 0x400) ? 3 : 0;
    if (bank) { uint32_t w = (o - (bank == 1 ? RISCV_CLICINTIP_OFFSET : bank == 2 ? RISCV_CLICINTIE_OFFSET : RISCV_CLICINTCFG_OFFSET)) / 4;
        for (uint32_t lane = 0; lane < 4; lane++) { uint32_t irq = w * 4 + lane; if (irq >= s->num_sources) continue;
            if (bank == 1 && (v & (1U << (lane * 8)))) s->pending[irq] = 0;
            if (bank == 2) s->enable[irq] = !!(v & (1U << (lane * 8)));
            if (bank == 3) s->config[irq] = (v >> (lane * 8 + 4)) & 0xf; } }
    else if (o == RISCV_CLICCFG_OFFSET) s->cliccfg = v & 0xff; else if (o == RISCV_CLIC_CONTROL_OFFSET) s->control = v;
    clic_update(s); }
static const MemoryRegionOps clic_ops = { .read = clic_read, .write = clic_write, .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 } };
static void clic_init(Object *obj) { RiscvCLICState *s = RISCV_CLIC(obj); s->num_sources = 32; s->aperture_size = RISCV_CLIC_MMIO_SIZE;
    memory_region_init_io(&s->mmio, obj, &clic_ops, s, TYPE_RISCV_CLIC, s->aperture_size);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio); qdev_init_gpio_in(DEVICE(obj), clic_set_irq, RISCV_CLIC_MAX_SOURCES); sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->output); }
static Property clic_props[] = { DEFINE_PROP_UINT32("num-sources", RiscvCLICState, num_sources, 32), DEFINE_PROP_UINT32("aperture-size", RiscvCLICState, aperture_size, RISCV_CLIC_MMIO_SIZE) };
static void clic_realize(DeviceState *d, Error **e) { RiscvCLICState *s = RISCV_CLIC(d); if (!s->num_sources || s->num_sources > RISCV_CLIC_MAX_SOURCES) error_setg(e, "invalid num-sources"); }
static void clic_class_init(ObjectClass *c, const void *d) { DeviceClass *dc = DEVICE_CLASS(c); dc->realize = clic_realize; device_class_set_props_n(dc, clic_props, ARRAY_SIZE(clic_props)); }
static const TypeInfo clic_info[] = {{ .name = TYPE_RISCV_CLIC, .parent = TYPE_SYS_BUS_DEVICE, .instance_size = sizeof(RiscvCLICState), .instance_init = clic_init, .class_init = clic_class_init }};
DEFINE_TYPES(clic_info)
