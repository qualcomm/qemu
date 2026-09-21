/*
 * TURING QDSP6SS PLL — simple QEMU PLL device
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Add Lucid PLL register set.
 *   Lucid is a general purpose PLL.
 */

#include "qemu/osdep.h"
#include "hw/core/boards.h"
#include "hw/core/irq.h"
#include "hw/misc/qcom-turing-cdsp-pll.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "trace.h"

/* ------------------------------------------------------------------------- */
/* Register offsets (relative to MMIO base)                                   */
/* ------------------------------------------------------------------------- */
#define PLL_MODE 0x00
#define PLL_OPMODE 0x04
#define PLL_STATE 0x08 /* R */
#define PLL_STATUS 0x0C /* R */
#define PLL_L_VAL 0x10
#define PLL_ALPHA_VAL 0x14
#define PLL_USER_CTL 0x18
#define PLL_USER_CTL_U 0x1C
#define PLL_CONFIG_CTL 0x20
#define PLL_CONFIG_CTL_U 0x24
#define PLL_CONFIG_CTL_U1 0x28
#define PLL_SSC 0x38
#define FUSA_STATUS_REGISTER 0x80

/* Size of the window: include up to 0x80 and a little slack */
#define TURING_QDSP6SS_PLL_MMIO_SIZE 0x100

#define PLL_MODE_ENABLE_BIT 1
#define PLL_MODE_RESET_BIT 2
#define PLL_MODE_LOCK_BIT 31

/* ------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* ------------------------------------------------------------------------- */

static inline void warn_bad_access(const char *dir, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_GUEST_ERROR,
                  "turing-qdsp6ss-pll: bad %s addr=0x%" HWADDR_PRIx
                  " size=%u\n",
                  dir, addr, size);
}

/* ------------------------------------------------------------------------- */
/* Read                                                                      */
/* ------------------------------------------------------------------------- */
static uint64_t turing_qdsp6ss_pll_read(void *opaque, hwaddr addr,
                                        unsigned size)
{
    TuringQdsp6PllState *s = (TuringQdsp6PllState *)opaque;
    uint32_t value;

    if (size != 4) {
        warn_bad_access("read", addr, size);
        return 0;
    }

    switch (addr) {
    case PLL_MODE:
        value = s->pll_mode;
        break;
    case PLL_OPMODE:
        value = s->pll_opmode;
        break;
    case PLL_STATE:
        value = s->pll_state;
        break;
    case PLL_STATUS:
        value = s->pll_status;
        break;
    case PLL_L_VAL:
        value = s->pll_l_val;
        break;
    case PLL_ALPHA_VAL:
        value = s->pll_alpha_val;
        break;
    case PLL_USER_CTL:
        value = s->pll_user_ctl;
        break;
    case PLL_USER_CTL_U:
        value = s->pll_user_ctl_u;
        break;
    case PLL_CONFIG_CTL:
        value = s->pll_config_ctl;
        break;
    case PLL_CONFIG_CTL_U:
        value = s->pll_config_ctl_u;
        break;
    case PLL_CONFIG_CTL_U1:
        value = s->pll_config_ctl_u1;
        break;
    case PLL_SSC:
        value = s->pll_ssc;
        break;
    case FUSA_STATUS_REGISTER:
        value = s->fusa_status_register;
        break;
    default:
        warn_bad_access("read", addr, size);
        return 0;
    }

    trace_qdsp6ss_pll_read(addr, value);
    return value;
}

/* ------------------------------------------------------------------------- */
/* Write                                                                     */
/* ------------------------------------------------------------------------- */
static void turing_qdsp6ss_pll_write(void *opaque, hwaddr addr, uint64_t value,
                                     unsigned size)
{
    TuringQdsp6PllState *s = (TuringQdsp6PllState *)opaque;
    uint32_t v = (uint32_t)value;

    if (size != 4) {
        warn_bad_access("write", addr, size);
        return;
    }

    trace_qdsp6ss_pll_write(addr, v);

    switch (addr) {
    case PLL_MODE:
        s->pll_mode = v;
        break;
    case PLL_OPMODE:
        s->pll_opmode = v;
        break;
    case PLL_STATE:
        /* read-only */
        qemu_log_mask(LOG_GUEST_ERROR,
                      "turing-qdsp6ss-pll: write to read-only PLL_STATE "
                      "@0x%08" HWADDR_PRIx "\n",
                      addr);
        break;
    case PLL_STATUS:
        /* read-only */
        qemu_log_mask(LOG_GUEST_ERROR,
                      "turing-qdsp6ss-pll: write to read-only PLL_STATUS "
                      "@0x%08" HWADDR_PRIx "\n",
                      addr);
        break;
    case PLL_L_VAL:
        s->pll_l_val = v;
        break;
    case PLL_ALPHA_VAL:
        s->pll_alpha_val = v;
        break;
    case PLL_USER_CTL:
        s->pll_user_ctl = v;
        if (v & PLL_MODE_ENABLE_BIT) {
            set_bit32(PLL_MODE_RESET_BIT, &s->pll_mode);
            set_bit32(PLL_MODE_LOCK_BIT, &s->pll_mode);
        }
        break;
    case PLL_USER_CTL_U:
        s->pll_user_ctl_u = v;
        break;
    case PLL_CONFIG_CTL:
        s->pll_config_ctl = v;
        break;
    case PLL_CONFIG_CTL_U:
        s->pll_config_ctl_u = v;
        break;
    case PLL_CONFIG_CTL_U1:
        s->pll_config_ctl_u1 = v;
        break;
    case PLL_SSC:
        s->pll_ssc = v;
        break;
    case FUSA_STATUS_REGISTER:
        s->fusa_status_register = v;
        break;
    default:
        warn_bad_access("write", addr, size);
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* MMIO ops                                                                  */
/* ------------------------------------------------------------------------- */
static const MemoryRegionOps turing_qdsp6ss_pll_ops = {
    .read = turing_qdsp6ss_pll_read,
    .write = turing_qdsp6ss_pll_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* ------------------------------------------------------------------------- */
/* Reset                                                                     */
/* ------------------------------------------------------------------------- */
static void turing_qdsp6ss_pll_reset_hold(Object *obj, ResetType type)
{
    TuringQdsp6PllState *s = TURING_QDSP6SS_PLL(obj);

    s->pll_mode = 0x00000100;
    s->pll_opmode = 0x00000000;
    s->pll_state = 0x00000004; /* RO */
    s->pll_status = 0x16000000; /* RO */
    s->pll_l_val = 0x00000010;
    s->pll_alpha_val = 0x00000000;
    s->pll_user_ctl = 0x00000001;
    s->pll_user_ctl_u = 0x00000805;
    s->pll_config_ctl = 0x20485699;
    s->pll_config_ctl_u = 0x00002261;
    s->pll_config_ctl_u1 = 0x02AA699C;
    s->pll_ssc = 0x00000000;
    s->fusa_status_register = 0x00000000;
}

/* ------------------------------------------------------------------------- */
/* Realize                                                                   */
/* ------------------------------------------------------------------------- */
static void turing_qdsp6ss_pll_realize(DeviceState *dev, Error **errp)
{
    TuringQdsp6PllState *s = TURING_QDSP6SS_PLL(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &turing_qdsp6ss_pll_ops, s,
                          TYPE_TURING_QDSP6SS_PLL,
                          TURING_QDSP6SS_PLL_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static const VMStateDescription vmstate_qdsp6ss_pll = {
    .name = "qdsp6ss-pll",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(pll_mode, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_opmode, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_state, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_status, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_l_val, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_alpha_val, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_user_ctl, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_user_ctl_u, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_config_ctl, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_config_ctl_u, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_config_ctl_u1, TuringQdsp6PllState),
        VMSTATE_UINT32(pll_ssc, TuringQdsp6PllState),
        VMSTATE_UINT32(fusa_status_register, TuringQdsp6PllState),
        VMSTATE_END_OF_LIST()
    }
};
/* ------------------------------------------------------------------------- */
/* Type info                                                                 */
/* ------------------------------------------------------------------------- */
static void turing_qdsp6ss_pll_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);
    dc->realize = turing_qdsp6ss_pll_realize;
    dc->vmsd = &vmstate_qdsp6ss_pll;
    rc->phases.hold = turing_qdsp6ss_pll_reset_hold;
    dc->desc = "Turing QDSP6SS PLL";
}

static const TypeInfo turing_qdsp6ss_pll_info = {
    .name = TYPE_TURING_QDSP6SS_PLL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(TuringQdsp6PllState),
    .class_init = turing_qdsp6ss_pll_class_init,
};

static void turing_qdsp6ss_pll_register_types(void)
{
    type_register_static(&turing_qdsp6ss_pll_info);
}

type_init(turing_qdsp6ss_pll_register_types);
