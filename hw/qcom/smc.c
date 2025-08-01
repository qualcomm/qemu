#include "qemu/osdep.h"
#include "hw/qcom/smc.h"
#include "cpu.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/multiprocessing.h"
#include "target/arm/gtimer.h"

#define SCM_SMC_N_REG_ARGS 4
#define SCM_SMC_FIRST_EXT_IDX (SCM_SMC_N_REG_ARGS - 1)
#define SCM_SMC_N_EXT_ARGS (MAX_QCOM_SCM_ARGS - SCM_SMC_N_REG_ARGS + 1)
#define SCM_SMC_FIRST_REG_IDX 2
#define SCM_SMC_LAST_REG_IDX (SCM_SMC_FIRST_REG_IDX + SCM_SMC_N_REG_ARGS - 1)

#define ARM_SMCCC_STD_CALL        _AC(0,U)
#define ARM_SMCCC_FAST_CALL        _AC(1,U)
#define ARM_SMCCC_TYPE_SHIFT      31

#define ARM_SMCCC_SMC_32 0
#define ARM_SMCCC_SMC_64 1
#define ARM_SMCCC_CALL_CONV_SHIFT 30

#define ARM_SMCCC_OWNER_MASK 0x3F
#define ARM_SMCCC_OWNER_SHIFT 24

#define ARM_SMCCC_FUNC_MASK 0xFFFF

#define MAX_QCOM_SCM_ARGS 10
#define MAX_QCOM_SCM_RETS 3

#define QCOM_SCM_SVC_BOOT       0x01
#define QCOM_SCM_BOOT_SET_ADDR      0x01
#define QCOM_SCM_BOOT_TERMINATE_PC  0x02
#define QCOM_SCM_BOOT_SDI_CONFIG    0x09
#define QCOM_SCM_BOOT_SET_DLOAD_MODE    0x10
#define QCOM_SCM_BOOT_SET_ADDR_MC   0x11
#define QCOM_SCM_BOOT_SET_REMOTE_STATE  0x0a
#define QCOM_SCM_FLUSH_FLAG_MASK    0x3
#define QCOM_SCM_BOOT_MAX_CPUS      4
#define QCOM_SCM_BOOT_MC_FLAG_AARCH64   BIT(0)
#define QCOM_SCM_BOOT_MC_FLAG_COLDBOOT  BIT(1)
#define QCOM_SCM_BOOT_MC_FLAG_WARMBOOT  BIT(2)

#define QCOM_SCM_SVC_PIL        0x02
#define QCOM_SCM_PIL_PAS_INIT_IMAGE 0x01
#define QCOM_SCM_PIL_PAS_MEM_SETUP  0x02
#define QCOM_SCM_PIL_PAS_AUTH_AND_RESET 0x05
#define QCOM_SCM_PIL_PAS_SHUTDOWN   0x06
#define QCOM_SCM_PIL_PAS_IS_SUPPORTED   0x07
#define QCOM_SCM_PIL_PAS_MSS_RESET  0x0a

#define QCOM_SCM_SVC_IO         0x05
#define QCOM_SCM_IO_READ        0x01
#define QCOM_SCM_IO_WRITE       0x02

#define QCOM_SCM_SVC_INFO       0x06
#define QCOM_SCM_INFO_IS_CALL_AVAIL 0x01

#define QCOM_SCM_SVC_MP             0x0c
#define QCOM_SCM_MP_RESTORE_SEC_CFG     0x02
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE  0x03
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT  0x04
#define QCOM_SCM_MP_IOMMU_SET_CP_POOL_SIZE  0x05
#define QCOM_SCM_MP_VIDEO_VAR           0x08
#define QCOM_SCM_MP_ASSIGN          0x16
#define QCOM_SCM_MP_SHM_BRIDGE_ENABLE       0x1c
#define QCOM_SCM_MP_SHM_BRIDGE_DELETE       0x1d
#define QCOM_SCM_MP_SHM_BRIDGE_CREATE       0x1e

#define QCOM_SCM_SVC_OCMEM      0x0f
#define QCOM_SCM_OCMEM_LOCK_CMD     0x01
#define QCOM_SCM_OCMEM_UNLOCK_CMD   0x02

#define QCOM_SCM_SVC_ES         0x10    /* Enterprise Security */
#define QCOM_SCM_ES_INVALIDATE_ICE_KEY  0x03
#define QCOM_SCM_ES_CONFIG_SET_ICE_KEY  0x04

#define QCOM_SCM_SVC_HDCP       0x11
#define QCOM_SCM_HDCP_INVOKE        0x01

#define QCOM_SCM_SVC_LMH            0x13
#define QCOM_SCM_LMH_LIMIT_PROFILE_CHANGE   0x01
#define QCOM_SCM_LMH_LIMIT_DCVSH        0x10

#define QCOM_SCM_SVC_SMMU_PROGRAM       0x15
#define QCOM_SCM_SMMU_PT_FORMAT         0x01
#define QCOM_SCM_SMMU_CONFIG_ERRATA1        0x03
#define QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL 0x02

#define QCOM_SCM_SVC_WAITQ          0x24
#define QCOM_SCM_WAITQ_RESUME           0x02
#define QCOM_SCM_WAITQ_GET_WQ_CTX       0x03

#define QCOM_SCM_SVC_GPU            0x28
#define QCOM_SCM_SVC_GPU_INIT_REGS      0x01

/* common error codes */
#define QCOM_SCM_V2_EBUSY   -12
#define QCOM_SCM_ENOMEM     -5
#define QCOM_SCM_EOPNOTSUPP -4
#define QCOM_SCM_EINVAL_ADDR    -3
#define QCOM_SCM_EINVAL_ARG -2
#define QCOM_SCM_ERROR      -1
#define QCOM_SCM_INTERRUPTED    1
#define QCOM_SCM_WAITQ_SLEEP    2

enum qcom_scm_convention {
    SMC_CONVENTION_UNKNOWN,
    SMC_CONVENTION_LEGACY,
    SMC_CONVENTION_ARM_32,
    SMC_CONVENTION_ARM_64,
};

enum qcom_scm_arg_types {
    QCOM_SCM_VAL,
    QCOM_SCM_RO,
    QCOM_SCM_RW,
    QCOM_SCM_BUFVAL,
};

/**
 * struct qcom_scm_desc
 * @arginfo:Metadata describing the arguments in args[]
 * @args:The array of arguments for the secure syscall
 */
struct qcom_scm_desc {
    uint32_t svc;
    uint32_t cmd;
    uint32_t arginfo;
    uint64_t args[MAX_QCOM_SCM_ARGS];
    uint32_t owner;
    bool multicall_allowed;
};

/**
 * struct qcom_scm_res
 * @result:The values returned by the secure syscall
 */
struct qcom_scm_res {
    uint64_t result[MAX_QCOM_SCM_RETS];
};

/**
 * struct arm_smccc_res - Result from SMC/HVC call
 * @a0-a3 result values from registers 0 to 3
 */
struct arm_smccc_res {
    union {
        struct {
            target_ulong a0;
            target_ulong a1;
            target_ulong a2;
            target_ulong a3;
        };

        target_ulong regs[4];
    };
};


enum qcom_scm_call_type {
    QCOM_SCM_CALL_NORMAL,
    QCOM_SCM_CALL_ATOMIC,
    QCOM_SCM_CALL_NORETRY,
};

enum qcom_scm_wq_feature {
    QCOM_SCM_SINGLE_SMC_ALLOW,
    QCOM_SCM_MULTI_SMC_WHITE_LIST_ALLOW, /* Release global lock for certain allowed SMC calls */
};

// static void dump_scm(const struct qcom_scm_desc* desc, const struct arm_smccc_res* res)
// {
//     printf("SVC: 0x%x\n", desc->svc);
//     printf("CMD: 0x%x\n", desc->cmd);
//     printf("arginfo: %d\n", desc->arginfo);

//     if (res) {
//         printf("res:\n");
//         for (size_t i = 0; i < ARRAY_SIZE(res->regs); ++i) {
//             printf("\ta%ld: 0x%lx\n", i, res->regs[i]);
//         }
//     }
// }

static struct qcom_scm_res smc_handle(ARMCPU* cpu, const struct qcom_scm_desc* desc, enum qcom_scm_call_type type, enum qcom_scm_convention conv)
{
    struct qcom_scm_res res = { 0 };
    size_t nargs = desc->arginfo & 0xf;

    switch (desc->svc) {
        case QCOM_SCM_SVC_INFO:
            assert(desc->cmd == QCOM_SCM_INFO_IS_CALL_AVAIL);
            assert(nargs == 1);

            res.result[0] = 1;

            break;
        default:
            // printf("[qcom-scm] Unhandled SVC.\n");
            break;
    }

    return res;
}

void qcom_smc_handler(ARMCPU* cpu)
{
    CPUARMState *env = &cpu->env;
    uint64_t param[SCM_SMC_FIRST_REG_IDX + SCM_SMC_N_REG_ARGS];

    for (size_t i = 0; i < ARRAY_SIZE(param); i++) {
        param[i] = is_a64(env) ? env->xregs[i] : env->regs[i];
    }

    uint32_t type = (param[0] >> ARM_SMCCC_TYPE_SHIFT) & 1;
    uint32_t calling_convention = (param[0] >> ARM_SMCCC_CALL_CONV_SHIFT) & 1;
    uint32_t owner = (param[0] >> ARM_SMCCC_OWNER_SHIFT) & ARM_SMCCC_OWNER_MASK;
    uint32_t func_num = param[0] & ARM_SMCCC_FUNC_MASK;

    uint32_t svc = (func_num >> 8) & 0xff;
    uint32_t cmd = func_num & 0xff;

    uint32_t arginfo = param[1];

    struct qcom_scm_desc desc = {
        .svc = svc,
        .cmd = cmd,
        .arginfo = arginfo,
        .owner = owner,
    };

    for (size_t i = 0; i < SCM_SMC_N_REG_ARGS; ++i) {
        desc.args[i] = param[SCM_SMC_FIRST_REG_IDX + i];
    }

    struct qcom_scm_res res = smc_handle(cpu, &desc, type, calling_convention);

    struct arm_smccc_res arch_reg = {
        .a0 = 0,
        .a1 = res.result[0],
        .a3 = res.result[0],
        .a2 = res.result[2],
    };

    // printf("SMC call:\n");
    // dump_scm(&desc, &arch_reg);
    // printf("\n");

    for (size_t i = 0; i < 4; ++i) {
        if (is_a64(env)) {
            env->xregs[i] = arch_reg.regs[i];
        } else {
            env->regs[i] = arch_reg.regs[i];
        }
    }

    return;
}
