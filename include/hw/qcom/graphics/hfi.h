/* Qualcomm HFI protocol implementation
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * It has been developed for the Adreno840, on canoe.
 * Versions supported (check the dts file): "qcom,gen8-gmu"
 *
 * As of now, it is the only version supported.
 * Using this device for another driver will have uninteded consequences, and will most likely not work.
 *
 */

#ifndef QEMU_QCOM_HFI_H
#define QEMU_QCOM_HFI_H

#include "qemu/osdep.h"

#define HFI_QUEUE_DEFAULT_CNT 3
#define HFI_QUEUE_DISPATCH_MAX_CNT 14
#define HFI_QUEUE_HDR_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_MAX_CNT)

#define HFI_VALUE_FT_POLICY		100
#define HFI_VALUE_RB_MAX_CMDS		101
#define HFI_VALUE_CTX_MAX_CMDS		102
#define HFI_VALUE_ADDRESS		103
#define HFI_VALUE_MAX_GPU_PERF_INDEX	104
#define HFI_VALUE_MIN_GPU_PERF_INDEX	105
#define HFI_VALUE_MAX_BW_PERF_INDEX	106
#define HFI_VALUE_MIN_BW_PERF_INDEX	107
#define HFI_VALUE_MAX_GPU_THERMAL_INDEX	108
#define HFI_VALUE_GPUCLK		109
#define HFI_VALUE_CLK_TIME		110
#define HFI_VALUE_LOG_GROUP		111
#define HFI_VALUE_LOG_EVENT_ON		112
#define HFI_VALUE_LOG_EVENT_OFF		113
#define HFI_VALUE_DCVS_OBJ		114
#define HFI_VALUE_LM_CS0		115
#define HFI_VALUE_DBG			116
#define HFI_VALUE_BIN_TIME		117
#define HFI_VALUE_LOG_STREAM_ENABLE	119
#define HFI_VALUE_PREEMPT_COUNT		120
#define HFI_VALUE_CONTEXT_QUEUE		121
#define HFI_VALUE_GMU_AB_VOTE		122
#define HFI_VALUE_RB_GPU_QOS		123
#define HFI_VALUE_RB_IB_RULE		124
#define HFI_VALUE_GMU_WARMBOOT		125
#define HFI_VALUE_DCVS_ENABLE		131
#define HFI_VALUE_DCVS_TUNING_PARAM	132
#define HFI_VALUE_RB_GPULEVEL_RULE	133
#define HFI_VALUE_GLOBAL_TOKEN		0xFFFFFFFF

#define HFI_VALUE_MAX 134

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

typedef struct QcomGMUState QcomGMUState;

/**
 * struct hfi_queue_table_header - HFI queue table structure
 * @version: HFI protocol version
 * @size: queue table size in dwords
 * @qhdr0_offset: first queue header offset (dwords) in this table
 * @qhdr_size: queue header size
 * @num_q: number of queues defined in this table
 * @num_active_q: number of active queues
 */
struct hfi_queue_table_header {
	uint32_t version;
	uint32_t size;
	uint32_t qhdr0_offset;
	uint32_t qhdr_size;
	uint32_t num_q;
	uint32_t num_active_q;
} __packed;

/**
 * struct hfi_queue_header - HFI queue header structure
 * @status: active: 1; inactive: 0
 * @start_addr: starting address of the queue in GMU VA space
 * @type: queue type encoded the priority, ID and send/recevie types
 * @queue_size: size of the queue
 * @msg_size: size of the message if each message has fixed size.
 *	Otherwise, 0 means variable size of message in the queue.
 * @read_index: read index of the queue
 * @write_index: write index of the queue
 */
struct hfi_queue_header {
	uint32_t status;
	uint32_t start_addr;
	uint32_t type;
	uint32_t queue_size;
	uint32_t msg_size;
	uint32_t unused0;
	uint32_t unused1;
	uint32_t unused2;
	uint32_t unused3;
	uint32_t unused4;
	uint32_t read_index;
	uint32_t write_index;
} __packed;

struct hfi_queue_table {
	struct hfi_queue_table_header qtbl_hdr;
	struct hfi_queue_header qhdr[HFI_QUEUE_HDR_MAX];
} __packed;

// device-side state of HGI
struct qcom_hfi_state {
    uint32_t msg_seqnum;
    struct hfi_queue_table cached_qtbl;
    uint32_t values[HFI_VALUE_MAX];
};

void qcom_hfi_init(struct qcom_hfi_state* s);
void qcom_hfi_handle(QcomGMUState* gmu);

#endif
