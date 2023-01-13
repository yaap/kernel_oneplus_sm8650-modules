/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023. Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_CONTROL_EXT_H_
#define _MSM_VIDC_CONTROL_EXT_H_

#include "msm_vidc_control.h"

int msm_vidc_adjust_ir_period(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_dec_frame_rate(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_dec_operating_rate(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_delivery_mode(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_set_ir_period(void *instance,
	enum msm_vidc_inst_capability_type cap_id);

#endif
