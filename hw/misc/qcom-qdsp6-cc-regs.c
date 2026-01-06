/*
 * TURING QDSP6SS Clock Controller (CLKCTL) — QEMU sysbus device
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/resettable.h"
#include "hw/misc/qcom-qdsp6-cc-regs.h"
#include "migration/vmstate.h"
#include "qemu/log.h"

static inline void clkctl_bad_access(const char *dir, hwaddr addr,
                                     unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
                  TYPE_QDSP6SS_CLKCTL ": bad %s addr=0x%" HWADDR_PRIx
                                      " size=%u\n",
                  dir, addr, size);
}

/* ------------------------------------------------------------------------- */
/* Read                                                                      */
/* ------------------------------------------------------------------------- */
static uint64_t clkctl_read(void *opaque, hwaddr addr, unsigned size)
{
    TuringQdsp6ClkctlState *s = opaque;
    if (size != 4) {
        clkctl_bad_access("read", addr, size);
        return 0;
    }

    switch (addr) {
    /* RCGR */
    case CLKCTL_CORE_CMD_RCGR:
        if (s->core_cmd_rcgr & CMD_RCGR_UPDATE_MSK) {
            /* Simulate a clock update */
            s->core_cmd_rcgr &= ~CMD_RCGR_UPDATE_MSK;
        }
        return s->core_cmd_rcgr;
    case CLKCTL_CORE_CFG_RCGR:
        return s->core_cfg_rcgr;

    /* CBCR */
    case CLKCTL_CORE_CBCR:
        return s->core_cbcr;
    case CLKCTL_SLPGEN_CBCR:
        return s->slpgen_cbcr;
    case CLKCTL_L2MEM_SLPGEN_CBCR:
        return s->l2mem_slpgen_cbcr;
    case CLKCTL_L2ITCM_SLPGEN_CBCR:
        return s->l2itcm_slpgen_cbcr;
    case CLKCTL_L2VTCM_SLPGEN_CBCR:
        return s->l2vtcm_slpgen_cbcr;
    case CLKCTL_MON_CBCR:
        return s->mon_cbcr;
    case CLKCTL_ACD_XO_CBCR:
        return s->acd_xo_cbcr;
    case CLKCTL_DEBUG_CBCR:
        return s->debug_cbcr;
    case CLKCTL_PLL_AHBS_CBCR:
        return s->pll_ahbs_cbcr;
    case CLKCTL_ACD_AHBS_CBCR:
        return s->acd_ahbs_cbcr;
    case CLKCTL_ACD_SCAN_CBCR:
        return s->acd_scan_cbcr;
    case CLKCTL_SM_OBS_CBCR:
        return s->sm_obs_cbcr;

    /* CDIVR */
    case CLKCTL_MON_DIV_CDIVR:
        return s->mon_div_cdivr;
    case CLKCTL_DEBUG_DIV_CDIVR:
        return s->debug_div_cdivr;
    case CLKCTL_ACD_SCAN_DIV_CDIVR:
        return s->acd_scan_div_cdivr;
    case CLKCTL_SM_DIV_CDIVR:
        return s->sm_div_cdivr;

    default:
        clkctl_bad_access("read", addr, size);
        return 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Write                                                                     */
/* ------------------------------------------------------------------------- */
static void clkctl_write(void *opaque, hwaddr addr, uint64_t value,
                         unsigned size)
{
    TuringQdsp6ClkctlState *s = opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4) {
        clkctl_bad_access("write", addr, size);
        return;
    }

    switch (addr) {
    /* RCGR */
    case CLKCTL_CORE_CMD_RCGR:
        s->core_cmd_rcgr = v;
        break;
    case CLKCTL_CORE_CFG_RCGR:
        s->core_cfg_rcgr = v;
        break;

    /* CBCR */
    case CLKCTL_CORE_CBCR:
        s->core_cbcr = v;
        break;
    case CLKCTL_SLPGEN_CBCR:
        s->slpgen_cbcr = v;
        break;
    case CLKCTL_L2MEM_SLPGEN_CBCR:
        s->l2mem_slpgen_cbcr = v;
        break;
    case CLKCTL_L2ITCM_SLPGEN_CBCR:
        s->l2itcm_slpgen_cbcr = v;
        break;
    case CLKCTL_L2VTCM_SLPGEN_CBCR:
        s->l2vtcm_slpgen_cbcr = v;
        break;
    case CLKCTL_MON_CBCR:
        s->mon_cbcr = v;
        break;
    case CLKCTL_ACD_XO_CBCR:
        s->acd_xo_cbcr = v;
        break;
    case CLKCTL_DEBUG_CBCR:
        s->debug_cbcr = v;
        break;
    case CLKCTL_PLL_AHBS_CBCR:
        s->pll_ahbs_cbcr = v;
        break;
    case CLKCTL_ACD_AHBS_CBCR:
        s->acd_ahbs_cbcr = v;
        break;
    case CLKCTL_ACD_SCAN_CBCR:
        s->acd_scan_cbcr = v;
        break;
    case CLKCTL_SM_OBS_CBCR:
        s->sm_obs_cbcr = v;
        break;

    /* CDIVR */
    case CLKCTL_MON_DIV_CDIVR:
        s->mon_div_cdivr = v;
        break;
    case CLKCTL_DEBUG_DIV_CDIVR:
        s->debug_div_cdivr = v;
        break;
    case CLKCTL_ACD_SCAN_DIV_CDIVR:
        s->acd_scan_div_cdivr = v;
        break;
    case CLKCTL_SM_DIV_CDIVR:
        s->sm_div_cdivr = v;
        break;

    default:
        clkctl_bad_access("write", addr, size);
        break;
    }
}

static const MemoryRegionOps clkctl_ops = {
    .read = clkctl_read,
    .write = clkctl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 4 },
};

/* ------------------------------------------------------------------------- */
/* Reset                                                                     */
/* ------------------------------------------------------------------------- */
static void clkctl_reset_hold(Object *obj, ResetType type)
{
    TuringQdsp6ClkctlState *s = QDSP6SS_CLKCTL(obj);

    /* RCGR */
    s->core_cmd_rcgr = 0x80000000;
    s->core_cfg_rcgr = 0x00100000;

    /* CBCR */
    s->core_cbcr = 0x00000002;
    s->slpgen_cbcr = 0x00000002;
    s->l2mem_slpgen_cbcr = 0x00000002;
    s->l2itcm_slpgen_cbcr = 0x00000002;
    s->l2vtcm_slpgen_cbcr = 0x00000002;
    s->mon_cbcr = 0x00000002;
    s->acd_xo_cbcr = 0x00000002;
    s->debug_cbcr = 0x00000000;
    s->pll_ahbs_cbcr = 0x00000002;
    s->acd_ahbs_cbcr = 0x00000002;
    s->acd_scan_cbcr = 0x00000000;
    s->sm_obs_cbcr = 0x00000002;

    /* CDIVR */
    s->mon_div_cdivr = 0x00000000;
    s->debug_div_cdivr = 0x00000001;
    s->acd_scan_div_cdivr = 0x00000000;
    s->sm_div_cdivr = 0x00000001;

    /* MUX */
    s->debug_mux_muxr = 0x00000000;

    /* STATUS / DIAG */
    s->parity_status_reg = 0x00000000; /* RO */
    s->fusa_status_register = 0x00000000;
}

/* ------------------------------------------------------------------------- */
/* Realize                                                                   */
/* ------------------------------------------------------------------------- */
static void clkctl_realize(DeviceState *dev, Error **errp)
{
    TuringQdsp6ClkctlState *s = QDSP6SS_CLKCTL(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &clkctl_ops, s,
                          TYPE_QDSP6SS_CLKCTL, QDSP6SS_CLKCTL_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_qdsp6_cc_regs = {
    .name = "qdsp6ss-cc-regs",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(core_cmd_rcgr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(core_cfg_rcgr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(core_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(slpgen_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(l2mem_slpgen_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(l2itcm_slpgen_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(l2vtcm_slpgen_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(mon_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(acd_xo_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(debug_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(pll_ahbs_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(acd_ahbs_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(acd_scan_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(sm_obs_cbcr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(mon_div_cdivr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(debug_div_cdivr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(acd_scan_div_cdivr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(sm_div_cdivr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(debug_mux_muxr, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(parity_status_reg, TuringQdsp6ClkctlState),
        VMSTATE_UINT32(fusa_status_register, TuringQdsp6ClkctlState),
        VMSTATE_END_OF_LIST()
    }
};

/* ------------------------------------------------------------------------- */
/* Type info                                                                 */
/* ------------------------------------------------------------------------- */
static void clkctl_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = clkctl_realize;
    dc->vmsd = &vmstate_qdsp6_cc_regs;
    rc->phases.hold = clkctl_reset_hold;
    dc->desc = "Turing QDSP6SS Clock Controller (CLKCTL)";
}

static const TypeInfo clkctl_info = {
    .name = TYPE_QDSP6SS_CLKCTL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringQdsp6ClkctlState),
    .class_init = clkctl_class_init,
};

static void clkctl_register_types(void)
{
    type_register_static(&clkctl_info);
}

type_init(clkctl_register_types)
