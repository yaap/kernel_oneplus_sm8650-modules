
/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp.h"
#include "cvp_power.h"

static inline int find_max(unsigned long *array, unsigned int num)
{
	int i, max = 0;

	for (i = 0; i < num; i++)
		max = array[i] > max ? array[i] : max;

	return max;
}

static bool is_subblock_profile_existed(struct msm_cvp_inst *inst)
{
	return (inst->prop.cycles[HFI_HW_OD] ||
			inst->prop.cycles[HFI_HW_MPU] ||
			inst->prop.cycles[HFI_HW_FDU] ||
			inst->prop.cycles[HFI_HW_ICA] ||
			inst->prop.cycles[HFI_HW_VADL] ||
			inst->prop.cycles[HFI_HW_TOF] ||
			inst->prop.cycles[HFI_HW_RGE] ||
			inst->prop.cycles[HFI_HW_XRA] ||
			inst->prop.cycles[HFI_HW_LSR]);
}

static char hw_names[HFI_MAX_HW_THREADS][8] = {{"FDU"}, {"OD"}, {"MPU"}, {"ICA"},
				{"VADL"}, {"TOF"}, {"RGE"}, {"XRA"},
				{"LSR"}};
static void aggregate_power_update(struct msm_cvp_core *core,
	struct cvp_power_level *nrt_pwr,
	struct cvp_power_level *rt_pwr,
	unsigned int max_clk_rate)
{
	struct msm_cvp_inst *inst;
	int i, j;
	unsigned long blocks_sum[2][HFI_MAX_HW_THREADS] = {0};
	unsigned long fw_sum[2] = {0}, max_cycle[2] = {0}, op_max_cycle[2] = {0};
	unsigned long op_blocks_max[2][HFI_MAX_HW_THREADS] = {0};
	unsigned long op_fw_max[2] = {0}, bw_sum[2] = {0}, op_bw_max[2] = {0};

	for (j = 0; j < HFI_MAX_HW_THREADS; j++)
		core->dyn_clk.sum_fps[j] = 0;

	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state == MSM_CVP_CORE_INVALID ||
			inst->state == MSM_CVP_CORE_UNINIT ||
			!is_subblock_profile_existed(inst))
			continue;
		if (inst->prop.priority <= CVP_RT_PRIO_THRESHOLD) {
			/* Non-realtime session use index 0 */
			i = 0;
		} else {
			i = 1;
		}
		for (j = 0; j < HFI_MAX_HW_THREADS; j++)
			if (inst->prop.cycles[j])
				dprintk(CVP_PWR, "pwrUpdate %s %u\n",
					hw_names[j], inst->prop.cycles[j]);

		for (j = 0; j < HFI_MAX_HW_THREADS; j++)
			if (inst->prop.op_cycles[j])
				dprintk(CVP_PWR, "pwrUpdate_OP %s %u\n",
					hw_names[j], inst->prop.op_cycles[j]);

		dprintk(CVP_PWR, " fw %u fw_o %u\n", inst->prop.fw_cycles,
						inst->prop.fw_op_cycles);

		for (j = 0; j < HFI_MAX_HW_THREADS; j++)
			blocks_sum[i][j] += inst->prop.cycles[j];

		fw_sum[i] += inst->prop.fw_cycles;

		for (j = 0; j < HFI_MAX_HW_THREADS; j++)
			op_blocks_max[i][j] =
				(op_blocks_max[i][j] >= inst->prop.op_cycles[j]) ?
				op_blocks_max[i][j] : inst->prop.op_cycles[j];

		op_fw_max[i] =
			(op_fw_max[i] >= inst->prop.fw_op_cycles) ?
			op_fw_max[i] : inst->prop.fw_op_cycles;

		bw_sum[i] += inst->prop.ddr_bw;

		op_bw_max[i] =
			(op_bw_max[i] >= inst->prop.ddr_op_bw) ?
			op_bw_max[i] : inst->prop.ddr_op_bw;

		for (j = 0; j < HFI_MAX_HW_THREADS; j++) {
			if (inst->prop.fps[j])
				dprintk(CVP_PWR, "fps %s %d ", hw_names[j],
						inst->prop.fps[j]);
			core->dyn_clk.sum_fps[j] += inst->prop.fps[j];
		}

		for (j = 0; j < HFI_MAX_HW_THREADS; j++)
			if (core->dyn_clk.sum_fps[j])
				dprintk(CVP_PWR, "sum_fps %s %d ", hw_names[j],
						core->dyn_clk.sum_fps[j]);
	}

	for (i = 0; i < 2; i++) {
		max_cycle[i] = find_max(&blocks_sum[i][0], HFI_MAX_HW_THREADS);
		op_max_cycle[i] = find_max(&op_blocks_max[i][0], HFI_MAX_HW_THREADS);

		op_max_cycle[i] =
			(op_max_cycle[i] > max_clk_rate) ?
			max_clk_rate : op_max_cycle[i];
		bw_sum[i] = (bw_sum[i] >= op_bw_max[i]) ?
			bw_sum[i] : op_bw_max[i];
	}

	nrt_pwr->core_sum += max_cycle[0];
	nrt_pwr->op_core_sum = (nrt_pwr->op_core_sum >= op_max_cycle[0]) ?
			nrt_pwr->op_core_sum : op_max_cycle[0];
	nrt_pwr->bw_sum += bw_sum[0];
	rt_pwr->core_sum += max_cycle[1];
	rt_pwr->op_core_sum = (rt_pwr->op_core_sum >= op_max_cycle[1]) ?
			rt_pwr->op_core_sum : op_max_cycle[1];
	rt_pwr->bw_sum += bw_sum[1];
}

/**
 * adjust_bw_freqs(): calculate CVP clock freq and bw required to sustain
 * required use case.
 * Bandwidth vote will be best-effort, not returning error if the request
 * b/w exceeds max limit.
 * Clock vote from non-realtime sessions will be best effort, not returning
 * error if the aggreated session clock request exceeds max limit.
 * Clock vote from realtime session will be hard request. If aggregated
 * session clock request exceeds max limit, the function will return
 * error.
 *
 * Ensure caller acquires clk_lock!
 */
static int adjust_bw_freqs(void)
{
	struct msm_cvp_core *core;
	struct iris_hfi_device *hdev;
	struct bus_info *bus;
	struct clock_set *clocks;
	struct clock_info *cl;
	struct allowed_clock_rates_table *tbl = NULL;
	unsigned int tbl_size;
	unsigned int cvp_min_rate, cvp_max_rate, max_bw, min_bw;
	struct cvp_power_level rt_pwr = {0}, nrt_pwr = {0};
	unsigned long tmp, core_sum, op_core_sum, bw_sum;
	int i, rc = 0;
	unsigned long ctrl_freq;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);

	hdev = core->device->hfi_device_data;
	clocks = &core->resources.clock_set;
	cl = &clocks->clock_tbl[clocks->count - 1];
	tbl = core->resources.allowed_clks_tbl;
	tbl_size = core->resources.allowed_clks_tbl_size;
	cvp_min_rate = tbl[0].clock_rate;
	cvp_max_rate = tbl[tbl_size - 1].clock_rate;
	bus = &core->resources.bus_set.bus_tbl[1];
	max_bw = bus->range[1];
	min_bw = max_bw/10;

	aggregate_power_update(core, &nrt_pwr, &rt_pwr, cvp_max_rate);
	dprintk(CVP_PWR, "PwrUpdate nrt %u %u rt %u %u\n",
		nrt_pwr.core_sum, nrt_pwr.op_core_sum,
		rt_pwr.core_sum, rt_pwr.op_core_sum);

	if (rt_pwr.core_sum > cvp_max_rate) {
		dprintk(CVP_WARN, "%s clk vote out of range %lld\n",
			__func__, rt_pwr.core_sum);
		return -ENOTSUPP;
	}

	core_sum = rt_pwr.core_sum + nrt_pwr.core_sum;
	op_core_sum = (rt_pwr.op_core_sum >= nrt_pwr.op_core_sum) ?
		rt_pwr.op_core_sum : nrt_pwr.op_core_sum;

	core_sum = (core_sum >= op_core_sum) ?
		core_sum : op_core_sum;

	if (core_sum > cvp_max_rate) {
		core_sum = cvp_max_rate;
	} else  if (core_sum <= cvp_min_rate) {
		core_sum = cvp_min_rate;
	} else {
		for (i = 1; i < tbl_size; i++)
			if (core_sum <= tbl[i].clock_rate)
				break;
		core_sum = tbl[i].clock_rate;
	}

	bw_sum = rt_pwr.bw_sum + nrt_pwr.bw_sum;
	bw_sum = bw_sum >> 10;
	bw_sum = (bw_sum > max_bw) ? max_bw : bw_sum;
	bw_sum = (bw_sum < min_bw) ? min_bw : bw_sum;

	dprintk(CVP_PWR, "%s %lld %lld\n", __func__,
		core_sum, bw_sum);
	if (!cl->has_scaling) {
		dprintk(CVP_ERR, "Cannot scale CVP clock\n");
		return -EINVAL;
	}

	tmp = core->curr_freq;
	core->curr_freq = core_sum;
	core->orig_core_sum = core_sum;
	rc = msm_cvp_set_clocks(core);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to set clock rate %u %s: %d %s\n",
			core_sum, cl->name, rc, __func__);
		core->curr_freq = tmp;
		return rc;
	}

	ctrl_freq = (core->curr_freq*3)>>1;
	core->dyn_clk.conf_freq = core->curr_freq;
	for (i = 0; i < HFI_MAX_HW_THREADS; ++i) {
		core->dyn_clk.hi_ctrl_lim[i] = core->dyn_clk.sum_fps[i] ?
			ctrl_freq/core->dyn_clk.sum_fps[i] : 0;
		core->dyn_clk.lo_ctrl_lim[i] =
			core->dyn_clk.hi_ctrl_lim[i];
	}

	hdev->clk_freq = core->curr_freq;
	rc = msm_cvp_set_bw(bus, bw_sum);

	return rc;
}

int msm_cvp_update_power(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct msm_cvp_core *core;
	struct msm_cvp_inst *s;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	core = inst->core;

	mutex_lock(&core->clk_lock);
	rc = adjust_bw_freqs();
	mutex_unlock(&core->clk_lock);
	cvp_put_inst(s);

	return rc;
}


static int cvp_readjust_clock(struct msm_cvp_core *core,
			u32 avg_cycles, enum hfi_hw_thread i)
{
	int rc = 0;
	struct allowed_clock_rates_table *tbl = NULL;
	unsigned int tbl_size = 0;
	unsigned int cvp_min_rate = 0, cvp_max_rate = 0;
	unsigned long tmp = core->curr_freq;
	unsigned long lo_freq = 0;
	u32 j;

	tbl = core->resources.allowed_clks_tbl;
	tbl_size = core->resources.allowed_clks_tbl_size;
	cvp_min_rate = tbl[0].clock_rate;
	cvp_max_rate = tbl[tbl_size - 1].clock_rate;

	if (!((avg_cycles > core->dyn_clk.hi_ctrl_lim[i] &&
			 core->curr_freq != cvp_max_rate) ||
			(avg_cycles <= core->dyn_clk.lo_ctrl_lim[i] &&
			 core->curr_freq != cvp_min_rate))) {
		return rc;
	}

	core->curr_freq = ((avg_cycles * core->dyn_clk.sum_fps[i]) << 1)/3;
	dprintk(CVP_PWR,
		"%s - cycles tot %u, avg %u. sum_fps %u, cur_freq %u\n",
		__func__,
		core->dyn_clk.cycle[i].total,
		avg_cycles,
		core->dyn_clk.sum_fps[i],
		core->curr_freq);

	if (core->curr_freq > cvp_max_rate) {
		core->curr_freq = cvp_max_rate;
		lo_freq = (tbl_size > 1) ?
			tbl[tbl_size - 2].clock_rate :
			cvp_min_rate;
	} else  if (core->curr_freq <= cvp_min_rate) {
		core->curr_freq = cvp_min_rate;
		lo_freq = cvp_min_rate;
	} else {
		for (j = 1; j < tbl_size; j++)
			if (core->curr_freq <= tbl[j].clock_rate)
				break;
		core->curr_freq = tbl[j].clock_rate;
		lo_freq = tbl[j-1].clock_rate;
	}

	if (core->orig_core_sum > core->curr_freq) {
		dprintk(CVP_PWR,
			"%s - %d - Cancel readjust, core %u, freq %u\n",
			__func__, i, core->orig_core_sum, core->curr_freq);
		core->curr_freq = tmp;
		return rc;
	}

	dprintk(CVP_PWR,
			"%s:%d - %d - Readjust to %u\n",
			__func__, __LINE__, i, core->curr_freq);
	rc = msm_cvp_set_clocks(core);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to set clock rate %u: %d %s\n",
			core->curr_freq, rc, __func__);
		core->curr_freq = tmp;
	} else {
		lo_freq = (lo_freq < core->dyn_clk.conf_freq) ?
			core->dyn_clk.conf_freq : lo_freq;
		core->dyn_clk.hi_ctrl_lim[i] = core->dyn_clk.sum_fps[i] ?
			((core->curr_freq*3)>>1)/core->dyn_clk.sum_fps[i] : 0;
		core->dyn_clk.lo_ctrl_lim[i] =
			core->dyn_clk.sum_fps[i] ?
			((lo_freq*3)>>1)/core->dyn_clk.sum_fps[i] : 0;

		dprintk(CVP_PWR,
			"%s - Readjust clk to %u. New lim [%d] hi %u lo %u\n",
			__func__, core->curr_freq, i,
			core->dyn_clk.hi_ctrl_lim[i],
			core->dyn_clk.lo_ctrl_lim[i]);
	}

	return rc;
}

int cvp_check_clock(struct msm_cvp_inst *inst,
		struct cvp_hfi_msg_session_hdr_ext *hdr)
{
	int rc = 0;
	u32 i, j;
	u32 hw_cycles[HFI_MAX_HW_THREADS] = {0};
	u32 fw_cycles = 0;
	struct msm_cvp_core *core = inst->core;

	for (i = 0; i < HFI_MAX_HW_ACTIVATIONS_PER_FRAME; ++i)
		fw_cycles += hdr->fw_cycles[i];

	for (i = 0; i < HFI_MAX_HW_THREADS; ++i)
		for (j = 0; j < HFI_MAX_HW_ACTIVATIONS_PER_FRAME; ++j)
			hw_cycles[i] += hdr->hw_cycles[i][j];

	dprintk(CVP_PWR, "%s - cycles fw %u. FDU %d MPU %d ODU %d ICA %d\n",
		__func__, fw_cycles, hw_cycles[0],
		hw_cycles[1], hw_cycles[2], hw_cycles[3]);

	mutex_lock(&core->clk_lock);
	for (i = 0; i < HFI_MAX_HW_THREADS; ++i) {
		dprintk(CVP_PWR, "%s - %d: hw_cycles %u, tens_thresh %u\n",
			__func__, i, hw_cycles[i],
			core->dyn_clk.hi_ctrl_lim[i]);
		if (core->dyn_clk.hi_ctrl_lim[i]) {
			if (core->dyn_clk.cycle[i].size < CVP_CYCLE_STAT_SIZE)
				core->dyn_clk.cycle[i].size++;
			else
				core->dyn_clk.cycle[i].total -=
					core->dyn_clk.cycle[i].busy[
					core->dyn_clk.cycle[i].idx];
			if (hw_cycles[i]) {
				core->dyn_clk.cycle[i].busy[
					core->dyn_clk.cycle[i].idx]
					= hw_cycles[i] + fw_cycles;
				core->dyn_clk.cycle[i].total
					+= hw_cycles[i] + fw_cycles;
				dprintk(CVP_PWR,
					"%s: busy (hw + fw) cycles = %u\n",
					__func__,
					core->dyn_clk.cycle[i].busy[
						core->dyn_clk.cycle[i].idx]);
				dprintk(CVP_PWR, "total cycles %u\n",
					core->dyn_clk.cycle[i].total);
			} else {
				core->dyn_clk.cycle[i].busy[
					core->dyn_clk.cycle[i].idx] =
					hdr->busy_cycles;
				core->dyn_clk.cycle[i].total +=
					hdr->busy_cycles;
				dprintk(CVP_PWR,
					"%s - busy cycles = %u total %u\n",
					__func__,
					core->dyn_clk.cycle[i].busy[
						core->dyn_clk.cycle[i].idx],
					core->dyn_clk.cycle[i].total);
			}

			core->dyn_clk.cycle[i].idx =
				(core->dyn_clk.cycle[i].idx ==
				  CVP_CYCLE_STAT_SIZE-1) ?
				0 : core->dyn_clk.cycle[i].idx+1;

			dprintk(CVP_PWR, "%s - %d: size %u, tens_thresh %u\n",
				__func__, i, core->dyn_clk.cycle[i].size,
				core->dyn_clk.hi_ctrl_lim[i]);
			if (core->dyn_clk.cycle[i].size == CVP_CYCLE_STAT_SIZE
				&& core->dyn_clk.hi_ctrl_lim[i] != 0) {
				u32 avg_cycles =
					core->dyn_clk.cycle[i].total>>3;

				rc = cvp_readjust_clock(core,
							avg_cycles,
							i);
			}
		}
	}
	mutex_unlock(&core->clk_lock);

	return rc;
}

unsigned int msm_cvp_get_hw_aggregate_cycles(enum hfi_hw_thread hwblk)
{
	struct msm_cvp_core *core;
	struct msm_cvp_inst *inst;
	unsigned long cycles_sum = 0;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);

	if (!core) {
		dprintk(CVP_ERR, "%s: invalid core\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&core->clk_lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state == MSM_CVP_CORE_INVALID ||
			inst->state == MSM_CVP_CORE_UNINIT ||
			!is_subblock_profile_existed(inst))
			continue;
		switch (hwblk) {
		case HFI_HW_FDU:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_FDU];
			break;
		}
		case HFI_HW_ICA:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_ICA];
			break;
		}
		case HFI_HW_MPU:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_MPU];
			break;
		}
		case HFI_HW_OD:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_OD];
			break;
		}
		case HFI_HW_VADL:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_VADL];
			break;
		}
		case HFI_HW_TOF:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_TOF];
			break;
		}
		case HFI_HW_RGE:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_RGE];
			break;
		}
		case HFI_HW_XRA:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_XRA];
			break;
		}
		case HFI_HW_LSR:
		{
			cycles_sum += inst->prop.cycles[HFI_HW_LSR];
			break;
		}
		default:
			dprintk(CVP_ERR, "unrecognized hw block %d\n",
				hwblk);
			break;
		}
	}
	mutex_unlock(&core->clk_lock);
	cycles_sum = cycles_sum&0xFFFFFFFF;
	return (unsigned int)cycles_sum;
}

bool check_clock_required(struct msm_cvp_inst *inst,
		struct eva_kmd_hfi_packet *hdr)
{
	struct cvp_hfi_msg_session_hdr_ext *ehdr =
		(struct cvp_hfi_msg_session_hdr_ext *)hdr;
	bool clock_check = false;

	if (!msm_cvp_dcvs_disable &&
			ehdr->packet_type == HFI_MSG_SESSION_CVP_FD) {
		if (ehdr->size == sizeof(struct cvp_hfi_msg_session_hdr_ext)
				+ sizeof(struct cvp_hfi_buf_type)) {
			struct msm_cvp_core *core = inst->core;

			dprintk(CVP_PWR, "busy cycle %d, total %d\n",
					ehdr->busy_cycles, ehdr->total_cycles);

			if (core->dyn_clk.sum_fps[HFI_HW_FDU] ||
					core->dyn_clk.sum_fps[HFI_HW_MPU] ||
					core->dyn_clk.sum_fps[HFI_HW_OD] ||
					core->dyn_clk.sum_fps[HFI_HW_ICA]) {
				clock_check = true;
			}
		} else {
			dprintk(CVP_WARN, "dcvs is disabled, %d != %d + %d\n",
					ehdr->size, sizeof(struct cvp_hfi_msg_session_hdr_ext),
					sizeof(struct cvp_hfi_buf_type));
		}
	}

	return clock_check;
}


