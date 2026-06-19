/*
 * Hexagon Global Registers
 *
 * Copyright(c) 2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/hexagon_globalreg.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/sysbus.h"
#include "hw/core/resettable.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#include "target/hexagon/cpu.h"
#include "hw/timer/qct-qtimer.h"
#include "target/hexagon/hex_regs.h"
#include "qemu/log.h"
#include "trace/trace-hw_hexagon.h"
#include "qapi/error.h"

#define IMMUTABLE (~0)
#define INVALID_REG_VAL 0xdeadbeef

static const char *hex_sreg_names[] = {
    [HEX_SREG_SGP0] = "sgp0",
    [HEX_SREG_SGP1] = "sgp1",
    [HEX_SREG_STID] = "stid",
    [HEX_SREG_ELR] = "elr",
    [HEX_SREG_BADVA0] = "badva0",
    [HEX_SREG_BADVA1] = "badva1",
    [HEX_SREG_SSR] = "ssr",
    [HEX_SREG_CCR] = "ccr",
    [HEX_SREG_HTID] = "htid",
    [HEX_SREG_BADVA] = "badva",
    [HEX_SREG_IMASK] = "imask",
    [HEX_SREG_GEVB] = "gevb",
    [HEX_SREG_VWCTRL] = "vwctrl",
    [HEX_SREG_EVB] = "evb",
    [HEX_SREG_MODECTL] = "modectl",
    [HEX_SREG_SYSCFG] = "syscfg",
    [HEX_SREG_IPENDAD] = "ipendad",
    [HEX_SREG_VID] = "vid",
    [HEX_SREG_VID1] = "vid1",
    [HEX_SREG_BESTWAIT] = "bestwait",
    [HEX_SREG_IEL] = "iel",
    [HEX_SREG_SCHEDCFG] = "schedcfg",
    [HEX_SREG_IAHL] = "iahl",
    [HEX_SREG_CFGBASE] = "cfgbase",
    [HEX_SREG_DIAG] = "diag",
    [HEX_SREG_REV] = "rev",
    [HEX_SREG_PCYCLELO] = "pcyclelo",
    [HEX_SREG_PCYCLEHI] = "pcyclehi",
    [HEX_SREG_ISDBST] = "isdbst",
    [HEX_SREG_ISDBCFG0] = "isdbcfg0",
    [HEX_SREG_ISDBCFG1] = "isdbcfg1",
    [HEX_SREG_LIVELOCK] = "livelock",
    [HEX_SREG_BRKPTPC0] = "brkptpc0",
    [HEX_SREG_BRKPTCFG0] = "brkptcfg0",
    [HEX_SREG_BRKPTPC1] = "brkptpc1",
    [HEX_SREG_BRKPTCFG1] = "brkptcfg1",
    [HEX_SREG_ISDBMBXIN] = "isdbmbxin",
    [HEX_SREG_ISDBMBXOUT] = "isdbmbxout",
    [HEX_SREG_ISDBEN] = "isdben",
    [HEX_SREG_ISDBGPR] = "isdbgpr",
    [HEX_SREG_PMUCNT4] = "pmucnt4",
    [HEX_SREG_PMUCNT5] = "pmucnt5",
    [HEX_SREG_PMUCNT6] = "pmucnt6",
    [HEX_SREG_PMUCNT7] = "pmucnt7",
    [HEX_SREG_PMUCNT0] = "pmucnt0",
    [HEX_SREG_PMUCNT1] = "pmucnt1",
    [HEX_SREG_PMUCNT2] = "pmucnt2",
    [HEX_SREG_PMUCNT3] = "pmucnt3",
    [HEX_SREG_PMUEVTCFG] = "pmuevtcfg",
    [HEX_SREG_PMUSTID0] = "pmustid0",
    [HEX_SREG_PMUEVTCFG1] = "pmuevtcfg1",
    [HEX_SREG_PMUSTID1] = "pmustid1",
    [HEX_SREG_TIMERLO] = "timerlo",
    [HEX_SREG_TIMERHI] = "timerhi",
    [HEX_SREG_PMUCFG] = "pmucfg",
    [HEX_SREG_S59] = "s59",
    [HEX_SREG_S60] = "s60",
    [HEX_SREG_S61] = "s61",
    [HEX_SREG_S62] = "s62",
    [HEX_SREG_S63] = "s63",
    [HEX_SREG_COMMIT1T] = "commit1t",
    [HEX_SREG_COMMIT2T] = "commit2t",
    [HEX_SREG_COMMIT3T] = "commit3t",
    [HEX_SREG_COMMIT4T] = "commit4t",
    [HEX_SREG_COMMIT5T] = "commit5t",
    [HEX_SREG_COMMIT6T] = "commit6t",
    [HEX_SREG_PCYCLE1T] = "pcycle1t",
    [HEX_SREG_PCYCLE2T] = "pcycle2t",
    [HEX_SREG_PCYCLE3T] = "pcycle3t",
    [HEX_SREG_PCYCLE4T] = "pcycle4t",
    [HEX_SREG_PCYCLE5T] = "pcycle5t",
    [HEX_SREG_PCYCLE6T] = "pcycle6t",
    [HEX_SREG_STFINST] = "stfinst",
    [HEX_SREG_ISDBCMD] = "isdbcmd",
    [HEX_SREG_ISDBVER] = "isdbver",
    [HEX_SREG_BRKPTINFO] = "brkptinfo",
    [HEX_SREG_RGDR3] = "rgdr3",
    [HEX_SREG_COMMIT7T] = "commit7t",
    [HEX_SREG_COMMIT8T] = "commit8t",
    [HEX_SREG_PCYCLE7T] = "pcycle7t",
    [HEX_SREG_PCYCLE8T] = "pcycle8t",
    [HEX_SREG_COMMIT9T] = "commit9t",
    [HEX_SREG_COMMIT10T] = "commit10t",
    [HEX_SREG_COMMIT11T] = "commit11t",
    [HEX_SREG_COMMIT12T] = "commit12t",
    [HEX_SREG_COMMIT13T] = "commit13t",
    [HEX_SREG_COMMIT14T] = "commit14t",
    [HEX_SREG_COMMIT15T] = "commit15t",
    [HEX_SREG_COMMIT16T] = "commit16t",
    [HEX_SREG_PCYCLE9T] = "pcycle9t",
    [HEX_SREG_PCYCLE10T] = "pcycle10t",
    [HEX_SREG_PCYCLE11T] = "pcycle11t",
    [HEX_SREG_PCYCLE12T] = "pcycle12t",
    [HEX_SREG_PCYCLE13T] = "pcycle13t",
    [HEX_SREG_PCYCLE14T] = "pcycle14t",
    [HEX_SREG_PCYCLE15T] = "pcycle15t",
    [HEX_SREG_PCYCLE16T] = "pcycle16t",
    [HEX_SREG_IPEND] = "ipend",
    [HEX_SREG_IAD] = "iad",
    [HEX_SREG_ISDBST1] = "isdbst1",
    [HEX_SREG_ISDBST2] = "isdbst2",
    [HEX_SREG_BRKPTINFO1] = "brkptinfo1",
};

static const char *get_sreg_name(uint32_t reg)
{
    if (reg < ARRAY_SIZE(hex_sreg_names) && hex_sreg_names[reg]) {
        return hex_sreg_names[reg];
    }
    return "UNKNOWN";
}

/* Global system register mutability masks */
static const uint32_t global_sreg_immut_masks[NUM_SREGS] = {
    [HEX_SREG_EVB] = 0x000000ff,
    [HEX_SREG_MODECTL] = IMMUTABLE,
    [HEX_SREG_SYSCFG] = 0x80001c00,
    [HEX_SREG_IPENDAD] = IMMUTABLE,
    [HEX_SREG_VID] = 0xfc00fc00,
    [HEX_SREG_VID1] = 0xfc00fc00,
    [HEX_SREG_BESTWAIT] = 0xfffffe00,
    [HEX_SREG_IAHL] = 0x00000000,
    [HEX_SREG_SCHEDCFG] = 0xfffffee0,
    [HEX_SREG_CFGBASE] = IMMUTABLE,
    [HEX_SREG_DIAG] = 0x00000000,
    [HEX_SREG_REV] = IMMUTABLE,
    [HEX_SREG_ISDBST] = IMMUTABLE,
    [HEX_SREG_ISDBCFG0] = 0xe0000000,
    [HEX_SREG_BRKPTPC0] = 0x00000003,
    [HEX_SREG_BRKPTCFG0] = 0xfc007000,
    [HEX_SREG_BRKPTPC1] = 0x00000003,
    [HEX_SREG_BRKPTCFG1] = 0xfc007000,
    [HEX_SREG_ISDBMBXIN] = IMMUTABLE,
    [HEX_SREG_ISDBMBXOUT] = 0x00000000,
    [HEX_SREG_ISDBEN] = 0xfffffffe,
    [HEX_SREG_TIMERLO] = IMMUTABLE,
    [HEX_SREG_TIMERHI] = IMMUTABLE,
    [HEX_SREG_ISDBVER] = IMMUTABLE,
    [HEX_SREG_BRKPTINFO] = IMMUTABLE,
    [HEX_SREG_IPEND] = IMMUTABLE,
    [HEX_SREG_IAD] = IMMUTABLE,
    [HEX_SREG_ISDBST1] = IMMUTABLE,
    [HEX_SREG_ISDBST2] = IMMUTABLE,
    [HEX_SREG_BRKPTINFO1] = IMMUTABLE,
};

static void hexagon_globalreg_init(Object *obj)
{
    HexagonGlobalRegState *s = HEXAGON_GLOBALREG(obj);

    memset(s->regs, 0, sizeof(target_ulong) * NUM_SREGS);
}

static inline uint32_t apply_write_mask(uint32_t new_val, uint32_t cur_val,
                                        uint32_t reg_mask)
{
    if (reg_mask) {
        return (new_val & ~reg_mask) | (cur_val & reg_mask);
    }
    return new_val;
}

uint32_t hexagon_globalreg_read(HexagonGlobalRegState *s, uint32_t reg)
{
    g_assert(reg < NUM_SREGS);
    g_assert(reg >= HEX_SREG_GLB_START);
    g_assert(s);

    uint32_t value;
    if ((reg == HEX_SREG_VID) || (reg == HEX_SREG_VID1)) {
        uint32_t vid_group = (reg == HEX_SREG_VID) ? 0 : 1;
        value = l2vic_read_vid(s->l2vic, vid_group);
    } else if (reg == HEX_SREG_TIMERLO) {
        value = s->qtimer ? qtimer_get_timer_lo(s->qtimer) : 0;
    } else if (reg == HEX_SREG_TIMERHI) {
        value = s->qtimer ? qtimer_get_timer_hi(s->qtimer) : 0;
    } else {
        value = qatomic_read(&s->regs[reg]);
    }

    trace_hexagon_globalreg_read(get_sreg_name(reg), value);
    return value;
}

void hexagon_globalreg_write(HexagonGlobalRegState *s, uint32_t reg,
                             uint32_t value)
{
    g_assert(s);
    g_assert(reg < NUM_SREGS);
    g_assert(reg >= HEX_SREG_GLB_START);

    if ((reg == HEX_SREG_VID) || (reg == HEX_SREG_VID1)) {
        /* Update VID register through L2VIC interface */
        if (s->l2vic) {
            uint32_t vid_group = (reg == HEX_SREG_VID) ? 0 : 1;
            l2vic_update_vid(s->l2vic, vid_group, value);
        }
    }

    qatomic_set(&s->regs[reg], value);

    /*
     * Keep IPENDAD (S20) in sync with separate IPEND (S101) and IAD (S102)
     * registers.  Guest code may read IPENDAD to check interrupt state,
     * so the combined register must reflect changes to the split registers.
     */
    if (reg == HEX_SREG_IPEND) {
        uint32_t ipendad = qatomic_read(&s->regs[HEX_SREG_IPENDAD]);
        ipendad = deposit32(ipendad, 0, 16, value & 0xFFFF);
        qatomic_set(&s->regs[HEX_SREG_IPENDAD], ipendad);
    } else if (reg == HEX_SREG_IAD) {
        uint32_t ipendad = qatomic_read(&s->regs[HEX_SREG_IPENDAD]);
        ipendad = deposit32(ipendad, 16, 16, value & 0xFFFF);
        qatomic_set(&s->regs[HEX_SREG_IPENDAD], ipendad);
    }

    trace_hexagon_globalreg_write(get_sreg_name(reg), qatomic_read(&s->regs[reg]));
}

uint32_t hexagon_globalreg_masked_value(HexagonGlobalRegState *s, uint32_t reg,
                                        uint32_t value)
{
    g_assert(s);
    g_assert(reg < NUM_SREGS);
    g_assert(reg >= HEX_SREG_GLB_START);
    const uint32_t reg_mask = global_sreg_immut_masks[reg];
    uint32_t cur_value = qatomic_read(&s->regs[reg]);
    return reg_mask == IMMUTABLE ?
            cur_value :
            apply_write_mask(value, cur_value, reg_mask);
}

void hexagon_globalreg_write_masked(HexagonGlobalRegState *s, uint32_t reg,
                                    uint32_t value)
{
    g_assert(s);
    uint32_t final_value = hexagon_globalreg_masked_value(s, reg, value);

    if ((reg == HEX_SREG_VID) || (reg == HEX_SREG_VID1)) {
        /* Update VID register through L2VIC interface */
        if (s->l2vic) {
            uint32_t vid_group = (reg == HEX_SREG_VID) ? 0 : 1;
            l2vic_update_vid(s->l2vic, vid_group, final_value);
        }
    }

    qatomic_set(&s->regs[reg], final_value);
}

uint64_t hexagon_globalreg_get_pcycle_base(HexagonGlobalRegState *s)
{
    g_assert(s);
    return s->g_pcycle_base;
}

uint32_t hexagon_globalreg_get_boot_evb(HexagonGlobalRegState *s)
{
    return s ? s->boot_evb : 0xffffffff;
}

void hexagon_globalreg_set_pcycle_base(HexagonGlobalRegState *s,
                                       uint64_t value)
{
    g_assert(s);
    s->g_pcycle_base = value;
}

static void do_hexagon_globalreg_reset(HexagonGlobalRegState *s)
{
    g_assert(s);
    memset(s->regs, 0, sizeof(target_ulong) * NUM_SREGS);

    s->g_pcycle_base = 0;

    s->regs[HEX_SREG_EVB] = s->boot_evb;
    s->regs[HEX_SREG_CFGBASE] = HEXAGON_CFG_ADDR_BASE(s->config_table_addr);
    s->regs[HEX_SREG_REV] = s->dsp_rev;

    target_ulong isdben_val = 0;
    if (s->isdben_etm_enable) {
        isdben_val |= (1 << 0);  /* ETM enable bit */
    }
    if (s->isdben_dfd_enable) {
        isdben_val |= (1 << 1);  /* DFD enable bit */
    }
    if (s->isdben_trusted) {
        isdben_val |= (1 << 2);  /* Trusted bit */
    }
    if (s->isdben_secure) {
        isdben_val |= (1 << 3);  /* Secure bit */
    }
    s->regs[HEX_SREG_ISDBEN] = isdben_val;
    s->regs[HEX_SREG_MODECTL] = 0x1;

    /*
     * These register indices are placeholders in these arrays
     * and their actual values are synthesized from state elsewhere.
     * We can initialize these with invalid values so that if we
     * mistakenly generate reads, they will look obviously wrong.
     */
    s->regs[HEX_SREG_PCYCLELO] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PCYCLEHI] = INVALID_REG_VAL;
    s->regs[HEX_SREG_TIMERLO] = INVALID_REG_VAL;
    s->regs[HEX_SREG_TIMERHI] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT0] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT1] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT2] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT3] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT4] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT5] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT6] = INVALID_REG_VAL;
    s->regs[HEX_SREG_PMUCNT7] = INVALID_REG_VAL;
    s->regs[HEX_SREG_IPENDAD] = INVALID_REG_VAL;
}

static void hexagon_globalreg_realize(DeviceState *dev, Error **errp)
{
}

void hexagon_globalreg_reset(HexagonGlobalRegState *s)
{
    do_hexagon_globalreg_reset(s);
}

static void hexagon_globalreg_reset_hold(Object *obj, ResetType type)
{
    HexagonGlobalRegState *s = HEXAGON_GLOBALREG(obj);
    do_hexagon_globalreg_reset(s);
}

static const VMStateDescription vmstate_hexagon_globalreg = {
    .name = "hexagon_globalreg",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]){
        VMSTATE_UINT32_ARRAY(regs, HexagonGlobalRegState, NUM_SREGS),
        VMSTATE_UINT64(g_pcycle_base, HexagonGlobalRegState),
        VMSTATE_UINT32(boot_evb, HexagonGlobalRegState),
        VMSTATE_UINT64(config_table_addr, HexagonGlobalRegState),
        VMSTATE_UINT32(dsp_rev, HexagonGlobalRegState),
        VMSTATE_BOOL(isdben_etm_enable, HexagonGlobalRegState),
        VMSTATE_BOOL(isdben_dfd_enable, HexagonGlobalRegState),
        VMSTATE_BOOL(isdben_trusted, HexagonGlobalRegState),
        VMSTATE_BOOL(isdben_secure, HexagonGlobalRegState),
        VMSTATE_END_OF_LIST()
    }
};

static const Property hexagon_globalreg_properties[] = {
    DEFINE_PROP_UINT32("boot-evb", HexagonGlobalRegState, boot_evb, 0x0),
    DEFINE_PROP_UINT64("config-table-addr", HexagonGlobalRegState,
                       config_table_addr, 0xffffffffULL),
    DEFINE_PROP_UINT32("dsp-rev", HexagonGlobalRegState, dsp_rev, 0),
    DEFINE_PROP_BOOL("isdben-etm-enable", HexagonGlobalRegState,
                     isdben_etm_enable, false),
    DEFINE_PROP_BOOL("isdben-dfd-enable", HexagonGlobalRegState,
                     isdben_dfd_enable, false),
    DEFINE_PROP_BOOL("isdben-trusted", HexagonGlobalRegState,
                     isdben_trusted, false),
    DEFINE_PROP_BOOL("isdben-secure", HexagonGlobalRegState,
                     isdben_secure, false),
    DEFINE_PROP_LINK("qtimer", HexagonGlobalRegState,
                     qtimer, TYPE_QTIMER_INTERFACE,
                     QTimerInterface *),
    DEFINE_PROP_LINK("l2vic", HexagonGlobalRegState,
                     l2vic, TYPE_L2VIC_INTERFACE,
                     L2VicInterface *),
};

static void hexagon_globalreg_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = hexagon_globalreg_realize;
    rc->phases.hold = hexagon_globalreg_reset_hold;
    dc->vmsd = &vmstate_hexagon_globalreg;
    dc->user_creatable = false;
    device_class_set_props(dc, hexagon_globalreg_properties);
}

static const TypeInfo hexagon_globalreg_info = {
    .name = TYPE_HEXAGON_GLOBALREG,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(HexagonGlobalRegState),
    .instance_init = hexagon_globalreg_init,
    .class_init = hexagon_globalreg_class_init,
};

static void hexagon_globalreg_register_types(void)
{
    type_register_static(&hexagon_globalreg_info);
}

type_init(hexagon_globalreg_register_types)
