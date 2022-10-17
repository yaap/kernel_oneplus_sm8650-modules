/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CVP_POWER_H_
#define _CVP_POWER_H_

#include "msm_cvp_internal.h"
#include "msm_cvp_common.h"
#include "msm_cvp_clocks.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_dsp.h"

struct cvp_power_level {
	unsigned long core_sum;
	unsigned long op_core_sum;
	unsigned long bw_sum;
};

int msm_cvp_update_power(struct msm_cvp_inst *inst);
unsigned int msm_cvp_get_hw_aggregate_cycles(enum hfi_hw_thread hwblk);
int cvp_check_clock(struct msm_cvp_inst *inst,
		struct cvp_hfi_msg_session_hdr_ext *hdr);
bool check_clock_required(struct msm_cvp_inst *inst,
		struct eva_kmd_hfi_packet *hdr);
#endif
