/*
 *  Copyright(c) 2019-2025 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEXAGON_CPU_H
#define HEXAGON_CPU_H

/* Forward declaration needed by some of the header files */
struct CPUArchState;
typedef struct CPUArchState CPUHexagonState;
typedef struct ProcessorState processor_t;

#include "fpu/softfloat-types.h"
#include "cpu-qom.h"
#include "exec/cpu-common.h"
#include "exec/target_long.h"
#include "exec/cpu-interrupt.h"
#include "exec/target_page.h"
#include "exec/cputlb.h"
#include "mmvec/mmvec.h"
#include "dma/dma.h"
#include "hmx_state.h"
#include "hw/core/registerfields.h"
#include "hw/hexagon/hexagon.h"
#include "hw/intc/l2vic.h"
#include "qemu/bitmap.h"
#include "migration/qemu-file.h"

#ifndef CONFIG_USER_ONLY
#include "reg_fields.h"
#define NUM_SREGS 106
#define NUM_GREGS 32
#define GREG_WRITES_MAX 2
#define SREG_WRITES_MAX 2
#endif

#include "hex_regs.h"

#define TARGET_LONG_BITS 32

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
#define THREADS_MAX 16
#define VECTOR_UNIT_MAX 8
#define PARANOID_VALUE (~0)
#define MAX_TLB_ENTRIES 1024
#define MAX_TLB_GUESS_ENTRIES (1024) /* power of 2 */

#define MAX_CORES 4 /* Cores per shared L2 */
#define MAX_CLUSTERS_DMA 2
#define MAX_THREADS_PER_CLUSTER 4
#define THREADS_PER_CORE \
    (MAX_CLUSTERS_DMA * MAX_THREADS_PER_CLUSTER) /* HW threads in a core */
#define THREADS_MAX_DMA (MAX_CORES * THREADS_PER_CORE)
#define DMA_MAX THREADS_MAX_DMA /* DMA: make this independent */

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
#define CPU_INTERRUPT_K0_UNLOCK CPU_INTERRUPT_TGT_INT_1
#define CPU_INTERRUPT_TLB_UNLOCK CPU_INTERRUPT_TGT_INT_2
#endif

#define CPU_RESOLVING_TYPE TYPE_HEXAGON_CPU

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
    uint32_t width;
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
    int QDSP6_DMAJTLB_SZ;
    int QDSP6_VX_IEEE_PRESENT;
    int QDSP6_VX_BF_EN;
    int udma_dmwait_latency;
    int udma_dmresume_latency;
    int udma_dmstart_latency;
    int udma_dmlink_latency;
    int udma_dmpoll_latency;
    int udma_dmpause_latency;
    int udma_ju_request_latency;
    int udma_startup_latency;
    int udma_prefetch_depth;
} arch_proc_opt_t;

typedef struct {
    uint64_t l2tcm_base;
    int testgen_mode;
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
    /* Flag to tell if we want to use aligned or unaligned mem address */
    uint8_t use_aligned_address:1;
    /* Flag to tell if you are using coproc range insns */
    uint8_t is_coproc_range:1;
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

#include "accel/tcg/cpu-ldst.h"

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
    HEX_LOCK_OWNER          = 2,
    HEX_LOCK_QUEUED        = 3
} hex_lock_state_t;

typedef struct PMUState {
    uint32_t vmstate_num_ctrs;
    uint32_t *g_ctrs_off;
    uint16_t *g_events;

    /* Internal counters */
    uint32_t num_packets;
    uint32_t hvx_packets;
} PMUState;

#endif


struct Einfo {
  uint8_t valid;
  uint8_t type;
  uint8_t cause;
  uint8_t bvs;
  uint8_t bv0;       /* valid for badva0 */
  uint8_t bv1;       /* valid for badva1 */
  uint32_t badva0;
  uint32_t badva1;
  uint32_t elr;
  uint16_t diag;
  uint16_t de_slotmask;
};
typedef struct Einfo hex_exception_info;
typedef struct Instruction Insn;
typedef unsigned systemstate_t;

typedef struct {
    uintptr_t pc;
    bool set;
    /* Where was it set: */
    const char *filename;
    int line;
} hex_memop_pc;

typedef struct CPUArchState {
    target_ulong gpr[TOTAL_PER_THREAD_REGS];
    target_ulong pred[NUM_PREGS];
    target_ulong cause_code;

    /* For comparing with LLDB on target - see adjust_stack_ptrs function */
    target_ulong last_pc_dumped;
    target_ulong stack_start;

    uint8_t slot_cancelled;

#ifndef CONFIG_USER_ONLY
    /* some system registers are per thread and some are global */
    target_ulong t_sreg[NUM_SREGS];
    target_ulong *g_gcycle;

    target_ulong greg[NUM_GREGS];
    target_ulong wait_next_pc;
#endif
    target_ulong new_value_usr;

    MemLog mem_log_stores[STORES_MAX];

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
    target_ulong vstore_pending[VSTORES_MAX];
    bool gather_issued;
    bool vtcm_pending;
    VTCMStoreLog vtcm_log;
    mem_access_info_t mem_access[SLOTS_MAX];

    int32_t status;
    uint8_t bq_on;

    uint32_t timing_on;

    uint64_t pktid;
    processor_t *processor_ptr;
    uint32_t threadId;

    uint64_t t_cycle_count;
    uint32_t exec_ctr_pkt;
    uint32_t exec_ctr_insn;
    uint32_t exec_ctr_hvx;
    uint32_t exec_ctr_hmx;
    uint32_t exec_ctr_tb;
    /* Used by cpu_{ld,st}* calls down in TCG code. Set by top level helpers. */
    hex_memop_pc memop_pc;
#ifndef CONFIG_USER_ONLY
    int32_t slot;                    /* Needed for exception generation */
    hex_exception_info einfo;
    systemstate_t systemstate;
    target_ulong imprecise_exception;
    hex_lock_state_t tlb_lock_state; /* different threads modify */
    hex_lock_state_t k0_lock_state; /* different threads modify */
    int32_t k0_lock_count;
    int32_t tlb_lock_count;
    uint16_t nmi_threads;
    uint32_t last_cpu;
    GList **g_dir_list;
    uint32_t exe_arch;
    gchar *lib_search_dir;
    PMUState pmu;
    bool ss_pending;
#endif
    target_ulong next_PC;
    qfrnd_mode_enum_t qfrnd_mode;
    int qfcoproc_mode;
    int t_veclogsize;


    /*
     * Per-CPU HMX scratch buffers for TCG GVec weight operations (128B each).
     * 16-byte aligned for tcg_gen_gvec_* requirements.
     * These must be at fixed env offsets for GVec standard ops.
     */
    uint32_t hmx_wei_raw[HMX_OUTPUT_CHANNELS] QEMU_ALIGNED(16);
    int32_t  hmx_wei_expanded[HMX_OUTPUT_CHANNELS] QEMU_ALIGNED(16);
    int32_t  hmx_mac_tmp[HMX_OUTPUT_CHANNELS] QEMU_ALIGNED(16);

    /* Pointer to shared HMX state (for TCG access via env) */
    HmxState *hmx_state;
} CPUHexagonState;
#define mmvecx_t CPUHexagonState

typedef struct HexagonCPUClass {
    CPUClass parent_class;

    DeviceRealize parent_realize;
    ResettablePhases parent_phases;

    const HexagonCPUDef *hex_def;
} HexagonCPUClass;

struct ArchCPU {
    CPUState parent_obj;

    CPUHexagonState env;

    HmxState *hmx;  /* HMX state (self-alloc or from device) */

#if !defined(CONFIG_USER_ONLY)
    bool count_gcycle_xt;
    bool sched_limit;
    bool cacheop_exceptions;
    gchar *usefs;
    gchar *coproc_path;
    gchar *cmdline;
    L2VicInterface *l2vic;
    hwaddr vtcm_base_addr;
    uint32_t vtcm_size_kb;
    uint32_t num_coproc_instance;
    uint32_t subsystem_id;
#endif
    bool hvx_bfloat;
    bool coproc2_bfloat;
    bool coproc2_present;
    bool lldb_compat;
    target_ulong lldb_stack_adjust;
    bool paranoid_commit_state;
    bool short_circuit;
    uint32_t cluster_thread_count;
    gchar *dump_json_file;
    uint32_t l2line_size;
    uint32_t vmstate_num_g_sreg;
    uint32_t vmstate_num_g_gcycle;
    uint32_t hvx_contexts;
#ifndef CONFIG_USER_ONLY
    struct HexagonGlobalRegState *globalregs;
    struct HexagonTLBState *tlb;
#endif
};

#ifndef CONFIG_USER_ONLY
#include "cpu_helper.h"
uint64_t hexagon_get_sys_pcycle_count(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_low(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_high(CPUHexagonState *env);
#endif

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

#include "cpu_memop_pc.h"

G_NORETURN void hexagon_raise_exception_err(CPUHexagonState *env,
                                            uint32_t exception,
                                            uintptr_t pc);

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
uint32_t hexagon_sreg_read(CPUHexagonState *env, uint32_t reg);
void hexagon_gdb_sreg_write(CPUHexagonState *env, uint32_t reg, uint32_t val);
#endif
uint32_t hexagon_creg_read_debug(CPUHexagonState *env, uint32_t reg);
typedef HexagonCPU ArchCPU;

HexagonVersion hexagon_version(HexagonCPU *hex_cpu);
static inline HexagonVersion hexagon_version_env(CPUHexagonState *env)
{
    HexagonCPU *hex_cpu = container_of(env, HexagonCPU, env);
    return hexagon_version(hex_cpu);
}

static inline bool rev_implements_64b_hvx(CPUHexagonState *env)
{
    return hexagon_version_env(env) <= HEX_VER_V66;
}

void hexagon_translate_init(void);
void hexagon_cpu_soft_reset(CPUHexagonState *env);
void hexagon_dump_json(CPUHexagonState *env);

void hexagon_translate_code(CPUState *cs, TranslationBlock *tb,
                            int *max_insns, vaddr pc, void *host_pc);

#endif /* HEXAGON_CPU_H */
