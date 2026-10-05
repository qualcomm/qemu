/*
 * DSPSS_PUB device
 *
 * Copyright (c) 2025 Qualcomm Technologies, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/dspss-pub.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "migration/vmstate.h"
#include "hw/core/irq.h"

#define DSPSS_PUB_REG_SIZE 0x3000

/* Register offsets */
#define QDSP6SS_VERSION                       0x00000
#define QDSP6SS_DBG_CFG                       0x00018
#define QDSP6SS_RET_CFG                       0x0001C
#define QDSP6SS_NMI                           0x00040
#define QDSP6SS_NMI_STATUS                    0x00044
#define QDSP6SS_BOOT_ARG_0                    0x00060
#define QDSP6SS_BOOT_ARG_1                    0x00064
#define QDSP6SS_BOOT_ARG_2                    0x00068
#define QDSP6SS_BOOT_ARG_3                    0x0006C
#define QDSP6SS_BOOT_ARG_4                    0x00070
#define QDSP6SS_BOOT_ARG_5                    0x00074
#define QDSP6SS_STRAP_TCM_BASE_STATUS         0x00100
#define QDSP6SS_STRAP_AHBUPPER_STATUS         0x00104
#define QDSP6SS_STRAP_AHBLOWER_STATUS         0x00108
#define QDSP6SS_STRAP_AHBS_BASE_STATUS        0x0010C
#define QDSP6SS_STRAP_CLADE_RWCFG_BASE_STATUS 0x00110
#define QDSP6SS_STRAP_AXIM2UPPER_STATUS       0x0011C
#define QDSP6SS_STRAP_AXIM2LOWER_STATUS       0x00120
#define QDSP6SS_DBG_NMI_PWR_STATUS            0x00304
#define QDSP6SS_BOOT_CORE_START               0x00400
#define QDSP6SS_BOOT_CMD                      0x00404
#define QDSP6SS_BOOT_STATUS                   0x00408
#define QDSP6SS_MEM_STATUS                    0x00438
#define QDSP6SS_L2MEM_EFUSE_STATUS            0x00490
#define QDSP6SS_CP_CLK_CTL                    0x00508
#define QDSP6SS_CPMEM_STATUS                  0x00528
#define QDSP6SS_L2ITCM_STATUS                 0x00538
#define QDSP6SS_DPM_CTL                       0x00800
#define QDSP6SS_DPM_STOP_STATUS               0x00804
#define QDSP6SS_LMH_CTL                       0x00818
#define QDSP6SS_LMH_STATUS                    0x0081C
#define QDSP6SS_LMH_CFG                       0x00824
#define QDSP6SS_ISENSE_STATUS                 0x00830
#define QDSP6SS_HMX_LMH_CTL                   0x00838
#define QDSP6SS_HMX_LMH_STATUS                0x0083C
#define QDSP6SS_TEST_BUS_VALUE                0x02004
#define QDSP6SS_HMX_STATUS                    0x02024
#define QDSP6SS_CORE_STATUS                   0x02028
#define QDSP6SS_INTF_HALTACK                  0x0208C
#define QDSP6SS_INTFCLAMP_STATUS              0x02098
#define QDSP6SS_CORE_BHS_STATUS               0x02418
#define QDSP6SS_RESET_STATUS                  0x02448
#define QDSP6SS_CLAMP_STATUS                  0x02458
#define QDSP6SS_CLK_STATUS                    0x02468
#define QDSP6SS_MEM_STAGGER_RESET_STATUS      0x02478

/* Register field masks */
#define NMI_CLEAR_STATUS_MASK 0x02
#define NMI_SET_MASK          0x01
#define BOOT_CORE_MASK        0x01
#define BOOT_CMD_MASK         0x01
#define PUBCSR_TRIG_MASK      0x01

typedef struct DSPSS_PUBState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    /* GPIO outputs */
    qemu_irq nmi_out;
    qemu_irq hex_halt_out;

    /* Register values */
    uint32_t dbg_cfg;
    uint32_t ret_cfg;
    uint32_t cp_clk_ctl;

    /* State variables */
    bool nmi_triggered;
    bool boot_core_start;
    bool boot_status;
    uint32_t boot_args[6];
    uint32_t dpm_ctl;
    uint32_t lmh_ctl;
    uint32_t lmh_cfg;
    uint32_t hmx_lmh_ctl;
} DSPSS_PUBState;

static uint64_t dspss_pub_read(void *opaque, hwaddr offset, unsigned size)
{
    DSPSS_PUBState *s = DSPSS_PUB(opaque);
    uint64_t value = 0;

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR, "dspss_pub: read with size %d at offset 0x%"
                      HWADDR_PRIx "\n", size, offset);
        return 0;
    }

    switch (offset) {
    case QDSP6SS_VERSION:
        value = 0x10020000;
        break;
    case QDSP6SS_DBG_CFG:
        value = s->dbg_cfg;
        break;
    case QDSP6SS_RET_CFG:
        value = s->ret_cfg;
        break;
    case QDSP6SS_NMI_STATUS:
        value = s->nmi_triggered ? PUBCSR_TRIG_MASK : 0x0;
        break;
    case QDSP6SS_STRAP_TCM_BASE_STATUS:
    case QDSP6SS_STRAP_AHBUPPER_STATUS:
    case QDSP6SS_STRAP_AHBLOWER_STATUS:
    case QDSP6SS_STRAP_AHBS_BASE_STATUS:
    case QDSP6SS_STRAP_CLADE_RWCFG_BASE_STATUS:
    case QDSP6SS_STRAP_AXIM2UPPER_STATUS:
    case QDSP6SS_STRAP_AXIM2LOWER_STATUS:
    case QDSP6SS_DBG_NMI_PWR_STATUS:
        value = 0x0;
        break;
    case QDSP6SS_BOOT_STATUS:
        value = s->boot_status ? 0x1 : 0x0;
        break;
    case QDSP6SS_BOOT_ARG_0:
        value = s->boot_args[0];
        break;
    case QDSP6SS_BOOT_ARG_1:
        value = s->boot_args[1];
        break;
    case QDSP6SS_BOOT_ARG_2:
        value = s->boot_args[2];
        break;
    case QDSP6SS_BOOT_ARG_3:
        value = s->boot_args[3];
        break;
    case QDSP6SS_BOOT_ARG_4:
        value = s->boot_args[4];
        break;
    case QDSP6SS_BOOT_ARG_5:
        value = s->boot_args[5];
        break;
    case QDSP6SS_MEM_STATUS:
        value = 0x1F001F;
        break;
    case QDSP6SS_L2MEM_EFUSE_STATUS:
        value = 0xF;
        break;
    case QDSP6SS_CP_CLK_CTL:
        value = s->cp_clk_ctl;
        break;
    case QDSP6SS_CPMEM_STATUS:
    case QDSP6SS_L2ITCM_STATUS:
        value = 0x0;
        break;
    case QDSP6SS_DPM_CTL:
        value = s->dpm_ctl;
        break;
    case QDSP6SS_DPM_STOP_STATUS:
        value = s->dpm_ctl & 0x1;
        break;
    case QDSP6SS_LMH_CTL:
        value = s->lmh_ctl;
        break;
    case QDSP6SS_LMH_STATUS:
        value = s->lmh_ctl & 0x1;
        break;
    case QDSP6SS_LMH_CFG:
        value = s->lmh_cfg;
        break;
    case QDSP6SS_HMX_LMH_CTL:
        value = s->hmx_lmh_ctl;
        break;
    case QDSP6SS_HMX_LMH_STATUS:
        value = s->hmx_lmh_ctl & 0x1;
        break;
    case QDSP6SS_ISENSE_STATUS:
    case QDSP6SS_TEST_BUS_VALUE:
        value = 0x0;
        break;
    case QDSP6SS_HMX_STATUS:
        value = 0x14;
        break;
    case QDSP6SS_CORE_STATUS:
    case QDSP6SS_INTF_HALTACK:
    case QDSP6SS_INTFCLAMP_STATUS:
    case QDSP6SS_CORE_BHS_STATUS:
    case QDSP6SS_RESET_STATUS:
    case QDSP6SS_CLAMP_STATUS:
        value = 0x0;
        break;
    case QDSP6SS_CLK_STATUS:
        value = 0x1FFF;
        break;
    case QDSP6SS_MEM_STAGGER_RESET_STATUS:
        value = 0x0;
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "dspss_pub: read from unknown offset 0x%"
                      HWADDR_PRIx "\n", offset);
        break;
    }

    return value;
}

static void dspss_pub_write(void *opaque, hwaddr offset, uint64_t value,
                         unsigned size)
{
    DSPSS_PUBState *s = DSPSS_PUB(opaque);

    if (size != 4) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dspss_pub: write with size %d at offset 0x%"
                      HWADDR_PRIx "\n", size, offset);
        return;
    }

    switch (offset) {
    case QDSP6SS_NMI:
        if (value & NMI_SET_MASK) {
            s->nmi_triggered = true;
            /* Pulse NMI signal as per SystemC model */
            qemu_irq_raise(s->nmi_out);
            qemu_irq_lower(s->nmi_out);
        }
        if (value & NMI_CLEAR_STATUS_MASK) {
            s->nmi_triggered = false;
        }
        break;
    case QDSP6SS_BOOT_ARG_0:
        s->boot_args[0] = value;
        break;
    case QDSP6SS_BOOT_ARG_1:
        s->boot_args[1] = value;
        break;
    case QDSP6SS_BOOT_ARG_2:
        s->boot_args[2] = value;
        break;
    case QDSP6SS_BOOT_ARG_3:
        s->boot_args[3] = value;
        break;
    case QDSP6SS_BOOT_ARG_4:
        s->boot_args[4] = value;
        break;
    case QDSP6SS_BOOT_ARG_5:
        s->boot_args[5] = value;
        break;
    case QDSP6SS_BOOT_CORE_START:
        s->boot_core_start = (value & BOOT_CORE_MASK);
        break;
    case QDSP6SS_BOOT_CMD:
        s->boot_status = (value & BOOT_CMD_MASK);
        /* Signal hex_halt as inverted boot_status as per SystemC model */
        qemu_set_irq(s->hex_halt_out, !s->boot_status);
        break;
    case QDSP6SS_DBG_CFG:
        s->dbg_cfg = value;
        break;
    case QDSP6SS_RET_CFG:
        s->ret_cfg = value;
        break;
    case QDSP6SS_CP_CLK_CTL:
        s->cp_clk_ctl = value;
        break;
    case QDSP6SS_DPM_CTL:
        s->dpm_ctl = value;
        break;
    case QDSP6SS_LMH_CTL:
        s->lmh_ctl = value;
        break;
    case QDSP6SS_LMH_CFG:
        s->lmh_cfg = value;
        break;
    case QDSP6SS_HMX_LMH_CTL:
        s->hmx_lmh_ctl = value;
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "dspss_pub: write to unknown offset 0x%"
                      HWADDR_PRIx " value 0x%" PRIx64 "\n", offset, value);
        break;
    }
}

static const MemoryRegionOps dspss_pub_ops = {
    .read = dspss_pub_read,
    .write = dspss_pub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void dspss_pub_reset_hold(Object *obj, ResetType type)
{
    DSPSS_PUBState *s = DSPSS_PUB(obj);

    s->nmi_triggered = false;
    s->boot_core_start = false;
    s->boot_status = false;
    s->dbg_cfg = 0;
    s->ret_cfg = 0;
    s->cp_clk_ctl = 0;
    s->dpm_ctl = 0x1;
    s->lmh_ctl = 0x1;
    s->lmh_cfg = 0x3;
    s->hmx_lmh_ctl = 0x1;

    /* Reset GPIO outputs if they've been initialized */
    if (s->nmi_out) {
        qemu_set_irq(s->nmi_out, 0);
    }
    if (s->hex_halt_out) {
        qemu_set_irq(s->hex_halt_out, 1); /* hex_halt is inverted boot_status */
    }
}

static void dspss_pub_realize(DeviceState *dev, Error **errp)
{
    DSPSS_PUBState *s = DSPSS_PUB(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &dspss_pub_ops, s,
                          TYPE_DSPSS_PUB, DSPSS_PUB_REG_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);

    /* Initialize GPIO outputs */
    qdev_init_gpio_out_named(dev, &s->nmi_out, "nmi", 1);
    qdev_init_gpio_out_named(dev, &s->hex_halt_out, "hex-halt", 1);

    /* Set initial state of GPIO outputs */
    qemu_set_irq(s->nmi_out, 0);
    qemu_set_irq(s->hex_halt_out, 1); /* hex_halt is inverted boot_status */
}

static const Property dspss_properties[] = {
    DEFINE_PROP_UINT32("boot-arg-0", DSPSS_PUBState, boot_args[0], 0),
    DEFINE_PROP_UINT32("boot-arg-1", DSPSS_PUBState, boot_args[1], 0),
    DEFINE_PROP_UINT32("boot-arg-2", DSPSS_PUBState, boot_args[2], 0),
    DEFINE_PROP_UINT32("boot-arg-3", DSPSS_PUBState, boot_args[3], 0),
    DEFINE_PROP_UINT32("boot-arg-4", DSPSS_PUBState, boot_args[4], 0),
    DEFINE_PROP_UINT32("boot-arg-5", DSPSS_PUBState, boot_args[5], 0),
};

static const VMStateDescription vmstate_dspss_pub = {
    .name = "dspss-pub",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(dbg_cfg, DSPSS_PUBState),
        VMSTATE_UINT32(ret_cfg, DSPSS_PUBState),
        VMSTATE_UINT32(cp_clk_ctl, DSPSS_PUBState),
        VMSTATE_BOOL(nmi_triggered, DSPSS_PUBState),
        VMSTATE_BOOL(boot_core_start, DSPSS_PUBState),
        VMSTATE_BOOL(boot_status, DSPSS_PUBState),
        VMSTATE_UINT32_ARRAY(boot_args, DSPSS_PUBState, 6),
        VMSTATE_UINT32(dpm_ctl, DSPSS_PUBState),
        VMSTATE_UINT32(lmh_ctl, DSPSS_PUBState),
        VMSTATE_UINT32(lmh_cfg, DSPSS_PUBState),
        VMSTATE_UINT32(hmx_lmh_ctl, DSPSS_PUBState),
        VMSTATE_END_OF_LIST()
    }
};

static void dspss_pub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = dspss_pub_realize;
    dc->vmsd = &vmstate_dspss_pub;
    device_class_set_props(dc, dspss_properties);
    rc->phases.hold = dspss_pub_reset_hold;
    dc->desc = "DSPSS pub";
}

static const TypeInfo dspss_pub_info = {
    .name          = TYPE_DSPSS_PUB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DSPSS_PUBState),
    .class_init    = dspss_pub_class_init,
};

static void dspss_pub_register_types(void)
{
    type_register_static(&dspss_pub_info);
}

type_init(dspss_pub_register_types)
