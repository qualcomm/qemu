/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEXAGON_CPU_H
#define HEXAGON_CPU_H

/* Forward declaration needed by some of the header files */
struct CPUArchState;
typedef struct CPUArchState CPUHexagonState;
typedef struct ProcessorState processor_t;
typedef struct SystemState system_t;

#include "fpu/softfloat-types.h"

#include "exec/cpu-defs.h"
#include "mmvec/mmvec.h"
#include "dma/dma.h"
#include "hw/registerfields.h"
#include "hw/hexagon/hexagon.h"
#include "max.h"

extern unsigned cpu_mmu_index(CPUHexagonState *env, bool ifetch);
#ifndef CONFIG_USER_ONLY
#include "reg_fields.h"
typedef struct CPUHexagonTLBContext CPUHexagonTLBContext;
#define NUM_SREGS 86
#define NUM_GREGS 32
#define GREG_WRITES_MAX 32
#define SREG_WRITES_MAX 64
#define NUM_TLB_REGS(PROC) NUM_TLB_ENTRIES
#endif

#include "hex_regs.h"

#define TARGET_LONG_BITS 32

/*
 * Hexagon processors have a strong memory model.
 */
#define TCG_GUEST_DEFAULT_MO      (TCG_MO_ALL)
#include "qom/object.h"
#include "hw/core/cpu.h"
#include "hw/registerfields.h"

#define NUM_PREGS 4
#define TOTAL_PER_THREAD_REGS 64
#define NUM_GPREGS 32
#define NUM_GLOBAL_GCYCLE 6
#define NUM_PMU_CTRS 8

#define SLOTS_MAX 4
#define STORES_MAX 2
#define REG_WRITES_MAX 32
#define PRED_WRITES_MAX 5                   /* 4 insns + endloop */
#define VSTORES_MAX 2
#define THREADS_MAX 8
#define VECTOR_UNIT_MAX 4
#define PARANOID_VALUE (~0)

#define TYPE_HEXAGON_CPU "hexagon-cpu"

#define VTCM_SIZE              0x40000LL
#define VTCM_OFFSET            0x200000LL

#ifndef CONFIG_USER_ONLY
/*
 * Represents the maximum number of consecutive
 * translation blocks to execute on a CPU before yielding
 * to another CPU.
 */
#define HEXAGON_TB_EXEC_PER_CPU_MAX 2000

#define CPU_INTERRUPT_SWI      CPU_INTERRUPT_TGT_INT_0
#endif

#define HEXAGON_CPU_TYPE_SUFFIX "-" TYPE_HEXAGON_CPU
#define HEXAGON_CPU_TYPE_NAME(name) (name HEXAGON_CPU_TYPE_SUFFIX)
#define CPU_RESOLVING_TYPE TYPE_HEXAGON_CPU

#define TYPE_HEXAGON_CPU_ANY HEXAGON_CPU_TYPE_NAME("any")
#define TYPE_HEXAGON_CPU_V66 HEXAGON_CPU_TYPE_NAME("v66")
#define TYPE_HEXAGON_CPU_V67 HEXAGON_CPU_TYPE_NAME("v67")

void hexagon_cpu_list(void);
#define cpu_list hexagon_cpu_list

typedef struct {
  int unused;
} rev_features_t;

enum mem_access_types {
    access_type_INVALID = 0,
    access_type_unknown = 1,
    access_type_load = 2,
    access_type_store = 3,
    access_type_fetch = 4,
    access_type_dczeroa = 5,
    access_type_dccleana = 6,
    access_type_dcinva = 7,
    access_type_dccleaninva = 8,
    access_type_icinva = 9,
    access_type_ictagr = 10,
    access_type_ictagw = 11,
    access_type_icdatar = 12,
    access_type_dcfetch = 13,
    access_type_l2fetch = 14,
    access_type_l2cleanidx = 15,
    access_type_l2cleaninvidx = 16,
    access_type_l2tagr = 17,
    access_type_l2tagw = 18,
    access_type_dccleanidx = 19,
    access_type_dcinvidx = 20,
    access_type_dccleaninvidx = 21,
    access_type_dctagr = 22,
    access_type_dctagw = 23,
    access_type_k0unlock = 24,
    access_type_l2locka = 25,
    access_type_l2unlocka = 26,
    access_type_l2kill = 27,
    access_type_l2gclean = 28,
    access_type_l2gcleaninv = 29,
    access_type_l2gunlock = 30,
    access_type_synch = 31,
    access_type_isync = 32,
    access_type_pause = 33,
    access_type_load_phys = 34,
    access_type_load_locked = 35,
    access_type_store_conditional = 36,
    access_type_barrier = 37,
#ifdef CLADE
    access_type_clade = 38,
#endif
    access_type_memcpy_load = 39,
    access_type_memcpy_store = 40,
#ifdef CLADE2
    access_type_clade2 = 41,
#endif
    access_type_udma_load = 51,
    access_type_udma_store = 52,
    access_type_unpause = 53,
    NUM_CORE_ACCESS_TYPES
};

typedef struct {
    target_ulong va;
    uint8_t width;
    uint32_t data32;
    uint64_t data64;
} MemLog;

typedef struct {
    target_ulong va;
    int size;
    DECLARE_BITMAP(mask, MAX_VEC_SIZE_BYTES) QEMU_ALIGNED(16);
    MMVector data QEMU_ALIGNED(16);
#ifndef CONFIG_USER_ONLY
    uint64_t pa;
#endif
} VStoreLog;

struct dma_state;
typedef uint32_t (*dma_insn_checker_ptr)(struct dma_state *);

typedef struct arch_proc_opt {
    int pmu_enable;
    FILE *dmadebugfile;
    int dmadebug_verbosity;
    int xfp_inexact_enable;
    int xfp_cvt_frac;
    int xfp_cvt_int;
    uint64_t vtcm_size;
    uint64_t vtcm_offset;
    int vtcm_original_mem_entries;
    int QDSP6_DMA_PRESENT;
    int QDSP6_DMA_EXTENDED_VA_PRESENT;
    int QDSP6_VX_PRESENT;
    int QDSP6_VX_CONTEXTS;
    int QDSP6_VX_MEM_ENTRIES;
    int QDSP6_VX_VEC_SZ;
} arch_proc_opt_t;

typedef struct {
    uint64_t l2tcm_base;
} options_struct;

struct ProcessorState {
    const rev_features_t *features;
    const options_struct *options;
    const arch_proc_opt_t *arch_proc_options;
    target_ulong runnable_threads_max;
    target_ulong thread_system_mask;
    CPUHexagonState *thread[THREADS_MAX];

    /* Useful information of the DMA engine per thread. */
    struct dma_adapter_engine_info *dma_engine_info[THREADS_MAX];
    struct dma_state *dma[DMA_MAX]; /* same as dma_t */
    dma_insn_checker_ptr dma_insn_checker[DMA_MAX];
    uint64_t monotonic_pcycles; /* never reset */

    int timing_on;
};

#include "xlate_info.h"

typedef struct {
    target_ulong vaddr;
    target_ulong bad_vaddr;
    uint64_t paddr;
    uint32_t range;
    uint64_t lddata;
    uint64_t stdata;
    int cancelled;
    int tnum;
    int size;
    enum mem_access_types type;
    unsigned char cdata[512];
    uint32_t width;
    uint32_t page_cross_base;
    uint32_t page_cross_sum;
    uint16_t slot;
    uint8_t check_page_crosses;
    xlate_info_t xlate_info;
    uint8_t is_dealloc:1;
    uint8_t is_memop:1;
    uint8_t valid:1;
    uint8_t log_as_tag:1;
    uint8_t no_deriveumaptr:1;
} mem_access_info_t;

#ifndef CONFIG_USER_ONLY
#define HEX_CPU_MODE_USER    1
#define HEX_CPU_MODE_GUEST   2
#define HEX_CPU_MODE_MONITOR 3

#define HEX_EXE_MODE_OFF     1
#define HEX_EXE_MODE_RUN     2
#define HEX_EXE_MODE_WAIT    3
#define HEX_EXE_MODE_DEBUG   4
#endif

#define MMU_USER_IDX         0
#ifndef CONFIG_USER_ONLY
#define MMU_GUEST_IDX        1
#define MMU_KERNEL_IDX       2
#endif

#define EXEC_STATUS_OK          0x0000
#define EXEC_STATUS_STOP        0x0002
#define EXEC_STATUS_REPLAY      0x0010
#define EXEC_STATUS_LOCKED      0x0020
#define EXEC_STATUS_EXCEPTION   0x0100


#define EXCEPTION_DETECTED      (env->status & EXEC_STATUS_EXCEPTION)
#define REPLAY_DETECTED         (env->status & EXEC_STATUS_REPLAY)
#define CLEAR_EXCEPTION         (env->status &= (~EXEC_STATUS_EXCEPTION))
#define SET_EXCEPTION           (env->status |= EXEC_STATUS_EXCEPTION)

/* Maximum number of vector temps in a packet */
#define VECTOR_TEMPS_MAX            4

#ifndef CONFIG_USER_ONLY
/*
 * TODO: Update for Hexagon: Meanings of the ARMCPU object's
 * four inbound GPIO lines
 */
#define HEXAGON_CPU_IRQ_0 0
#define HEXAGON_CPU_IRQ_1 1
#define HEXAGON_CPU_IRQ_2 2
#define HEXAGON_CPU_IRQ_3 3
#define HEXAGON_CPU_IRQ_4 4
#define HEXAGON_CPU_IRQ_5 5
#define HEXAGON_CPU_IRQ_6 6
#define HEXAGON_CPU_IRQ_7 7

typedef enum {
    HEX_LOCK_UNLOCKED       = 0,
    HEX_LOCK_WAITING        = 1,
    HEX_LOCK_OWNER          = 2
} hex_lock_state_t;

typedef struct PMUState {
    uint32_t *g_ctrs_off;
    uint16_t *g_events;

    /* Internal counters */
    uint32_t num_packets;
    uint32_t hvx_packets;
} PMUState;

#define ATOMIC_LOAD(VAR) (__atomic_load_n(&(VAR), __ATOMIC_SEQ_CST), (VAR))
#define ATOMIC_STORE(VAR, VAL)                              \
    {                                                       \
        __atomic_store_n(&(VAR), (VAL), __ATOMIC_SEQ_CST);  \
        smp_mb(); /* multiple hw threads access this VAR */ \
    }
#define ATOMIC_EXCHANGE(VAR_ADDR, NEW, OLD)                             \
    {                                                                   \
        OLD = __atomic_exchange_n((VAR_ADDR), (NEW), __ATOMIC_SEQ_CST); \
        smp_mb(); /* multiple hw threads access this VAR */             \
    }
#endif

#define LOCK_IOTHREAD(VAR)          \
    if (!(VAR)) {                   \
        qemu_mutex_lock_iothread(); \
    }
#define UNLOCK_IOTHREAD(VAR)          \
    if (!(VAR)) {                     \
        qemu_mutex_unlock_iothread(); \
    }


struct Einfo {
  uint8_t valid;
  uint8_t type;
  uint8_t cause;
  uint8_t bvs:1;
  uint8_t bv0:1;       /* valid for badva0 */
  uint8_t bv1:1;       /* valid for badva1 */
  uint32_t badva0;
  uint32_t badva1;
  uint32_t elr;
  uint16_t diag;
  uint16_t de_slotmask;
};
typedef struct Einfo hex_exception_info;
typedef struct Instruction Insn;
typedef unsigned systemstate_t;

typedef struct CPUArchState {
    target_ulong gpr[TOTAL_PER_THREAD_REGS];
    target_ulong pred[NUM_PREGS];
    target_ulong branch_taken;
    target_ulong cause_code;

    /* For comparing with LLDB on target - see adjust_stack_ptrs function */
    target_ulong last_pc_dumped;
    target_ulong stack_start;

    uint8_t slot_cancelled;
    target_ulong new_value[TOTAL_PER_THREAD_REGS];
    target_ulong gpreg_written;
    /*
     * This value will be a TCGv treated as a mask of the registers
     * written multiple times in this packet.
     */
    target_ulong mult_reg_written;
    QEMU_BUILD_BUG_MSG(NUM_GPREGS > CHAR_BIT * sizeof(target_ulong),
                       "Hexagon's CPUArchState.gpreg_written type is too small");

#ifndef CONFIG_USER_ONLY
    /* some system registers are per thread and some are global */
    target_ulong t_sreg[NUM_SREGS];
    target_ulong t_sreg_new_value[NUM_SREGS];
    target_ulong t_sreg_written[NUM_SREGS];
    target_ulong *g_sreg;
    target_ulong *g_gcycle;

    target_ulong greg[NUM_GREGS];
    target_ulong greg_new_value[NUM_GREGS];
    target_ulong greg_written[NUM_GREGS];
    target_ulong wait_next_pc;
#endif

    /*
     * Only used when HEX_DEBUG is on, but unconditionally included
     * to reduce recompile time when turning HEX_DEBUG on/off.
     */
    target_ulong this_PC;
    target_ulong reg_written[TOTAL_PER_THREAD_REGS];

    target_ulong new_pred_value[NUM_PREGS];
    target_ulong pred_written;

    MemLog mem_log_stores[STORES_MAX];
    target_ulong pkt_has_scalar_store_s1;
    target_ulong dczero_addr;

    float_status fp_status;

    target_ulong llsc_addr;
    target_ulong llsc_val;
    uint64_t     llsc_val_i64;

    MMVector VRegs[NUM_VREGS] QEMU_ALIGNED(16);
    MMVector future_VRegs[VECTOR_TEMPS_MAX] QEMU_ALIGNED(16);
    MMVector tmp_VRegs[VECTOR_TEMPS_MAX] QEMU_ALIGNED(16);

    VRegMask VRegs_updated_tmp;

    MMQReg QRegs[NUM_QREGS] QEMU_ALIGNED(16);
    MMQReg future_QRegs[NUM_QREGS] QEMU_ALIGNED(16);

    /* Temporaries used within instructions */
    MMVectorPair VuuV QEMU_ALIGNED(16);
    MMVectorPair VvvV QEMU_ALIGNED(16);
    MMVectorPair VxxV QEMU_ALIGNED(16);
    MMVector     vtmp QEMU_ALIGNED(16);
    MMQReg       qtmp QEMU_ALIGNED(16);

    VStoreLog vstore[VSTORES_MAX];
    uint8_t store_pending[VSTORES_MAX];          /* FIXME - Is this needed? */
    target_ulong vstore_pending[VSTORES_MAX];
    bool vtcm_pending;
    VTCMStoreLog vtcm_log;
    mem_access_info_t mem_access[SLOTS_MAX];

    int status;
    uint8_t bq_on;

    unsigned int timing_on;

    uint64_t pktid;
    processor_t *processor_ptr;
    unsigned int threadId;
    system_t *system_ptr;
    uint64_t t_packet_count;
    uint64_t *g_pcycle_base;
    void *vtcm_haddr;
    hwaddr vtcm_base;
#ifndef CONFIG_USER_ONLY
    int slot;                    /* Needed for exception generation */
    hex_exception_info einfo;
    systemstate_t systemstate;
    const char *cmdline;
    CPUHexagonTLBContext *hex_tlb;
    target_ulong imprecise_exception;
    volatile hex_lock_state_t tlb_lock_state; /* different threads modify */
    volatile hex_lock_state_t k0_lock_state; /* different threads modify */
    uint16_t nmi_threads;
    uint32_t last_cpu;
    GList *dir_list;
    uint32_t exe_arch;
    gchar *lib_search_dir;
    PMUState pmu;
    bool ss_pending;
#endif
} CPUHexagonState;
#define mmvecx_t CPUHexagonState

OBJECT_DECLARE_CPU_TYPE(HexagonCPU, HexagonCPUClass, HEXAGON_CPU)

typedef struct HexagonCPUClass {
    /*< private >*/
    CPUClass parent_class;
    /*< public >*/
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
} HexagonCPUClass;

struct ArchCPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/
    CPUNegativeOffsetState neg;
    CPUHexagonState env;

#if !defined(CONFIG_USER_ONLY)
    bool count_gcycle_xt;
    bool sched_limit;
    bool cacheop_exceptions;
    gchar *usefs;
    uint64_t config_table_addr;
    bool vp_mode;
    uint32_t boot_addr;
    uint32_t boot_evb;
    uint32_t l2vic_base_addr;
    uint32_t qtimer_base_addr;
#endif
    uint32_t rev_reg;
    bool lldb_compat;
    target_ulong lldb_stack_adjust;
    bool paranoid_commit_state;
};

#ifndef CONFIG_USER_ONLY
#include "cpu_helper.h"
#endif

uint64_t hexagon_get_sys_pcycle_count(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_low(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_high(CPUHexagonState *env);

#include "cpu_bits.h"

#define cpu_signal_handler cpu_hexagon_signal_handler
extern int cpu_hexagon_signal_handler(int host_signum, void *pinfo, void *puc);

FIELD(TB_FLAGS, IS_TIGHT_LOOP, 0, 1)
FIELD(TB_FLAGS, MMU_INDEX, 1, 3)
FIELD(TB_FLAGS, PCYCLE_ENABLED, 4, 1)
FIELD(TB_FLAGS, HVX_COPROC_ENABLED, 5, 1)
FIELD(TB_FLAGS, HVX_64B_MODE, 6, 1)
FIELD(TB_FLAGS, PMU_ENABLED, 7, 1)
FIELD(TB_FLAGS, SS_ACTIVE, 8, 1)
FIELD(TB_FLAGS, SS_PENDING, 9, 1)


static inline bool rev_implements_64b_hvx(CPUHexagonState *env)
{
    HexagonCPU *hex_cpu = container_of(env, HexagonCPU, env);
    return (hex_cpu->rev_reg & 255) <= (v66_rev & 255);
}

static inline void cpu_get_tb_cpu_state(CPUHexagonState *env, target_ulong *pc,
                                        target_ulong *cs_base, uint32_t *flags)
{
    uint32_t hex_flags = 0;
    *pc = env->gpr[HEX_REG_PC];
    *cs_base = 0;

#ifndef CONFIG_USER_ONLY
    target_ulong syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);

    bool pcycle_enabled = extract32(syscfg,
                                    reg_field_info[SYSCFG_PCYCLEEN].offset,
                                    reg_field_info[SYSCFG_PCYCLEEN].width);

    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, MMU_INDEX,
                           cpu_mmu_index(env, false));

    if (pcycle_enabled) {
        hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, PCYCLE_ENABLED, 1);
    }

    bool hvx_enabled = extract32(ssr, reg_field_info[SSR_XE].offset,
                                 reg_field_info[SSR_XE].width);
    hex_flags =
        FIELD_DP32(hex_flags, TB_FLAGS, HVX_COPROC_ENABLED, hvx_enabled);

    if (rev_implements_64b_hvx(env)) {
        int v2x = extract32(syscfg, reg_field_info[SYSCFG_V2X].offset,
                            reg_field_info[SYSCFG_V2X].width);
        hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, HVX_64B_MODE, !v2x);
    }

    bool pmu_enabled = extract32(syscfg,
                                 reg_field_info[SYSCFG_PM].offset,
                                 reg_field_info[SYSCFG_PM].width);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, PMU_ENABLED, pmu_enabled);
#else
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, PCYCLE_ENABLED, true);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, HVX_COPROC_ENABLED, true);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, MMU_INDEX, MMU_USER_IDX);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, HVX_64B_MODE,
                           rev_implements_64b_hvx(env));
#endif

    if (*pc == env->gpr[HEX_REG_SA0]) {
        hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, IS_TIGHT_LOOP, 1);
    }

#ifndef CONFIG_USER_ONLY
    bool ss_active = extract32(ssr,
                                 reg_field_info[SSR_SS].offset,
                                 reg_field_info[SSR_SS].width);
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, SS_ACTIVE, ss_active);

    bool ss_pending = env->ss_pending;
    hex_flags = FIELD_DP32(hex_flags, TB_FLAGS, SS_PENDING, ss_pending);
#endif

    *flags = hex_flags;
}

#ifndef CONFIG_USER_ONLY

/*
 * Fill @a ints with the interrupt numbers that are currently asserted.
 * @param list_size will be written with the count of interrupts found.
 */
void hexagon_find_int_threads(CPUHexagonState *env, uint32_t int_num,
                              HexagonCPU *threads[], size_t *list_size);
/*
 * @return true if the @a thread_env hardware thread is
 * not stopped.
 */
bool hexagon_thread_is_enabled(CPUHexagonState *thread_env);
/*
 * @return true if interrupt number @a int_num is disabled.
 */
bool hexagon_int_disabled(CPUHexagonState *global_env, uint32_t int_num);

/*
 * Disable interrupt number @a int_num for the @a thread_env hardware thread.
 */
void hexagon_disable_int(CPUHexagonState *global_env, uint32_t int_num);

/*
 * @return true if thread_env is busy with an interrupt or one is
 * queued.
 */
bool hexagon_thread_is_busy(CPUHexagonState *thread_env);

void raise_tlbmiss_exception(CPUState *cs, target_ulong VA, int slot,
                             MMUAccessType access_type);

void raise_perm_exception(CPUState *cs, target_ulong VA, int slot,
                          MMUAccessType access_type, int32_t excp);

/*
 * @return pointer to the lowest priority thread.
 * @a only_waiters if true, only consider threads in the WAIT state.
 */
HexagonCPU *hexagon_find_lowest_prio_thread(HexagonCPU *threads[],
                                            size_t list_size,
                                            uint32_t int_num,
                                            bool only_waiters,
                                            uint32_t *low_prio);

uint32_t hexagon_greg_read(CPUHexagonState *env, uint32_t reg);
#endif
typedef HexagonCPU ArchCPU;

void hexagon_translate_init(void);
void hexagon_cpu_soft_reset(CPUHexagonState *env);

extern void hexagon_cpu_do_interrupt(CPUState *cpu);
void register_trap_exception(CPUHexagonState *env, int type, int imm,
                             target_ulong PC);

#include "exec/cpu-all.h"

#endif /* HEXAGON_CPU_H */
