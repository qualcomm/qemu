/* 
 * Qualcomm Android RPMh clock device
 *
 * Author: Romain Malmain <rmalmain@qti.qualcomm.com>
 *
 * Only provides minimal support, mostly to pass probe checks.
 */

#ifndef QEMU_QCOM_RPMH_RSC_H
#define QEMU_QCOM_RPMH_RSC_H

#include "qemu/osdep.h"
#include "qemu/bitops.h"

/**
 * struct clk_rpmh - individual rpmh clock data structure
 * @hw:			handle between common and hardware-specific interfaces
 * @res_name:		resource name for the rpmh clock
 * @div:		clock divider to compute the clock rate
 * @res_addr:		base address of the rpmh resource within the RPMh
 * @res_on_val:		rpmh clock enable value
 * @state:		rpmh clock requested state
 * @aggr_state:		rpmh clock aggregated state
 * @last_sent_aggr_state: rpmh clock last aggr state sent to RPMh
 * @valid_state_mask:	mask to determine the state of the rpmh clock
 * @unit:		divisor to convert rate to rpmh msg in magnitudes of Khz
 * @dev:		device to which it is attached
 * @peer:		pointer to the clock rpmh sibling
 */
struct clk_rpmh {
	// struct clk_hw hw;
	const char *res_name;
	uint8_t div;
	bool optional;
	uint32_t res_addr;
	uint32_t res_on_val;
	uint32_t state;
	uint32_t aggr_state;
	uint32_t last_sent_aggr_state;
	uint32_t valid_state_mask;
	uint32_t unit;
	// struct device *dev;
	struct clk_rpmh *peer;
};

struct clk_rpmh_desc {
	struct clk_rpmh **clks;
	size_t num_clks;
};

extern const struct clk_rpmh_desc clk_rpmh_canoe;

#endif
