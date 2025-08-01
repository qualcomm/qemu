#include "qemu/osdep.h"
#include "hw/sysbus-of.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/qcom/rpmh-rsc.h"
#include "hw/irq.h"
#include "hw/qcom/cmd-db.h"

#define RPMH_RSC_LOG(dev, fmt, ...) QDEV_LOG_INFO(dev, fmt __VA_OPT__(,) __VA_ARGS__)
#define RPMH_RSC_LOG_WARN(dev, fmt, ...) QDEV_LOG_WARN(dev, fmt __VA_OPT__(,) __VA_ARGS__)
#define RPMH_RSC_LOG_ERROR(dev, fmt, ...) QDEV_LOG_ERROR(dev, fmt __VA_OPT__(,) __VA_ARGS__)

static const char* rpmh_rsc_str[] = {
    [RSC_DRV_TCS_OFFSET] = "RSC_DRV_TCS_OFFSET",
    [RSC_DRV_CMD_OFFSET] = "RSC_DRV_CMD_OFFSET",
    // drv regs | base: drv
    [DRV_ID] = "DRV_ID",
    [DRV_SOLVER_CONFIG] = "DRV_SOLVER_CONFIG",
    [DRV_PRNT_CHLD_CONFIG] = "DRV_PRNT_CHLD_CONFIG",
    // tcs common regs | base: tcs
    [RSC_DRV_IRQ_ENABLE] = "RSC_DRV_IRQ_ENABLE",
    [RSC_DRV_IRQ_STATUS] = "RSC_DRV_IRQ_STATUS",
    [RSC_DRV_IRQ_CLEAR] = "RSC_DRV_IRQ_CLEAR",
    // tcs specific regs | base: tcs
    [RSC_DRV_CMD_WAIT_FOR_CMPL] = "RSC_DRV_CMD_WAIT_FOR_CMPL",
    [RSC_DRV_CONTROL] = "RSC_DRV_CONTROL",
    [RSC_DRV_STATUS] = "RSC_DRV_STATUS",
    [RSC_DRV_CMD_ENABLE] = "RSC_DRV_CMD_ENABLE",
    // tcs cmd regs | base: cmd
    [RSC_DRV_CMD_MSGID] = "RSC_DRV_CMD_MSGID",
    [RSC_DRV_CMD_ADDR] = "RSC_DRV_CMD_ADDR",
    [RSC_DRV_CMD_DATA] = "RSC_DRV_CMD_DATA",
    [RSC_DRV_CMD_STATUS] = "RSC_DRV_CMD_STATUS",
    [RSC_DRV_CMD_RESP_DATA] = "RSC_DRV_CMD_RESP_DATA",
    // DRV channel Registers | base: drv_base
    [RSC_DRV_CHN_TCS_TRIGGER] = "RSC_DRV_CHN_TCS_TRIGGER",
    [RSC_DRV_CHN_TCS_COMPLETE] = "RSC_DRV_CHN_TCS_COMPLETE",
    [RSC_DRV_CHN_SEQ_BUSY] = "RSC_DRV_CHN_SEQ_BUSY",
    [RSC_DRV_CHN_SEQ_PC] = "RSC_DRV_CHN_SEQ_PC",
    [RSC_DRV_CHN_UPDATE] = "RSC_DRV_CHN_UPDATE",
    [RSC_DRV_CHN_BUSY] = "RSC_DRV_CHN_BUSY",
    [RSC_DRV_CHN_EN] = "RSC_DRV_CHN_EN",
    [RSC_MAX] = "RSC_MAX",
};

// for cam rsc
static rpmh_reset_regs cam_reset_regs = {
    [DRV_ID]                        = 0x00040300,
    [DRV_SOLVER_CONFIG]             = 0x00010100,
    [DRV_PRNT_CHLD_CONFIG]          = 0x60004104,

    [RSC_DRV_IRQ_ENABLE]            = 0x00000000,
    [RSC_DRV_IRQ_STATUS]            = 0x00000000,
    [RSC_DRV_IRQ_CLEAR]             = 0x00000000,

    [RSC_DRV_CMD_WAIT_FOR_CMPL]     = 0x00000000,
    [RSC_DRV_CONTROL]               = 0x00000000,
    [RSC_DRV_STATUS]                = 0x00000001,
    [RSC_DRV_CMD_ENABLE]            = 0x00000000,
};

// for apps rsc
static rpmh_reset_regs apps_reset_regs = {
     [DRV_ID]                       = 0x00040300,
     [DRV_SOLVER_CONFIG]            = 0x04010100,
     [DRV_PRNT_CHLD_CONFIG]         = 0x800C8104,

     [RSC_DRV_IRQ_ENABLE]           = 0x00000000,
     [RSC_DRV_IRQ_STATUS]           = 0x00000000,
     [RSC_DRV_IRQ_CLEAR]            = 0x00000000,

     [RSC_DRV_CMD_WAIT_FOR_CMPL]    = 0x00000000,
     [RSC_DRV_CONTROL]              = 0x00000000,
     [RSC_DRV_STATUS]               = 0x00000001,
     [RSC_DRV_CMD_ENABLE]           = 0x00000000,

     // [RSC_DRV_CMD_MSGID],
     // [RSC_DRV_CMD_ADDR],
     // [RSC_DRV_CMD_DATA],
     // [RSC_DRV_CMD_STATUS],
     // [RSC_DRV_CMD_RESP_DATA],

     // [RSC_DRV_CHN_SEQ_BUSY],
     // [RSC_DRV_CHN_SEQ_PC],
     // [RSC_DRV_CHN_TCS_TRIGGER],
     // [RSC_DRV_CHN_TCS_COMPLETE],
     // [RSC_DRV_CHN_UPDATE],
     // [RSC_DRV_CHN_BUSY],
     // [RSC_DRV_CHN_EN],
};

static uint32_t rpmh_rsc_reg_offset_ver_3_0[] = {
    [RSC_DRV_TCS_OFFSET] = 672,
    [RSC_DRV_CMD_OFFSET] = 24,

    // global drv settings
    [DRV_ID] = 0x00,
    [DRV_SOLVER_CONFIG] = 0x04,
    [DRV_PRNT_CHLD_CONFIG] = 0x0C,

    // tcs settings
    [RSC_DRV_IRQ_ENABLE] = 0x00,
    [RSC_DRV_IRQ_STATUS] = 0x04,
    [RSC_DRV_IRQ_CLEAR] = 0x08,

    // per tcs
    [RSC_DRV_CMD_WAIT_FOR_CMPL] = 0x20,
    [RSC_DRV_CONTROL] = 0x24,
    [RSC_DRV_STATUS] = 0x28,
    [RSC_DRV_CMD_ENABLE] = 0x2C,

    // per tcs cmd
    [RSC_DRV_CMD_MSGID] = 0x34,
    [RSC_DRV_CMD_ADDR] = 0x38,
    [RSC_DRV_CMD_DATA] = 0x3C,
    [RSC_DRV_CMD_STATUS] = 0x40,
    [RSC_DRV_CMD_RESP_DATA] = 0x44,

    [RSC_DRV_CHN_SEQ_BUSY] = 0x464,
    [RSC_DRV_CHN_SEQ_PC] = 0x468,
    [RSC_DRV_CHN_TCS_TRIGGER] = 0x490,
    [RSC_DRV_CHN_TCS_COMPLETE] = 0x494,
    [RSC_DRV_CHN_UPDATE] = 0x498,
    [RSC_DRV_CHN_BUSY] = 0x49C,
    [RSC_DRV_CHN_EN] = 0x4A0,
};

/* XOB and PBS voting registers are found in the VRM hardware module */
#define CMD_DB_HW_XOB CMD_DB_HW_VRM
#define CMD_DB_HW_PBS CMD_DB_HW_VRM

struct regulator_md {
    const char* compatible;
    enum cmd_db_hw_type ty;

    const char* prop_id[2];
};

const struct regulator_md regulators[] = {
    {
        .compatible = "qcom,rpmh-vrm-regulator",
        .ty = CMD_DB_HW_VRM,
        .prop_id = {
            "qcom,resource-name"
        },
    },
    {
        .compatible = "qcom,rpmh-arc-regulator",
        .ty = CMD_DB_HW_ARC,
        .prop_id = {
            "qcom,resource-name"
        },
    },
    {
        .compatible = "qcom,rpmh-xob-regulator",
        .ty = CMD_DB_HW_XOB,
        .prop_id = {
            "qcom,resource-name"
        },
    },
    {
        .compatible = "qcom,rpmh-pbs-regulator",
        .ty = CMD_DB_HW_PBS,
        .prop_id = {
            "qcom,resource-name"
        },
    },
    {
        .compatible = "qcom,dcvs-fp",
        .ty = CMD_DB_HW_PBS, // random, not checked
        .prop_id = {
            "qcom,ddr-bcm-name",
            "qcom,llcc-bcm-name",
        },
    },
};

static void qcom_rpmh_rsc_init(Object* obj)
{
    QcomRpmhRscState* rpmhs = QCOM_RPMH_RSC(obj);

    rpmhs->cmd_db_entries = g_array_new(false, true, sizeof(struct cmd_db_entry));
}

static uint32_t read_drv_reg(struct rpmh_drv* drv, enum rpmh_regs reg)
{
    return drv->regs[reg - RSC_DRV_START];
}

static void write_drv_reg(struct rpmh_drv* drv, enum rpmh_regs reg, uint32_t val)
{
    drv->regs[reg - RSC_DRV_START] = val;
}

static void reset_drv_regs(rpmh_reset_regs reset_table, struct rpmh_drv* drv)
{
    for (enum rpmh_regs reg = RSC_DRV_START; reg < RSC_DRV_END; ++reg) {
        write_drv_reg(drv, reg, reset_table[reg]);
    }
}

static uint32_t read_tcs_common_reg(struct rpmh_drv* drv, enum rpmh_regs reg)
{
    return drv->tcs_common_regs[reg - RSC_TCS_COMMON_START];
}

static void write_tcs_common_reg(struct rpmh_drv* drv, enum rpmh_regs reg, uint32_t value)
{
    drv->tcs_common_regs[reg - RSC_TCS_COMMON_START] = value;
}

static void reset_tcs_common_regs(rpmh_reset_regs reset_table, struct rpmh_drv* drv)
{
    for (enum rpmh_regs reg = RSC_TCS_COMMON_START; reg < RSC_TCS_COMMON_END; ++reg) {
        write_tcs_common_reg(drv, reg, reset_table[reg]);
    }
}

static uint32_t read_tcs_reg(struct rpmh_tcs* tcs, enum rpmh_regs reg)
{
    return tcs->regs[reg - RSC_TCS_START];
}

static void write_tcs_reg(struct rpmh_tcs* tcs, enum rpmh_regs reg, uint32_t val)
{
    tcs->regs[reg - RSC_TCS_START] = val;
}

static void reset_tcs_regs(rpmh_reset_regs reset_table, struct rpmh_tcs* tcs)
{
    for (enum rpmh_regs reg = RSC_TCS_START; reg < RSC_TCS_END; ++reg) {
        write_tcs_reg(tcs, reg, reset_table[reg]);
    }
}

static uint32_t read_tcs_cmd_reg(struct rpmh_tcs_cmd* cmd, enum rpmh_regs reg)
{
    return cmd->regs[reg - RSC_TCS_CMD_START];
}

static void write_tcs_cmd_reg(struct rpmh_tcs_cmd* tcs_cmd, enum rpmh_regs reg, uint32_t val)
{
    tcs_cmd->regs[reg - RSC_TCS_CMD_START] = val;
}

static void reset_tcs_cmd_regs(rpmh_reset_regs reset_table, struct rpmh_tcs_cmd* tcs_cmd)
{
    for (enum rpmh_regs reg = RSC_TCS_CMD_START; reg < RSC_TCS_CMD_END; ++reg) {
        write_tcs_cmd_reg(tcs_cmd, reg, reset_table[reg]);
    }
}

static enum rpmh_regs decode_drv_addr(QcomRpmhRscState* s, const uint32_t* regtable, hwaddr drv_addr) {
    for (size_t i = RSC_DRV_START; i < RSC_DRV_END; ++i) {
        if (regtable[i] == drv_addr) {
            return i;
        }
    }

    RPMH_RSC_LOG_ERROR(s, "Illegal drv addr: 0x%lx\n", drv_addr);

    return RSC_MAX;
}

static enum rpmh_regs decode_tcs_common_addr(QcomRpmhRscState* s, const uint32_t* regtable, hwaddr tcs_common_addr) {
    for (size_t i = RSC_TCS_COMMON_START; i < RSC_TCS_COMMON_END; ++i) {
        if (regtable[i] == tcs_common_addr) {
            return i;
        }
    }

    RPMH_RSC_LOG_ERROR(s, "Illegal tcs common addr: 0x%lx\n", tcs_common_addr);

    return RSC_MAX;
}

static enum rpmh_regs decode_tcs_addr(QcomRpmhRscState* s, const uint32_t* regtable, hwaddr tcs_addr) {
    for (size_t i = RSC_TCS_START; i < RSC_TCS_END; ++i) {
        if (regtable[i] == tcs_addr) {
            return i;
        }
    }

    RPMH_RSC_LOG_ERROR(s, "Illegal tcs addr: 0x%lx\n", tcs_addr);

    return RSC_MAX;
}

static enum rpmh_regs decode_tcs_cmd_addr(QcomRpmhRscState* s, const uint32_t* regtable, hwaddr tcs_cmd_addr) {
    for (size_t i = RSC_TCS_CMD_START; i < RSC_TCS_CMD_END; ++i) {
        if (regtable[i] == tcs_cmd_addr) {
            return i;
        }
    }

    RPMH_RSC_LOG_WARN(s, "Illegal tcs cmd addr: 0x%lx\n", tcs_cmd_addr);

    return RSC_MAX;
}

static size_t get_drv_id(QcomRpmhRscState* s, hwaddr addr)
{
    // we ignore size / alignment considerations since it is enforced by the definition of the memop.
    for (size_t i = 0; i < s->nb_drvs; ++i) {
        if (s->drvs[i].present && addr >= s->drvs[i].base && addr < s->drvs[i].base + s->drvs[i].size) {
            return i;
        }
    }

    RPMH_RSC_LOG_ERROR(s, "Illegal drv id: 0x%lx\n", addr);

    return RSC_MAX;
}

static int get_tcs_id(QcomRpmhRscState* s, hwaddr addr, struct rpmh_drv* drv)
{
    for (size_t i = 0; i < drv->nb_tcss; ++i) {
        hwaddr tcs_addr = drv->tcs_base + drv->tcs_size * i;
        if (addr >= tcs_addr && addr < tcs_addr + drv->tcs_size) {
            return i;
        }
    }

    RPMH_RSC_LOG_ERROR(s, "Illegal tcs id: 0x%lx\n", addr);
    exit(1);
}

static int get_tcs_cmd_id(QcomRpmhRscState* s, hwaddr addr, struct rpmh_tcs* tcs)
{
    for (size_t i = 0; i < tcs->nb_tcs_cmds; ++i) {
        hwaddr tcs_cmd_addr = tcs->tcs_cmd_base + tcs->tcs_cmd_size * i;
        if (addr >= tcs_cmd_addr && addr < tcs_cmd_addr + tcs->tcs_cmd_size) {
            return i;
        }
    }

    RPMH_RSC_LOG_ERROR(s, "Illegal tcs id: 0x%lx\n", addr);
    exit(1);
}

static hwaddr get_tcs_common_addr(QcomRpmhRscState* s, hwaddr addr, struct rpmh_drv* drv)
{
    return addr - drv->tcs_base;
}


static hwaddr get_tcs_addr(QcomRpmhRscState* s, hwaddr addr, struct rpmh_drv* drv, size_t tcs_id)
{
    return addr - (tcs_id * drv->tcs_size + drv->tcs_base);
}

static hwaddr get_tcs_cmd_addr(QcomRpmhRscState* s, hwaddr addr, struct rpmh_tcs* tcs, size_t tcs_cmd_id)
{
    return addr - (tcs_cmd_id * tcs->tcs_cmd_size + tcs->tcs_cmd_base);
}

static bool is_tcs_cmd_access(QcomRpmhRscState* s, hwaddr addr, struct rpmh_tcs* tcs)
{
    return addr >= tcs->tcs_cmd_base + s->regtable[RSC_TCS_CMD_START] && addr < tcs->tcs_cmd_base + tcs->nb_tcs_cmds * tcs->tcs_cmd_size;
}

static uint64_t rpmh_read_tcs_cmd(QcomRpmhRscState* s, hwaddr addr, unsigned size, struct rpmh_tcs* tcs)
{
    size_t tcs_cmd_id = get_tcs_cmd_id(s, addr, tcs);
    size_t tcs_cmd_addr = get_tcs_cmd_addr(s, addr, tcs, tcs_cmd_id);
    enum rpmh_regs tcs_cmd_reg = decode_tcs_cmd_addr(s, s->regtable, tcs_cmd_addr);

    if (tcs_cmd_reg == RSC_MAX) {
        RPMH_RSC_LOG_ERROR(s, "[!] Illegal read, returning 0.\n");
        return 0;
    }

    struct rpmh_tcs_cmd* tcs_cmd = &tcs->tcs_cmds[tcs_cmd_id];

    RPMH_RSC_LOG(s, "tcs_cmd read for tcs_cmd %lx @addr 0x%lx (reg %s)\n", tcs_cmd_id, tcs_cmd_addr, rpmh_rsc_str[tcs_cmd_reg]);

    return read_tcs_cmd_reg(tcs_cmd, tcs_cmd_reg);
}

static uint64_t rpmh_read_drv(QcomRpmhRscState* s, hwaddr addr, unsigned size, struct rpmh_drv* drv)
{
    hwaddr drv_addr = addr % drv->size;
    enum rpmh_regs drv_reg = decode_drv_addr(s, s->regtable, drv_addr);

    RPMH_RSC_LOG(s, "drv read @addr 0x%lx (reg %s)\n", drv_addr, rpmh_rsc_str[drv_reg]);

    return read_drv_reg(drv, drv_reg);
}

static uint64_t rpmh_read_tcs_common(QcomRpmhRscState* s, hwaddr addr, unsigned size, struct rpmh_drv* drv)
{
    size_t tcs_common_addr = get_tcs_common_addr(s, addr, drv);
    enum rpmh_regs tcs_reg = decode_tcs_common_addr(s, s->regtable, tcs_common_addr);

    RPMH_RSC_LOG(s, "TCS common read @addr 0x%lx (reg %s)\n", tcs_common_addr, rpmh_rsc_str[tcs_reg]);

    switch (tcs_reg) {
        case RSC_DRV_IRQ_ENABLE:
            /* fallthrough */
        case RSC_DRV_IRQ_STATUS:
            return read_tcs_common_reg(drv, tcs_reg);
        case RSC_DRV_IRQ_CLEAR:
            // illegal read
            return 0;
        default:
            RPMH_RSC_LOG_ERROR(s, "Unhandled TCS common read.\n");
            return 0;
    }
}

static uint64_t rpmh_read_tcs(QcomRpmhRscState* s, hwaddr addr, unsigned size, struct rpmh_drv* drv)
{
    size_t tcs_id = get_tcs_id(s, addr, drv);
    struct rpmh_tcs* tcs = &drv->tcss[tcs_id];

    if (is_tcs_cmd_access(s, addr, tcs)) {
        return rpmh_read_tcs_cmd(s, addr, size, tcs);
    } else {
        size_t tcs_addr = get_tcs_addr(s, addr, drv, tcs_id);
        enum rpmh_regs tcs_reg = decode_tcs_addr(s, s->regtable, tcs_addr);

        RPMH_RSC_LOG(s, "read for tcs %lx @addr 0x%lx (reg %s)\n", tcs_id, tcs_addr, rpmh_rsc_str[tcs_reg]);

        return read_tcs_reg(tcs, tcs_reg);
    }
}

static void rpmh_write_drv(QcomRpmhRscState* s, hwaddr addr, unsigned size, uint32_t val, struct rpmh_drv* drv)
{
    hwaddr drv_addr = addr % drv->size;
    enum rpmh_regs drv_reg = decode_drv_addr(s, s->regtable, drv_addr);

    RPMH_RSC_LOG_ERROR(s, "[!] drv write 0x%x at reg %s -> trying to write at read-only register!!!\n", val, rpmh_rsc_str[drv_reg]);
}

static void rpmh_write_tcs_common(QcomRpmhRscState* s, hwaddr addr, unsigned size, uint32_t val, struct rpmh_drv* drv)
{
    size_t tcs_common_addr = get_tcs_common_addr(s, addr, drv);
    enum rpmh_regs tcs_reg = decode_tcs_common_addr(s, s->regtable, tcs_common_addr);

    RPMH_RSC_LOG(s, "common 0x%x at reg %s\n", val, rpmh_rsc_str[tcs_reg]);

    switch (tcs_reg) {
        case RSC_DRV_IRQ_ENABLE:
            // do nothing, useless
           return;
        case RSC_DRV_IRQ_STATUS:
            // read only
            return;
        case RSC_DRV_IRQ_CLEAR: {
            uint32_t irq_status = read_tcs_common_reg(drv, RSC_DRV_IRQ_STATUS);
            irq_status &= ~val;
            write_tcs_common_reg(drv, RSC_DRV_IRQ_STATUS, irq_status);

            qemu_set_irq(drv->irq, 0);
            return;
        }
        default:
            RPMH_RSC_LOG_ERROR(s, "Unhandled TCS common write.\n");
            return;
    }
}

static void rpmh_write_tcs_cmd(QcomRpmhRscState* s, hwaddr addr, unsigned size, uint32_t val, struct rpmh_tcs* tcs)
{
    size_t tcs_cmd_id = get_tcs_cmd_id(s, addr, tcs);
    hwaddr tcs_cmd_addr = get_tcs_cmd_addr(s, addr, tcs, tcs_cmd_id);
    enum rpmh_regs tcs_reg = decode_tcs_cmd_addr(s, s->regtable, tcs_cmd_addr);

    if (tcs_reg == RSC_MAX) {
        RPMH_RSC_LOG_WARN(s, "[!] Illegal write @addr 0x%lx\n", addr);
        return;
    }

    RPMH_RSC_LOG(s, "[*] TCS common 0x%x at reg %s\n", val, rpmh_rsc_str[tcs_reg]);

    struct rpmh_tcs_cmd* tcs_cmd = &tcs->tcs_cmds[tcs_cmd_id];

    write_tcs_cmd_reg(tcs_cmd, tcs_reg, val);
}

#define TCS_CONTROL_TRIGGER (1 << 24)
#define TCS_CMD_ENABLE_MASK GENMASK(11, 0)

static void rpmh_write_tcs(QcomRpmhRscState* s, hwaddr addr, unsigned size, uint32_t val, struct rpmh_drv* drv)
{
    size_t tcs_id = get_tcs_id(s, addr, drv);
    struct rpmh_tcs* tcs = &drv->tcss[tcs_id];

    if (is_tcs_cmd_access(s, addr, tcs)) {
        rpmh_write_tcs_cmd(s, addr, size, val, tcs);
    } else {
        size_t tcs_addr = get_tcs_addr(s, addr, drv, tcs_id);
        enum rpmh_regs tcs_reg = decode_tcs_addr(s, s->regtable, tcs_addr);

        RPMH_RSC_LOG(s, "[*] TCS write for tcs %lx 0x%x at reg %s\n", tcs_id, val, rpmh_rsc_str[tcs_reg]);

        switch (tcs_reg) {
            case RSC_DRV_CMD_WAIT_FOR_CMPL: {
                write_tcs_reg(tcs, tcs_reg, val);
                break;
            }
            case RSC_DRV_CONTROL: {
                if (val & TCS_CONTROL_TRIGGER) {
                    uint32_t irq_status = read_tcs_common_reg(drv, RSC_DRV_IRQ_STATUS);
                    irq_status |= (1 << tcs_id);
                    write_tcs_common_reg(drv, RSC_DRV_IRQ_STATUS, irq_status);

                    qemu_set_irq(drv->irq, 1);
                }

                write_tcs_reg(tcs, tcs_reg, val);
                break;
            }
            case RSC_DRV_STATUS: {
                // read only
                break;
            }
            case RSC_DRV_CMD_ENABLE: {
                tcs->cmd_enabled = val & TCS_CMD_ENABLE_MASK;
                break;
            }
            default: {
                RPMH_RSC_LOG_ERROR(s, "Unexpected TSC reg: %s (%d)\n", rpmh_rsc_str[tcs_reg], tcs_reg);
                break;
            }
        }
    }
}

static bool is_tcs_common_access(QcomRpmhRscState* s, hwaddr addr, struct rpmh_drv* drv)
{
    return addr >= drv->tcs_base && addr < drv->tcs_base + s->regtable[RSC_TCS_COMMON_END];
}

static bool is_tcs_access(QcomRpmhRscState* s, hwaddr addr, struct rpmh_drv* drv)
{
    return addr >= drv->tcs_base + s->regtable[RSC_TCS_START] && addr < drv->tcs_base + drv->nb_tcss * drv->tcs_size;
}

static uint32_t rpmh_resolve_rsc_read(QcomRpmhRscState* s, hwaddr addr, unsigned size, struct rpmh_drv* drv)
{
    if (is_tcs_common_access(s, addr, drv)) {
        return rpmh_read_tcs_common(s, addr, size, drv);
    } else if (is_tcs_access(s, addr, drv)) {
        return rpmh_read_tcs(s, addr, size, drv);
    } else {
        return rpmh_read_drv(s, addr, size, drv);
    }
}

static void rpmh_resolve_rsc_write(QcomRpmhRscState* s, hwaddr addr, unsigned size, uint32_t val, struct rpmh_drv* drv)
{
    if (is_tcs_common_access(s, addr, drv)) {
        rpmh_write_tcs_common(s, addr, size, val, drv);
    } else if (is_tcs_access(s, addr, drv)) {
        rpmh_write_tcs(s, addr, size, val, drv);
    } else {
        rpmh_write_drv(s, addr, size, val, drv);
    }
}

static uint64_t qcom_rpmh_rsc_read(void *opaque, hwaddr addr, unsigned size)
{
    QcomRpmhRscState *s = QCOM_RPMH_RSC(opaque);

    size_t drv_id = get_drv_id(s, addr);
    struct rpmh_drv* drv = &s->drvs[drv_id];

    return rpmh_resolve_rsc_read(s, addr, size, drv);
}

static void qcom_rpmh_rsc_write(void *opaque, hwaddr addr,
                              uint64_t value, unsigned int size)
{
    QcomRpmhRscState *s = QCOM_RPMH_RSC(opaque);

    size_t drv_id = get_drv_id(s, addr);
    struct rpmh_drv* drv = &s->drvs[drv_id];

    rpmh_resolve_rsc_write(s, addr, size, value, drv);
}

static const MemoryRegionOps qcom_rpmh_rsc_ops = {
    .read = qcom_rpmh_rsc_read,
    .write = qcom_rpmh_rsc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void qcom_rpmh_rsc_realize(OfSysBusDevice* ofdev, Error **errp)
{
    QcomRpmhRscState *s = QCOM_RPMH_RSC(ofdev);
    SysBusDevice* sbd = SYS_BUS_DEVICE(ofdev);
    int len;

    const uint32_t* reset_table;
    if (!strcmp(ofdev->name, "cam_rsc")) {
        reset_table = cam_reset_regs;
    } else if (!strcmp(ofdev->name, "apps_rsc")) {
        reset_table = apps_reset_regs;
    } else {
        error_setg(errp, "%s: unknown RPMh device: %s",
                   __func__, ofdev->name);
        return;
    }
    
    s->regtable = rpmh_rsc_reg_offset_ver_3_0; // TODO: check correctly
    s->drvs = g_new0(struct rpmh_drv, ofdev->nb_regs);
    s->nb_drvs = ofdev->nb_regs;

    for (size_t i = 0; i < ofdev->nb_regs; ++i) {
        char* drv_node = g_strdup_printf("%s/drv@%ld", ofdev->node_path, i);

        if (!qemu_fdt_node_exists(ofdev->fdt, drv_node)) {
            continue;
        }

        struct rpmh_drv* drv = &s->drvs[i];

        drv->present = true;
        sysbus_init_irq(sbd, &drv->irq);

        size_t tcs_distance = qemu_fdt_getprop_cell(ofdev->fdt, drv_node, "qcom,tcs-distance", &len, errp);
        drv->tcs_offset = qemu_fdt_getprop_cell(ofdev->fdt, drv_node, "qcom,tcs-offset", &len, errp);
        drv->tcs_distance = tcs_distance;

        drv->base = ofdev->regs[i].addr;
        drv->size = ofdev->regs[i].size;

        drv->tcs_base= drv->base + drv->tcs_offset;

        if (tcs_distance) {
            drv->tcs_size = tcs_distance;
        } else {
            drv->tcs_size = drv->regs[RSC_DRV_TCS_OFFSET];
        }

        reset_drv_regs(reset_table, &s->drvs[i]);
        reset_tcs_common_regs(reset_table, &s->drvs[i]);

        uint32_t parentchild_config = read_drv_reg(drv, DRV_PRNT_CHLD_CONFIG);
        drv->nb_tcss = EXTRACT_BITS(parentchild_config, 6 * i + 5, 6 * i);
        s->nb_cmds_per_tcs = EXTRACT_BITS(parentchild_config, 31, 27);

        drv->tcss = g_new0(struct rpmh_tcs, drv->nb_tcss);

        for (size_t j = 0; j < drv->nb_tcss; ++j) {
            struct rpmh_tcs* tcs = &drv->tcss[j];
            reset_tcs_regs(reset_table, tcs);
            tcs->tcs_cmds = g_new0(struct rpmh_tcs_cmd, s->nb_cmds_per_tcs);
            tcs->nb_tcs_cmds = s->nb_cmds_per_tcs;
            tcs->tcs_cmd_base = drv->tcs_base + drv->tcs_size * j;
            tcs->tcs_cmd_size = drv->regs[RSC_DRV_CMD_OFFSET];
            tcs->parent = drv;

            for (size_t k = 0; k < tcs->nb_tcs_cmds; ++k) {
                struct rpmh_tcs_cmd* tcs_cmd = &tcs->tcs_cmds[k];
                reset_tcs_cmd_regs(reset_table, tcs_cmd);
                tcs_cmd->parent = tcs;
            }
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(regulators); ++i) {
        const struct regulator_md* reg = &regulators[i];
        struct fdt_iter iter = qemu_fdt_compat_iter_create(ofdev->fdt, reg->compatible, ofdev->node_path);
        char* node_path;
        while((node_path = qemu_fdt_compat_iter_next(ofdev->fdt, &iter))) {
            for (size_t j = 0; j < ARRAY_SIZE(reg->prop_id); ++j) {
                const char* prop_id = reg->prop_id[j];
                if (prop_id) {
                    const char* id = qemu_fdt_getprop_string(ofdev->fdt, node_path, prop_id, errp);

                    struct bcm_db bcm_db;
                    qcom_cmd_db_array_add_entry(s->cmd_db_entries, id, reg->ty, 1, &bcm_db, sizeof(bcm_db));
                }
            }

            g_free(node_path);
        }
    }

	assert(ofdev->mem_size);
    memory_region_init_io(&s->iomem, OBJECT(ofdev), &qcom_rpmh_rsc_ops, s, TYPE_QCOM_RPMH_RSC, ofdev->mem_size);
    sysbus_init_mmio(sbd, &s->iomem);
}

// static const struct of_device_id crm_drv_match[] = {
// 	{ .compatible = "qcom,cam-crm-v3", .data = &cam_crm_desc_v3},
// 	{ .compatible = "qcom,pcie-crm-v3", .data = &pcie_crm_desc_v3},
// 	{ .compatible = "qcom,disp-crm-v3", .data = &disp_crm_desc_v3},
// 	{ }
// };

static void qcom_rpmh_rsc_class_init(ObjectClass* oc, void* data)
{
	OfSysBusDeviceClass* kofdev = OF_SYS_BUS_DEVICE_CLASS(oc);

    kofdev->realize = qcom_rpmh_rsc_realize;
}

static const TypeInfo qcom_rpmh_rsc_info = {
    .name = TYPE_QCOM_RPMH_RSC,
    .parent = TYPE_OF_SYS_BUS_DEVICE,
    .instance_size = sizeof(QcomRpmhRscState),
    .instance_init = qcom_rpmh_rsc_init,
    .class_init = qcom_rpmh_rsc_class_init,
};

static void qcom_rpmh_rsc_register_types(void)
{
    type_register_static(&qcom_rpmh_rsc_info);
}

type_init(qcom_rpmh_rsc_register_types);
