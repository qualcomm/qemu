/* 
 * Qualcomm Android RPMh RSC device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Only provides minimal support, mostly to pass probe checks.
 */

#ifndef QEMU_QCOM_RPMH_RSC_H
#define QEMU_QCOM_RPMH_RSC_H

#include "qemu/osdep.h"
#include "qom/object.h"
#include "hw/sysbus-of.h"

#define TYPE_QCOM_RPMH_RSC "qcom-rpmh-rsc"
OBJECT_DECLARE_SIMPLE_TYPE(QcomRpmhRscState, QCOM_RPMH_RSC)

// can be taken from reset value for 
// #define CANOE_NB_TCS                4
// #define CANOE_NUM_CMDS_PER_TCS      12

enum rpmh_regs {
    /* offsets */
    RSC_DRV_TCS_OFFSET,
    RSC_DRV_CMD_OFFSET,
    // drv regs | base: drv
    DRV_ID,
    DRV_SOLVER_CONFIG,
    DRV_PRNT_CHLD_CONFIG,
    // tcs common regs | base: tcs
    RSC_DRV_IRQ_ENABLE,
    RSC_DRV_IRQ_STATUS,
    RSC_DRV_IRQ_CLEAR,
    // tcs specific regs | base: tcs
    RSC_DRV_CMD_WAIT_FOR_CMPL,
    RSC_DRV_CONTROL,
    RSC_DRV_STATUS,
    RSC_DRV_CMD_ENABLE,
    // tcs cmd regs | base: cmd
    RSC_DRV_CMD_MSGID,
    RSC_DRV_CMD_ADDR,
    RSC_DRV_CMD_DATA,
    RSC_DRV_CMD_STATUS,
    RSC_DRV_CMD_RESP_DATA,
    // DRV channel Registers | base: drv_base
    RSC_DRV_CHN_TCS_TRIGGER,
    RSC_DRV_CHN_TCS_COMPLETE,
    RSC_DRV_CHN_SEQ_BUSY,
    RSC_DRV_CHN_SEQ_PC,
    RSC_DRV_CHN_UPDATE,
    RSC_DRV_CHN_BUSY,
    RSC_DRV_CHN_EN,
    RSC_MAX,
};

#define RSC_DRV_START           DRV_ID
#define RSC_DRV_END             RSC_DRV_IRQ_ENABLE
#define RSC_DRV_SIZE            (RSC_DRV_END - RSC_DRV_START)

#define RSC_TCS_COMMON_START    RSC_DRV_IRQ_ENABLE
#define RSC_TCS_COMMON_END      RSC_DRV_CMD_WAIT_FOR_CMPL
#define RSC_TCS_COMMON_SIZE     (RSC_TCS_COMMON_END - RSC_TCS_COMMON_START)

#define RSC_TCS_START           RSC_DRV_CMD_WAIT_FOR_CMPL
#define RSC_TCS_END             RSC_DRV_CMD_MSGID
#define RSC_TCS_SIZE            (RSC_TCS_END - RSC_TCS_START)

#define RSC_TCS_CMD_START       RSC_DRV_CMD_MSGID
#define RSC_TCS_CMD_END         RSC_DRV_CHN_TCS_TRIGGER
#define RSC_TCS_CMD_SIZE        (RSC_TCS_CMD_END - RSC_TCS_CMD_START)

#define RSC_CHN_START           RSC_DRV_CHN_TCS_TRIGGER
#define RSC_CHN_END             RSC_MAX
#define RSC_CHN_SIZE            (RSC_CHN_END - RSC_CHN_START)

typedef const uint32_t rpmh_reset_regs[RSC_MAX];

typedef uint32_t rpmh_drv_regs[RSC_DRV_SIZE];
typedef uint32_t rpmh_tcs_common_regs[RSC_TCS_COMMON_SIZE];
typedef uint32_t rpmh_tcs_regs[RSC_TCS_SIZE];
typedef uint32_t rpmh_tcs_cmd_regs[RSC_TCS_CMD_SIZE];

struct rpmh_drv;
struct rpmh_tcs;
struct rpmh_tcs_cmd;

struct rpmh_tcs_cmd {
    struct rpmh_tcs* parent;

    rpmh_tcs_cmd_regs regs;
};

struct rpmh_tcs {
    struct rpmh_drv* parent;

    rpmh_tcs_regs regs;
    struct rpmh_tcs_cmd* tcs_cmds;
    size_t nb_tcs_cmds;
    hwaddr tcs_cmd_base;
    hwaddr tcs_cmd_size;

    bool triggered;
    uint32_t cmd_enabled;
};

struct rpmh_drv {
    bool present;

    rpmh_drv_regs regs;

    hwaddr base;
    hwaddr size;

    rpmh_tcs_common_regs tcs_common_regs;
    struct rpmh_tcs* tcss;
    size_t nb_tcss;
    hwaddr tcs_base;
    hwaddr tcs_size;
    hwaddr tcs_offset;
    hwaddr tcs_distance;

    uint32_t tcs_irq_status;

    qemu_irq irq;
};

struct QcomRpmhRscState {
    OfSysBusDevice parent;

    const char* name;
    uint64_t mem_size;

    const uint32_t* regtable;

    size_t nb_drvs;
    struct rpmh_drv* drvs;
    size_t nb_cmds_per_tcs;

    MemoryRegion iomem;

    GArray* cmd_db_entries;
};

QcomRpmhRscState* rpmh_rsc_create(void* out_fdt, void* in_fdt, const char* node_path, const char* name, uint64_t mem_size);

QcomRpmhRscState* rpmh_rsc_create_by_label(void* out_fdt, void* in_fdt, const char* label, uint64_t mem_size);

#endif
