#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "hw/qcom/graphics/gmu.h"
#include "hw/qcom/graphics/hfi.h"
#include "hw/qcom/graphics/gen8_reg.h"

#define HFI_PREFIX "HFI: "
#define HFI_LOG(dev, fmt, ...) QDEV_LOG_INFO(dev, HFI_PREFIX fmt __VA_OPT__(,) __VA_ARGS__)
#define HFI_LOG_ERROR(dev, fmt, ...) QDEV_LOG_ERROR(dev, HFI_PREFIX fmt __VA_OPT__(,) __VA_ARGS__)

#define CREATE_MSG_HDR(id, type) \
	(((type) << 16) | ((id) & 0xFF))

#define HFI_MSG_CMD 0 /* V1 and V2 */
#define HFI_MSG_ACK 1 /* V2 only */

#define ACK_MSG_HDR(id) CREATE_MSG_HDR(id, HFI_MSG_ACK)
#define CMD_MSG_HDR(cmd, id) \
	_CMD_MSG_HDR(&(cmd).hdr, id, sizeof(cmd))

#define HFI_IRQ_MSGQ_MASK BIT(0)

#define HFI_CMD_ID 0
#define HFI_MSG_ID 1
#define HFI_DBG_ID 2
#define HFI_DSP_ID_0 3

#define HFI_CMD_IDX 0
#define HFI_MSG_IDX 1
#define HFI_DBG_IDX 2
#define HFI_DSP_IDX_BASE 3
#define HFI_DSP_IDX_0 3

#define HFI_CMD_IDX_LEGACY 0
#define HFI_DSP_IDX_0_LEGACY 1
#define HFI_MSG_IDX_LEGACY 4
#define HFI_DBG_IDX_LEGACY 5

#define HFI_QUEUE_STATUS_DISABLED 0
#define HFI_QUEUE_STATUS_ENABLED 1

#define SZ_1				0x00000001
#define SZ_2				0x00000002
#define SZ_4				0x00000004
#define SZ_8				0x00000008
#define SZ_16				0x00000010
#define SZ_32				0x00000020
#define SZ_64				0x00000040
#define SZ_128				0x00000080
#define SZ_256				0x00000100
#define SZ_512				0x00000200

#define SZ_1K				0x00000400
#define SZ_2K				0x00000800
#define SZ_4K				0x00001000
#define SZ_8K				0x00002000
#define SZ_16K				0x00004000
#define SZ_32K				0x00008000
#define SZ_64K				0x00010000
#define SZ_128K				0x00020000
#define SZ_256K				0x00040000
#define SZ_512K				0x00080000

#define SZ_1M				0x00100000
#define SZ_2M				0x00200000
#define SZ_4M				0x00400000
#define SZ_8M				0x00800000
#define SZ_16M				0x01000000
#define SZ_32M				0x02000000
#define SZ_64M				0x04000000
#define SZ_128M				0x08000000
#define SZ_256M				0x10000000
#define SZ_512M				0x20000000

#define SZ_1G				0x40000000
#define SZ_2G				0x80000000

#define MSG_HDR_GET_ID(hdr) ((hdr) & 0xFF)
#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_TYPE(hdr) (((hdr) >> 16) & 0xF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

#define MSG_HDR_SET_SEQNUM_SIZE(hdr, seqnum, sizedwords) \
	(FIELD_PREP(GENMASK(31, 20), seqnum) | FIELD_PREP(GENMASK(15, 8), sizedwords) | hdr)

#define MSG_HDR_SET_TYPE(hdr, type) \
    (((hdr) & 0xFFFFF) | ((type) << 16))

#define RB_SIZE_IN_BYTES  (32 * 1024) // 32KB per RB
#define RB_SIZE_IN_DWORDS (RB_SIZE_IN_BYTES >> 2)
#define RB_BLOCK_SIZE     4 // In 64-bit quadwords

// generic msg, which needs to be decoded
struct qcom_hfi_msg {
    uint32_t* raw;
    uint32_t size_dwords;
    uint32_t align_size;
    uint32_t seqnum;
    uint32_t type;
    uint32_t id;
};

enum hfi_msg_type {
	H2F_MSG_INIT			= 0,
	H2F_MSG_FW_VER			= 1,
	H2F_MSG_LM_CFG			= 2,
	H2F_MSG_BW_VOTE_TBL		= 3,
	H2F_MSG_PERF_TBL		= 4,
	H2F_MSG_TEST			= 5,
	H2F_MSG_ACD_TBL			= 7,
	H2F_MSG_CLX_TBL			= 8,
	H2F_MSG_THERM_TBL		= 9,
	H2F_MSG_START			= 10,
	H2F_MSG_FEATURE_CTRL		= 11,
	H2F_MSG_GET_VALUE		= 12,
	H2F_MSG_SET_VALUE		= 13,
	H2F_MSG_CORE_FW_START		= 14,
	H2F_MSG_TABLE			= 15,
	F2H_MSG_MEM_ALLOC		= 20,
	H2F_MSG_GX_BW_PERF_VOTE		= 30,
	H2F_MSG_FW_HALT			= 32,
	H2F_MSG_PREPARE_SLUMBER		= 33,
	F2H_MSG_ERR			= 100,
	F2H_MSG_DEBUG			= 101,
	F2H_MSG_LOG_BLOCK		= 102,
	F2H_MSG_GMU_CNTR_REGISTER	= 110,
	F2H_MSG_GMU_CNTR_RELEASE	= 111,
	F2H_MSG_ACK			= 126, /* Deprecated for v2.0*/
	H2F_MSG_ACK			= 127, /* Deprecated for v2.0*/
	H2F_MSG_REGISTER_CONTEXT	= 128,
	H2F_MSG_UNREGISTER_CONTEXT	= 129,
	H2F_MSG_ISSUE_CMD		= 130,
	H2F_MSG_ISSUE_CMD_RAW		= 131,
	H2F_MSG_TS_NOTIFY		= 132,
	F2H_MSG_TS_RETIRE		= 133,
	H2F_MSG_CONTEXT_POINTERS	= 134,
	H2F_MSG_ISSUE_LPAC_CMD_RAW	= 135,
	H2F_MSG_CONTEXT_RULE		= 140, /* AKA constraint */
	H2F_MSG_ISSUE_RECURRING_CMD	= 141,
	F2H_MSG_CONTEXT_BAD		= 150,
	H2F_MSG_HW_FENCE_INFO		= 151,
	H2F_MSG_ISSUE_SYNCOBJ		= 152,
	F2H_MSG_SYNCOBJ_QUERY		= 153,
	H2F_MSG_WARMBOOT_CMD		= 154,
	F2H_MSG_PROCESS_TRACE		= 155,
	F2H_MSG_PLATFORM_LA		= 200,
	H2F_MSG_PLATFORM_LA		= 201,
	F2H_MSG_PLATFORM_WIN		= 202, /* Reserved */
	H2F_MSG_PLATFORM_WIN		= 203, /* Reserved */
	HFI_MAX_ID,
};

enum gmu_ret_type {
	GMU_SUCCESS = 0,
	GMU_ERROR_FATAL,
	GMU_ERROR_MEM_FAIL,
	GMU_ERROR_INVAL_PARAM,
	GMU_ERROR_NULL_PTR,
	GMU_ERROR_OUT_OF_BOUNDS,
	GMU_ERROR_TIMEOUT,
	GMU_ERROR_NOT_SUPPORTED,
	GMU_ERROR_NO_ENTRY,
};

enum hfi_error {
    HFI_SUCCESS = 0,
    HFI_ERROR = 0xffffffff,
};


enum hfi_mem_kind {
    MEMKIND_GENERIC                 = 0,
    MEMKIND_RB                      = 1,
    MEMKIND_SCRATCH                 = 2,
                                        
    MEMKIND_CSW_SMMU_INFO           = 3,
    MEMKIND_CSW_PRIV_NON_SECURE     = 4,
    MEMKIND_CSW_PRIV_SECURE         = 5,
    MEMKIND_CSW_NON_PRIV            = 6,
    MEMKIND_CSW_COUNTER             = 7,
    MEMKIND_CTXTREC_PREEMPT_CNTR    = 8,
    MEMKIND_SYS_LOG                 = 9,
    MEMKIND_CRASH_DUMP              = 10,
    MEMKIND_MMIO_DPU                = 11,
    MEMKIND_MMIO_TCSR               = 12,
    MEMKIND_MMIO_QDSS_STM           = 13,
    MEMKIND_PROFILE                 = 14,
                                         
    MEMKIND_USER_PROFILE_IBS        = 15,
    MEMKIND_CMD_BUFFER              = 16,
    MEMKIND_GPU_BUSY_DATA_BUFFER    = 17,
    MEMKIND_GPU_BUSY_CMD_BUFFER     = 18,
    MEMKIND_MMIO_IPCC               = 19,
    MEMKIND_MMIO_IPCC_AOSS          = 20,
    MEMKIND_CSW_LPAC_PRIV_NON_SECURE= 21,
    MEMKIND_MEMSTORE                = 22,
    MEMKIND_HW_FENCE                = 23,
    MEMKIND_PREEMPT_SCRATCH         = 24,
    MEMKIND_CSW_AQE_BUFFER          = 25,
    NUM_HFI_MEMKINDS,
    MEMKIND_NONE = 0x7FFFFFFF            
};

enum hfi_mem_handle {
    GMU_MEM_HANDLE_GENERIC                 = 1,
    GMU_MEM_HANDLE_SCRATCH                 = 2,
    GMU_MEM_HANDLE_CSW_SMMU_INFO           = 3,
    GMU_MEM_HANDLE_CSW_PRIV_NON_SECURE     = 4,
    GMU_MEM_HANDLE_CSW_PRIV_SECURE         = 5,
    GMU_MEM_HANDLE_CSW_NON_PRIV            = 6,
    GMU_MEM_HANDLE_CSW_COUNTER             = 7,
    GMU_MEM_HANDLE_CTXTREC_PREEMPT_CNTR    = 8,
    GMU_MEM_HANDLE_SYS_LOG                 = 9,
    GMU_MEM_HANDLE_CRASH_DUMP              = 10,
    GMU_MEM_HANDLE_MMIO_DPU                = 11,
    GMU_MEM_HANDLE_MMIO_TCSR               = 12,
    GMU_MEM_HANDLE_MMIO_QDSS_STM           = 13,
    GMU_MEM_HANDLE_PROFILE                 = 14,
    GMU_MEM_HANDLE_USER_PROFILE_IBS        = 15,
    GMU_MEM_HANDLE_CMD_BUFFER              = 16,
    GMU_MEM_HANDLE_GPU_BUSY_DATA_BUFFER    = 17,
    GMU_MEM_HANDLE_GPU_BUSY_CMD_BUFFER     = 18,
    GMU_MEM_HANDLE_MMIO_IPCC               = 19,
    GMU_MEM_HANDLE_MMIO_IPCC_AOSS          = 20,
    GMU_MEM_HANDLE_CSW_LPAC_PRIV_NON_SECURE= 21,
    GMU_MEM_HANDLE_MEMSTORE                = 22,
    GMU_MEM_HANDLE_HW_FENCE                = 23,
    GMU_MEM_HANDLE_PREEMPT_SCRATCH         = 24,
    GMU_MEM_HANDLE_CSW_AQE_BUFFER          = 25,
    GMU_MEM_HANDLE_RB0                     = 26,
    GMU_MEM_HANDLE_RB1                     = 27,
    GMU_MEM_HANDLE_RB2                     = 28,
    GMU_MEM_HANDLE_RB3                     = 29,
    GMU_MEM_HANDLE_LPAC_RB                 = 30,
    MAX_GMU_MEM_HANDLE,
};

#define MEMFLAG_GFX_ACC                 BIT(0)
#define MEMFLAG_GFX_PRIV                BIT(1)
#define MEMFLAG_GFX_WRITEABLE           BIT(2)
#define MEMFLAG_GMU_ACC                 BIT(3)
#define MEMFLAG_GMU_PRIV                BIT(4)
#define MEMFLAG_GMU_WRITEABLE           BIT(5)
#define MEMFLAG_GMU_BUFFERABLE          BIT(6)
#define MEMFLAG_GMU_CACHEABLE           BIT(7)
#define MEMFLAG_HOST_ACC                BIT(8)
#define MEMFLAG_HOST_INIT               BIT(9)
#define MEMFLAG_GMU_EXECUTABLE          BIT(10)
#define MEMFLAG_GVM_WRITEABLE           BIT(11)
#define MEMFLAG_GFX_SECURE              BIT(12)

#define MEMKIND_GENERIC_FLAGS \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_RB_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC | MEMFLAG_HOST_INIT

#define MEMKIND_SCRATCH_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC | MEMFLAG_HOST_INIT

#define MEMKIND_CSW_SMMU_INFO_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_CSW_PRIV_NON_SECURE_FLAGS\
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC | MEMFLAG_HOST_INIT

#define MEMKIND_CSW_PRIV_SECURE_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | MEMFLAG_GFX_SECURE

#define MEMKIND_CSW_NON_PRIV_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_CSW_COUNTER_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_CTXTREC_PREEMPT_CNTR_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_SYS_LOG_FLAGS \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_CRASH_DUMP_FLAGS \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_MMIO_DPU_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_MMIO_TCSR_FLAGS \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_MMIO_QDSS_STM_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_PROFILE_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC | MEMFLAG_HOST_INIT

#define MEMKIND_USER_PROFILE_IBS_FLAGS \
    MEMFLAG_GFX_ACC | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC | MEMFLAG_HOST_INIT

#define MEMKIND_CMD_BUFFER_FLAGS \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_GPU_BUSY_DATA_BUFFER_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_GPU_BUSY_CMD_BUFFER_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_MMIO_IPCC_FLAGS \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_CSW_LPAC_PRIV_NON_SECURE_FLAGS \
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_MEMSTORE_FLAGS \
    MEMFLAG_GFX_ACC | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_HW_FENCE_FLAGS \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC

#define MEMKIND_CSW_AQE_BUFFER_FLAGS\
    MEMFLAG_GFX_ACC | MEMFLAG_GFX_PRIV | MEMFLAG_GFX_WRITEABLE | \
    MEMFLAG_GMU_ACC | MEMFLAG_GMU_PRIV | MEMFLAG_GMU_WRITEABLE | MEMFLAG_GMU_BUFFERABLE | \
    MEMFLAG_HOST_ACC | MEMFLAG_HOST_INIT

struct hfi_mem_desc {
    enum hfi_mem_kind mem_kind;
    enum hfi_mem_handle mem_handle;
    uint32_t size;
    uint32_t flags;
    uint32_t align_va;
    uint32_t align_sz;
};

static struct hfi_mem_desc mem_descs[] = {
    {
        .mem_kind = MEMKIND_SCRATCH,
        .mem_handle = GMU_MEM_HANDLE_SCRATCH,
        .size = 4096,
        .flags = MEMKIND_SCRATCH_FLAGS,
    },
    {
        .mem_kind = MEMKIND_CMD_BUFFER,
        .mem_handle = GMU_MEM_HANDLE_CMD_BUFFER,
        .size = RB_SIZE_IN_BYTES,
        .flags = MEMKIND_CMD_BUFFER_FLAGS,
    },
    {
        .mem_kind = MEMKIND_RB,
        .mem_handle = GMU_MEM_HANDLE_RB0,
        .size = RB_SIZE_IN_BYTES,
        .flags = MEMKIND_RB_FLAGS,
    },
    // {
    //     .mem_kind = MEMKIND_CSW_PRIV_NON_SECURE,
    //     .mem_handle = GMU_MEM_HANDLE_CSW_PRIV_NON_SECURE,
    //     .size = 13536 * SZ_1K * 4, // not the "official" size, may be a problem?
    //     .flags = MEMKIND_CSW_PRIV_NON_SECURE_FLAGS,
    // }
};

/*
 * === reply types ===
 *
 * All reply types should start with a header and an ack field.
 *
 */

struct hfi_reply {
    uint32_t header;
    uint32_t ack;
    uint32_t error_code;
} __packed;

struct hfi_reply_get {
    uint32_t header;
    uint32_t ack;
    uint32_t data;
} __packed;

/* H2F */
struct hfi_get_value_cmd {
	uint32_t hdr;
	uint32_t type;
	uint32_t subtype;
} __packed;

/* H2F */
struct hfi_start_cmd {
	uint32_t hdr;
} __packed;

struct hfi_mem_alloc_desc {
	uint64_t gpu_addr;
	uint32_t flags;
	uint32_t mem_kind;
	uint32_t host_mem_handle;
	uint32_t gmu_mem_handle;
	uint32_t gmu_addr;
	uint32_t size; /* Bytes */
	/**
	 * @align: bits[0:7] specify alignment requirement of the GMU VA specified as a power of
	 * two. bits[8:15] specify alignment requirement for the size of the GMU mapping. For
	 * example, a decimal value of 20 = (1 << 20) = 1 MB alignment
	 */
	uint32_t align;
} __packed;

struct hfi_mem_alloc_cmd {
	uint32_t hdr;
	uint32_t version;
	struct hfi_mem_alloc_desc desc;
} __packed;

/* H2F */
struct hfi_mem_alloc_reply_cmd {
	uint32_t hdr;
	uint32_t req_hdr;
	struct hfi_mem_alloc_desc desc;
} __packed;

struct qcom_hfi_cmd_handler {
    struct qcom_hfi_msg msg;
};

typedef void (*msg_handler)(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg);

struct qcom_hfi_ops {
    const char* name;
    msg_handler handler;
};

static uint32_t default_values[HFI_VALUE_MAX] = {
    [HFI_VALUE_GMU_AB_VOTE] = 1,
};

static inline int _CMD_MSG_HDR(uint32_t *hdr, int id, size_t size)
{
	*hdr = CREATE_MSG_HDR(id, HFI_MSG_CMD);
	return 0;
}

// static void print_tbl_hdr(struct hfi_queue_table_header* hdr)
// {
//     printf("Table header:\n");
//     printf("\tversion: 0x%x\n", hdr->version);
//     printf("\tsize: 0x%x\n", hdr->size);
//     printf("\tqhdr0_offset: 0x%x\n", hdr->qhdr0_offset);
//     printf("\tqhdr_size: 0x%x\n", hdr->qhdr_size);
//     printf("\tnum_q: 0x%x\n", hdr->num_q);
//     printf("\tnum_active_q: 0x%x\n", hdr->num_active_q);
//     printf("\n");
// }

static void print_hdr(QcomGMUState* s, struct hfi_queue_header* hdr, size_t idx)
{
    HFI_LOG(s, "Header %ld\n", idx);
    HFI_LOG(s, "\tstatus: 0x%x\n", hdr->status);
    HFI_LOG(s, "\tstart_addr: 0x%x\n", hdr->start_addr);
    HFI_LOG(s, "\ttype: 0x%x\n", hdr->type);
    HFI_LOG(s, "\tqueue size: 0x%x\n", hdr->queue_size);
    HFI_LOG(s, "\tmsg size: 0x%x\n", hdr->msg_size);
    HFI_LOG(s, "\tread idx: 0x%u\n", hdr->read_index);
    HFI_LOG(s, "\twrite idx: 0x%u\n", hdr->write_index);
}

static void print_msg(QcomGMUState* s, struct qcom_hfi_msg* msg)
{
    HFI_LOG(s, "Message %u\n", msg->id);
    HFI_LOG(s, "\traw value: 0x%x\n", msg->raw[0]);
    HFI_LOG(s, "\tsize_dwords: 0x%x\n", msg->size_dwords);
    HFI_LOG(s, "\talign_size: 0x%x\n", msg->align_size);
    HFI_LOG(s, "\tseqnum: %u\n", msg->seqnum);
    HFI_LOG(s, "\ttype: 0x%x\n", msg->type);
}

static void qcom_hfi_fetch_qtbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl)
{
    assert(gmu->regs[GEN8_GMUCX_HFI_QTBL_INFO] == 1);

    gpuaddr tbl_hdr_gaddr = gmu->regs[GEN8_GMUCX_HFI_QTBL_ADDR];
    // printf("Reading HFI table header @ gpuaddr 0x%lx\n", tbl_hdr_gaddr);

    assert(qcom_gmu_gpumem_read(gmu, 0, tbl_hdr_gaddr, (char*) qtbl, sizeof(struct hfi_queue_table)));
}

// TODO: be a bit more granular
static void qcom_hfi_commit_qtbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl)
{
    gpuaddr tbl_hdr_gaddr = gmu->regs[GEN8_GMUCX_HFI_QTBL_ADDR];
    assert(qcom_gmu_gpumem_write(gmu, 0, tbl_hdr_gaddr, (char*) qtbl, sizeof(struct hfi_queue_table)));
}

// buf must be allocated and of sufficient size.
// buf will be overwritten with the read result.
static bool queue_read(QcomGMUState* gmu, struct hfi_queue_header* qhdr, uint32_t* buf, uint32_t nb_words)
{
    uint32_t read_index = qhdr->read_index;
    uint32_t write_index = qhdr->write_index;

    uint32_t available_words = (write_index >= read_index) ? write_index - read_index : qhdr->queue_size - read_index + write_index;

    // printf("read_index: %d | write_index: %d | available_words: %d | nb_words: %d\n", read_index, write_index, available_words, nb_words);
    if (available_words < nb_words) {
        return false;
    }

    uint32_t high_write_index = MIN(read_index + nb_words, qhdr->queue_size);
    uint32_t to_read = high_write_index - read_index;
    if (!qcom_gmu_gpumem_read(gmu, 0, qhdr->start_addr + (read_index * sizeof(uint32_t)), (char*) buf, to_read * sizeof(uint32_t))) {
        HFI_LOG_ERROR(gmu, "Error while reading GPU memory at gaddr 0x%lx of 0x%lx bytes\n", qhdr->start_addr + (read_index * sizeof(uint32_t)), to_read * sizeof(uint32_t));
        HFI_LOG_ERROR(gmu, "Read index: 0x%x\n", read_index);
        HFI_LOG_ERROR(gmu, "Write index: 0x%x\n", write_index);
        HFI_LOG_ERROR(gmu, "queue size: 0x%x\n", qhdr->queue_size);

        exit(1);
    }

    if (high_write_index == qhdr->queue_size) {
        // write_index is lower than read_index, we need to continue reading at the beginning of the queue.
        uint32_t remaining_to_read = nb_words - to_read;
        assert(qcom_gmu_gpumem_read(gmu, 0, qhdr->start_addr, (char*) (buf + to_read), remaining_to_read * sizeof(uint32_t)));
    }

    qhdr->read_index = (qhdr->read_index + nb_words) % qhdr->queue_size;

    return true;
}

static bool queue_write(QcomGMUState* gmu, struct hfi_queue_header* qhdr, uint32_t* buf, uint32_t nb_words)
{
    uint32_t read_index = qhdr->read_index;
    uint32_t write_index = qhdr->write_index;

    uint32_t available_words = (write_index >= read_index) ? qhdr->queue_size - write_index + read_index : read_index - write_index;

    // printf("read_index: %d | write_index: %d | available_words: %d | nb_words: %d\n", read_index, write_index, available_words, nb_words);
    if (available_words < nb_words) {
        return false;
    }

    uint32_t high_write_index = MIN(write_index + nb_words, qhdr->queue_size);
    uint32_t to_write = high_write_index - write_index;
    // printf("Reading at gaddr 0x%x 0x%lx bytes\n", qhdr->start_addr + read_index, to_read * sizeof(uint32_t));
    assert(qcom_gmu_gpumem_write(gmu, 0, qhdr->start_addr + (write_index * sizeof(uint32_t)), (char*) buf, to_write * sizeof(uint32_t)));

    if (high_write_index == qhdr->queue_size) {
        // write_index is lower than read_index, we need to continue reading at the beginning of the queue.
        uint32_t remaining_to_write = nb_words - to_write;
        assert(qcom_gmu_gpumem_write(gmu, 0, qhdr->start_addr, (char*) (buf + to_write), remaining_to_write * sizeof(uint32_t)));
    }

    qhdr->write_index = (qhdr->write_index + nb_words) % qhdr->queue_size;

    return true;
}

static bool queue_write_aligned(QcomGMUState* gmu, struct hfi_queue_header* qhdr, void* buf, uint32_t nb_words)
{
    uint32_t nb_words_aligned = QEMU_ALIGN_UP(nb_words, SZ_4);

    if (!queue_write(gmu, qhdr, buf, nb_words)) {
        return false;
    }

    if (nb_words_aligned > nb_words) {
        qhdr->write_index += nb_words_aligned - nb_words;
    }

    return true;
}

static bool send_reply(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct qcom_hfi_msg* msg, void* reply_ptr, uint32_t reply_size_bytes)
{ 
    struct hfi_queue_header* qhdr = &qtbl->qhdr[HFI_MSG_ID];

    uint32_t* reply = (uint32_t*) reply_ptr;

    assert(reply_size_bytes % 4 == 0);
    uint32_t nb_words = reply_size_bytes >> 2;

    assert(nb_words >= 3);

    reply[0] = ACK_MSG_HDR(0); // id is never used, only the type matters
    reply[0] = MSG_HDR_SET_SEQNUM_SIZE(reply[0], gmu->hfi.msg_seqnum++, nb_words);
    reply[1] = msg->raw[0];

    return queue_write_aligned(gmu, qhdr, reply_ptr, nb_words);
}

static bool send_ack_simple(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct qcom_hfi_msg* msg, enum hfi_error error_code)
{
    struct hfi_reply reply = { 0 };
    reply.error_code = HFI_SUCCESS;

    return send_reply(gmu, qtbl, msg, &reply, sizeof(reply));
}

static bool send_ack_get(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct qcom_hfi_msg* msg, uint32_t value)
{
    struct hfi_reply_get reply = { 0 };
    reply.data = value;

    return send_reply(gmu, qtbl, msg, &reply, sizeof(reply));
}

static bool send_mem_alloc(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_mem_desc* desc)
{
    struct hfi_queue_header* qhdr = &qtbl->qhdr[HFI_MSG_ACK];

    struct hfi_mem_alloc_cmd mem_alloc = { 0 };

    assert(sizeof(mem_alloc) % 4 == 0);
    uint32_t nb_words = sizeof(mem_alloc) >> 2;

    mem_alloc.hdr = CREATE_MSG_HDR(F2H_MSG_MEM_ALLOC, HFI_MSG_CMD);
    mem_alloc.hdr = MSG_HDR_SET_SEQNUM_SIZE(mem_alloc.hdr, gmu->hfi.msg_seqnum++, nb_words);

    HFI_LOG(gmu, "mem_alloc hdr: 0x%x\n", mem_alloc.hdr);

    mem_alloc.version = 1;

	mem_alloc.desc.flags = desc->flags;
	mem_alloc.desc.mem_kind = desc->mem_kind;
	mem_alloc.desc.gmu_mem_handle = desc->mem_handle;
	mem_alloc.desc.size = desc->size;
	mem_alloc.desc.align = (desc->align_va & 0xFF) | ((desc->align_sz & 0xFF) << 8);

    return queue_write_aligned(gmu, qhdr, &mem_alloc, nb_words);
}

static bool qcom_hfi_get_pending_msg(QcomGMUState* gmu, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg)
{
    uint32_t hdr;
    if (queue_read(gmu, qhdr, &hdr, 1)) {
        uint32_t size_dwords = MSG_HDR_GET_SIZE(hdr);
        uint32_t seqnum = MSG_HDR_GET_SEQNUM(hdr);
        uint32_t type = MSG_HDR_GET_TYPE(hdr);
        uint32_t id = MSG_HDR_GET_ID(hdr);
        uint32_t align_size = QEMU_ALIGN_UP(size_dwords, SZ_4);

        msg->raw = g_new(uint32_t, align_size);
        msg->size_dwords = size_dwords;
        msg->align_size = align_size;
        msg->seqnum = seqnum;
        msg->type = type;
        msg->id = id;

        // remove one, since we already read the 
        if (align_size == 0) {
            HFI_LOG_ERROR(gmu, "invalid align_size: 0x%x (size_dwords = 0x%x)\n", align_size, size_dwords);
            print_msg(gmu, msg);
            exit(1);
        }

        msg->raw[0] = hdr;
        assert(queue_read(gmu, qhdr, msg->raw + 1, align_size - 1));

        return true;
    }

    return false;
}

static void qcom_hfi_msg_table(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_get_value(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    struct hfi_get_value_cmd* get_value_cmd = (struct hfi_get_value_cmd*) msg->raw;

    HFI_LOG(gmu, "Get value for type %u and subtype %u\n", get_value_cmd->type, get_value_cmd->subtype);

    assert(send_ack_get(gmu, qtbl, msg, gmu->hfi.values[get_value_cmd->type]));
}

static void qcom_hfi_msg_bw_vote_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_feature_ctrl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_acd_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_set_value(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    assert(send_ack_simple(gmu, qtbl, msg, HFI_SUCCESS));
}

static void qcom_hfi_msg_clx_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_therm_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_core_fw_start(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_simple_ack(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    assert(send_ack_simple(gmu, qtbl, msg, HFI_SUCCESS));
}

static void qcom_hfi_msg_start(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    // allocate memory from descriptors
    // TODO: take feature flags into account
    for (size_t i = 0; i < ARRAY_SIZE(mem_descs); ++i) {
        HFI_LOG(gmu, "allocating memory...\n");
        struct hfi_mem_desc* desc = &mem_descs[i];
        assert(send_mem_alloc(gmu, qtbl, desc));
    }

    assert(send_ack_simple(gmu, qtbl, msg, HFI_SUCCESS));
}

static struct qcom_hfi_ops hfi_ops[] = {
    [H2F_MSG_TABLE] = {
        .name = "MSG_TABLE",
        .handler = qcom_hfi_msg_table,
    },
    [H2F_MSG_GET_VALUE] = {
        .name = "MSG_GET_VALUE",
        .handler = qcom_hfi_msg_get_value,
    },
    [H2F_MSG_BW_VOTE_TBL] = {
        .name = "MSG_BW_VOTE_TBL",
        .handler = qcom_hfi_msg_bw_vote_tbl,
    },
    [H2F_MSG_FEATURE_CTRL] = {
        .name = "MSG_FEATURE_CTRL",
        .handler = qcom_hfi_msg_feature_ctrl,
    },
    [H2F_MSG_ACD_TBL] = {
        .name = "MSG_ACD_TBL",
        .handler = qcom_hfi_msg_acd_tbl,
    },
    [H2F_MSG_SET_VALUE] = {
        .name = "MSG_SET_VALUE",
        .handler = qcom_hfi_msg_set_value,
    },
    [H2F_MSG_CLX_TBL] = {
        .name = "MSG_CLX_TBL",
        .handler = qcom_hfi_msg_clx_tbl,
    },
    [H2F_MSG_THERM_TBL] = {
        .name = "MSG_THERM_TBL",
        .handler = qcom_hfi_msg_therm_tbl,
    },
    [H2F_MSG_CORE_FW_START] = {
        .name = "MSG_CORE_FW_START",
        .handler = qcom_hfi_msg_core_fw_start,
    },
    [H2F_MSG_START] = {
        .name = "MSG_START",
        .handler = qcom_hfi_msg_start,
    },
    [H2F_MSG_ISSUE_CMD_RAW] = {
        .name = "MSG_ISSUE_CMD_RAW",
        .handler = qcom_hfi_msg_simple_ack,
    },
    [H2F_MSG_PREPARE_SLUMBER] = {
        .name = "MSG_ISSUE_CMD_RAW",
        .handler = qcom_hfi_msg_simple_ack,
    },

    // invalid entry
    [HFI_MAX_ID] = {
        .name = "MAX ID - Invalid",
        .handler = NULL,
    },
};

void qcom_hfi_init(struct qcom_hfi_state* s)
{
    assert(sizeof(default_values) == sizeof(s->values));
    memcpy(&s->values, default_values, sizeof(default_values));
}

void qcom_hfi_handle(QcomGMUState* gmu)
{
    struct hfi_queue_table* qtbl = &gmu->hfi.cached_qtbl;

    qcom_hfi_fetch_qtbl(gmu, qtbl);

    // print_tbl_hdr(&qtbl->qtbl_hdr);

    struct qcom_hfi_msg msg;
    struct hfi_queue_header* qhdr = &qtbl->qhdr[HFI_CMD_ID];
    while(qcom_hfi_get_pending_msg(gmu, qhdr, &msg)) {
        assert(msg.id < HFI_MAX_ID);

        struct qcom_hfi_ops* ops = &hfi_ops[msg.id];
        if (ops->handler) {
            HFI_LOG(gmu, "Received Message: %s\n", ops->name);
            print_msg(gmu, &msg);
            ops->handler(gmu, qtbl, qhdr, &msg);
        } else {
            HFI_LOG(gmu, "Received unknown message with ID %u\n", msg.id);
            print_msg(gmu, &msg);
        }

        g_free(msg.raw);
    }

    qcom_hfi_commit_qtbl(gmu, qtbl);

    gmu->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO] |= HFI_IRQ_MSGQ_MASK;

    print_hdr(gmu, &qtbl->qhdr[HFI_MSG_ID], HFI_MSG_ID);
}
