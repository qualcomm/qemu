#include "qemu/osdep.h"
#include "hw/qcom/graphics.h"
#include "hw/qcom/graphics/gen8_reg.h"

#define GX_GDSC_POWER_OFF	BIT(0)
#define GX_CLK_OFF		BIT(1)
#define is_on(val)		(!(val & (GX_GDSC_POWER_OFF | GX_CLK_OFF)))

static hwaddr decode_addr(hwaddr addr)
{
    return addr >> 2;
}

static bool is_snapshot_addr(hwaddr addr_idx) {
    switch (addr_idx) {
        case GEN8_CP_SQE_STAT_ADDR_PIPE:
        case GEN8_CP_DRAW_STATE_ADDR_PIPE:
        case GEN8_CP_ROQ_DBG_ADDR_PIPE:
        case GEN8_CP_SQE_UCODE_DBG_ADDR_PIPE:
        case GEN8_CP_RESOURCE_TABLE_DBG_ADDR_BV:
        case GEN8_CP_FIFO_DBG_ADDR_DDE_PIPE:
        case GEN8_CP_SQE_STAT_DATA_PIPE:
        case GEN8_CP_DRAW_STATE_DATA_PIPE:
        case GEN8_CP_ROQ_DBG_DATA_PIPE:
        case GEN8_CP_SQE_UCODE_DBG_DATA_PIPE:
        case GEN8_CP_RESOURCE_TABLE_DBG_DATA_BV:
        case GEN8_CP_FIFO_DBG_DATA_DDE_PIPE:

        case GEN8_CX_DBGC_TCM_DBG_DATA:
            return true;
    }

    return false;
}

static bool is_rsc_addr(hwaddr addr)
{
    return addr >= 0x50000 && addr < 0x50000 + 0x10000;
}

static uint64_t qcom_graphics_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomGraphicsState *s = QCOM_GRAPHICS(opaque);

    hwaddr addr_idx = decode_addr(addr);

    if (is_snapshot_addr(addr_idx)) {
        // it's very spammy, let's quick ignore these ones.
        return 0;
    }

    printf("[qcom_gpu] read detected @addr 0x%lx (addr_idx = 0x%lx)\n", addr, addr_idx);

    if (is_rsc_addr(addr)) {
        addr_idx = decode_addr(addr - 0x50000);

        switch (addr_idx) {
            case GEN8_RSCC_TCS0_DRV0_STATUS:
            case GEN8_RSCC_TCS1_DRV0_STATUS:
            case GEN8_RSCC_TCS2_DRV0_STATUS:
            case GEN8_RSCC_TCS3_DRV0_STATUS:
            case GEN8_RSCC_TCS4_DRV0_STATUS:
            case GEN8_RSCC_TCS5_DRV0_STATUS:
            case GEN8_RSCC_TCS6_DRV0_STATUS:
            case GEN8_RSCC_TCS7_DRV0_STATUS:
            case GEN8_RSCC_TCS8_DRV0_STATUS:
            case GEN8_RSCC_TCS9_DRV0_STATUS: {
                printf("\tTCS read\n");
                return 1;
            }
            default: {
                printf("\t(RSC) Unknown addr\n");
            }
        }
    } else {
        switch (addr_idx) {
            case GEN8_GBIF_REINIT_DONE: {
                printf("\tGBIF reinit done request\n");
                return 1;
            }
            case GEN8_GBIF_HALT_ACK: {
                printf("\tGBIF halt ACK\n");
                return GEN8_GBIF_ARB_HALT_MASK;
            }
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
    }


    return 0;
}

static void qcom_graphics_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomGraphicsState *s = QCOM_GRAPHICS(opaque);

    hwaddr addr_idx = decode_addr(addr);

    if (is_snapshot_addr(addr_idx)) {
        // it's very spammy, let's quick ignore these ones.
        return;
    }

    printf("[qcom_gpu] write detected @addr 0x%lx (addr_idx = 0x%lx) of value 0x%lx\n", addr, addr_idx, value);

    if (is_rsc_addr(addr)) {
        addr_idx = decode_addr(addr - 0x50000);

        switch (addr_idx) {
            default: {
                printf("\t(RSC) Unknown addr\n");
            }
        }
    } else {
        switch (addr_idx) {
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
            case GEN8_GMUCX_CM3_FW_INIT_RESULT:
                s->regs[GEN8_GMUCX_CM3_FW_INIT_RESULT] = value;
                break;
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
            case GEN8_GMUCX_HFI_CTRL_INIT: {
                if (value & BIT(0)) {
                    s->regs[GEN8_GMUCX_HFI_CTRL_STATUS] = BIT(0);
                }
                break;
            }
            default: {
                printf("\tUnknown addr\n");
                break;
            }
        }
    }

}

static void qcom_graphics_init(Object* obj)
{
}

static const MemoryRegionOps qcom_graphics_ops = {
    .read = qcom_graphics_read,
    .write = qcom_graphics_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_graphics_realize(DeviceState* dev, Error **errp)
{
    QcomGraphicsState *s = QCOM_GRAPHICS(dev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(dev);

    // Put the whole MMIO range all together.
    // We need to do this because some components sometimes interact
    // with other components "dirtily" (like KGSL with GMU).
    memory_region_init_io(&s->iomem, OBJECT(dev), &qcom_graphics_ops, s, TYPE_QCOM_GRAPHICS, QCOM_GRAPHICS_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void qcom_graphics_class_init(ObjectClass* oc, void* data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = qcom_graphics_realize;
}

static const TypeInfo qcom_graphics_info = {
    .name = TYPE_QCOM_GRAPHICS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomGraphicsState),
    .instance_init = qcom_graphics_init,
    .class_init = qcom_graphics_class_init,
};

static void qcom_graphics_register_types(void)
{
    type_register_static(&qcom_graphics_info);
}

type_init(qcom_graphics_register_types);
