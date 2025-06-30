#include "qemu/osdep.h"
#include "hw/qcom/icc-rpmh.h"

#define MASTER_GPU_TCU				0
#define MASTER_SYS_TCU				1
#define MASTER_APPSS_PROC				2
#define MASTER_LLCC				3
#define MASTER_QSPI_0				4
#define MASTER_QUP_2				5
#define MASTER_QUP_3				6
#define MASTER_QUP_4				7
#define MASTER_A1NOC_SNOC				8
#define MASTER_A2NOC_SNOC				9
#define MASTER_APSS_NOC				10
#define MASTER_CAMNOC_HF				11
#define MASTER_CAMNOC_NRT_ICP_SF				12
#define MASTER_CAMNOC_RT_CDM_SF				13
#define MASTER_CAMNOC_SF				14
#define MASTER_CNOC_SNOC				15
#define MASTER_GEM_NOC_CNOC				16
#define MASTER_GEM_NOC_PCIE_SNOC				17
#define MASTER_GFX3D				18
#define MASTER_LPASS_GEM_NOC				19
#define MASTER_LPASS_LPINOC				20
#define MASTER_LPIAON_NOC				21
#define MASTER_LPASS_PROC				22
#define MASTER_MDP				23
#define MASTER_MSS_PROC				24
#define MASTER_MDSS_DCP				25
#define MASTER_MNOC_HF_MEM_NOC				26
#define MASTER_MNOC_SF_MEM_NOC				27
#define MASTER_CDSP_PROC				28
#define MASTER_COMPUTE_NOC				29
#define MASTER_ANOC_PCIE_GEM_NOC				30
#define MASTER_QPACE				31
#define MASTER_SNOC_SF_MEM_NOC				32
#define MASTER_CDSP_HCP				33
#define MASTER_VIDEO_CV_PROC				34
#define MASTER_VIDEO_EVA				35
#define MASTER_VIDEO_MVP				36
#define MASTER_VIDEO_V_PROC				37
#define MASTER_WLAN_Q6				38
#define MASTER_CNOC_CFG				39
#define MASTER_CNOC_MNOC_CFG				40
#define MASTER_PCIE_ANOC_CFG				41
#define MASTER_QUP_CORE_0				42
#define MASTER_QUP_CORE_1				43
#define MASTER_QUP_CORE_2				44
#define MASTER_QUP_CORE_3				45
#define MASTER_QUP_CORE_4				46
#define MASTER_CRYPTO				47
#define MASTER_IPA				48
#define MASTER_QUP_1				49
#define MASTER_SOCCP_AGGR_NOC				50
#define MASTER_SP				51
#define MASTER_GIC				52
#define MASTER_PCIE_0				53
#define MASTER_QDSS_ETR				54
#define MASTER_QDSS_ETR_1				55
#define MASTER_SDCC_2				56
#define MASTER_SDCC_4				57
#define MASTER_UFS_MEM				58
#define MASTER_USB3				59
#define SLAVE_EBI1				512
#define SLAVE_AHB2PHY_SOUTH				513
#define SLAVE_AHB2PHY_NORTH				514
#define SLAVE_AOSS				515
#define SLAVE_CAMERA_CFG				516
#define SLAVE_CLK_CTL				517
#define SLAVE_CRYPTO_0_CFG				518
#define SLAVE_DISPLAY_CFG				519
#define SLAVE_EVA_CFG				520
#define SLAVE_GFX3D_CFG				521
#define SLAVE_I2C				522
#define SLAVE_I3C_IBI0_CFG				523
#define SLAVE_I3C_IBI1_CFG				524
#define SLAVE_IMEM_CFG				525
#define SLAVE_IPA_CFG				526
#define SLAVE_IPC_ROUTER_CFG				527
#define SLAVE_IPC_ROUTER_FENCE				528
#define SLAVE_CNOC_MSS				529
#define SLAVE_PCIE_CFG				530
#define SLAVE_PRNG				531
#define SLAVE_QDSS_CFG				532
#define SLAVE_QSPI_0				533
#define SLAVE_QUP_1				534
#define SLAVE_QUP_2				535
#define SLAVE_QUP_3				536
#define SLAVE_QUP_4				537
#define SLAVE_SDCC_2				538
#define SLAVE_SDCC_4				539
#define SLAVE_SOCCP				540
#define SLAVE_SPSS_CFG				541
#define SLAVE_TCSR				542
#define SLAVE_TLMM				543
#define SLAVE_TME_CFG				544
#define SLAVE_UFS_MEM_CFG				545
#define SLAVE_USB3				546
#define SLAVE_VENUS_CFG				547
#define SLAVE_VSENSE_CTRL_CFG				548
#define SLAVE_A1NOC_SNOC				549
#define SLAVE_A2NOC_SNOC				550
#define SLAVE_APPSS				551
#define SLAVE_GEM_NOC_CNOC				552
#define SLAVE_SNOC_GEM_NOC_SF				553
#define SLAVE_LLCC				554
#define SLAVE_LPASS_GEM_NOC				555
#define SLAVE_LPIAON_NOC_LPASS_AG_NOC				556
#define SLAVE_LPICX_NOC_LPIAON_NOC				557
#define SLAVE_MNOC_HF_MEM_NOC				558
#define SLAVE_MNOC_SF_MEM_NOC				559
#define SLAVE_CDSP_MEM_NOC				560
#define SLAVE_MEM_NOC_PCIE_SNOC				561
#define SLAVE_ANOC_PCIE_GEM_NOC				562
#define SLAVE_CNOC_CFG				563
#define SLAVE_DDRSS_CFG				564
#define SLAVE_CNOC_MNOC_CFG				565
#define SLAVE_PCIE_ANOC_CFG				566
#define SLAVE_QUP_CORE_0				567
#define SLAVE_QUP_CORE_1				568
#define SLAVE_QUP_CORE_2				569
#define SLAVE_QUP_CORE_3				570
#define SLAVE_QUP_CORE_4				571
#define SLAVE_BOOT_IMEM				572
#define SLAVE_IMEM				573
#define SLAVE_SERVICE_MNOC				574
#define SLAVE_SERVICE_PCIE_ANOC				575
#define SLAVE_PCIE_0				576
#define SLAVE_QDSS_STM				577
#define SLAVE_TCU				578
#define MASTER_LLCC_CAM_IFE_0				1000
#define MASTER_CAMNOC_HF_CAM_IFE_0				1001
#define MASTER_CAMNOC_NRT_ICP_SF_CAM_IFE_0				1002
#define MASTER_CAMNOC_RT_CDM_SF_CAM_IFE_0				1003
#define MASTER_CAMNOC_SF_CAM_IFE_0				1004
#define MASTER_MNOC_HF_MEM_NOC_CAM_IFE_0				1005
#define MASTER_MNOC_SF_MEM_NOC_CAM_IFE_0				1006
#define SLAVE_EBI1_CAM_IFE_0				1512
#define SLAVE_LLCC_CAM_IFE_0				1513
#define SLAVE_MNOC_HF_MEM_NOC_CAM_IFE_0				1514
#define SLAVE_MNOC_SF_MEM_NOC_CAM_IFE_0				1515
#define MASTER_LLCC_CAM_IFE_1				2000
#define MASTER_CAMNOC_HF_CAM_IFE_1				2001
#define MASTER_CAMNOC_NRT_ICP_SF_CAM_IFE_1				2002
#define MASTER_CAMNOC_RT_CDM_SF_CAM_IFE_1				2003
#define MASTER_CAMNOC_SF_CAM_IFE_1				2004
#define MASTER_MNOC_HF_MEM_NOC_CAM_IFE_1				2005
#define MASTER_MNOC_SF_MEM_NOC_CAM_IFE_1				2006
#define SLAVE_EBI1_CAM_IFE_1				2512
#define SLAVE_LLCC_CAM_IFE_1				2513
#define SLAVE_MNOC_HF_MEM_NOC_CAM_IFE_1				2514
#define SLAVE_MNOC_SF_MEM_NOC_CAM_IFE_1				2515
#define MASTER_LLCC_CAM_IFE_2				3000
#define MASTER_CAMNOC_HF_CAM_IFE_2				3001
#define MASTER_CAMNOC_NRT_ICP_SF_CAM_IFE_2				3002
#define MASTER_CAMNOC_RT_CDM_SF_CAM_IFE_2				3003
#define MASTER_CAMNOC_SF_CAM_IFE_2				3004
#define MASTER_MNOC_HF_MEM_NOC_CAM_IFE_2				3005
#define MASTER_MNOC_SF_MEM_NOC_CAM_IFE_2				3006
#define SLAVE_EBI1_CAM_IFE_2				3512
#define SLAVE_LLCC_CAM_IFE_2				3513
#define SLAVE_MNOC_HF_MEM_NOC_CAM_IFE_2				3514
#define SLAVE_MNOC_SF_MEM_NOC_CAM_IFE_2				3515
#define MASTER_IPA_CORE_PCIE_CRM_HW_0				4000
#define MASTER_LLCC_PCIE_CRM_HW_0				4001
#define MASTER_ANOC_PCIE_GEM_NOC_PCIE_CRM_HW_0				4002
#define MASTER_PCIE_0_PCIE_CRM_HW_0				4003
#define SLAVE_EBI1_PCIE_CRM_HW_0				4512
#define SLAVE_IPA_CORE_PCIE_CRM_HW_0				4513
#define SLAVE_LLCC_PCIE_CRM_HW_0				4514
#define SLAVE_ANOC_PCIE_GEM_NOC_PCIE_CRM_HW_0				4515
#define MASTER_LLCC_DISP_CRM_SW_0				5000
#define MASTER_MDP_DISP_CRM_SW_0				5001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_SW_0				5002
#define SLAVE_EBI1_DISP_CRM_SW_0				5512
#define SLAVE_LLCC_DISP_CRM_SW_0				5513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_SW_0				5514
#define MASTER_LLCC_DISP_CRM_HW_0				6000
#define MASTER_MDP_DISP_CRM_HW_0				6001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_0				6002
#define SLAVE_EBI1_DISP_CRM_HW_0				6512
#define SLAVE_LLCC_DISP_CRM_HW_0				6513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_0				6514
#define MASTER_LLCC_DISP_CRM_HW_1				7000
#define MASTER_MDP_DISP_CRM_HW_1				7001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_1				7002
#define SLAVE_EBI1_DISP_CRM_HW_1				7512
#define SLAVE_LLCC_DISP_CRM_HW_1				7513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_1				7514
#define MASTER_LLCC_DISP_CRM_HW_2				8000
#define MASTER_MDP_DISP_CRM_HW_2				8001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_2				8002
#define SLAVE_EBI1_DISP_CRM_HW_2				8512
#define SLAVE_LLCC_DISP_CRM_HW_2				8513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_2				8514
#define MASTER_LLCC_DISP_CRM_HW_3				9000
#define MASTER_MDP_DISP_CRM_HW_3				9001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_3				9002
#define SLAVE_EBI1_DISP_CRM_HW_3				9512
#define SLAVE_LLCC_DISP_CRM_HW_3				9513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_3				9514
#define MASTER_LLCC_DISP_CRM_HW_4				10000
#define MASTER_MDP_DISP_CRM_HW_4				10001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_4				10002
#define SLAVE_EBI1_DISP_CRM_HW_4				10512
#define SLAVE_LLCC_DISP_CRM_HW_4				10513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_4				10514
#define MASTER_LLCC_DISP_CRM_HW_5				11000
#define MASTER_MDP_DISP_CRM_HW_5				11001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_5				11002
#define SLAVE_EBI1_DISP_CRM_HW_5				11512
#define SLAVE_LLCC_DISP_CRM_HW_5				11513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_5				11514
#define MASTER_LLCC_DISP_CRM_HW_6				12000
#define MASTER_MDP_DISP_CRM_HW_6				12001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_6				12002
#define SLAVE_EBI1_DISP_CRM_HW_6				12512
#define SLAVE_LLCC_DISP_CRM_HW_6				12513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_6				12514
#define MASTER_LLCC_DISP_CRM_HW_7				13000
#define MASTER_MDP_DISP_CRM_HW_7				13001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_7				13002
#define SLAVE_EBI1_DISP_CRM_HW_7				13512
#define SLAVE_LLCC_DISP_CRM_HW_7				13513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_7				13514
#define MASTER_LLCC_DISP_CRM_HW_8				14000
#define MASTER_MDP_DISP_CRM_HW_8				14001
#define MASTER_MNOC_HF_MEM_NOC_DISP_CRM_HW_8				14002
#define SLAVE_EBI1_DISP_CRM_HW_8				14512
#define SLAVE_LLCC_DISP_CRM_HW_8				14513
#define SLAVE_MNOC_HF_MEM_NOC_DISP_CRM_HW_8				14514

/**
 * enum crm_drv_type:       CRM DRV type
 *
 * @CRM_HW_DRV:             DRV is HW (HW Client)
 * @CRM_SW_DRV:             DRV is SW (SW Client)
 */
enum crm_drv_type {
	CRM_HW_DRV,
	CRM_SW_DRV,
};


/**
 * struct qcom_icc_provider - Qualcomm specific interconnect provider
 * @provider: generic interconnect provider
 * @dev: reference to the NoC device
 * @bcms: list of bcms that maps to the provider
 * @num_bcms: number of @bcms
 * @voter: bcm voter targeted by this provider
 */
struct qcom_icc_provider {
	// struct icc_provider provider;
	struct device *dev;
	struct qcom_icc_bcm * const *bcms;
	size_t num_bcms;
	struct qcom_icc_node * const *nodes;
	size_t num_nodes;
	// struct list_head probe_list;
	struct regmap *regmap;
	struct clk_bulk_data *clks;
	int num_clks;
	struct bcm_voter **voters;
	size_t num_voters;
	bool stub;
	bool skip_qos;
};

/**
 * struct bcm_db - Auxiliary data pertaining to each Bus Clock Manager (BCM)
 * @unit: divisor used to convert bytes/sec bw value to an RPMh msg
 * @width: multiplier used to convert bytes/sec bw value to an RPMh msg
 * @vcd: virtual clock domain that this bcm belongs to
 * @reserved: reserved field
 */
struct bcm_db {
	__le32 unit;
	__le16 width;
	uint8_t vcd;
	uint8_t reserved;
};

#define MAX_LINKS		128
#define MAX_BCMS		64
#define MAX_BCM_PER_NODE	3
#define MAX_VCD			10

struct qcom_icc_crm_voter {
	const char *name;
	const struct device *dev;
	enum crm_drv_type client_type;
	uint32_t client_idx;
	uint32_t pwr_states;
};

/**
 * struct qcom_icc_node - Qualcomm specific interconnect nodes
 * @name: the node name used in debugfs
 * @links: an array of nodes where we can go next while traversing
 * @id: a unique node identifier
 * @num_links: the total number of @links
 * @channels: num of channels at this node
 * @buswidth: width of the interconnect between a node and the bus
 * @sum_avg: current sum aggregate value of all avg bw requests
 * @max_peak: current max aggregate value of all peak bw requests
 * @perf_mode: current OR aggregate value of all QCOM_ICC_TAG_PERF_MODE votes
 * @bcms: list of bcms associated with this logical node
 * @num_bcms: num of @bcms
 * @clk: the local clock at this node
 * @clk_name: the local clock name at this node
 * @toggle_clk: flag used to indicate whether local clock can be enabled/disabled
 * @clk_enabled: flag used to indicate whether local clock have been enabled
 * @bw_scale_numerator: the numerator of the bandwidth scale factor
 * @bw_scale_denominator: the denominator of the bandwidth scale factor
 * @disabled : flag used to indicate state of icc node
 */
struct qcom_icc_node {
	const char *name;
	uint16_t links[MAX_LINKS];
	uint16_t id;
	uint16_t num_links;
	uint16_t channels;
	uint16_t buswidth;
	uint64_t sum_avg[QCOM_ICC_NUM_BUCKETS];
	uint64_t max_peak[QCOM_ICC_NUM_BUCKETS];
	bool perf_mode[QCOM_ICC_NUM_BUCKETS];
	uint32_t init_avg;
	uint32_t init_peak;
	struct qcom_icc_bcm *bcms[MAX_BCM_PER_NODE];
	size_t num_bcms;
	struct regmap *regmap;
	struct qcom_icc_qosbox *qosbox;
	const struct qcom_icc_noc_ops *noc_ops;
	struct clk *clk;
	const char *clk_name;
	bool toggle_clk;
	bool clk_enabled;
	uint16_t bw_scale_numerator;
	uint16_t bw_scale_denominator;
	bool disabled;
};

enum {
	VOTER_IDX_HLOS,
	VOTER_IDX_CAM_IFE_0,
	VOTER_IDX_CAM_IFE_1,
	VOTER_IDX_CAM_IFE_2,
	VOTER_IDX_PCIE_CRM_HW_0,
	VOTER_IDX_DISP_CRM_SW_0,
	VOTER_IDX_DISP_CRM_HW_0,
	VOTER_IDX_DISP_CRM_HW_1,
	VOTER_IDX_DISP_CRM_HW_2,
	VOTER_IDX_DISP_CRM_HW_3,
	VOTER_IDX_DISP_CRM_HW_4,
	VOTER_IDX_DISP_CRM_HW_5,
	VOTER_IDX_DISP_CRM_HW_6,
	VOTER_IDX_DISP_CRM_HW_7,
	VOTER_IDX_DISP_CRM_HW_8,
};

static struct qcom_icc_bcm bcm_acv = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_HLOS,
	.perf_mode_mask = 0x2,
	.num_nodes = 1,
	//.nodes = { &ebi },
};

// static struct qcom_icc_bcm bcm_ce0 = {
// 	.name = "CE0",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.num_nodes = 1,
// 	//.nodes = { &qxm_crypto },
// };
// 
// static struct qcom_icc_bcm bcm_cn0 = {
// 	.name = "CN0",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.enable_mask = 0x1,
// 	.keepalive = true,
// 	.num_nodes = 43,
// };
// 
// static struct qcom_icc_bcm bcm_cn1 = {
// 	.name = "CN1",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.num_nodes = 6,
// };
// 
// static struct qcom_icc_bcm bcm_co0 = {
// 	.name = "CO0",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.enable_mask = 0x1,
// 	.num_nodes = 2,
// 	//.nodes = { &qnm_nsp, &qns_nsp_gemnoc },
// };
// 
// static struct qcom_icc_bcm bcm_lp0 = {
// 	.name = "LP0",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.num_nodes = 2,
// 	//.nodes = { &qnm_lpass_lpinoc, &qns_lpass_aggnoc },
// };
 
static struct qcom_icc_bcm bcm_mc0 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_HLOS,
	.keepalive = true,
	.num_nodes = 1,
	//.nodes = { &ebi },
};

// static struct qcom_icc_bcm bcm_mm0 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf },
// };
// 
// static struct qcom_icc_bcm bcm_mm1 = {
// 	.name = "MM1",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.enable_mask = 0x1,
// 	.num_nodes = 9,
// };
 
static struct qcom_icc_bcm bcm_qpc0 = {
	.name = "QPC0",
	.voter_idx = VOTER_IDX_HLOS,
	.num_nodes = 1,
	//.nodes = { &qnm_qpace },
};

// static struct qcom_icc_bcm bcm_qup0 = {
// 	.name = "QUP0",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.vote_scale = 1,
// 	.num_nodes = 1,
// 	//.nodes = { &qup0_core_slave },
// };
// 
// static struct qcom_icc_bcm bcm_qup1 = {
// 	.name = "QUP1",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.vote_scale = 1,
// 	.num_nodes = 1,
// 	//.nodes = { &qup1_core_slave },
// };
// 
// static struct qcom_icc_bcm bcm_qup2 = {
// 	.name = "QUP2",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.vote_scale = 1,
// 	.num_nodes = 1,
// 	//.nodes = { &qup2_core_slave },
// };
// 
// static struct qcom_icc_bcm bcm_qup3 = {
// 	.name = "QUP3",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.vote_scale = 1,
// 	.num_nodes = 1,
// 	//.nodes = { &qup3_core_slave },
// };
// 
// static struct qcom_icc_bcm bcm_qup4 = {
// 	.name = "QUP4",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.vote_scale = 1,
// 	.num_nodes = 1,
// 	//.nodes = { &qup4_core_slave },
// };

static struct qcom_icc_bcm bcm_sh0 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_HLOS,
	.keepalive = true,
	.num_nodes = 1,
	//.nodes = { &qns_llcc },
};

static struct qcom_icc_bcm bcm_sh1 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_HLOS,
	.enable_mask = 0x1,
	.num_nodes = 14,
};

// static struct qcom_icc_bcm bcm_sn0 = {
// 	.name = "SN0",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.keepalive = true,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_gemnoc_sf },
// };

// static struct qcom_icc_bcm bcm_sn2 = {
// 	.name = "SN2",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.num_nodes = 1,
// 	//.nodes = { &qnm_aggre1_noc },
// };

// static struct qcom_icc_bcm bcm_sn3 = {
// 	.name = "SN3",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.num_nodes = 1,
// 	//.nodes = { &qnm_aggre2_noc },
// };

// static struct qcom_icc_bcm bcm_sn4 = {
// 	.name = "SN4",
// 	.voter_idx = VOTER_IDX_HLOS,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_pcie_gemnoc },
// };

static struct qcom_icc_bcm bcm_acv_cam_ife_0 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_CAM_IFE_0,
	.num_nodes = 1,
	//.nodes = { &ebi_cam_ife_0 },
};

static struct qcom_icc_bcm bcm_mc0_cam_ife_0 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_CAM_IFE_0,
	.num_nodes = 1,
	//.nodes = { &ebi_cam_ife_0 },
};

// static struct qcom_icc_bcm bcm_mm0_cam_ife_0 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_CAM_IFE_0,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_cam_ife_0 },
// };

// static struct qcom_icc_bcm bcm_mm1_cam_ife_0 = {
// 	.name = "MM1",
// 	.voter_idx = VOTER_IDX_CAM_IFE_0,
// 	.enable_mask = 0x1,
// 	.num_nodes = 5,
// };

static struct qcom_icc_bcm bcm_sh0_cam_ife_0 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_CAM_IFE_0,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_cam_ife_0 },
};

static struct qcom_icc_bcm bcm_sh1_cam_ife_0 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_CAM_IFE_0,
	.enable_mask = 0x1,
	.num_nodes = 2,
	//.nodes = { &qnm_mnoc_hf_cam_ife_0, &qnm_mnoc_sf_cam_ife_0 },
};

static struct qcom_icc_bcm bcm_acv_cam_ife_1 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_CAM_IFE_1,
	.num_nodes = 1,
	//.nodes = { &ebi_cam_ife_1 },
};

static struct qcom_icc_bcm bcm_mc0_cam_ife_1 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_CAM_IFE_1,
	.num_nodes = 1,
	//.nodes = { &ebi_cam_ife_1 },
};

// static struct qcom_icc_bcm bcm_mm0_cam_ife_1 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_CAM_IFE_1,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_cam_ife_1 },
// };

// static struct qcom_icc_bcm bcm_mm1_cam_ife_1 = {
// 	.name = "MM1",
// 	.voter_idx = VOTER_IDX_CAM_IFE_1,
// 	.enable_mask = 0x1,
// 	.num_nodes = 5,
// };

static struct qcom_icc_bcm bcm_sh0_cam_ife_1 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_CAM_IFE_1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_cam_ife_1 },
};

static struct qcom_icc_bcm bcm_sh1_cam_ife_1 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_CAM_IFE_1,
	.enable_mask = 0x1,
	.num_nodes = 2,
	//.nodes = { &qnm_mnoc_hf_cam_ife_1, &qnm_mnoc_sf_cam_ife_1 },
};

static struct qcom_icc_bcm bcm_acv_cam_ife_2 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_CAM_IFE_2,
	.num_nodes = 1,
	//.nodes = { &ebi_cam_ife_2 },
};

static struct qcom_icc_bcm bcm_mc0_cam_ife_2 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_CAM_IFE_2,
	.num_nodes = 1,
	//.nodes = { &ebi_cam_ife_2 },
};

// static struct qcom_icc_bcm bcm_mm0_cam_ife_2 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_CAM_IFE_2,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_cam_ife_2 },
// };

// static struct qcom_icc_bcm bcm_mm1_cam_ife_2 = {
// 	.name = "MM1",
// 	.voter_idx = VOTER_IDX_CAM_IFE_2,
// 	.enable_mask = 0x1,
// 	.num_nodes = 5,
// };

static struct qcom_icc_bcm bcm_sh0_cam_ife_2 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_CAM_IFE_2,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_cam_ife_2 },
};

static struct qcom_icc_bcm bcm_sh1_cam_ife_2 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_CAM_IFE_2,
	.enable_mask = 0x1,
	.num_nodes = 2,
	//.nodes = { &qnm_mnoc_hf_cam_ife_2, &qnm_mnoc_sf_cam_ife_2 },
};

static struct qcom_icc_bcm bcm_acv_pcie_crm_hw_0 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_PCIE_CRM_HW_0,
	.crm_node = 5,
	.num_nodes = 1,
	//.nodes = { &ebi_pcie_crm_hw_0 },
};

// static struct qcom_icc_bcm bcm_ip0_pcie_crm_hw_0 = {
// 	.name = "IP0",
// 	.voter_idx = VOTER_IDX_PCIE_CRM_HW_0,
// 	.crm_node = 4,
// 	.vote_scale = 1,
// 	.num_nodes = 1,
// 	//.nodes = { &ipa_core_slave_pcie_crm_hw_0 },
// };

static struct qcom_icc_bcm bcm_mc0_pcie_crm_hw_0 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_PCIE_CRM_HW_0,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_pcie_crm_hw_0 },
};

static struct qcom_icc_bcm bcm_sh0_pcie_crm_hw_0 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_PCIE_CRM_HW_0,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_pcie_crm_hw_0 },
};

static struct qcom_icc_bcm bcm_sh1_pcie_crm_hw_0 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_PCIE_CRM_HW_0,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_pcie_pcie_crm_hw_0 },
};

// static struct qcom_icc_bcm bcm_sn4_pcie_crm_hw_0 = {
// 	.name = "SN4",
// 	.voter_idx = VOTER_IDX_PCIE_CRM_HW_0,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_pcie_gemnoc_pcie_crm_hw_0 },
// };

static struct qcom_icc_bcm bcm_acv_disp_crm_sw_0 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_SW_0,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_sw_0 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_sw_0 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_SW_0,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_sw_0 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_sw_0 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_SW_0,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_sw_0 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_sw_0 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_SW_0,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_sw_0 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_sw_0 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_SW_0,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_sw_0 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_0 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_0,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_0 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_0 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_0,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_0 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_0 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_0,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_0 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_0 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_0,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_0 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_0 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_0,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_0 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_1 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_1,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_1 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_1 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_1,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_1 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_1 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_1,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_1 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_1 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_1,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_1 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_1 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_1,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_1 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_2 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_2,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_2 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_2 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_2,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_2 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_2 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_2,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_2 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_2 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_2,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_2 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_2 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_2,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_2 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_3 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_3,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_3 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_3 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_3,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_3 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_3 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_3,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_3 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_3 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_3,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_3 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_3 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_3,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_3 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_4 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_4,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_4 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_4 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_4,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_4 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_4 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_4,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_4 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_4 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_4,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_4 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_4 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_4,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_4 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_5 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_5,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_5 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_5 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_5,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_5 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_5 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_5,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_5 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_5 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_5,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_5 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_5 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_5,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_5 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_6 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_6,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_6 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_6 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_6,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_6 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_6 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_6,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_6 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_6 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_6,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_6 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_6 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_6,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_6 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_7 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_7,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_7 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_7 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_7,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_7 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_7 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_7,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_7 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_7 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_7,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_7 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_7 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_7,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_7 },
};

static struct qcom_icc_bcm bcm_acv_disp_crm_hw_8 = {
	.name = "ACV",
	.type = QCOM_ICC_BCM_TYPE_MASK,
	.voter_idx = VOTER_IDX_DISP_CRM_HW_8,
	.crm_node = 4,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_8 },
};

static struct qcom_icc_bcm bcm_mc0_disp_crm_hw_8 = {
	.name = "MC0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_8,
	.crm_node = 0,
	.num_nodes = 1,
	//.nodes = { &ebi_disp_crm_hw_8 },
};

// static struct qcom_icc_bcm bcm_mm0_disp_crm_hw_8 = {
// 	.name = "MM0",
// 	.voter_idx = VOTER_IDX_DISP_CRM_HW_8,
// 	.crm_node = 3,
// 	.num_nodes = 1,
// 	//.nodes = { &qns_mem_noc_hf_disp_crm_hw_8 },
// };

static struct qcom_icc_bcm bcm_sh0_disp_crm_hw_8 = {
	.name = "SH0",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_8,
	.crm_node = 1,
	.num_nodes = 1,
	//.nodes = { &qns_llcc_disp_crm_hw_8 },
};

static struct qcom_icc_bcm bcm_sh1_disp_crm_hw_8 = {
	.name = "SH1",
	.voter_idx = VOTER_IDX_DISP_CRM_HW_8,
	.crm_node = 2,
	.enable_mask = 0x1,
	.num_nodes = 1,
	//.nodes = { &qnm_mnoc_hf_disp_crm_hw_8 },
};

static struct qcom_icc_bcm bcm_cn0 = {
	.name = "CN0",
	.voter_idx = VOTER_IDX_HLOS,
	.enable_mask = 0x1,
	.keepalive = true,
	.num_nodes = 43,
	// .nodes = { &qsm_cfg, &qhs_ahb2phy0,
	// 	   &qhs_ahb2phy1, &qhs_camera_cfg,
	// 	   &qhs_clk_ctl, &qhs_crypto0_cfg,
	// 	   &qhs_eva_cfg, &qhs_gpuss_cfg,
	// 	   &qhs_i3c_ibi0_cfg, &qhs_i3c_ibi1_cfg,
	// 	   &qhs_imem_cfg, &qhs_ipc_router,
	// 	   &qhs_mss_cfg, &qhs_pcie_cfg,
	// 	   &qhs_prng, &qhs_qdss_cfg,
	// 	   &qhs_qspi, &qhs_sdc2,
	// 	   &qhs_sdc4, &qhs_spss_cfg,
	// 	   &qhs_tcsr, &qhs_tlmm,
	// 	   &qhs_ufs_mem_cfg, &qhs_usb3,
	// 	   &qhs_venus_cfg, &qhs_vsense_ctrl_cfg,
	// 	   &qss_mnoc_cfg, &qss_pcie_anoc_cfg,
	// 	   &xs_qdss_stm, &xs_sys_tcu_cfg,
	// 	   &qnm_gemnoc_cnoc, &qnm_gemnoc_pcie,
	// 	   &qhs_aoss, &qhs_ipa,
	// 	   &qhs_ipc_router_fence, &qhs_soccp,
	// 	   &qhs_tme_cfg, &qns_apss,
	// 	   &qss_cfg, &qss_ddrss_cfg,
	// 	   &qxs_boot_imem, &qxs_imem,
	// 	   &xs_pcie },
};

static struct qcom_icc_bcm *gem_noc_bcms[] = {
	&bcm_qpc0,
	&bcm_sh0,
	&bcm_sh1,
	&bcm_sh0_cam_ife_0,
	&bcm_sh1_cam_ife_0,
	&bcm_sh0_cam_ife_1,
	&bcm_sh1_cam_ife_1,
	&bcm_sh0_cam_ife_2,
	&bcm_sh1_cam_ife_2,
	&bcm_sh0_pcie_crm_hw_0,
	&bcm_sh1_pcie_crm_hw_0,
	&bcm_sh0_disp_crm_sw_0,
	&bcm_sh1_disp_crm_sw_0,
	&bcm_sh0_disp_crm_hw_0,
	&bcm_sh1_disp_crm_hw_0,
	&bcm_sh0_disp_crm_hw_1,
	&bcm_sh1_disp_crm_hw_1,
	&bcm_sh0_disp_crm_hw_2,
	&bcm_sh1_disp_crm_hw_2,
	&bcm_sh0_disp_crm_hw_3,
	&bcm_sh1_disp_crm_hw_3,
	&bcm_sh0_disp_crm_hw_4,
	&bcm_sh1_disp_crm_hw_4,
	&bcm_sh0_disp_crm_hw_5,
	&bcm_sh1_disp_crm_hw_5,
	&bcm_sh0_disp_crm_hw_6,
	&bcm_sh1_disp_crm_hw_6,
	&bcm_sh0_disp_crm_hw_7,
	&bcm_sh1_disp_crm_hw_7,
	&bcm_sh0_disp_crm_hw_8,
	&bcm_sh1_disp_crm_hw_8,
};

static struct qcom_icc_bcm *mc_virt_bcms[] = {
	&bcm_acv,
	&bcm_mc0,
	&bcm_acv_cam_ife_0,
	&bcm_mc0_cam_ife_0,
	&bcm_acv_cam_ife_1,
	&bcm_mc0_cam_ife_1,
	&bcm_acv_cam_ife_2,
	&bcm_mc0_cam_ife_2,
	&bcm_acv_pcie_crm_hw_0,
	&bcm_mc0_pcie_crm_hw_0,
	&bcm_acv_disp_crm_sw_0,
	&bcm_mc0_disp_crm_sw_0,
	&bcm_acv_disp_crm_hw_0,
	&bcm_mc0_disp_crm_hw_0,
	&bcm_acv_disp_crm_hw_1,
	&bcm_mc0_disp_crm_hw_1,
	&bcm_acv_disp_crm_hw_2,
	&bcm_mc0_disp_crm_hw_2,
	&bcm_acv_disp_crm_hw_3,
	&bcm_mc0_disp_crm_hw_3,
	&bcm_acv_disp_crm_hw_4,
	&bcm_mc0_disp_crm_hw_4,
	&bcm_acv_disp_crm_hw_5,
	&bcm_mc0_disp_crm_hw_5,
	&bcm_acv_disp_crm_hw_6,
	&bcm_mc0_disp_crm_hw_6,
	&bcm_acv_disp_crm_hw_7,
	&bcm_mc0_disp_crm_hw_7,
	&bcm_acv_disp_crm_hw_8,
	&bcm_mc0_disp_crm_hw_8,
};

static struct qcom_icc_bcm *cnoc_main_bcms[] = {
	&bcm_cn0,
};

const struct qcom_icc_desc canoe_gem_noc = {
	// .config = &icc_regmap_config,
	//.nodes = gem_noc_nodes,
	// .num_nodes = ARRAY_SIZE(gem_noc_nodes),
	.bcms = gem_noc_bcms,
	.num_bcms = ARRAY_SIZE(gem_noc_bcms),
	// .voters = gem_noc_voters,
	// .num_voters = ARRAY_SIZE(gem_noc_voters),
};

const struct qcom_icc_desc canoe_mc_virt = {
	// .config = &icc_regmap_config,
	// .nodes = mc_virt_nodes,
	// .num_nodes = ARRAY_SIZE(mc_virt_nodes),
	.bcms = mc_virt_bcms,
	.num_bcms = ARRAY_SIZE(mc_virt_bcms),
	// .voters = mc_virt_voters,
	// .num_voters = ARRAY_SIZE(mc_virt_voters),
};

const struct qcom_icc_desc canoe_cnoc_main = {
	.bcms = cnoc_main_bcms,
	.num_bcms = ARRAY_SIZE(cnoc_main_bcms),
};

