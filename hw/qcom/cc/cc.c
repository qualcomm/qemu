#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus-of.h"
#include "hw/qcom/cc/cc.h"

#include "hw/qcom/cc/gpucc.h"
#include "hw/qcom/cc/dispcc.h"
#include "hw/qcom/cc/gcc.h"

static const struct of_device_id cc_of_match_table[] = {
    { .compatible = GPUCC_COMPATIBLE, .data = &gpu_cc_canoe_desc },
    { .compatible = DISPCC_COMPATIBLE, .data = &disp_cc_canoe_desc },
    { .compatible = GCC_COMPATIBLE, .data = &gcc_canoe_desc },
    { },
};

QcomCCState* cc_create_by_label(void* fdt, void* in_fdt, const char* label, uint64_t mem_size) {
	DeviceState* dev = qdev_new(TYPE_QCOM_CC);
	QcomCCState* ccs = QCOM_CC(dev);

	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_IN_FDT, in_fdt);
	qdev_prop_set_ptr(dev, OF_SYSBUS_PARAM_FDT, fdt);
	qdev_prop_set_string(dev, OF_SYSBUS_PARAM_NODE_LABEL, label);

	ccs->mem_size = mem_size;
	ccs->name = label;

	sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

	return ccs;
}

// poorly optimized, ideally we should use a hashmap, or simply use more space.
static enum qcom_cc_reg_kind decode_addr(struct QcomCCState* ccs, struct qcom_cc_desc* desc, hwaddr addr)
{

    if (desc->gdscs) {
        // CC has gdscs
        for (size_t i = 0; i < desc->num_gdscs; ++i) {
            struct gdsc* gdsc = desc->gdscs[i];

            if (addr == gdsc->gdscr) {
                return CC_REG_GDSCR;
            } else if (addr == gdsc->gdscr + CFG_GDSCR_OFFSET) {
                return CC_REG_GDSCR_CFG;
            } else if (addr == gdsc->gds_hw_ctrl) {
                return CC_REG_HW_CTRL;
            }
        }
    }

    if (desc->alpha_plls) {
        // CC has plls
        for (size_t i = 0; i < desc->num_alpha_plls; ++i) {
            struct clk_alpha_pll* pll = desc->alpha_plls[i];

            if (addr == PLL_MODE(pll)) {
                return CC_REG_PLL_MODE;
            } else if (addr == PLL_L_VAL(pll)) {
                return CC_REG_PLL_L_VAL;
            } else if (addr == PLL_CAL_L_VAL(pll)) {
                return CC_REG_PLL_CAL_L_VAL;
            } else if (addr == PLL_ALPHA_VAL(pll)) {
                return CC_REG_PLL_ALPHA_VAL;
            } else if (addr == PLL_ALPHA_VAL_U(pll)) {
                return CC_REG_PLL_ALPHA_VAL_U;
            } else if (addr == PLL_USER_CTL(pll)) {
                return CC_REG_PLL_USER_CTL;
            }
        }
    }

    // TODO: fix this dirty hack
    if (addr == 0x90e4) {
        return CC_REG_CXO_CBCR;
    } else if (addr == 0x90d4) {
        return CC_REG_GMU_CBCR;
    }

    printf("[%s] Unknown address: 0x%lx\n", ccs->name, addr);

    return CC_REG_MAX;
}

static uint64_t qcom_cc_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomCCState* ccs = QCOM_CC(opaque);
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(opaque);
    struct qcom_cc_desc* cc_desc = (struct qcom_cc_desc*) ofdev->data;
    uint32_t val;

    enum qcom_cc_reg_kind reg = decode_addr(ccs, cc_desc, addr);

    switch (reg) {
        case CC_REG_PLL_MODE:
            val = ccs->reg[CC_REG_PLL_MODE];
            break;
        case CC_REG_PLL_L_VAL:
            val = ccs->reg[CC_REG_PLL_L_VAL];
            break;
        case CC_REG_PLL_CAL_L_VAL:
            val = ccs->reg[CC_REG_PLL_CAL_L_VAL];
            break;
        case CC_REG_PLL_ALPHA_VAL:
            val = ccs->reg[CC_REG_PLL_ALPHA_VAL];
            break;
        case CC_REG_PLL_ALPHA_VAL_U:
            val = ccs->reg[CC_REG_PLL_ALPHA_VAL_U];
            break;
        case CC_REG_PLL_USER_CTL:
            val = ccs->reg[CC_REG_PLL_USER_CTL];
            break;

        case CC_REG_GDSCR:
            val = ccs->reg[CC_REG_GDSCR];
            break;
        case CC_REG_GDSCR_CFG:
            val = ccs->reg[CC_REG_GDSCR_CFG];
            break;
        case CC_REG_HW_CTRL:
            val = ccs->reg[CC_REG_HW_CTRL];
            break;
        case CC_REG_CXO_CBCR:
            val = ccs->reg[CC_REG_CXO_CBCR];
            break;
        case CC_REG_GMU_CBCR:
            val = ccs->reg[CC_REG_GMU_CBCR];
            break;

        default:
            printf("[%s - !]\tRead @addr %lx failed, defaulting to 0.\n", ccs->name, addr);
            return 0;
    }

    printf("[%s] read @addr 0x%lx of value 0x%x\n", ccs->name, addr, val);

    return val;
}

static void qcom_cc_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomCCState* ccs = QCOM_CC(opaque);
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(opaque);
    struct qcom_cc_desc* cc_desc = (struct qcom_cc_desc*) ofdev->data;

    printf("[%s] write @addr 0x%lx of value %lx\n", ccs->name, addr, value);

    enum qcom_cc_reg_kind reg = decode_addr(ccs, cc_desc, addr);

    switch (reg) {
        case CC_REG_PLL_MODE:
            ccs->reg[CC_REG_PLL_MODE] = value;

            if (value & BIT(14)) {
                printf("ACK Latch\n");
                ccs->reg[CC_REG_PLL_MODE] |= BIT(13);
            }

			break;
        case CC_REG_PLL_L_VAL:
            ccs->reg[CC_REG_PLL_L_VAL] = value;
			break;
        case CC_REG_PLL_CAL_L_VAL:
            ccs->reg[CC_REG_PLL_CAL_L_VAL] = value;
			break;
        case CC_REG_PLL_ALPHA_VAL:
            ccs->reg[CC_REG_PLL_ALPHA_VAL] = value;
			break;
        case CC_REG_PLL_ALPHA_VAL_U:
            ccs->reg[CC_REG_PLL_ALPHA_VAL_U] = value;
			break;
        case CC_REG_PLL_USER_CTL:
            ccs->reg[CC_REG_PLL_USER_CTL] = value;
			break;

        case CC_REG_GDSCR:
            ccs->reg[CC_REG_GDSCR] = value;
            break;
        case CC_REG_GDSCR_CFG:
            ccs->reg[CC_REG_GDSCR_CFG] = value;
            break;
        case CC_REG_HW_CTRL:
            ccs->reg[CC_REG_HW_CTRL] = value;
            break;
        case CC_REG_GMU_CBCR: {
            if (value & 1) {
                value &= ~BIT(31);
            } else {
                value |= BIT(31);
            }

            printf("\twrite to GMU CBCR of value %lx\n", value);

            ccs->reg[CC_REG_GMU_CBCR] = value;
            break;
        }
        case CC_REG_CXO_CBCR: {
            if (value & 1) {
                value &= ~BIT(31);
            } else {
                value |= BIT(31);
            }

            printf("\twrite to CXO CBCR of value %lx\n", value);

            ccs->reg[CC_REG_CXO_CBCR] = value;
            break;
        }
        default:
            printf("[%s]\tWrite failed, nothing changed.\n", ccs->name);
            return;
    }
}

static const MemoryRegionOps qcom_cc_ops = {
    .read = qcom_cc_read,
    .write = qcom_cc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_cc_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomCCState* ccs = QCOM_CC(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);
    struct qcom_cc_desc* cc_desc = (struct qcom_cc_desc*) ofdev->data;
    assert(cc_desc);

    memcpy(ccs->reg, cc_desc->reset_regs, sizeof(qcom_cc_regs));

    memory_region_init_io(&ccs->iomem, OBJECT(ofdev), &qcom_cc_ops, ccs, TYPE_QCOM_CC, ccs->mem_size);
    sysbus_init_mmio(sbd, &ccs->iomem);
}

static void qcom_cc_init(Object* obj)
{
}

static void qcom_cc_class_init(ObjectClass* oc, void* data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(oc);

    kofdev->realize = qcom_cc_realize;
    kofdev->of_match_table = cc_of_match_table;
}

static const TypeInfo qcom_cc_info = {
    .name = TYPE_QCOM_CC,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomCCState),
    .instance_init = qcom_cc_init,
    .class_init = qcom_cc_class_init,
};

static void qcom_cc_register_types(void)
{
    type_register_static(&qcom_cc_info);
}

type_init(qcom_cc_register_types);
