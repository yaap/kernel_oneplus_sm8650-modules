// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023. Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <media/v4l2_vidc_extensions.h>
#include "msm_vidc_control_ext.h"
#include "hfi_packet.h"
#include "hfi_property.h"
#include "venus_hfi.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_driver.h"
#include "msm_venc.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_debug.h"

int msm_vidc_adjust_ir_period(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value, all_intra = 0, roi_enable = 0,
		pix_fmts = MSM_VIDC_FMT_NONE;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[IR_PERIOD].value;

	if (msm_vidc_get_parent_value(inst, IR_PERIOD, ALL_INTRA,
		&all_intra, __func__) ||
		msm_vidc_get_parent_value(inst, IR_PERIOD, META_ROI_INFO,
		&roi_enable, __func__))
		return -EINVAL;

	if (all_intra) {
		adjusted_value = 0;
		i_vpr_h(inst, "%s: intra refresh unsupported, all intra: %d\n",
			__func__, all_intra);
		goto exit;
	}

	if (roi_enable) {
		i_vpr_h(inst,
			"%s: intra refresh unsupported with roi metadata\n",
			__func__);
		adjusted_value = 0;
		goto exit;
	}

	if (inst->codec == MSM_VIDC_HEVC) {
		if (msm_vidc_get_parent_value(inst, IR_PERIOD,
			PIX_FMTS, &pix_fmts, __func__))
			return -EINVAL;

		if (is_10bit_colorformat(pix_fmts)) {
			i_vpr_h(inst,
				"%s: intra refresh is supported only for 8 bit\n",
				__func__);
			adjusted_value = 0;
			goto exit;
		}
	}

	/*
	 * BITRATE_MODE dependency is NOT common across all chipsets.
	 * Hence, do not return error if not specified as one of the parent.
	 */
	if (is_parent_available(inst, IR_PERIOD, BITRATE_MODE, __func__) &&
		inst->hfi_rc_type != HFI_RC_CBR_CFR &&
		inst->hfi_rc_type != HFI_RC_CBR_VFR)
		adjusted_value = 0;

exit:
	msm_vidc_update_cap_value(inst, IR_PERIOD,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_frame_rate(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 adjusted_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (is_encode_session(inst)) {
		d_vpr_e("%s: adjust framerate invalid for enc\n", __func__);
		return -EINVAL;
	}

	capability = inst->capabilities;
	adjusted_value = ctrl ? ctrl->val : capability->cap[FRAME_RATE].value;
	msm_vidc_update_cap_value(inst, FRAME_RATE, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_dec_operating_rate(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;
	u32 adjusted_value = 0;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (is_encode_session(inst)) {
		d_vpr_e("%s: adjust operating rate invalid for enc\n", __func__);
		return -EINVAL;
	}

	capability = inst->capabilities;
	adjusted_value = ctrl ? ctrl->val : capability->cap[OPERATING_RATE].value;
	msm_vidc_update_cap_value(inst, OPERATING_RATE, adjusted_value, __func__);

	return 0;
}

int msm_vidc_adjust_delivery_mode(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst_capability *capability;
	s32 adjusted_value;
	s32 slice_mode = -1;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *) instance;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (is_decode_session(inst))
		return 0;

	capability = inst->capabilities;

	adjusted_value = ctrl ? ctrl->val : capability->cap[DELIVERY_MODE].value;

	if (msm_vidc_get_parent_value(inst, DELIVERY_MODE, SLICE_MODE,
		&slice_mode, __func__))
		return -EINVAL;

	/* Slice encode delivery mode is only supported for Max MB slice mode */
	if (slice_mode != V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB)
		adjusted_value = 0;

	msm_vidc_update_cap_value(inst, DELIVERY_MODE,
		adjusted_value, __func__);

	return 0;
}

int msm_vidc_set_ir_period(void *instance,
	enum msm_vidc_inst_capability_type cap_id)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)instance;
	u32 ir_type = 0;
	struct msm_vidc_core *core;

	if (!inst || !inst->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;

	if (inst->capabilities->cap[IR_TYPE].value ==
	    V4L2_MPEG_VIDEO_VIDC_INTRA_REFRESH_RANDOM) {
		if (inst->bufq[OUTPUT_PORT].vb2q->streaming) {
			i_vpr_h(inst, "%s: dynamic random intra refresh not allowed\n",
				__func__);
			return 0;
		}
		ir_type = HFI_PROP_IR_RANDOM_PERIOD;
	} else if (inst->capabilities->cap[IR_TYPE].value ==
		   V4L2_MPEG_VIDEO_VIDC_INTRA_REFRESH_CYCLIC) {
		ir_type = HFI_PROP_IR_CYCLIC_PERIOD;
	} else {
		i_vpr_e(inst, "%s: invalid ir_type %d\n",
			__func__, inst->capabilities->cap[IR_TYPE]);
		return -EINVAL;
	}

	rc = venus_hfi_set_ir_period(inst, ir_type, cap_id);
	if (rc) {
		i_vpr_e(inst, "%s: failed to set ir period %d\n",
			__func__, inst->capabilities->cap[IR_PERIOD].value);
		return rc;
	}

	return rc;
}

