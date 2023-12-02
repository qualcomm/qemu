/*
 * Copyright (c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 */

/*
 * This file defines the API that is exported by the architecture implementation
 * to the outside world
 *
 *
 *
 */


#ifndef _EXTERNAL_API_H_
#define _EXTERNAL_API_H_

/* Section 0: types used in this file */

#ifdef ARCH_SIDE
struct ProcessorState;
typedef struct ProcessorState processor_t;
typedef void system_t;
#else
//typedef void processor_t;
typedef struct SystemState system_t;
#endif

/********************************************************************/
/*                Constants used for communication                  */
/********************************************************************/

/* memory type, may want to add more later */
enum {
	IFETCH,						/* Instruction Fetches      */
	DREAD,						/* Data Reads               */
	DWRITE,						/* Data Writes              */
	DCASTOUT,					/* DCache Cast Outs         */
	DREADLOCK,					/* Read from UC mem_locked  */
	DWRITELOCK,					/* Write from UC mem_locked */
	SYNCH,						/* Bus synch from syncht    */
	BARRIER,					/* Barrier for memory order */
	DPREFETCH,					/* Data Prefetch            */
	IPREFETCH,					/* Instruction Prefetch     */
    VSCATTER,					/* Vector Scatter           */
    VGATHER,					/* Vector Gather            */
	LAST_MEMORY_TYPE
};

enum {
	DREAD_UNCACHED = LAST_MEMORY_TYPE,
	DREAD_CACHED,
	LAST_MEMORY_TYPE_EXT1
};

enum {
	DCASTOUT_CLEAN = LAST_MEMORY_TYPE_EXT1,
	DCASTOUT_DIRTY,
	LAST_MEMORY_TYPE_EXT2
};

typedef enum {
  ITLB,
  DTLB,
  COUNT_UTLB_TYPES
} utlb_type_e;

typedef enum {
  SG_WARN_RAW_POISON=0,
  SG_WARN_WAR_POISON=1,
  SG_WARN_WAW_POISON=2,
  SG_SYNC=3,
  SG_DESYNC=4,
  SG_READ_POISON=5,
  SG_WRITE_POISON=6,
  SG_RDWR_POISON=7,
  SG_DEPOISON=8,
  COUNT_SG_EVENTS=9
} sg_event_type_e;

/* Callbacks to CU */
enum cu_iq_event_type {
    CU_IQ_PKT_PUSH,
    CU_IQ_PKT_POP,
    CU_IQ_PKT_FLUSH
};
typedef enum cu_iq_event_type cu_iq_event_e;

enum cu_iq_state_type {
    UCU_IQ_EMPTY,
    UCU_IQ_FULL,
    UCU_IQ_READY
};
typedef enum cu_iq_state_type cu_iq_state_e;

struct cu_iq_callback_info_struct {
    int tnum;

    cu_iq_event_e event;
    cu_iq_state_e state;

    vaddr_t pc_va;
    paddr_t pc_pa;

    size8u_t pktid;
};
typedef struct cu_iq_callback_info_struct cu_iq_callback_info_t;

typedef struct {
  	int tnum;
  	vaddr_t pc;
  	paddr_t pa;
  	sg_event_type_e event;
} scaga_callback_info_t;

typedef void (*uarch_cu_iq_callback_t)(system_t *sys, processor_t *proc, cu_iq_callback_info_t *info);

/* End callbacks for CU */

/* For logging of trap1 instruction for verif debug and uarch trace print */
typedef enum trap1_info_log {
	TRAP1_NONE,
	TRAP1,
	TRAP1_VIRTINSN_RTE,
	TRAP1_VIRTINSN_SETIE,
	TRAP1_VIRTINSN_GETIE,
	TRAP1_VIRTINSN_SPSWAP
} trap1_info_log_t;




/* Callbacks for sending a string to uarchtrace */
struct string_info_struct {
    processor_t *proc;
    char *mystring;
};
typedef struct string_info_struct string_info_t;
typedef void (*uarch_string_callback_t)(system_t *sys, processor_t *proc, string_info_t *info);

/* Callbacks for credit system */
#define MAX_CREDIT_PRIVATE_BUCKETS 20
struct uarch_credit_manager_state_struct {
    char *credit_pool_name;
    int shared_total;
    int shared_available;

    int private_buckets;
    int private_total[MAX_CREDIT_PRIVATE_BUCKETS];
    int private_available[MAX_CREDIT_PRIVATE_BUCKETS];
};
typedef struct uarch_credit_manager_state_struct uarch_credit_manager_state_t;

enum uarch_credit_events {
    CREDIT_ALLOCATED,
    CREDIT_RELEASED,
    CREDIT_DENIED
};
typedef enum uarch_credit_events uarch_credit_events_e;

enum uarch_credit_requesters {
    UARCH_CREDIT_REQUESTER_DU,
    UARCH_CREDIT_REQUESTER_IU,
    UARCH_CREDIT_REQUESTER_L2FETCH,
    UARCH_CREDIT_REQUESTER_EVICT,
    UARCH_CREDIT_REQUESTER_SLAVE,
    UARCH_CREDIT_REQUESTER_PIPELINE,
    UARCH_CREDIT_REQUESTER_DU_STORE_RELEASE,
    UARCH_CREDIT_LAST_REQUESTER
};
typedef enum uarch_credit_requesters uarch_credit_requester_e;

enum uarch_credit_buckets {
    UARCH_CREDIT_BUCKET_SHARED,
    UARCH_CREDIT_BUCKET_PRIVATE,
    UARCH_LAST_CREDIT_BUCKET
};
typedef enum uarch_credit_buckets uarch_credit_buckets_e;

struct uarch_credit_callback_info_struct {
    /* Credit pool information */
    char *credit_pool_name;

    /* Requester information */
    int tnum;
    vaddr_t pc_va;
    vaddr_t vaddr;
    paddr_t paddr;
    uarch_credit_requester_e requester;
	size8u_t  credit_id;
  	size8u_t held_for;

    /* Event information */
    uarch_credit_events_e event;

    /* Final state of the credit pool */
    uarch_credit_buckets_e bucket;
    uarch_credit_manager_state_t cm_state;
};
typedef struct uarch_credit_callback_info_struct uarch_credit_callback_info_t;

typedef void (*uarch_credit_callback_t)(system_t *sys, processor_t *proc, uarch_credit_callback_info_t *credit_callback_info);


/* End credit callback */

enum icache_access_type {
    ICACHEACCESS_FETCH,    /* IU instruction fetch type */
    ICACHEACCESS_IPREFETCH,    /* IU instruction prefetch type */
    LAST_ICACHEACCESS_TYPE
};

/* L2 cache access/hit/replace callback types */
enum l2cache_access_type {
    L2CACHEACCESS_DEMAND_SCALAR,   /* Demand read or write - scalar */
    L2CACHEACCESS_DEMAND_VECTOR,   /* Demand read or write - vector*/
    L2CACHEACCESS_FETCH,    /* IU instruction fetch type */
    L2CACHEACCESS_IPREFETCH,    /* IU instruction prefetch type */
    L2CACHEACCESS_L2FETCH_L2F,  /* L2 prefetch type, l2f */
    L2CACHEACCESS_L2FETCH_IU,  /* L2 prefetch type, iprefetch (through L2F engine) */
    L2CACHEACCESS_DCFETCH,  /* DCFETCH type */
    L2CACHEACCESS_DCMUMPREF, /* DC miss-under-miss prefetch */
    L2CACHEACCESS_DCHWPREF, /* DC other hw prefetch */
	L2CACHEACCESS_VFETCH,  /* VFETCH type */
    LAST_L2CACHEACCESS_TYPE
};

/* TCM access types */
enum tcm_access_type {
    TCMACCESS_FETCH, /* An IU type */
    TCMACCESS_READ,  /* A DU read request */
    TCMACCESS_WRITE, /* A DU write request */
    LAST_TCMACCESS_TYPE
};

/* Dcache access/hit/replace callback types */
enum dcache_access_type {
    DCACHEACCESS_DEMAND_LOAD,   /* Demand read or write */
    DCACHEACCESS_DEMAND_STORE,  /* IU instruction fetch type */
    DCACHEACCESS_DCZERO,        /* DCZERO type */
    DCACHEACCESS_INVALIDATE,    /* Invalidate type */
    DCACHEACCESS_DCFETCH,       /* DCFETCH type */
    DCACHEACCESS_DCMUMPREF,     /* Miss-under-miss prefetch type */
    DCACHEACCESS_DCHWPREF,      /* Other hw prefetch type */
    LAST_DCACHEACCESS_TYPE
};

/// The type of an architectural register.
typedef enum {
	arch_reg_kind_none = '\0',
	arch_reg_kind_C = 'C',
	arch_reg_kind_G = 'G',
	arch_reg_kind_M = 'M',
	arch_reg_kind_N = 'N',
	arch_reg_kind_O = 'O',
	arch_reg_kind_P = 'P',
	arch_reg_kind_Q = 'Q',
	arch_reg_kind_R = 'R',
	arch_reg_kind_S = 'S',
	arch_reg_kind_V = 'V',
	arch_reg_kind_Z = 'Z'
} reg_type_t;

extern char *memtypenames[];
extern char *memtypenames_long[];
//extern const char *l2bus_status_names[];

/* change-of-flow types, may want to add more later */
typedef enum {
	COF_TYPE_JUMPR,				/* Jump Reg                 */
	COF_TYPE_JUMP,				/* Jump                     */
	COF_TYPE_JUMPNEW,			/* Jump dot new             */
	COF_TYPE_CALL,				/* Call                     */
	COF_TYPE_CALLR,				/* Call Register            */
	COF_TYPE_LOOPEND0,			/* Loop 0 loopend           */
	COF_TYPE_LOOPEND1,			/* Loop 1 loopend           */
	COF_TYPE_RTE,				/* Return from Exception    */
	COF_TYPE_TRAP,				/* Trap */
	COF_TYPE_LOOPFALLTHROUGH,	/* COF_TYPE_LOOPFALLTHROUGH */
} coftype_t;

typedef enum {
  	RAS_PUSH =0,
    RAS_POP = 1,
  	URAS_PUSH =2,
    URAS_POP =3,
    URAS_MISPRED =4,
    URAS_REWIND =5,
    URAS_PRED_OK =6,
} ras_event_t;

typedef enum {
    L2FIFO_PUSH,
    L2FIFO_POP,
    L2FIFO_FREED,
    L2FIFO_LAST_EVENT
} l2fifo_trace_event_t;

typedef enum {
    VFIFO_CREDIT_ACQUIRE,
    VFIFO_CREDIT_RETURN,
    VFIFO_PUSH,
    VFIFO_POP,
    VFIFO_LAST_EVENT
} vfifo_trace_event_t;

typedef enum {
    MEMCPY_COMMAND_RECEIVED=0,
	MEMCPY_ACTIVATED=1,
    MEMCPY_BUS_REQUEST=2,
	MEMCPY_BUS_RESPONSE=3,
	MEMCPY_VTCM_WRITE=4,
	MEMCPY_DONE=5,
    MEMCPY_LAST_EVENT=6
} memcpy_trace_event_t;


typedef enum {
    VTCM_SB_INSERT=0,
    VTCM_SB_DEALLOCATE=1,
    VTCM_SB_FULL=2,
    VTCM_LD_RAW=3,
    VTCM_SCALAR_HAZARD=4,
    VTCM_ST_ST_BANK_CONFLICT=5,
    VTCM_LD_ST_BANK_CONFLICT=6,
    VTCM_LD_LD_BANK_CONFLICT=7,
    VTCM_SCALAR_LD_ST_BANK_CONFLICT=8,
    VTCM_SCALAR_LD_HIT_ACCESS=9,
    VTCM_SCALAR_LD_MISS_ACCESS=10,
    VTCM_SCALAR_ST_ACCESS=11,
    VTCM_LD_BUFFER_FULL=12,
    VTCM_LD_BUFFER_POP=13,
    VTCM_LD_BUFFER_PUSH=14,
    VTCM_SFIFO_PUSH=15,
    VTCM_SFIFO_POP=16,
	VTCM_DMA_COMMAND=17,
	VTCM_DMA_WRITE=18,
	VTCM_DMA_ST_CONFLICT=19,
    VTCM_LAST_EVENT=20
} vtcm_trace_event_t;

typedef enum {
	L2SB_TRACE_INSTALL=0,
	L2SB_TRACE_UNINSTALL=1,
	L2SB_TRACE_LINK_HEAD=2,
	L2SB_TRACE_LINK_TAIL=3,
	L2SB_TRACE_UNLINK=4,
	L2SB_TRACE_STEP_GENERIC=5,
	L2SB_TRACE_STEP_LOAD=6,
	L2SB_TRACE_STEP_LOAD_HIT=7,
	L2SB_TRACE_STEP_LOAD_MISS=8,
	L2SB_TRACE_STEP_POPULATE1=9,
	L2SB_TRACE_STEP_POPULATE2=10,
	L2SB_TRACE_STEP_STOREINIT=11,
	L2SB_TRACE_STEP_STORELOOKUP=12,
	L2SB_TRACE_STEP_STORECOPYBACK=13,
	L2SB_TRACE_STEP_EVICT=14,
	L2SB_TRACE_STEP_VSTOREINIT1=15,
	L2SB_TRACE_STEP_VSTOREINIT2=16,
	L2SB_TRACE_STEP_CLEANA=17,
	L2SB_TRACE_STEP_CLEANI1=18,
	L2SB_TRACE_STEP_CLEANI2=19,
	L2SB_TRACE_STEP_RETIRE=20,
	L2SB_TRACE_FORWARD=21,
	L2SB_TRACE_COALESCE=22,
	L2SB_TRACE_CVI_TAG=23,
	L2SB_TRACE_CVI_DATA=24,
	L2SB_TRACE_CVI_DATA_ACK=25,
	L2SB_TRACE_FAIL_STORE=26,
	L2SB_TRACE_FAIL_EVICT=27,
	L2SB_TRACE_FAIL_SB=28,
	L2SB_TRACE_FAIL_DATAB=29,
	L2SB_TRACE_FAIL_TAGB=30,
	L2SB_TRACE_FAIL_OTHER=31,
	L2SB_TRACE_STEP_STORELOOKUP_PARTIAL=32,
	L2SB_TRACE_NUM=33,
} l2sb_trace_event_t;

typedef enum {
	L2PT_TRACE_DATABANK=0,
	L2PT_TRACE_TAGBANK=1,
	L2PT_TRACE_RW_CHANNEL=2,
	L2PT_TRACE_R_CHANNEL=3,
	L2PT_TRACE_W_CHANNEL=4,
	L2PT_TRACE_NUM=5,
} l2pt_trace_event_t;


typedef enum {
    FE_TRACE_PKR_L2_RETURN =0,
    FE_TRACE_PKR_CU_REDIRECT=1,
    FE_TRACE_PKR_NEXT_FETCH=2,
    FE_TRACE_PKR_IPREFETCH=3,
    FE_TRACE_PKR_NONE=4,
} fe_trace_pkr_pick_type_t;

enum {
	CUSTOM,
	VERSION_V66H,
	VERSION_V66Z,
	VERSION_V68N
};

/* for set thread state API*/
typedef enum {
	SET_THREAD_STATE_WAIT,
	SET_THREAD_STATE_EXEC,
	SET_THREAD_STATE_N
} set_thread_state_t;

typedef enum {
    SET_THREAD_STATE_RETURN_SUCCESS,
    SET_THREAD_STATE_RETURN_NOT_IN_TRACE_MODE,
    SET_THREAD_STATE_RETURN_NOT_PKT_EXECUTED,
    SET_THREAD_STATE_RETURN_NOT_IN_WAIT_MODE,
    SET_THREAD_STATE_RETURN_SET_STATE_NOT_FOUND,
    SET_THREAD_STATE_RETURN_UNKNOWN_ERROR,
} set_thread_state_return_t;

typedef enum {
    SCHEDULE_TRACE_RETURN_SUCCESS,
    SCHEDULE_TRACE_NOT_IN_TRACE_MODE,
    SCHEDULE_TRACE_FAIL
} schedule_trace_return_t;

/* User-DMA instruction (command) identifier. */
typedef enum {
  DMA_CMD_START = 0,            // Y6_dmstart
  DMA_CMD_LINK = 1,             // Y6_dmlink
  DMA_CMD_POLL = 2,             // Y6_dmpoll
  DMA_CMD_WAIT = 3,             // Y6_dmwait
  DMA_CMD_SYNCHT = 4,           // Y6_dmsyncht
  DMA_CMD_WAITDESCRIPTOR = 5,   // Y6_dmwaitdescriptor
  DMA_CMD_CFGRD = 6,            // Y6_dmcfgrd
  DMA_CMD_CFGWR = 7,            // Y6_dmcfgwr
  DMA_CMD_PAUSE = 8,            // Y6_dmpause
  DMA_CMD_RESUME = 9,           // Y6_dmresume
  DMA_CMD_TLBSYNCH = 10,        // Y6_dmtlbsynch
  DMA_CMD_UND = 11              // DO NOT USE THIS. JUST TERMINATOR.
} dma_cmd_t;

typedef enum {
	DMA_DESC_STATE_START=0,
	DMA_DESC_STATE_DONE=1,
	DMA_DESC_STATE_PAUSE=2,
	DMA_DESC_STATE_EXCEPT=3,
	DMA_DESC_STATE_NUM=4
} dma_callback_state_t;

/********************************************************************/
/*                These are ISA defined constants                   */
/********************************************************************/

#ifdef FIXME
/* Event types */
typedef enum {
#define DEF_EVENT_TYPE(NAME,NUM) \
	NAME = NUM,
#include "event_types.odef"
#undef DEF_EVENT_TYPE
} event_types_t;

/* Precise exception cause codes */
typedef enum {
#define DEF_EXCEPTION_CAUSE_TRAP(CAUSE_CODE,EVENT_TYPE,DESCR,EVENT_NUMBER,NOTES)
#define DEF_EXCEPTION_CAUSE_RESERVED(EVENT_NUMBER,EVENT_TYPE)
#define DEF_EXCEPTION_CAUSE(NAME,CAUSE_CODE,EVENT_TYPE,DESCR,EVENT_NUMBER,NOTES) \
	NAME = CAUSE_CODE,
#include "exception_causes.odef"
#undef DEF_EXCEPTION_CAUSE
#undef DEF_EXCEPTION_CAUSE_RESERVED
#undef DEF_EXCEPTION_CAUSE_TRAP
} exception_cause_t;

#define UARCH_MAX_OPCODES_PER_PACKET 16



/* L2 TCM Memory MAP offsets */
#define L2TCM_ETM_BASE	(64*1024)
#define L2TCM_L2CFG_BASE	(128*1024)
#define L2TCM_STREAMER_BASE	(256*1024)
#define L2TCM_CLADE_BASE	(320*1024)
#define L2TCM_CLADE2_BASE	(512*1024)  // FIXME?
#define L2TCM_FASTL2VIC_BASE	(384*1024)
#define L2TCM_ECCREG_BASE	(448*1024)

/* ECC Memory Mapped register offsets */
#define ECC_IUDATA_REGS_START (0)
#define ECC_DUDATA_REGS_START (4*36)
#define ECC_L2DATA_REGS_START (4*72)
#define ECC_VTCM_REGS_START   (4*108)
#define ECC_L2TAG_REGS_START  (4*144)

typedef enum {
  EXT_NONE = 0,
  EXT_HVX = 1,
} ext_type_e;


struct uarch_status_info_struct {
    processor_t *proc;
    int tnum;
	long long int pcycle;
    vaddr_t pc;
    size8u_t pktid;
    int status;
    int pmu_stall_type;

    struct uarch_stall_details {
        int pmu_stall_type;
        size1u_t interlock_details_valid;

        struct uarch_interlock_details {
            size2u_t producer_opcode;
            int producer_iclass;
            int producer_tclass;

            size2u_t consumer_opcode;
            int consumer_iclass;
            int consumer_tclass;

            int interlock_reg;
        } interlock_details;
    } stall_details;

    struct uarch_pkt_details {
	vaddr_t pc_va;
	paddr_t pc_pa;
	size1u_t num_insns;
	size2u_t opcodes[UARCH_MAX_OPCODES_PER_PACKET];
    size1u_t opcode_slot[UARCH_MAX_OPCODES_PER_PACKET];
    } pkt_details;
};

typedef struct uarch_status_info_struct uarch_status_info_t;

struct pmu_update_callback_info_struct {
    int tnum;
    int packet_id;
    vaddr_t pc;
    int pmu_event_num;
    size8u_t old_value;
    size8u_t new_value;
};
typedef struct pmu_update_callback_info_struct pmu_update_callback_info_t;

enum sbuf_push_cause {
    SBUF_NVSTORE = 0,
    SBUF_MEMOP = 1,
    SBUF_STORE = 2,
    SBUF_NOTAPP = 3
};
typedef enum sbuf_push_cause sbuf_push_cause_t;

enum sbuf_event {
    SBUF_PUSH =0,
    SBUF_PUSH_FAIL =1,
    SBUF_DRAIN_NORMAL =2,
    SBUF_DRAIN_FORCED =3 ,
    SBUF_HIT_OVERLAP =4 ,
    SBUF_HIT_VAHI_MISMATCH= 5,
    SBUF_LAST_EVENT=6
};
typedef enum sbuf_event sbuf_event_t;

struct uarch_sbuf_update_callback_info_struct {
    int usb_num;
    int tnum;
    vaddr_t pc;
    vaddr_t va;
    paddr_t pa;
    vaddr_t hit_pc;
  	int hit_tnum;
    enum sbuf_event event;
    enum sbuf_push_cause push_cause;
};
typedef struct uarch_sbuf_update_callback_info_struct uarch_sbuf_update_callback_info_t;

typedef struct {
  int tnum;
  vaddr_t pc;
} cu_dispatch_callback_info_t;

typedef enum {
  IU =0,
  DU =1
} l1_type_e;

typedef enum {
  UTLBHIT =0,
  UTLBMISS =1,
} utlb_event_e;

typedef struct {
  int tnum;
  l1_type_e type;
  utlb_event_e event;
  vaddr_t pc;
  vaddr_t va;
  paddr_t pa;
  int index;
  int way;
} uarch_utlb_update_callback_info_t;

typedef enum fill_type_t {
  DEMAND,
  PREFETCH,
  FETCH,
  I_PREFETCH
} fill_type_t;

typedef struct {
  	int tnum;
    vaddr_t pc;
    vaddr_t va;
    paddr_t pa;
  	int index;
  	fill_type_t fill_type;
} uarch_fill_update_callback_info_t;

enum branch_event {
    BRANCH_COMMIT = 0,
    BRANCH_PREDICT = 1
};
typedef enum branch_event branch_event_enum_t;

enum branch_result {
    BRANCH_NOT_TAKEN = 0,
    BRANCH_TAKEN = 1
};
typedef enum branch_result branch_result_enum_t;

enum branch_type {
    DIRECT_UNCONDITIONAL = 0,
    INDIRECT_JUMP =1,
    DOT_NEW_JUMP =2 ,
    DOT_OLD_JUMP =3 ,
    NEW_VALUE_JUMP =4,
    DIRECT_CALL =5,
    INDIRECT_CALL =6,
    RETURN = 7,
    ENDLOOP =8
    /* EXCEPTION, */
    /* INTERRUPT, */
    /* NULL // Per Ankit */
};
typedef enum branch_type branch_type_enum_t;

struct uarch_branch_update_callback_info_struct {
    size8u_t pcycles;
    vaddr_t pc;
    vaddr_t target;
    vaddr_t jump_va;
    int tnum;
    branch_event_enum_t branch_event;
    branch_result_enum_t branch_result;
    branch_type_enum_t branch_type;
    size2u_t opcode;
};
typedef struct uarch_branch_update_callback_info_struct uarch_branch_update_callback_info_t;

typedef struct {
	const unsigned char *value;
	vaddr_t pc;
	int tnum;
	reg_type_t reg_type;
	unsigned int reg_width;
	unsigned int rnum;
	size2u_t opcode;
	int iswrite : 1;
	int isglobal : 1;
} reg_access_callback_info_t;

typedef struct {
  	unsigned int tnum;
  	ras_event_t event;
  	vaddr_t pc;
  	unsigned int pos;
    int occupancy;
    vaddr_t jump_addr;
    vaddr_t target_addr;
    vaddr_t return_addr;
    vaddr_t old_return_addr;

} uarch_ras_update_callback_info_t ;

typedef struct {
  	l1_type_e target;
  	size1u_t isodd;
  	size1u_t ispremature;
	paddr_t pa;
  	unsigned int available_private;
  	unsigned int available_shared;
	unsigned int tnum;
  	vaddr_t pc;
	vaddr_t va;
  	size8u_t held_for;
	l2fifo_trace_event_t event;
} l2fifo_callback_info_t;


typedef struct {
  	ext_type_e target;
	paddr_t pa;
    vaddr_t pc;

  	size1u_t occupancy;
	size1u_t context_num;
    size1u_t type;

    vaddr_t conflict_pc;
    paddr_t conflict_pa;
    size1u_t conflict_context;
  	size8u_t held_for;
	size2u_t all_events;
	vtcm_trace_event_t event;
} vtcm_callback_info_t;

typedef struct {
	paddr_t src_pa;
	paddr_t dst_pa;
	size4u_t size;
	size4u_t transactions;
	memcpy_trace_event_t event;
} memcpy_callback_info_t;

typedef struct {
	size4u_t dmano;
	vaddr_t desc_va;
	paddr_t desc_pa;
	size4u_t state;
	size4u_t type;
	vaddr_t src_va;
	vaddr_t dst_va;
	paddr_t src_pa;
	paddr_t dst_pa;
	size4u_t len;
	size4u_t src_offset;
	size4u_t dst_offset;
	size4u_t hlen;
	size4u_t vlen;
} dma_descriptor_callback_info_t;



typedef enum iu_l1s_status {
	IU_L1S_IFETCH=0,
	IU_L1S_PREFETCH=1,
	IU_L1S_ACCESS=2,
	IU_L1S_ARB_ACCESS=3,
	IU_L1S_IFETCH_RETURN=4,
	IU_L1S_PREFETCH_RETURN=5,
} iu_l1s_status_t;

typedef struct {
	paddr_t pa;
    vaddr_t pc;
	int tnum;
	iu_l1s_status_t status;
	size8u_t qos_cycles;
} iu_l1s_callback_info_t;

typedef struct {
  	ext_type_e target;
	paddr_t pa;
  	unsigned int available_private;
  	unsigned int available_shared;
	unsigned int tnum;
    unsigned int success;
  	unsigned int vfid;
  	vaddr_t pc;
  	size8u_t held_for;
	vfifo_trace_event_t event;
} vfifo_callback_info_t;


/// TODO: Move this to one of the "mmvec" directories


typedef enum hvx_resource {
	HVX_RESOURCE_LOAD=0,
	HVX_RESOURCE_STORE=1,
	HVX_RESOURCE_PERM=2,
	HVX_RESOURCE_SHIFT=3,
	HVX_RESOURCE_MPY0=4,
	HVX_RESOURCE_MPY1=5,
	HVX_RESOURCE_ZR=6,
	HVX_RESOURCE_ZW=7
} hvx_resource_t;

struct vmem_callback_info_struct {
	int num_vmem;
	int context_num;
	vaddr_t pc_va;
  paddr_t pc_pa;
	vaddr_t vaddr[2];
	paddr_t paddr[2];
	int width[2];
	int type[2];
};
typedef struct vmem_callback_info_struct vmem_callback_info_t;


enum l2fetch_callback_event {
    arch_l2fetch_command,
    arch_l2fetch_overwrite,
    arch_l2fetch_terminate
};
typedef enum l2fetch_callback_event l2fetch_callback_event_e;
struct l2fetch_callback_command_info_struct {
    int tnum;
    vaddr_t pc_va;
    vaddr_t vaddr;
    paddr_t paddr;
    int stride;
    int width;
    int height;
};
typedef struct l2fetch_callback_command_info_struct l2fetch_callback_command_info_t;
struct l2fetch_callback_terminate_info_struct {
    int tnum;
    paddr_t paddr;
};
typedef struct l2fetch_callback_terminate_info_struct l2fetch_callback_terminate_info_t;
struct l2fetch_callback_overwrite_info_struct {
    int tnum;
    vaddr_t oldpc;
    vaddr_t oldpa;
    vaddr_t newpc;
    vaddr_t newpa;
};
typedef struct l2fetch_callback_overwrite_info_struct l2fetch_callback_overwrite_info_t;

struct l2fetch_callback_info_struct {
    system_t *sys;
    processor_t *proc;
    l2fetch_callback_event_e event;
    union {
        l2fetch_callback_command_info_t command_info; // When event == l2fetch_command
        l2fetch_callback_overwrite_info_t overwrite_info; // When event == l2fetch_overwrite
        l2fetch_callback_terminate_info_t terminate_info; // When event == l2fetch_terminate
    };
};
typedef struct l2fetch_callback_info_struct l2fetch_callback_info_t;

enum fe_callback_event {
    arch_fe_pick,
    arch_fe_schedule,
    arch_fe_pktq_push,
    arch_fe_btb,
};
typedef enum fe_callback_event fe_callback_event_e;

struct fe_picker_info_struct {
    fe_trace_pkr_pick_type_t pick_type;
    size8u_t when;
};
typedef struct fe_picker_info_struct fe_picker_info_t;

struct fe_btb_info_struct {
    vaddr_t dest_pc;
    char    is_hit;
};
typedef struct fe_btb_info_struct fe_btb_info_t;

struct fe_pktq_info_struct {
	size8u_t when;  /* In how many cycles, a PKTQ event will occur. */
	size8u_t ready; /* At which cycle, a PKTQ entry will be ready to pop. */
	size8u_t pktid; /* Packet ID of an event. */
};

typedef struct fe_pktq_info_struct fe_pktq_info_t;

struct fe_callback_info_struct {
	system_t *sys;
	processor_t *proc;
	fe_callback_event_e event;
	int tnum;
	vaddr_t pc;
	union {
		fe_picker_info_t picker_info; /* Used when event == arch_fe_pick */
		fe_pktq_info_t   pktq_info;   /* Used when event == arch_pktq_push */
		fe_btb_info_t    btb_info;    /* Used when event == arch_pktq_push */
	};
};

typedef enum {
    I_BRKPT,
    I_LAST_ENTER_INSTRUCTION_TYPE
} enter_instruction_type_e;

typedef struct fe_callback_info_struct fe_callback_info_t;

typedef void (*memory_callback_t) (system_t * sys, processor_t * proc,
								   int threadno, size4u_t va, paddr_t pa,
								   size4u_t width, int type,
								   size8u_t data);
typedef void (*dma_callback_t) (system_t * sys, processor_t * proc,
								   dma_descriptor_callback_info_t * info);
typedef void (*system_callback_t) (system_t * sys, processor_t * proc,
								   int threadno, int info);
typedef void (*exception_callback_t) (system_t * sys, processor_t * proc,
									  int threadno, size4u_t event_type,
									  size4u_t cause, size4u_t badva,
									  size4u_t elr);
typedef void (*cof_callback_t) (system_t * sys, processor_t * proc,
								int threadno, size4u_t to_va,
								coftype_t type);
typedef void (*tcm_access_callback_t) (system_t * sys, processor_t * proc,
									   int threadno, size4u_t pc,
									   size4u_t vaddr, paddr_t paddr,
                                       int width,  int type);
typedef void (*stall_callback_t) (system_t * sys, processor_t * proc,
								  int threadno, int stall_id,
								  size4u_t * data);
typedef void (*uarch_utlb_update_callback_t) (system_t * sys, processor_t * proc, void *ptr);
typedef void (*cache_callback_t) (system_t * sys, processor_t * proc,
								  int threadno, size4u_t pc, size4u_t va,
								  paddr_t pa, int iswrite, int way, int width);
typedef void (*l2cache_callback_t) (system_t * sys, processor_t * proc,
									int threadno, size4u_t pc, size4u_t va,
									paddr_t pa, int iswrite, int isfetch,
									int way);
typedef void (*l2fifo_callback_t)(system_t *sys, processor_t *proc, void *info);
typedef void (*vfifo_callback_t)(system_t *sys, processor_t *proc, void *ptr);
typedef void (*vtcm_callback_t)(system_t *sys, processor_t *proc, void *info);
typedef void (*memcpy_callback_t)(system_t *sys, processor_t *proc, void *info);
typedef void (*iu_l1s_callback_t)(system_t *sys, processor_t *proc, void *info);

typedef void (*scaga_callback_t)(system_t *sys, processor_t *proc, scaga_callback_info_t *info);
typedef void (*cycle_complete_callback_t)(system_t *sys, processor_t *proc);
typedef void (*fe_callback_t)(system_t *sys, processor_t *proc, const fe_callback_info_t *info);
typedef void (*l2sb_callback_t)(system_t * sys, processor_t * proc, int tnum,
                     l2sb_trace_event_t event,
                     int interleave_id, int entry_id, int link_id,
                     vaddr_t pc, paddr_t paddr, int occupancy, size8u_t delta);

typedef void (*l2arb_callback_t)(system_t * sys, processor_t * proc, int tnum,
                     int interleave_id, int arb_type, int fail_code,
					 int l2fetch, int iu,
                     vaddr_t pc, paddr_t paddr);

typedef void (*l2pt_callback_t)(system_t * sys, processor_t * proc, int tnum,
                     l2pt_trace_event_t event,
                     vaddr_t pc, paddr_t paddr, int bank_id, size8u_t when,
					 int duration);


typedef void (*cache_replace_callback_t) (system_t * sys,
										  processor_t * proc, int threadno,
										  size4u_t pc, size4u_t va,
										  paddr_t pa, paddr_t oldpa,
										  int wasdirty, int way);
typedef size8u_t(*user_insn_callback_t) (system_t * sys,
										 processor_t * proc, int threadno,
										 size8u_t RssV, size8u_t RttV,
										 size8u_t RxxV, int uiV);
typedef void (*l2prefetch_callback_t)(system_t *sys, processor_t *proc, l2fetch_callback_info_t *l2fetch_callback_info);

typedef void (*reg_access_callback_t)(system_t *sys, processor_t *proc, void *ptr);

typedef void (*vmem_callback_t)(system_t *sys, processor_t *proc, vmem_callback_info_t *info);


typedef void(*arch_stats_reset_callback_t)(system_t *sys, processor_t *proc);

typedef void (*pmu_update_callback_t)(system_t *sys, processor_t *proc, void *ptr);

typedef void (*uarch_sbuf_update_callback_t)(system_t *sys, processor_t *proc, void *ptr);

typedef void (*uarch_cu_dispatch_callback_t)(system_t *sys, processor_t *proc, void *ptr);

typedef void (*uarch_fill_update_callback_t)(system_t *sys, processor_t *proc, void *ptr);

typedef void (*uarch_branch_update_callback_t)(system_t *sys, processor_t *proc, const uarch_branch_update_callback_info_t *ptr);

typedef size8u_t (*sim_qtimer_read_callback_t)(system_t *sys, processor_t *proc);

typedef void (*sim_busaccess_callback_t)(system_t *sys, processor_t *proc, int tnum, vaddr_t pc, paddr_t paddr, int width, int type);

typedef void (*arch_entering_instruction_t)(system_t *sys, processor_t *proc, int tnum, enter_instruction_type_e type);




/* This thread got a chance to execute this cycle.
   Note that this can be called more than once in a given pcycle */
typedef void (*cycle_callback_mc_t) (system_t * sys, processor_t * proc,
									 int tnum);
typedef void (*cycle_callback_uarch_t)(system_t *sys, processor_t *proc,
                                       void *ptr);


/********************************************************************/
/*                Simulator-implemented functions                   */
/********************************************************************/


/* Section 1: processor initialization */

/*
 * This defines all the options to the simulator
 * This structure is intialized to all 0's.
 */
typedef struct options_struct {
	/* These are *NOT* changeable at runtime */
	/* General simulator options */
	size4u_t check_restrictions;	/* check certain restrictions */
	size4u_t disable_angelswi;	/* don't do angel callback */
	size4u_t testgen_mode;	        /* test generation mode  */
	/* Every I Read, D Read, or D Write can callback to the simulator for tracing purposes */
	/* NULL means don't call back. */
	memory_callback_t sim_memory_callback;
	memory_callback_t sim_vtcm_memory_callback;
	cof_callback_t sim_cof_callback;
	dma_callback_t dma_callback;

	user_insn_callback_t userinsn_callback;

	/* Marker for when system events happen */
	exception_callback_t exception_callback;
	exception_callback_t exception_precommit_callback;
	system_callback_t swi_callback;	/* info = bits set */
	system_callback_t interrupt_serviced_callback;	/* info = interrupt # serviced */
	system_callback_t rte_callback;	/* info = 0 */
	system_callback_t trace_callback; /* info == val */

    /* Marker when entering some special instructions the outside world cares about */
    arch_entering_instruction_t entering_instruction;

	/* FIXME: To be deprecated */
//    cycle_callback_t cycle_callback;  /* callback each cycle */

	cycle_callback_mc_t cycle_callback_mc;	/* callback for each cycle */
	system_callback_t tlbw_callback;	/* callback on TLBW */
	system_callback_t trap0_callback;	/* Trap callbacks */
	system_callback_t trap1_callback;
	system_callback_t coreready_callback;	/* callback for when core ready is signaled */
    cycle_callback_uarch_t cycle_uarch_callback;
    cycle_complete_callback_t cycle_complete_callback;
	/* Stall callback will be made for the duration of
	   the stall whenever the thread profiling is on and
	   the associated model is enabled (--icache --dcache)
	 */
	vmem_callback_t sim_vmem_callback;
	stall_callback_t stall_callback;	/* callback from a stall */
	pmu_update_callback_t sim_pmu_update_callback;
	uarch_sbuf_update_callback_t sim_sbuf_update_callback;
	uarch_cu_dispatch_callback_t sim_cu_dispatch_callback;
	scaga_callback_t sim_scaga_callback;
	uarch_fill_update_callback_t sim_fill_update_callback;
	uarch_credit_callback_t sim_credit_callback;
  	uarch_utlb_update_callback_t sim_utlb_update_callback;
  	uarch_utlb_update_callback_t sim_ras_update_callback;
    uarch_cu_iq_callback_t sim_cu_iq_callback;
    uarch_string_callback_t sim_string_callback;

        /* Branch event Callback */
        uarch_branch_update_callback_t sim_branch_update_callback;

	/* qtimer read callback */
	sim_qtimer_read_callback_t qtimer_read_callback;

	/* Call back when the proc resets the stats */
	arch_stats_reset_callback_t arch_stats_reset_callback;

	/* Machine revid */
	size4u_t revid;

	paddr_t ahb_base;
	paddr_t ahb_size;

	/* Bus enable options */
	size4u_t ahb_enable;
	size4u_t axi_enable;
	size4u_t axi2_enable;

        /* AXI interleaving options */
        size4u_t axi_interleave_enable;
        size4u_t axi_interleave_bit;

	/* AXI2 options */
	paddr_t  axi2_lowaddr;
	paddr_t  axi2_highaddr;

	int bus_bytes_per_beat;

	/* Initial EVB option */
	size4u_t initial_evb;

	/* Timing on/off? */
	size4u_t timing;
    size4u_t timing_pipeonly;
  	size1u_t timing_in_transition;
	size4u_t analyze;
	int data_backed_cache;
	int sil_verif;

    /* Any archstrings. Pointer to the archstring. */
    char *archstring;

	/* Version */
	size4u_t version;

	/* Start-needed for v3 and v2 */
	size4u_t icache_enable;		/* enable Icache model */
	size4u_t icache_grans;		/* Number of granuales per line */
	size4u_t icache_prefetch;	/* enable Icache prefetching */
	size4u_t icache_linesize;	/* line size for Icache */
	size4u_t icache_ways;		/* number of ways in Icache */
	size4u_t icache_sets;		/* number of sets in Icache */
	size4u_t icache_rand;		/* Use random replacement */
	size4u_t icache_lru;		/* Use random replacement */
	size4u_t icache_nol2;		/* Don't use L2 cache */
	size4u_t icache_model_data;	/* Keep track of Icache data ourselves */
	/* end-needed for v3 and v2 */
	cache_callback_t sim_icachemiss_callback;
	cache_callback_t sim_icachehit_callback;
	cache_callback_t sim_icacheaccess_callback;
	cache_replace_callback_t sim_icachereplace_callback;

    sim_busaccess_callback_t sim_busaccess_callback;

	/* Start-needed for v3 and v2 */
	size4u_t dcache_enable;		/* enable Icache model */
	size4u_t dcache_grans;		/* Number of granuales per line */
	size4u_t dcache_rand;		/* Use random replacement */
	size4u_t dcache_lru;		/* Use random replacement */
	size4u_t dcache_nol2;		/* Don't use L2 cache */
	size4u_t dcache_prefetch;	/* enable Dcache prefetching */
	size4u_t dcache_prefpost;	/* enable Dcache prefetching on post-update */
	size4u_t dcache_prefaux;	/* prefetch into auxiliary partition */
	size4u_t dcache_auxways;	/* how many ways in auxiliary partition */
	size4u_t dcache_smartfifo;	/* better fifo stuff */
	size4u_t dcache_writenoalloc;	/* enable Dcache prefetching */
	size4u_t dcache_wt;			/* write through caching */
	size4u_t dcache_linesize;	/* line size for Icache */
	size4u_t dcache_ways;		/* number of ways in Icache */
	size4u_t dcache_sets;		/* number of sets in Icache */
	size4u_t dcache_model_data;	/* Keep track of Icache data ourselves */
	/* end-needed for v3 and v2 */
	cache_callback_t sim_dcachemiss_callback;
	cache_callback_t sim_dcachehit_callback;
	cache_callback_t sim_dcacheaccess_callback;
	cache_replace_callback_t sim_dcachereplace_callback;

	/* l2sb */
	l2sb_callback_t sim_l2sb_callback;
	l2arb_callback_t sim_l2arb_callback;
	l2pt_callback_t sim_l2pt_callback;

	/* Start-needed for v3 and v2 */
	size4u_t l2cache_enable;	/* enable Icache model */
	size4u_t l2cache_rand;		/* Use random replacement */
	size4u_t l2cache_grans;		/* Number of granuales per line */
	size4u_t l2cache_ignorethreads;	/* Number of threads to not use Cache (EJP Hack) */
	size4u_t l2cache_linesize;	/* line size for L2cache */
	size4u_t l2cache_ways;		/* number of ways in L2cache */
	size4u_t l2cache_sets;		/* number of sets in L2cache */
	size4u_t l2cache_model_data;	/* Keep track of L2cache data ourselves */
	size4u_t l2cache_sz;		/* Syscfg's l2sz is set at processor design time */
	/* end-needed for v3 and v2 */
	l2fifo_callback_t sim_l2fifo_callback;
	vfifo_callback_t sim_vfifo_callback;
    vtcm_callback_t sim_vtcm_callback;

	memcpy_callback_t sim_memcpy_callback;

	iu_l1s_callback_t sim_iu_l1s_callback;

	l2cache_callback_t sim_l2cachemiss_callback;
	l2cache_callback_t sim_l2cachehit_callback;
	l2cache_callback_t sim_l2cacheaccess_callback;
	tcm_access_callback_t sim_tcmaccess_callback;
	cache_replace_callback_t sim_l2cachereplace_callback;
	l2prefetch_callback_t sim_l2prefetch_callback;
	reg_access_callback_t sim_reg_access_callback;
	fe_callback_t sim_fe_callback;
	tcm_access_callback_t sim_l2itcmaccess_callback;

	size4u_t model_busaccess;	/* Call sim_busaccess when needed */

	/* Strappings */
//    paddr_t cfgbase;
	paddr_t l2tcm_base;
	paddr_t l1tcm_base;
	paddr_t subsystem_base;
	paddr_t etm_base;
	paddr_t l2cfg_base;
	paddr_t l2itcm_base;
	//paddr_t axi2_base; axi2_lowaddr above
	paddr_t streamer_base;
	paddr_t clade_base;
	int clade_enable;
	paddr_t clade2_base;
	int clade2_enable;
	int clade2_interrupt;
	int clade2_stable_assert_hack;

	paddr_t vtcm_base;
	paddr_t vtcm_size;


	int l2itcm_enable;
	int l2itcm_size;

    int dont_verify_extensively; /* When set to 0, does all forms of self checking in timing and non timing mode. Inverted from sim side. Dont wnat to mess with verification. */
	int cache_lean;
	int check_ssr_xe;
} options_t;

/* Managing structures */
/* allocate internal processor structure */
/* Simulator can give the processor a pointer which the processor will
   give back on callbacks. */
/* Simulator gives the processor a structure with the initial options */
/* Note that for options you donate the memory to me, I don't make a copy. */

/* This does a combined alloc and init based on options */
processor_t *arch_alloc_processor(system_t * sys, options_t * options);

/* This frees the processor, thread and cache memory */
void arch_free_processor(processor_t * proc);

/* Print last packet processed from tnum to a buf */
void snprint_last_verif_pkt(char *buf, int n, processor_t * proc, int tnum);
void arch_snprint_last_pkt(char *buf, int n, processor_t * proc, int tnum);
void arch_snprint_tags_only(char *buf, int n, processor_t * proc, int tnum);
int arch_snprint_a_pkt(char *buf, int n, processor_t * proc, int tnum, size4u_t *encodings);
void arch_snprint_last_pkt_fields(char *buf, int n, processor_t * proc, int tnum, size4u_t fields);
size4u_t arch_get_last_pkt_pcva(processor_t * proc, int tnum);
size4u_t arch_get_last_vpkt_pcva(processor_t * proc, int tnum);
void arch_print_ext_regs(processor_t * proc, int tnum, FILE *fp);
void arch_print_ext_reg(processor_t *proc, int tnum, FILE *fp, char* ext_name, int rnum);
void arch_dump_ext_regs(FILE *fp, system_t *sys, processor_t *proc);
void arch_load_ext_regs(FILE *fp, system_t *sys, processor_t *proc);
size4u_t arch_get_pcva_for_stalltrace(processor_t *proc, int tnum);
char *arch_get_stall_name(processor_t *proc, int tnum, int stall_type);

/* Reset the processor */
void arch_reset_processor(processor_t * proc);

/* Reset all global and thread specific cycle,stall counters
   and restart the caches, utlbs, etc. */
void arch_reset_timing(processor_t * proc);

/* flush any non-architecture state kept by the model between low_addr and high_addr */
void arch_internal_flush(processor_t * proc, size4u_t low_addr,
						 size4u_t high_addr);


/* Section 2: processor control */

/* return codes for arch_go / arch_cycle / arch_thread_cycle:
    low 16-bits contain the TNUM
    high 16-bits contain the execution code */
#define ARCH_CYCLE_PACKET_EXECUTED  0x010000	/* Packet executed */
#define ARCH_CYCLE_NOEXEC_OFFMODE   0x020000	/* No execution due to Off mode */
#define ARCH_CYCLE_NOEXEC_WAITMODE  0x040000	/* No execution due to Wait mode */
#define ARCH_CYCLE_NOEXEC_STALL     0x080000	/* No execution due to Stall/Replay */
#define ARCH_CYCLE_NOEXEC_EXCEPTION 0x100000	/* No execution due to Exception */
#define ARCH_CYCLE_NOEXEC_PAUSED    0x200000	/* No execution due to a Pause instruction */
#define ARCH_CYCLE_NOEXEC_LOCKWAIT  0x400000	/* No execution due to wait on a HW lock */

/* Functions to control execution */
unsigned int arch_thread_cycle(processor_t * proc, int threadno);	/* advance by a single cycle */

/* for multi-core, execute 1 cycle. This returns a bitmask of all
   the threads that executed in the cycle */
unsigned int arch_cycle_mc(processor_t * proc);	/* advance by a single cycle */

/* Return the status of a thread as per the defines above */
unsigned int arch_get_thread_status(processor_t * proc, int threadno);

/* step the given thread exactly one packet
   this function ignores stalls, stop mode, wait mode, interrupts.
   It always runs one packet */
size4u_t arch_iss_thread_step(processor_t * proc, int threadno);


/* Section 3: Interfaces to Core */

/* Read Registers */
size4u_t arch_get_thread_reg(processor_t * proc, int threadno, int index);	/*per thread */
char *arch_get_thread_reg_name(processor_t *proc, int index);
size4u_t arch_get_global_reg(processor_t * proc, int index);	/*global */
size8u_t arch_get_TLB_reg(processor_t * proc, int index);	/*global */

const unsigned char * arch_get_ext_vreg_bytes(const processor_t *proc, int tnum, int extno, size4u_t regno);
const unsigned char * arch_get_ext_qreg_bytes(const processor_t *proc, int tnum, int extno, size4u_t regno);

/* Extension Interface, can return failure, 0 on success */
int arch_get_ext_zreg_word(processor_t *proc, int threadno, int extno, size4u_t regno, size4u_t widx, size4u_t *result);
int arch_get_ext_vreg_word(processor_t *proc, int threadno, int extno, size4u_t regno, size4u_t widx, size4u_t *result);
int arch_get_ext_qreg_word(processor_t *proc, int threadno, int extno, size4u_t regno, size4u_t widx, size4u_t *result);
int arch_set_ext_vreg_word(processor_t *proc, int threadno, int extno, size4u_t regno, size4u_t widx, size4u_t val);
int arch_set_ext_qreg_word(processor_t *proc, int threadno, int extno, size4u_t regno, size4u_t widx, size4u_t val);
int arch_set_ext_zreg_word(processor_t *proc, int threadno, int extno, size4u_t regno, size4u_t widx, size4u_t val);

int arch_get_ext_accumulator_word(processor_t *proc, int arrayno, int spatial_idx, int channel_idx, size4u_t word_idx, size4u_t *result);
int arch_set_ext_accumulator_word(processor_t *proc, int arrayno, int spatial_idx, int channel_idx, size4u_t word_idx, size4u_t val);
int arch_get_ext_accumulator_word(processor_t *proc, int arrayno, int spatial_idx, int channel_idx, size4u_t word_idx, size4u_t *result);
int arch_set_ext_accumulator_word(processor_t *proc, int arrayno, int spatial_idx, int channel_idx, size4u_t word_idx, size4u_t val);
int arch_get_ext_bias_word(processor_t *proc, int arrayno, int spatial_idx, int channel_idx, size4u_t word_idx, size4u_t *result);
int arch_set_ext_bias_word(processor_t *proc, int arrayno, int spatial_idx, int channel_idx, size4u_t word_idx, size4u_t val);


int arch_has_hvx(processor_t *proc);


/* Translate a VA to PA using just the TLB */
/* Uses the ASID of the thread threadno */
/* Returns 0 on failure, nonzero otherwise */
/* Fills in pa if successful */
int arch_tlb_va2pa(processor_t * proc, int threadno, size4u_t va,
						paddr_t * pa);
int arch_tlb_va2pa_by_asid(processor_t *proc, int threadno, size4u_t asid, size4u_t va, paddr_t *pa);

/* return num runnable threads */
int arch_get_num_threads(processor_t * proc);

/* arch-side will set this variable whenever all threads enter STOP mode */
void arch_register_allstop_pointer(processor_t * proc, int *all_stop);

/* write registers */
void arch_set_thread_reg(processor_t * proc, int threadno, int index, size4u_t val);	/*per thread */
void arch_set_global_reg(processor_t * proc, int index, size4u_t val);	/*global */
void arch_set_global_reg_with_effects(processor_t * proc, int index, size4u_t val);	/*global */
void arch_set_TLB_reg(processor_t * proc, int index, size8u_t val);	/*global */

/* Cache Manipulation */
void arch_invalidate_icache(processor_t * proc, int tnum, paddr_t paddr);
void arch_invalidate_dcache(processor_t * proc, int tnum, paddr_t paddr);
void arch_clean_dcache(processor_t * proc, int tnum, paddr_t paddr);


/* Trigger an edge-triggered interrupt (pulse)
 * This is the equivelant of arch_set_intpin_high()
 * followed by arch_set_intpin_low() */
void arch_trigger_interrupt(processor_t * proc, int interrupt_number);

/* Set interrupt pin levels
 * This allows for arbitrary edge/level interrutps */
void arch_set_intpin_high(processor_t * proc, int interrupt_number);
void arch_set_intpin_low(processor_t * proc, int interrupt_number);

/* Profiling and stats */
/* return what kind of stall the thread is currently experiencing, or -1 if
   not in stall condition */
int arch_get_thread_stall_type(processor_t * proc, int tnum);
char *arch_get_thread_stall_name(processor_t * proc, int tnum);
int arch_get_thread_stall_info(processor_t *proc, int tnum, int n, char *buf);
char *arch_get_iclass_name(processor_t *proc, size2u_t opcode, int iclass);
char *arch_get_tclass_name(processor_t *proc, int tclass);
const char *arch_get_hvx_tclass_name(processor_t *proc, int hvx_tclass);
int arch_get_hvx_tclass(processor_t *proc, size2u_t opcode);
int arch_last_pkt_has_extension(processor_t *proc, int tnum);
int arch_last_pkt_has_vhist(processor_t *proc, int tnum);
int get_cid_from_tnum(processor_t *proc, int tnum);

/* API for implementing timing in uarch trace playback */
set_thread_state_return_t arch_set_thread_state(processor_t *proc, int tnum, set_thread_state_t tstate);
void arch_force_thread_state(processor_t *proc, int tnum, set_thread_state_t tstat);

schedule_trace_return_t arch_schedule_trace(processor_t *proc, int tnum, char* tfname);

//extern char *stall_names[];
int arch_get_num_stall_types(processor_t *proc);
size8u_t arch_get_proc_stat(processor_t * proc, int proc_stat_id);
/* FIXME: hack to work around use of pcycles stat where monotonic_pcycles
	 should be used */
void arch_set_proc_stat(processor_t *proc, int id, size8u_t value);
size8u_t arch_get_proc_stat_pcycles(processor_t * proc);
size8u_t arch_get_thread_stat(processor_t * proc, int tnum,
							  int thread_stat_id);
size8u_t arch_get_thread_stall(processor_t * proc, int tnum,
							   int thread_stall_id);
size8u_t *arch_get_proc_histo(processor_t * proc, int proc_stat_id);
size8u_t *arch_get_thread_histo(processor_t * proc, int tnum,
								int thread_histo_id);
int arch_get_thread_count(processor_t * proc);

void arch_stats_display(processor_t * proc, FILE * file);
void arch_stats_display_help(processor_t * proc, FILE * file);
void arch_sync_stats(processor_t *proc);

/*  Power related functions. */

/* Start a new interval of power data collection */
void arch_power_reset_stats(processor_t * proc);

/* Return the percent of time in special all-wait power mode */
float arch_power_get_allwait_pct(processor_t * proc);

/* Return the avg number of active threads when at least 1 is running. It excludes
   time in all-wait */
float arch_power_get_num_active_threads(processor_t * proc);

/* Return the avg packet density over all threds */
float arch_power_get_avg_insns_per_packet(processor_t * proc);


/********************************************************************/
/*                Simulator-implemented functions                   */
/********************************************************************/


/* Section 4: Memory */

/* The ISS will call this function with a physical address.
   The external code should return a pointer to the memory
   on the host where the data is stored. The 'stable_4k_page'
   should be set to 1 if the ISS can continuously access
   this page in host memory without needing to make
   mem_read/mem_write calls.

   For normal physical memory that is read and written only
   by the ISS, this should be set to 1. For other memory
   which is touched externally, such as peripheral memory
   this should be set to 0. In that case, the ISS
   will access the memory through mem_read/mem_write calls.
*/

char *sim_mem_get_host_ptr(system_t * sys, paddr_t paddr,
						   int size, int *stable_4k_page);

/* Read memory.  This does not imply an actual bus transaction,
   if we are not modling the cache memories we will use these
   functions to get the memory from the simulator.
   If we actually needed the bus, we will also call sim_busaccess
*/

void sim_mem_write128(system_t * S, int threadno, paddr_t paddr,
					  size4u_t * src);

/* Memory operations that actually need extra support from the device.
   sim_mem_read4_locked corresponds to the R=mem_locked(R) instruction on UC memory.
   sim_mem_write4_locked corresponds to the mem_locked(R,P)=R instruction on UC memory.
   sim_mem_write4_locked should return 1 if the store was successful, 0 otherwise.  */

size4u_t sim_mem_read4_locked(system_t * sys, int threadno, paddr_t paddr);
size8u_t sim_mem_read8_locked(system_t * sys, int threadno, paddr_t paddr);
int sim_mem_write4_locked(system_t * sys, int threadno, paddr_t paddr,
						  size4u_t value);
int sim_mem_write8_locked(system_t * sys, int threadno, paddr_t paddr,
						  size8u_t value);

/* This API is used by the simulator to indicate which ranges of
   memory are getting initialized. Typically used in trace mode, but
   could be used any where, really */
void simmem_memory_register_initialized(system_t *sys, paddr_t paddr, paddr_t size);
int simmem_memory_check_initialized(system_t *sys, paddr_t paddr, paddr_t size);

/* Section 5: Bus Timing */

/* We need to use the bus.  This is probably due to a cache miss,
	but could also be because of a load/store to uncachable memory.

arch_bus_finished_callback_t: function pointer for simulator to notify arch side
	when bus is finished with a task

	proc/threadno: processor and threadno of the bus access
	paddr: physical address of the bus access
	width: width of access in bytes
	type: type of access
	id: identifier for access

	(Note that most of these are passed back from the sim_busaccess call)

sim_busaccess: request bus access from simulator
	sys: system pointer (for sim side)
	proc/threadno: processor and thred number
	paddr: physical address
	width: width of access in bytes
	type: type of access
	callback: When bus is finished, call this callback.
	id: identifier for access.  Pass back to simulator. */


typedef void (*arch_bus_finished_callback_t) (processor_t * proc,
                                             int threadno, paddr_t paddr,
                                             size4u_t width, int type,
                                             int id,
                                             unsigned char *dataptr);

int sim_busaccess(system_t * sys, processor_t * proc, int threadno,
				  paddr_t paddr, int width, int type,
				  arch_bus_finished_callback_t callback, int id,
				  unsigned char *dataptr);

typedef int (*sim_busaccess_t) (system_t * sys, processor_t * proc,
								int threadno, paddr_t paddr, int width,
								int type,
								arch_bus_finished_callback_t callback,
								int id, unsigned char *dataptr);


/* Section 6: Debug support */
/* Return the lower addres, upper address and the number of instructions in a packet */
int arch_get_packet_range(processor_t * proc, int tnum, size4u_t * lower,
						  size4u_t * upper, int *ninsns);


/* Section 7: Other Interfaces */

int sim_handle_trap(system_t * sys, processor_t * proc, int threadno,
					 size4s_t imm);
void sim_handle_debug(system_t * sys, processor_t * proc, int threadno,
					  size4s_t imm);

void sim_err_warn(system_t * sys, processor_t * proc, int threadno,
				  char *funcname, char *file, int line, char *fmt, ...);
void sim_err_fatal(system_t * sys, processor_t * proc, int threadno,
				   char *funcname, char *file, int line, char *fmt, ...);
void sim_err_panic(system_t * sys, processor_t * proc, int threadno,
				   char *funcname, char *file, int line, char *fmt, ...);
void sim_err_debug(char *fmt, ...);

void arch_set_cachedebug(processor_t *proc, int val);

/* APIs for memory mapped registers.
   Why is this needed by external_api.h? For pseudo devices, such as streamer, and debuggers. */
/* Callback type when q6 does a read to the cluster of memory mapped registers. Should return the value read. */
typedef size8u_t (*arch_memmap_q6read_access_t)(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width);

/* Callback type when q6 does a write to the cluster of memory mapped registers. Should write the value in val. */
typedef void (*arch_memmap_q6write_access_t)(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width, size8u_t val);

/* Acquires a cluster of memory map registers, and returns an id to the memory mapped register cluster */
int arch_memmap_cluster_acquire(processor_t *proc, paddr_t base_addr, int nbytes,
																arch_memmap_q6read_access_t read_call,
																arch_memmap_q6write_access_t write_call,
																arch_memmap_q6read_access_t ext_read_call,
																arch_memmap_q6write_access_t ext_write_call);

/* Release a previously-acquired memmap cluster */
void arch_memmap_cluster_release(processor_t *proc, int memmap_cluster_id);


/* When a entity, other than q6 wants to read from the cluster of memory mapped registers. This
   includes the entities that have registered for a cluster of memory mapped registers. */
size1u_t arch_memmap_cluster_read1(processor_t *proc, int memmap_cluster_id, paddr_t paddr);
size2u_t arch_memmap_cluster_read2(processor_t *proc, int memmap_cluster_id, paddr_t paddr);
size4u_t arch_memmap_cluster_read4(processor_t *proc, int memmap_cluster_id, paddr_t paddr);
size8u_t arch_memmap_cluster_read8(processor_t *proc, int memmap_cluster_id, paddr_t paddr);

/* When an entity, other than q6 wants to write into the cluster of memory mapped registers.
   This includes the entities that have registered for a cluster of memory mapped registers. */
void arch_memmap_cluster_write1(processor_t *proc, int memmap_cluster_id, paddr_t paddr, size1u_t data);
void arch_memmap_cluster_write2(processor_t *proc, int memmap_cluster_id, paddr_t paddr, size2u_t data);
void arch_memmap_cluster_write4(processor_t *proc, int memmap_cluster_id, paddr_t paddr, size4u_t data);
void arch_memmap_cluster_write8(processor_t *proc, int memmap_cluster_id, paddr_t paddr, size8u_t data);

/* Search memory map registers for an paddr, and return the id for that entity that has registered
   for that address. Return -1 if there is none registered to that address. */
int arch_memmap_get_cluster_id(processor_t *proc, paddr_t paddr);

/* Dump any memmap regs that have been written */
void arch_memmap_dump(FILE *fp, system_t *sys, processor_t *proc);


/* End Memory mapped register APIs */


/* If we want to specify a different bus accessor method than sim_busaccess, this specifies it */
void arch_set_busaccess(processor_t * proc, sim_busaccess_t busaccess);


#define ARCH_DBG_ACCESS_ADDR_FOUND 0
#define ARCH_DBG_ACCESS_ADDR_NOT_FOUND 1
#define ARCH_DBG_ACCESS_ADDR_IN_TRANSIT 2
#define ARCH_DBG_ACCESS_FAIL_ADDR_UNALIGNED -1
#define ARCH_DBG_ACCESS_FAIL_ADDR_OUT_OF_RANGE -2
#define ARCH_DBG_ACCESS_FAIL_PARTIAL_ADDRESS_FOUND -2
#define ARCH_DBG_ACCESS_FAIL_DBC_NOT_SET -3
#define ARCH_DBG_ACCESS_FAIL_DATA_ERROR -4

#define SIM_BUSACCESS_FAIL 0
#define SIM_BUSACCESS_SUCCESS 1
#define SIM_BUSACCESS_SUCCESS_LOCK_INVALID 2


int arch_dbg_write1(processor_t * proc, int tnum, paddr_t paddr,
					size1u_t data);
int arch_dbg_write2(processor_t * proc, int tnum, paddr_t paddr,
					size2u_t data);
int arch_dbg_write4(processor_t * proc, int tnum, paddr_t paddr,
					size4u_t data);
int arch_dbg_write8(processor_t * proc, int tnum, paddr_t paddr,
					size8u_t data);

int arch_dbg_read1(processor_t * proc, int tnum, paddr_t paddr,
				   size1u_t * dataptr);
int arch_dbg_read2(processor_t * proc, int tnum, paddr_t paddr,
				   size2u_t * dataptr);
int arch_dbg_read4(processor_t * proc, int tnum, paddr_t paddr,
				   size4u_t * dataptr);
int arch_dbg_read8(processor_t * proc, int tnum, paddr_t paddr,
				   size8u_t * dataptr);


int arch_dbg_tcm_write1(processor_t * proc, int tnum, paddr_t paddr,
						size1u_t data);
int arch_dbg_tcm_write2(processor_t * proc, int tnum, paddr_t paddr,
						size2u_t data);
int arch_dbg_tcm_write4(processor_t * proc, int tnum, paddr_t paddr,
						size4u_t data);
int arch_dbg_tcm_write8(processor_t * proc, int tnum, paddr_t paddr,
						size8u_t data);

int arch_dbg_tcm_read1(processor_t * proc, int tnum, paddr_t paddr,
					   size1u_t * dataptr);
int arch_dbg_tcm_read2(processor_t * proc, int tnum, paddr_t paddr,
					   size2u_t * dataptr);
int arch_dbg_tcm_read4(processor_t * proc, int tnum, paddr_t paddr,
					   size4u_t * dataptr);
int arch_dbg_tcm_read8(processor_t * proc, int tnum, paddr_t paddr,
					   size8u_t * dataptr);

int arch_dbg_itcm_read1(processor_t * proc, int tnum, paddr_t paddr,
						size1u_t * dataptr);
int arch_dbg_itcm_read2(processor_t * proc, int tnum, paddr_t paddr,
						size2u_t * dataptr);
int arch_dbg_itcm_read4(processor_t * proc, int tnum, paddr_t paddr,
						size4u_t * dataptr);
int arch_dbg_itcm_read8(processor_t * proc, int tnum, paddr_t paddr,
						size8u_t * dataptr);

int arch_dbg_itcm_write1(processor_t * proc, int tnum, paddr_t paddr,
						 size1u_t data);
int arch_dbg_itcm_write2(processor_t * proc, int tnum, paddr_t paddr,
						 size2u_t data);
int arch_dbg_itcm_write4(processor_t * proc, int tnum, paddr_t paddr,
						 size4u_t data);
int arch_dbg_itcm_write8(processor_t * proc, int tnum, paddr_t paddr,
						 size8u_t data);


void arch_set_nmipin_low(processor_t * proc);
void arch_set_nmipin_high(processor_t * proc);

/* API for the TCM slave port. */
int arch_tcm_slave_access(system_t *sys, processor_t * proc, int tnum,
						  paddr_t paddr, int width, int type,
						  arch_bus_finished_callback_t callback, int id,
						  unsigned char *dataptr);

int arch_mnoc_to_l1s_access(system_t *sys, processor_t * proc, int tnum,
						  paddr_t paddr, int width, int type,
						  arch_bus_finished_callback_t callback, int id,
						  unsigned char *dataptr);

int arch_axi_slave_access(void *caller, processor_t * proc, int tnum,
						  paddr_t paddr, int width, int type,
						  arch_bus_finished_callback_t callback, int id,
						  unsigned char *dataptr);

int arch_mlm_to_l1s_access(void *caller, processor_t * proc, int tnum,
						  paddr_t paddr, int width, int type,
						  arch_bus_finished_callback_t callback, int id,
						  unsigned char *dataptr);



/* pe_num is the PMU event number. It get translated to the stats maintained internally
   and returns the stat value. */
size8u_t arch_get_pmu_indexed_stats(processor_t * proc, int tnum,
									int pe_num);

/* Given a PMU event number, this function returns if the PMU event is modeled in the
   simulator or not. 1 for yes, 0 for no */
int arch_pmu_is_stat_modeled(processor_t * proc, int tnum, int pe_num);

/* Return the Name for the PMU number */
char *arch_pmu_get_name(processor_t * proc, int tnum, int pe_num);

/* Return if the PMU is  */
int arch_pmu_is_maskable(processor_t * proc, int tnum, int pe_num);

/* Find the last PMU event number */
int arch_pmu_get_last_pmu_number(processor_t * proc, int tnum);

/* Find the last architected PMU event number */
int arch_pmu_get_last_arch_pmu_number(processor_t * proc);

/* Find if the pmu number is valid.
   This could be achieved with arch_pmu_get_name too. */
int arch_pmu_get_valid(processor_t *proc, int tnum, size4u_t pe_num);

/* Given a stall type, it returns the pmu number. This will be
   deprecated later. */
int arch_pmu_get_tstall_to_pmu_mapping(processor_t *proc, int stall_type);

/* Given set and way, this function returns the tag for that set and way in the dcache */
size4u_t arch_dbg_get_dctag(processor_t * proc, int tnum, int set,
							int way);

/* This function returns the data values in the given set and way in dcache. The dataptr
   should be atleast 32bytes wide */
void arch_dbg_get_dcdata(processor_t * proc, int tnum, int set, int way,
						 unsigned char *dataptr);

/* Returns the state for a given set and way. The return value is to be decoded as follows:
   state = retval & 0xffffffff
   cccc = (retval >> 32) & 0xffffffff
   interpreting state:
   2'b00 for invalid,
   2'b01 for valid and clean,
   2'b10 for valid and reserved,
   2'b11 for valid and dirty */
size8u_t arch_dbg_get_dcstate(processor_t * proc, int tnum, int set,
							  int way);

/* Given set and way, this function returns the tag for that set and way in the icache */
size4u_t arch_dbg_get_ictag(processor_t * proc, int tnum, int set,
							int way);

/* Given set and way, this function returns the tag for that set and way in the l2cache */
size4u_t arch_dbg_get_l2tag(processor_t * proc, int tnum, int set,
							int way);

/* Note: dataptr should be 32bytes wide for l2cache with one granule,
   and 64 bytes wide for l2cache with 2 granules, and
   128 bytes wide for l2cache with 4 granules.
   So best to allocate it 128 bytes wide */
void arch_dbg_get_l2data(processor_t * proc, int tnum, int set, int way,
						 unsigned char *dataptr);

unsigned int arch_get_num_tlb_entries(processor_t * proc);

/* arch_get_cache_configuration returns the number of sets, ways, granules and line size of each cache.
   The cache is specified in the enum.
   Note: The number of sets and granules for L2 can change at run time based on SYSCFG value. */
enum {
	_ICACHE_,
	_DCACHE_,
	_L2CACHE_
};

/* enum for fields in the uarchtrace */
enum {
    SNFIELD_SLOTTAG,
    SNFIELD_EAPA,
    LAST_SNFIELD
};

/* Opcode information */
#ifndef GENERATE_ENUM
#define GENERATE_ENUM(ENUM) ENUM,
#endif

#ifndef GENERATE_STRING
#define GENERATE_STRING(STRING) #STRING,
#endif


size4u_t arch_get_max_opcodes(processor_t *proc);


struct opcode_info_struct {
    char *opcode_name;
	char *opcode_syntax;
    size1u_t iclass;
    char *iclass_name;
    ext_type_e ext;
};
typedef struct opcode_info_struct opcode_info_t;

int set_verif_cmdline_feature(processor_t *proc, int feature, int value);

int arch_get_opcode_info(processor_t *proc, size2u_t opcode, opcode_info_t *info);

void arch_get_cache_configuration(processor_t * proc, int tnum, int cache,
								  int *sets, int *ways, int *granules,
								  int *linesize);

void arch_cache_dump_caches(FILE *fp, system_t * sys, processor_t *proc);
void arch_cache_load_caches(FILE *fp, system_t * sys, processor_t *proc);
void arch_dump_tlb(FILE *fp, system_t * sys, processor_t *proc);
void arch_load_tlb(FILE *fp, system_t * sys, processor_t *proc);
void arch_dump_cfgtbl(FILE *fp, processor_t *proc);

size4u_t arch_get_vregw(processor_t * proc, int vnum, int num);

/* FIXME: This should be a parameter in sim_busaccess */
vaddr_t arch_get_pc_for_access(processor_t *proc, int tnum, int id);
char *arch_get_access_type(processor_t *proc, int tnum, int id);

enum arch_system_qos_states {
    ARCH_SYSTEM_QOS_SAFE,
    ARCH_SYSTEM_QOS_DANGER,
    ARCH_SYSTEM_QOS_EXTREME_DANGER,
    ARCH_SYSTEM_QOS_MODES
};
typedef enum arch_system_qos_states arch_system_qos_state_t;
void arch_set_system_qos_mode(processor_t *proc, arch_system_qos_state_t qos_state);

int arch_get_cache_configuration_string(processor_t *proc, int buffer_length, char *buffer);

size4u_t arch_get_tcm_size(processor_t * proc);

size4u_t arch_get_thread_mask(processor_t *proc);

/********************************************************************/
/*                Depracated functions from older versions          */
/*                This will simply fatal out with an error msg      */
/********************************************************************/

unsigned int arch_go(processor_t * proc);	/* Keep going for a while */
void arch_nogo(processor_t * proc);	/* Stop going */

unsigned int arch_cycle(processor_t * proc);	/* advance by a single cycle */

/* advance by a single cycle -- faked for cache trace driving */
unsigned int arch_fake_thread_cycle(processor_t * proc, int threadno);
unsigned int arch_fake_cycle(processor_t * proc);

/* Just initialize the processor based on options */
void arch_onlyinit_processor(system_t * S, processor_t * proc,
							 options_t * options);
/* Just alloc the processor */
processor_t *arch_onlyalloc_processor(system_t * S);

options_t *arch_get_options(processor_t * proc);	/* get options structure from processor */

void arch_profiling_control(processor_t * proc, int tnum, int enable);

/* This call is non-intrusive - it does not change any arch-side state.
   The external world can use this call to check what the status will be
   for a subsequent call to arch_cycle.
   The idea is that the external code needs to see if a breakpoint set at
   the address of the next thread's PC will actually commit.
   Replay and exception conditions cause a PC to appear multiple times,
   until it is finally committed. It is desirable, for cycle reproducibility,
   to have the breakpoint reported only when the packet commits.
   This function can be used to query wheter the next-to-execute-tnum
   will indeed commit or not.
   The return value is exactly the same as arch_cycle, which can be used
   to determine the commit/stall status.
*/
unsigned int arch_get_ondeck_cycle_status(processor_t * proc);

/* Return the PC of the thread which is next-up */
unsigned int arch_get_ondeck_vpc(processor_t * proc);

/* Return the TNUM of the thread which is next-up */
int arch_get_ondeck_tnum(processor_t * proc);


// FIXME: This should be deleted. This is a temporary hack till Milind generates code for V3
void arch_model_bug_2280(processor_t * proc);

/* EJP: don't use these, they're dangerous */
void arch_dangerous_timing_on(processor_t *proc);
void arch_dangerous_timing_on_minimal(processor_t *proc);
void arch_dangerous_timing_off(processor_t *proc);
void arch_dangerous_timing_on_doit(processor_t *proc);
void arch_dangerous_timing_transition_on_to_off(processor_t *proc);
int arch_timing_check_for_turnoff(processor_t *proc);

//icache lean check for hit or miss
size8u_t arch_get_cache_lean_pmu_stat(processor_t *proc, int pmu_stat);
size8u_t arch_get_cache_lean_pcycles(processor_t *proc);
void arch_reset_cache_lean(processor_t *proc);

void arch_dma_step(processor_t *proc, int dmanum);
#endif


/* Deprecated world */

/* End deprecated world */

#endif							/* #ifndef _EXTERNAL_API_H_ */
