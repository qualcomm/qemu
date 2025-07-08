#include "qemu/osdep.h"
#include "hw/qcom/graphics/gmu.h"
#include "hw/qcom/graphics/hfi.h"
#include "hw/qcom/graphics/gen8_reg.h"

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

#define MSG_HDR_GET_ID(hdr) ((hdr) & 0xFF)
#define MSG_HDR_GET_SIZE(hdr) (((hdr) >> 8) & 0xFF)
#define MSG_HDR_GET_TYPE(hdr) (((hdr) >> 16) & 0xF)
#define MSG_HDR_GET_SEQNUM(hdr) (((hdr) >> 20) & 0xFFF)

#define MSG_HDR_SET_SEQNUM_SIZE(hdr, seqnum, sizedwords) \
	(FIELD_PREP(GENMASK(31, 20), seqnum) | FIELD_PREP(GENMASK(15, 8), sizedwords) | hdr)

#define MSG_HDR_SET_TYPE(hdr, type) \
    (((hdr) & 0xFFFFF) | ((type) << 16))

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

static void print_hdr(struct hfi_queue_header* hdr, size_t idx)
{
    printf("Header %ld\n", idx);
    printf("\tstatus: 0x%x\n", hdr->status);
    printf("\tstart_addr: 0x%x\n", hdr->start_addr);
    printf("\ttype: 0x%x\n", hdr->type);
    printf("\tqueue size: 0x%x\n", hdr->queue_size);
    printf("\tmsg size: 0x%x\n", hdr->msg_size);
    printf("\tread idx: 0x%u\n", hdr->read_index);
    printf("\twrite idx: 0x%u\n", hdr->write_index);
}

static void print_msg(struct qcom_hfi_msg* msg)
{
    printf("Message %u\n", msg->id);
    printf("\traw value: 0x%x\n", msg->raw[0]);
    printf("\tsize_dwords: 0x%x\n", msg->size_dwords);
    printf("\talign_size: 0x%x\n", msg->align_size);
    printf("\tseqnum: %u\n", msg->seqnum);
    printf("\ttype: 0x%x\n", msg->type);
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
    // printf("Reading at gaddr 0x%x 0x%lx bytes\n", qhdr->start_addr + read_index, to_read * sizeof(uint32_t));
    assert(qcom_gmu_gpumem_read(gmu, 0, qhdr->start_addr + (read_index * sizeof(uint32_t)), (char*) buf, to_read * sizeof(uint32_t)));

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

static bool send_ack(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct qcom_hfi_msg* msg, uint32_t* data, uint32_t nb_dwords)
{ 
    struct hfi_queue_header* qhdr = &qtbl->qhdr[HFI_MSG_ID];

    uint32_t size_dwords = 2 + nb_dwords;
    uint32_t align_size = QEMU_ALIGN_UP(size_dwords, SZ_4);

    uint32_t ack_hdr = ACK_MSG_HDR(0); // id is never used, only the type matters
    ack_hdr = MSG_HDR_SET_SEQNUM_SIZE(ack_hdr, gmu->hfi.msg_seqnum++, size_dwords);

    assert(queue_write(gmu, qhdr, &ack_hdr, 1));
    assert(queue_write(gmu, qhdr, &msg->raw[0], 1));
    assert(queue_write(gmu, qhdr, data, nb_dwords));

    if (align_size > size_dwords) {
        qhdr->write_index += align_size - size_dwords;
    }

    return true;
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
            printf("Error: invalid align_size: 0x%x (size_dwords = 0x%x)\n", align_size, size_dwords);
            print_msg(msg);
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

    printf("Get value for type %u and subtype %u\n", get_value_cmd->type, get_value_cmd->subtype);

    assert(send_ack(gmu, qtbl, msg, &gmu->hfi.values[get_value_cmd->type], 1));

    
}

static void qcom_hfi_msg_bw_vote_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_feature_ctrl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_acd_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_set_value(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_clx_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_therm_tbl(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_core_fw_start(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    
}

static void qcom_hfi_msg_start(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    uint32_t data = 0;
    assert(send_ack(gmu, qtbl, msg, &data, 1));
}

static void qcom_hfi_msg_issue_cmd_raw(QcomGMUState* gmu, struct hfi_queue_table* qtbl, struct hfi_queue_header* qhdr, struct qcom_hfi_msg* msg) {
    uint32_t data = 0;
    assert(send_ack(gmu, qtbl, msg, &data, 1));
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
        .handler = qcom_hfi_msg_issue_cmd_raw,
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
            printf("Received Message: %s\n", ops->name);
            print_msg(&msg);
            ops->handler(gmu, qtbl, qhdr, &msg);
        } else {
            printf("Received unknown message with ID %u\n", msg.id);
            print_msg(&msg);
        }

        g_free(msg.raw);
    }

    qcom_hfi_commit_qtbl(gmu, qtbl);

    gmu->regs[GEN8_GMUCX_GMU2HOST_INTR_INFO] |= HFI_IRQ_MSGQ_MASK;

    print_hdr(&qtbl->qhdr[HFI_MSG_ID], HFI_MSG_ID);
}
