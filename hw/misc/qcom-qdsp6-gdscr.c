/*
 * QCOM Turing GDSCR Registers — QEMU sysbus device
 * GDSCR: Global Distributed Switch Controller Regulator
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/misc/qcom-qdsp6-gdscr.h"
#include "migration/vmstate.h"
#include "qemu/log.h"

/* Helper for bad accesses */
static inline void gdscr_bad_access(const char *dir, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
                  TYPE_QCOM_GDSCR ": bad %s addr=0x%" HWADDR_PRIx " size=%u\n",
                  dir, addr, size);
}

/* ------------------------------------------------------------------------- */
/* Read */
static uint64_t gdscr_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomTuringGdscrState *s = opaque;
    if (size != 4) {
        gdscr_bad_access("read", addr, size);
        return 0;
    }

    switch (addr) {
    case GCC_0_GDSCR:
        return s->gdscr;
    case GCC_0_CFG_GDSCR:
        return s->cfg_gdscr;
    case GCC_0_CFG2_GDSCR:
        return s->cfg2_gdscr;
    case GCC_0_CFG3_GDSCR:
        return s->cfg3_gdscr;
    case GCC_0_CFG4_GDSCR:
        return s->cfg4_gdscr;
    /* New Turing registers */
    case GCC_0_Q6_TBU0_CBCR:
        return s->q6_tbu0_cbcr;
    case GCC_0_Q6_TBU0_SREGR:
        return s->q6_tbu0_sregr;
    case GCC_0_Q6_TBU1_CBCR:
        return s->q6_tbu1_cbcr;
    case GCC_0_Q6_TBU1_SREGR:
        return s->q6_tbu1_sregr;
    case GCC_0_Q6_AXI_CBCR:
        return s->q6_axi_cbcr;
    case GCC_0_CFG_AHB_CBCR:
        return s->cfg_ahb_cbcr;
    case GCC_0_THROTTLE_NSP_AHB_CBCR:
        return s->throttle_nsp_ahb_cbcr;
    case GCC_0_AT_CBCR:
        return s->at_cbcr;
    case GCC_0_TRIG_CBCR:
        return s->trig_cbcr;
    case GCC_0_Q6_AXI_CMD_RCGR:
        return s->q6_axi_cmd_rcgr;
    case GCC_0_Q6_AXI_CFG_RCGR:
        return s->q6_axi_cfg_rcgr;
    case GCC_RPMH_CDSP_NOC0_CMD_DFSR:
        return s->rpmh_cdhsp_noc0_cmd_dfsr;
    default:
        gdscr_bad_access("read", addr, size);
        return 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Write */
static void gdscr_write(void *opaque, hwaddr addr, uint64_t value,
                        unsigned size)
{
    QcomTuringGdscrState *s = opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4) {
        gdscr_bad_access("write", addr, size);
        return;
    }

    switch (addr) {
    case GCC_0_GDSCR:
        if (v & 0x1) {
            /* Enable clock */
            v &= ~0x80000000; /* Clear clock off bit */
        } else {
            /* Disable clock */
            v |= 0x80000000; /* Set clock off bit */
        }
        s->gdscr = v;
        break;
    case GCC_0_CFG_GDSCR:
        s->cfg_gdscr = v;
        break;
    case GCC_0_CFG2_GDSCR:
        s->cfg2_gdscr = v;
        break;
    case GCC_0_CFG3_GDSCR:
        s->cfg3_gdscr = v;
        break;
    case GCC_0_CFG4_GDSCR:
        s->cfg4_gdscr = v;
        break;
    /* New Turing registers */
    case GCC_0_Q6_TBU0_CBCR:
        s->q6_tbu0_cbcr = v;
        break;
    case GCC_0_Q6_TBU0_SREGR:
        s->q6_tbu0_sregr = v;
        break;
    case GCC_0_Q6_TBU1_CBCR:
        s->q6_tbu1_cbcr = v;
        break;
    case GCC_0_Q6_TBU1_SREGR:
        s->q6_tbu1_sregr = v;
        break;
    case GCC_0_Q6_AXI_CBCR:
        s->q6_axi_cbcr = v;
        break;
    case GCC_0_CFG_AHB_CBCR:
        s->cfg_ahb_cbcr = v;
        break;
    case GCC_0_THROTTLE_NSP_AHB_CBCR:
        s->throttle_nsp_ahb_cbcr = v;
        break;
    case GCC_0_AT_CBCR:
        s->at_cbcr = v;
        break;
    case GCC_0_TRIG_CBCR:
        s->trig_cbcr = v;
        break;
    case GCC_0_Q6_AXI_CMD_RCGR:
        s->q6_axi_cmd_rcgr = v;
        break;
    case GCC_0_Q6_AXI_CFG_RCGR:
        s->q6_axi_cfg_rcgr = v;
        break;
    case GCC_RPMH_CDSP_NOC0_CMD_DFSR:
        s->rpmh_cdhsp_noc0_cmd_dfsr = v;
        break;
    default:
        gdscr_bad_access("write", addr, size);
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* MemoryRegionOps */
static const MemoryRegionOps gdscr_ops = {
    .read = gdscr_read,
    .write = gdscr_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

/* ------------------------------------------------------------------------- */
/* Reset */
static void gdscr_reset_hold(Object *obj, ResetType type)
{
    QcomTuringGdscrState *s = QCOM_GDSCR(obj);

    /* Initialize registers with default values from specification */
    /* The exception we show all the CBCR's as having clocks on (bit-31 == 0)*/
    s->gdscr = 0x0022F001;
    s->cfg_gdscr = 0x98000;
    s->cfg2_gdscr = 0x2022A;
    s->cfg3_gdscr = 0x4F00000;
    s->cfg4_gdscr = 0x222222;
    /* New Turing registers default values */
    s->q6_tbu0_cbcr = 0x02000220;
    s->q6_tbu0_sregr = 0x10000;
    s->q6_tbu1_cbcr = 0x02000220;
    s->q6_tbu1_sregr = 0x10000;
    s->q6_axi_cbcr = 0x02000000;
    s->cfg_ahb_cbcr = 0x00000008;
    s->throttle_nsp_ahb_cbcr = 0x00000000;
    s->at_cbcr = 0x00000000;
    s->trig_cbcr = 0x00000000;
    s->q6_axi_cmd_rcgr = 0x0;
    s->q6_axi_cfg_rcgr = 0x100000;
    s->rpmh_cdhsp_noc0_cmd_dfsr = 0x8020;
}

/* ------------------------------------------------------------------------- */
/* Realize */
static void gdscr_realize(DeviceState *dev, Error **errp)
{
    QcomTuringGdscrState *s = QCOM_GDSCR(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &gdscr_ops, s, TYPE_QCOM_GDSCR,
                          QCOM_GDSCR_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_qdsp6_gdscr = {
    .name = "qdsp6ss-gdscr",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(gdscr, QcomTuringGdscrState),
        VMSTATE_UINT32(cfg_gdscr, QcomTuringGdscrState),
        VMSTATE_UINT32(cfg2_gdscr, QcomTuringGdscrState),
        VMSTATE_UINT32(cfg3_gdscr, QcomTuringGdscrState),
        VMSTATE_UINT32(cfg4_gdscr, QcomTuringGdscrState),
        VMSTATE_UINT32(q6_tbu0_cbcr, QcomTuringGdscrState),
        VMSTATE_UINT32(q6_tbu0_sregr, QcomTuringGdscrState),
        VMSTATE_UINT32(q6_tbu1_cbcr, QcomTuringGdscrState),
        VMSTATE_UINT32(q6_tbu1_sregr, QcomTuringGdscrState),
        VMSTATE_UINT32(q6_axi_cbcr, QcomTuringGdscrState),
        VMSTATE_UINT32(cfg_ahb_cbcr, QcomTuringGdscrState),
        VMSTATE_UINT32(throttle_nsp_ahb_cbcr, QcomTuringGdscrState),
        VMSTATE_UINT32(at_cbcr, QcomTuringGdscrState),
        VMSTATE_UINT32(trig_cbcr, QcomTuringGdscrState),
        VMSTATE_UINT32(q6_axi_cmd_rcgr, QcomTuringGdscrState),
        VMSTATE_UINT32(q6_axi_cfg_rcgr, QcomTuringGdscrState),
        VMSTATE_UINT32(rpmh_cdhsp_noc0_cmd_dfsr, QcomTuringGdscrState),
        VMSTATE_END_OF_LIST()
    }

};
/* ------------------------------------------------------------------------- */
/* Type info */
static void gdscr_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    dc->realize = gdscr_realize;
    rc->phases.hold = gdscr_reset_hold;
    dc->vmsd = &vmstate_qdsp6_gdscr;
    dc->desc = "QCOM Turing GDSCR Registers";
}

static const TypeInfo gdscr_info = {
    .name = TYPE_QCOM_GDSCR,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomTuringGdscrState),
    .class_init = gdscr_class_init,
};

static void gdscr_register_types(void)
{
    type_register_static(&gdscr_info);
}

type_init(gdscr_register_types)
