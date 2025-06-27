#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus-of.h"
#include "hw/qcom/cc/cc.h"

#include "hw/qcom/cc/gpucc.h"
#include "hw/qcom/cc/dispcc.h"

static const struct of_device_id cc_of_match_table[] = {
    { .compatible = GPUCC_COMPATIBLE, .data = &gpu_cc_canoe_desc },
    { .compatible = DISPCC_COMPATIBLE, .data = &disp_cc_canoe_desc },
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

// poorly optimized, ideally we should use a hashmap.
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

    if (desc->plls) {
        // CC has plls
        for (size_t i = 0; i < desc->num_plls; ++i) {
            struct clk_alpha_pll* pll = desc->plls[i];

            if (addr == PLL_MODE(pll)) {
                return CC_REG_PLL_MODE;
            }
        }
    }

    printf("[%s] Unknown address: 0x%lx\n", ccs->name, addr);

    return CC_REG_MAX;
}

static uint64_t qcom_cc_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomCCState* ccs = QCOM_CC(opaque);
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(opaque);
    struct qcom_cc_desc* cc_desc = (struct qcom_cc_desc*) ofdev->data;

    printf("[%s] read detected @addr 0x%lx\n", ccs->name, addr);

    enum qcom_cc_reg_kind reg = decode_addr(ccs, cc_desc, addr);

    switch (reg) {
        case CC_REG_PLL_MODE:
            return ccs->reg[CC_REG_PLL_MODE];
        case CC_REG_GDSCR:
            return ccs->reg[CC_REG_GDSCR];
        case CC_REG_GDSCR_CFG:
            return ccs->reg[CC_REG_GDSCR_CFG];
        case CC_REG_HW_CTRL:
            return ccs->reg[CC_REG_HW_CTRL];
        default:
            printf("[%s]\tRead failed, defaulting to 0.\n", ccs->name);
            return 0;
    }
}

static void qcom_cc_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomCCState* ccs = QCOM_CC(opaque);
    OfSysBusDevice* ofdev = OF_SYS_BUS_DEVICE(opaque);
    struct qcom_cc_desc* cc_desc = (struct qcom_cc_desc*) ofdev->data;

    printf("[%s] write detected @addr 0x%lx\n", ccs->name, addr);

    enum qcom_cc_reg_kind reg = decode_addr(ccs, cc_desc, addr);

    switch (reg) {
        case CC_REG_PLL_MODE:
            ccs->reg[CC_REG_PLL_MODE] = value;
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
