
/* 
 * Qualcomm Android ICC RPMh stuff
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Only provides minimal support, mostly to pass probe checks.
 */

#ifndef QEMU_QCOM_ICC_RPMH_H
#define QEMU_QCOM_ICC_RPMH_H

#include "qemu/osdep.h"

#define QCOM_ICC_BUCKET_0		0
#define QCOM_ICC_BUCKET_1		1
#define QCOM_ICC_BUCKET_2		2
#define QCOM_ICC_BUCKET_3		3
#define QCOM_ICC_BUCKET_4		4
#define QCOM_ICC_NUM_BUCKETS		5

/**
 * enum qcom_icc_bcm_type - The type of aggregation used by a BCM
 *
 * @QCOM_ICC_BCM_TYPE_BW: Aggregates SUM of vote_x and MAX of vote_y
 * @QCOM_ICC_BCM_TYPE_MASK: Aggregates bitwise OR of vote_y
 */
enum qcom_icc_bcm_type {
	QCOM_ICC_BCM_TYPE_BW,
	QCOM_ICC_BCM_TYPE_MASK,
};

/**
 * struct qcom_icc_bcm - Qualcomm specific hardware accelerator nodes
 * known as Bus Clock Manager (BCM)
 * @name: the bcm node name used to fetch BCM data from command db
 * @type: aggregation strategy used by this BCM
 * @addr: address offsets used when voting to RPMH
 * @vote_x: aggregated threshold values, represents sum_bw when @type is bw bcm
 * @vote_y: aggregated threshold values, represents peak_bw when @type is bw bcm
 * @vote_scale: scaling factor for vote_x and vote_y
 * @enable_mask: optional mask to send as vote instead of vote_x/vote_y
 * @perf_mode_mask: mask to OR with enable_mask when QCOM_ICC_TAG_PERF_MODE is set
 * @dirty: flag used to indicate whether the bcm needs to be committed
 * @keepalive: flag used to indicate whether a keepalive is required
 * @keepalive_early: keepalive only prior to sync-state
 * @qos_proxy: flag used to indicate whether a proxy vote needed as part of
 * qos configuration
 * @disabled: flag used to indicate state of bcm node
 * @aux_data: auxiliary data used when calculating threshold values and
 * communicating with RPMh
 * @list: used to link to other bcms when compiling lists for commit
 * @ws_list: used to keep track of bcms that may transition between wake/sleep
 * @num_nodes: total number of @num_nodes
 * @nodes: list of qcom_icc_nodes that this BCM encapsulates
 */
struct qcom_icc_bcm {
	const char *name;
	enum qcom_icc_bcm_type type;
	uint32_t addr;
	uint64_t vote_x[QCOM_ICC_NUM_BUCKETS];
	uint64_t vote_y[QCOM_ICC_NUM_BUCKETS];
	uint64_t vote_scale;
	uint32_t enable_mask;
	uint32_t perf_mode_mask;
	bool dirty;
	bool keepalive;
	bool keepalive_early;
	bool qos_proxy;
	bool disabled;
	// struct bcm_db aux_data;
	// struct list_head list;
	// struct list_head ws_list;
	int voter_idx;
	uint8_t crm_node;
	size_t num_nodes;
	struct qcom_icc_node *nodes[];
};

struct qcom_icc_desc {
	// struct qcom_icc_node * const *nodes;
	// const struct regmap_config *config;
	// size_t num_nodes;
	struct qcom_icc_bcm * const *bcms;
	size_t num_bcms;
	// char **voters;
	// size_t num_voters;
	// bool qos_clks_required;
};

struct qcom_icc_md {
    const char* label;
    const struct qcom_icc_desc* desc;
};

struct qcom_icc_collection {
    const struct qcom_icc_md* icc_mds;
    size_t num_icc_mds;
};

extern const struct qcom_icc_collection canoe_icc_collection;

#endif
