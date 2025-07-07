#include "hw/qcom/graphics/gmu.h"
#include "hw/qcom/graphics.h"
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/qcom/graphics/gmu.h"
#include "hw/qcom/graphics/gen8_reg.h"
#include "hw/sysbus-of.h"

#define GX_GDSC_POWER_OFF	BIT(0)
#define GX_CLK_OFF		BIT(1)
#define is_on(val)		(!(val & (GX_GDSC_POWER_OFF | GX_CLK_OFF)))

static uint64_t qcom_global_gmu_read(QcomGmuState* s, hwaddr addr, unsigned size)
{
    OfSysBusDevice* of = OF_SYS_BUS_DEVICE(s);
    hwaddr addr_idx = qcom_graphics_decode_addr(addr);

    printf("[%s] read detected @addr 0x%lx (addr_idx = 0x%lx)\n", of->name, addr, addr_idx);

    switch (addr_idx) {
        case GEN8_GMUCX_GFX_PWR_CLK_STATUS: {
            uint32_t read_val = s->regs[addr_idx];
            printf("\tpwr clk status: 0x%x\n", read_val);
            return read_val;
        }
        case GEN8_GMUCX_CM3_FW_INIT_RESULT: {
            /* completely undocumented register */
            return s->regs[GEN8_GMUCX_CM3_FW_INIT_RESULT];
        }
        case GEN8_GMUCX_HFI_CTRL_STATUS: {
            return s->regs[GEN8_GMUCX_HFI_CTRL_STATUS];
        }
        case GEN8_GMUCX_GMU2HOST_INTR_INFO: {
            return 1;
        }
        default: {
            printf("\tUnknown addr\n");
        }
    }

    return 0;
}

static void qcom_global_gmu_write(QcomGmuState* s, hwaddr addr, uint64_t value, unsigned size)
{
    OfSysBusDevice* of = OF_SYS_BUS_DEVICE(s);
    hwaddr addr_idx = qcom_graphics_decode_addr(addr);

    printf("[%s] write detected @addr 0x%lx (addr_idx = 0x%lx) of value 0x%lx\n", of->name, addr, addr_idx, value);

    switch(addr_idx) {
        case GEN8_GMUAO_RSCC_CONTROL_REQ: {
            uint32_t is_off = GX_GDSC_POWER_OFF | GX_CLK_OFF;

            if (value & BIT(0)) {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] |= is_off;
            }

            if (value & BIT(1)) {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] &= ~is_off;
            }

            printf("\tpwr rscc control req\n");

            break;
        }
        case GEN8_GMUCX_CM3_FW_INIT_RESULT:
            s->regs[GEN8_GMUCX_CM3_FW_INIT_RESULT] = value;
            break;
        case GEN8_GMUCX_HFI_CTRL_INIT: {
            if (value & BIT(0)) {
                s->regs[GEN8_GMUCX_HFI_CTRL_STATUS] = BIT(0);
            }
            break;
        }
        case GEN8_GMUCX_CM3_SYSRESET: {
            uint32_t is_off = GX_GDSC_POWER_OFF | GX_CLK_OFF;

            if (value & BIT(0)) {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] |= is_off;
            } else {
                s->regs[GEN8_GMUCX_GFX_PWR_CLK_STATUS] &= ~is_off;
            }

            s->regs[GEN8_GMUCX_CM3_FW_INIT_RESULT] = BIT(8);

            printf("\tcm3 sysreset\n");

            break;
        }
        default: {
            printf("\tUnknown addr\n");
            break;
        }
    }
}

static uint64_t qcom_gmu_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomGmuState* s = QCOM_GMU(opaque);

    return qcom_global_gmu_read(s, addr + s->gpu_offset, size);
}

static void qcom_gmu_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomGmuState* s = QCOM_GMU(opaque);

    qcom_global_gmu_write(s, addr + s->gpu_offset, value, size);
}

static uint64_t qcom_gmu_ao_blk_dec0_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomGmuState* s = QCOM_GMU(opaque);

    return qcom_global_gmu_read(s, addr + s->gpu_offset_ao_blk, size);
}

static void qcom_gmu_ao_blk_dec0_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomGmuState* s = QCOM_GMU(opaque);

    qcom_global_gmu_write(s, addr + s->gpu_offset_ao_blk, value, size);
}

static void qcom_gmu_init(Object* obj)
{
}

static const MemoryRegionOps qcom_gmu_ops = {
    .read = qcom_gmu_read,
    .write = qcom_gmu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps qcom_gmu_ao_blk_dec0_ops = {
    .read = qcom_gmu_ao_blk_dec0_read,
    .write = qcom_gmu_ao_blk_dec0_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_gmu_realize(OfSysBusDevice* of, Error **errp)
{
    QcomGmuState *s = QCOM_GMU(of);
    SysBusDevice* sbd = SYS_BUS_DEVICE(of);

    memory_region_init_io(&s->iomem, OBJECT(of), &qcom_gmu_ops, s, TYPE_QCOM_GMU, of->regs[0].size);
    sysbus_init_mmio(sbd, &s->iomem);

    memory_region_init_io(&s->iomem_ao_blk, OBJECT(of), &qcom_gmu_ao_blk_dec0_ops, s, TYPE_QCOM_GMU "-ao-blk", of->regs[1].size);
    sysbus_init_mmio(sbd, &s->iomem_ao_blk);

    GArray* props = qemu_fdt_collect_phandle_props(of->fdt, of->node_path, errp);
    for (size_t i = 0; i < props->len; ++i) {
        struct fdt_phandle_prop_data* prop_data = ((struct fdt_phandle_prop_data*) props->data) + i;
        if (prop_data->kind == FDT_PROP_IOMMU) {
            assert(!s->smmu);

            s->smmu = QCOM_SMMU(of_sysbus_find_by_phandle(prop_data->phandle));
            s->vmid = ((uint32_t*) prop_data->params->data)[1];
        }
    }

    assert(s->smmu);
}

static void qcom_gmu_class_init(ObjectClass* oc, void* data)
{
    OfSysBusDeviceClass* klass = OF_SYS_BUS_DEVICE_CLASS(oc);

    klass->realize = qcom_gmu_realize;
}

static const TypeInfo qcom_gmu_info = {
    .name = TYPE_QCOM_GMU,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomGmuState),
    .instance_init = qcom_gmu_init,
    .class_init = qcom_gmu_class_init,
};

static void qcom_gmu_register_types(void)
{
    type_register_static(&qcom_gmu_info);
}

type_init(qcom_gmu_register_types);

