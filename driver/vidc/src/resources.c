// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */
/* Copyright (c) 2022. Qualcomm Innovation Center, Inc. All rights reserved. */

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/interconnect.h>

#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_dt.h"
#include "msm_vidc_power.h"
#include "venus_hfi.h"

/* Less than 50MBps is treated as trivial BW change */
#define TRIVIAL_BW_THRESHOLD 50000
#define TRIVIAL_BW_CHANGE(a, b) \
	((a) > (b) ? (a) - (b) < TRIVIAL_BW_THRESHOLD : \
		(b) - (a) < TRIVIAL_BW_THRESHOLD)

enum reset_state {
	INIT = 1,
	ASSERT,
	DEASSERT,
};

static void __fatal_error(bool fatal)
{
	WARN_ON(fatal);
}

static bool is_sys_cache_present(struct msm_vidc_core *core)
{
	return core->dt->sys_cache_present;
}

static void __deinit_clocks(struct msm_vidc_core *core)
{
	struct clock_info *cl;

	core->power.clk_freq = 0;
	venus_hfi_for_each_clock_reverse(core, cl) {
		if (cl->clk) {
			clk_put(cl->clk);
			cl->clk = NULL;
		}
	}
}

static int __init_clocks(struct msm_vidc_core *core)
{
	int rc = 0;
	struct clock_info *cl = NULL;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	venus_hfi_for_each_clock(core, cl) {
		d_vpr_h("%s: scalable? %d, count %d\n",
				cl->name, cl->has_scaling, cl->count);
	}

	venus_hfi_for_each_clock(core, cl) {
		if (!cl->clk) {
			cl->clk = clk_get(&core->pdev->dev, cl->name);
			if (IS_ERR_OR_NULL(cl->clk)) {
				d_vpr_e("Failed to get clock: %s\n", cl->name);
				rc = PTR_ERR(cl->clk) ?
					PTR_ERR(cl->clk) : -EINVAL;
				cl->clk = NULL;
				goto err_clk_get;
			}
		}
	}
	core->power.clk_freq = 0;
	return 0;

err_clk_get:
	__deinit_clocks(core);
	return rc;
}

static int __handle_reset_clk(struct msm_vidc_core *core,
			int reset_index, enum reset_state state)
{
	int rc = 0;
	struct msm_vidc_dt *dt = core->dt;
	struct reset_control *rst;
	struct reset_set *rst_set = &dt->reset_set;

	if (!rst_set->reset_tbl)
		return 0;

	rst = rst_set->reset_tbl[reset_index].rst;
	d_vpr_h("reset_clk: name %s reset_state %d rst %pK\n",
		rst_set->reset_tbl[reset_index].name, state, rst);

	switch (state) {
	case INIT:
		if (rst)
			goto skip_reset_init;

		rst = devm_reset_control_get(&core->pdev->dev,
				rst_set->reset_tbl[reset_index].name);
		if (IS_ERR(rst))
			rc = PTR_ERR(rst);

		rst_set->reset_tbl[reset_index].rst = rst;
		break;
	case ASSERT:
		if (!rst) {
			rc = PTR_ERR(rst);
			goto failed_to_reset;
		}

		rc = reset_control_assert(rst);
		break;
	case DEASSERT:
		if (!rst) {
			rc = PTR_ERR(rst);
			goto failed_to_reset;
		}
		rc = reset_control_deassert(rst);
		break;
	default:
		d_vpr_e("%s: invalid reset request\n", __func__);
		if (rc)
			goto failed_to_reset;
	}

	return 0;

skip_reset_init:
failed_to_reset:
	return rc;
}

static int __reset_ahb2axi_bridge(struct msm_vidc_core *core)
{
	int rc, i;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < core->dt->reset_set.count; i++) {
		rc = __handle_reset_clk(core, i, ASSERT);
		if (rc) {
			d_vpr_e("failed to assert reset clocks\n");
			goto failed_to_reset;
		}

		/* wait for deassert */
		usleep_range(1000, 1100);
	}

	for (i = 0; i < core->dt->reset_set.count; i++) {
		rc = __handle_reset_clk(core, i, DEASSERT);
		if (rc) {
			d_vpr_e("failed to deassert reset clocks\n");
			goto failed_to_reset;
		}
	}

	return 0;

failed_to_reset:
	return rc;
}

static void __deinit_bus(struct msm_vidc_core *core)
{
	struct bus_info *bus = NULL;

	if (!core)
		return;

	core->power.bw_ddr = 0;
	core->power.bw_llcc = 0;

	venus_hfi_for_each_bus_reverse(core, bus) {
		if (!bus->path)
			continue;
		icc_put(bus->path);
		bus->path = NULL;
	}
}

static int __init_bus(struct msm_vidc_core *core)
{
	struct bus_info *bus = NULL;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid param\n", __func__);
		return -EINVAL;
	}

	venus_hfi_for_each_bus(core, bus) {
		if (!strcmp(bus->name, "venus-llcc")) {
			if (msm_vidc_syscache_disable) {
				d_vpr_h("Skipping LLC bus init: %s\n",
					bus->name);
				continue;
			}
		}
		bus->path = of_icc_get(bus->dev, bus->name);
		if (IS_ERR_OR_NULL(bus->path)) {
			rc = PTR_ERR(bus->path) ?
				PTR_ERR(bus->path) : -EBADHANDLE;

			d_vpr_e("Failed to register bus %s: %d\n",
					bus->name, rc);
			bus->path = NULL;
			goto err_add_dev;
		}
	}

	return 0;

err_add_dev:
	__deinit_bus(core);
	return rc;
}

static int __acquire_regulator(struct msm_vidc_core *core,
			       struct regulator_info *rinfo)
{
	int rc = 0;

	if (!core || !rinfo) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (rinfo->has_hw_power_collapse) {
		if (!rinfo->regulator) {
			d_vpr_e("%s: invalid regulator\n", __func__);
			rc = -EINVAL;
			goto exit;
		}

		if (regulator_get_mode(rinfo->regulator) ==
				REGULATOR_MODE_NORMAL) {
			core->handoff_done = false;
			d_vpr_h("Skip acquire regulator %s\n", rinfo->name);
			goto exit;
		}

		rc = regulator_set_mode(rinfo->regulator,
				REGULATOR_MODE_NORMAL);
		if (rc) {
			/*
			 * This is somewhat fatal, but nothing we can do
			 * about it. We can't disable the regulator w/o
			 * getting it back under s/w control
			 */
			d_vpr_e("Failed to acquire regulator control: %s\n",
				rinfo->name);
			goto exit;
		} else {
			core->handoff_done = false;
			d_vpr_h("Acquired regulator control from HW: %s\n",
					rinfo->name);

		}

		if (!regulator_is_enabled(rinfo->regulator)) {
			d_vpr_e("%s: Regulator is not enabled %s\n",
				__func__, rinfo->name);
			__fatal_error(true);
		}
	}

exit:
	return rc;
}

static int __acquire_regulators(struct msm_vidc_core *core)
{
	int rc = 0;
	struct regulator_info *rinfo;

	venus_hfi_for_each_regulator(core, rinfo)
		__acquire_regulator(core, rinfo);

	return rc;
}

static int __hand_off_regulator(struct msm_vidc_core *core,
	struct regulator_info *rinfo)
{
	int rc = 0;

	if (rinfo->has_hw_power_collapse) {
		if (!rinfo->regulator) {
			d_vpr_e("%s: invalid regulator\n", __func__);
			return -EINVAL;
		}

		rc = regulator_set_mode(rinfo->regulator,
				REGULATOR_MODE_FAST);
		if (rc) {
			d_vpr_e("Failed to hand off regulator control: %s\n",
				rinfo->name);
			return rc;
		} else {
			core->handoff_done = true;
			d_vpr_h("Hand off regulator control to HW: %s\n",
					rinfo->name);
		}

		if (!regulator_is_enabled(rinfo->regulator)) {
			d_vpr_e("%s: Regulator is not enabled %s\n",
				__func__, rinfo->name);
			__fatal_error(true);
		}
	}

	return rc;
}

static int __hand_off_regulators(struct msm_vidc_core *core)
{
	struct regulator_info *rinfo;
	int rc = 0, c = 0;

	venus_hfi_for_each_regulator(core, rinfo) {
		rc = __hand_off_regulator(core, rinfo);
		/*
		 * If one regulator hand off failed, driver should take
		 * the control for other regulators back.
		 */
		if (rc)
			goto err_reg_handoff_failed;
		c++;
	}

	return rc;
err_reg_handoff_failed:
	venus_hfi_for_each_regulator_reverse_continue(core, rinfo, c)
		__acquire_regulator(core, rinfo);

	return rc;
}

static int __disable_regulator(struct msm_vidc_core *core, const char *reg_name)
{
	int rc = 0;
	struct regulator_info *rinfo;
	bool found;

	if (!core || !reg_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_regulator(core, rinfo) {
		if (!rinfo->regulator) {
			d_vpr_e("%s: invalid regulator %s\n",
				__func__, rinfo->name);
			return -EINVAL;
		}
		if (strcmp(rinfo->name, reg_name))
			continue;
		found = true;

		rc = __acquire_regulator(core, rinfo);
		if (rc) {
			d_vpr_e("%s: failed to acquire %s, rc = %d\n",
				__func__, rinfo->name, rc);
			/* Bring attention to this issue */
			WARN_ON(true);
			return rc;
		}
		core->handoff_done = false;

		rc = regulator_disable(rinfo->regulator);
		if (rc) {
			d_vpr_e("%s: failed to disable %s, rc = %d\n",
				__func__, rinfo->name, rc);
			return rc;
		}
		d_vpr_h("%s: disabled regulator %s\n", __func__, rinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: regulator %s not found\n", __func__, reg_name);
		return -EINVAL;
	}

	return rc;
}

static int __enable_regulator(struct msm_vidc_core *core, const char *reg_name)
{
	int rc = 0;
	struct regulator_info *rinfo;
	bool found;

	if (!core || !reg_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_regulator(core, rinfo) {
		if (!rinfo->regulator) {
			d_vpr_e("%s: invalid regulator %s\n",
				__func__, rinfo->name);
			return -EINVAL;
		}
		if (strcmp(rinfo->name, reg_name))
			continue;
		found = true;

		rc = regulator_enable(rinfo->regulator);
		if (rc) {
			d_vpr_e("%s: failed to enable %s, rc = %d\n",
				__func__, rinfo->name, rc);
			return rc;
		}
		if (!regulator_is_enabled(rinfo->regulator)) {
			d_vpr_e("%s: regulator %s not enabled\n",
				__func__, rinfo->name);
			regulator_disable(rinfo->regulator);
			return -EINVAL;
		}
		d_vpr_h("%s: enabled regulator %s\n", __func__, rinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: regulator %s not found\n", __func__, reg_name);
		return -EINVAL;
	}

	return rc;
}

#ifdef CONFIG_MSM_MMRM
static void __deregister_mmrm(struct msm_vidc_core *core)
{
	struct clock_info *cl;

	if (!core || !core->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}

	if (!core->capabilities[MMRM].value) {
		d_vpr_h("%s: MMRM not supported\n", __func__);
		return;
	}

	venus_hfi_for_each_clock(core, cl) {
		if (cl->has_scaling && cl->mmrm_client) {
			mmrm_client_deregister(cl->mmrm_client);
			cl->mmrm_client = NULL;
		}
	}
}
#else
static void __deregister_mmrm(struct msm_vidc_core *core)
{
}
#endif

#ifdef CONFIG_MSM_MMRM
static int __register_mmrm(struct msm_vidc_core *core)
{
	int rc = 0;
	struct clock_info *cl;

	if (!core ||!core->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!core->capabilities[MMRM].value) {
		d_vpr_h("%s: MMRM not supported\n", __func__);
		return 0;
	}

	venus_hfi_for_each_clock(core, cl) {
		struct mmrm_client_desc desc;
		char *name = (char *)desc.client_info.desc.name;

		// TODO: set notifier data vals
		struct mmrm_client_notifier_data notifier_data = {
			MMRM_CLIENT_RESOURCE_VALUE_CHANGE,
			{{0, 0}},
			NULL};

		// TODO: add callback fn
		desc.notifier_callback_fn = NULL;

		if (!cl->has_scaling)
			continue;

		if (IS_ERR_OR_NULL(cl->clk)) {
			d_vpr_e("%s: Invalid clock: %s\n", __func__, cl->name);
			rc = PTR_ERR(cl->clk) ? PTR_ERR(cl->clk) : -EINVAL;
			goto err_register_mmrm;
		}

		desc.client_type = MMRM_CLIENT_CLOCK;
		desc.client_info.desc.client_domain = MMRM_CLIENT_DOMAIN_VIDEO;
		desc.client_info.desc.client_id = cl->clk_id;
		strlcpy(name, cl->name, sizeof(desc.client_info.desc.name));
		desc.client_info.desc.clk = cl->clk;
		desc.priority = MMRM_CLIENT_PRIOR_LOW;
		desc.pvt_data = notifier_data.pvt_data;

		d_vpr_h("%s: domain(%d) cid(%d) name(%s) clk(%pK)\n",
			__func__,
			desc.client_info.desc.client_domain,
			desc.client_info.desc.client_id,
			desc.client_info.desc.name,
			desc.client_info.desc.clk);

		d_vpr_h("%s: type(%d) pri(%d) pvt(%pK) notifier(%pK)\n",
			__func__,
			desc.client_type,
			desc.priority,
			desc.pvt_data,
			desc.notifier_callback_fn);

		cl->mmrm_client = mmrm_client_register(&desc);
		if (!cl->mmrm_client) {
			d_vpr_e("%s: Failed to register clk(%s): %d\n",
				__func__, cl->name, rc);
			rc = -EINVAL;
			goto err_register_mmrm;
		}
	}

	return 0;

err_register_mmrm:
	__deregister_mmrm(core);
	return rc;
}
#else
static int __register_mmrm(struct msm_vidc_core *core)
{
	return 0;
}
#endif

static void __deinit_regulators(struct msm_vidc_core *core)
{
	struct regulator_info *rinfo = NULL;

	venus_hfi_for_each_regulator_reverse(core, rinfo) {
		if (rinfo->regulator) {
			regulator_put(rinfo->regulator);
			rinfo->regulator = NULL;
		}
	}
}

static int __init_regulators(struct msm_vidc_core *core)
{
	int rc = 0;
	struct regulator_info *rinfo = NULL;

	venus_hfi_for_each_regulator(core, rinfo) {
		rinfo->regulator = regulator_get(&core->pdev->dev, rinfo->name);
		if (IS_ERR_OR_NULL(rinfo->regulator)) {
			rc = PTR_ERR(rinfo->regulator) ?
				PTR_ERR(rinfo->regulator) : -EBADHANDLE;
			d_vpr_e("Failed to get regulator: %s\n", rinfo->name);
			rinfo->regulator = NULL;
			goto err_reg_get;
		}
	}

	return 0;

err_reg_get:
	__deinit_regulators(core);
	return rc;
}

static void __deinit_subcaches(struct msm_vidc_core *core)
{
	struct subcache_info *sinfo = NULL;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		goto exit;
	}

	if (!is_sys_cache_present(core))
		goto exit;

	venus_hfi_for_each_subcache_reverse(core, sinfo) {
		if (sinfo->subcache) {
			d_vpr_h("deinit_subcaches: %s\n", sinfo->name);
			llcc_slice_putd(sinfo->subcache);
			sinfo->subcache = NULL;
		}
	}

exit:
	return;
}

static int __init_subcaches(struct msm_vidc_core *core)
{
	int rc = 0;
	struct subcache_info *sinfo = NULL;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!is_sys_cache_present(core))
		return 0;

	venus_hfi_for_each_subcache(core, sinfo) {
		if (!strcmp("vidsc0", sinfo->name)) {
			sinfo->subcache = llcc_slice_getd(LLCC_VIDSC0);
		} else if (!strcmp("vidsc1", sinfo->name)) {
			sinfo->subcache = llcc_slice_getd(LLCC_VIDSC1);
		} else if (!strcmp("vidscfw", sinfo->name)) {
			sinfo->subcache = llcc_slice_getd(LLCC_VIDFW);
		}else if (!strcmp("vidvsp", sinfo->name)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0))
			sinfo->subcache = llcc_slice_getd(LLCC_VIDVSP);
#endif
		} else {
			d_vpr_e("Invalid subcache name %s\n",
					sinfo->name);
		}
		if (IS_ERR_OR_NULL(sinfo->subcache)) {
			rc = PTR_ERR(sinfo->subcache) ?
				PTR_ERR(sinfo->subcache) : -EBADHANDLE;
			d_vpr_e("init_subcaches: invalid subcache: %s rc %d\n",
				sinfo->name, rc);
			sinfo->subcache = NULL;
			goto err_subcache_get;
		}
		d_vpr_h("init_subcaches: %s\n", sinfo->name);
	}

	return 0;

err_subcache_get:
	__deinit_subcaches(core);
	return rc;
}

static int __disable_subcaches(struct msm_vidc_core *core)
{
	struct subcache_info *sinfo;
	int rc = 0;

	if (msm_vidc_syscache_disable || !is_sys_cache_present(core))
		return 0;

	/* De-activate subcaches */
	venus_hfi_for_each_subcache_reverse(core, sinfo) {
		if (sinfo->isactive) {
			d_vpr_h("De-activate subcache %s\n",
				sinfo->name);
			rc = llcc_slice_deactivate(sinfo->subcache);
			if (rc) {
				d_vpr_e("Failed to de-activate %s: %d\n",
					sinfo->name, rc);
			}
			sinfo->isactive = false;
		}
	}

	return 0;
}

static int __enable_subcaches(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 c = 0;
	struct subcache_info *sinfo;

	if (msm_vidc_syscache_disable || !is_sys_cache_present(core))
		return 0;

	/* Activate subcaches */
	venus_hfi_for_each_subcache(core, sinfo) {
		rc = llcc_slice_activate(sinfo->subcache);
		if (rc) {
			d_vpr_e("Failed to activate %s: %d\n", sinfo->name, rc);
			__fatal_error(true);
			goto err_activate_fail;
		}
		sinfo->isactive = true;
		d_vpr_h("Activated subcache %s\n", sinfo->name);
		c++;
	}

	d_vpr_h("Activated %d Subcaches to Venus\n", c);

	return 0;

err_activate_fail:
	__disable_subcaches(core);
	return rc;
}

static int llcc_enable(struct msm_vidc_core *core, bool enable)
{
	int ret;

	if (enable)
		ret = __enable_subcaches(core);
	else
		ret = __disable_subcaches(core);

	return ret;
}

static int __vote_bandwidth(struct bus_info *bus, unsigned long bw_kbps)
{
	int rc = 0;

	if (!bus->path) {
		d_vpr_e("%s: invalid bus\n", __func__);
		return -EINVAL;
	}

	d_vpr_p("Voting bus %s to ab %lu kBps\n", bus->name, bw_kbps);

	rc = icc_set_bw(bus->path, bw_kbps, 0);
	if (rc)
		d_vpr_e("Failed voting bus %s to ab %lu, rc=%d\n",
			bus->name, bw_kbps, rc);

	return rc;
}

static int __unvote_buses(struct msm_vidc_core *core)
{
	int rc = 0;
	struct bus_info *bus = NULL;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core->power.bw_ddr = 0;
	core->power.bw_llcc = 0;

	venus_hfi_for_each_bus(core, bus) {
		rc = __vote_bandwidth(bus, 0);
		if (rc)
			goto err_unknown_device;
	}

err_unknown_device:
	return rc;
}

static int __vote_buses(struct msm_vidc_core *core,
			unsigned long bw_ddr, unsigned long bw_llcc)
{
	int rc = 0;
	struct bus_info *bus = NULL;
	unsigned long bw_kbps = 0, bw_prev = 0;
	enum vidc_bus_type type;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	venus_hfi_for_each_bus(core, bus) {
		if (bus && bus->path) {
			type = get_type_frm_name(bus->name);

			if (type == DDR) {
				bw_kbps = bw_ddr;
				bw_prev = core->power.bw_ddr;
			} else if (type == LLCC) {
				bw_kbps = bw_llcc;
				bw_prev = core->power.bw_llcc;
			} else {
				bw_kbps = bus->range[1];
				bw_prev = core->power.bw_ddr ?
						bw_kbps : 0;
			}

			/* ensure freq is within limits */
			bw_kbps = clamp_t(typeof(bw_kbps), bw_kbps,
						 bus->range[0], bus->range[1]);

			if (TRIVIAL_BW_CHANGE(bw_kbps, bw_prev) && bw_prev) {
				d_vpr_l("Skip voting bus %s to %lu kBps\n",
					bus->name, bw_kbps);
				continue;
			}

			rc = __vote_bandwidth(bus, bw_kbps);

			if (type == DDR)
				core->power.bw_ddr = bw_kbps;
			else if (type == LLCC)
				core->power.bw_llcc = bw_kbps;
		} else {
			d_vpr_e("No BUS to Vote\n");
		}
	}

	return rc;
}

static int set_bw(struct msm_vidc_core *core, unsigned long bw_ddr,
		  unsigned long bw_llcc)
{
	if (!bw_ddr && !bw_llcc)
		return __unvote_buses(core);

	return __vote_buses(core, bw_ddr, bw_llcc);
}

#ifdef CONFIG_MSM_MMRM
static int __set_clk_rate(struct msm_vidc_core *core, struct clock_info *cl,
			  u64 rate)
{
	int rc = 0;
	struct mmrm_client_data client_data;
	struct mmrm_client *client;

	/* not registered */
	if (!core || !cl || !core->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (core->capabilities[MMRM].value && !cl->mmrm_client) {
		d_vpr_e("%s: invalid mmrm client\n", __func__);
		return -EINVAL;
	}

	/*
	 * This conversion is necessary since we are scaling clock values based on
	 * the branch clock. However, mmrm driver expects source clock to be registered
	 * and used for scaling.
	 * TODO: Remove this scaling if using source clock instead of branch clock.
	 */
	rate = rate * MSM_VIDC_CLOCK_SOURCE_SCALING_RATIO;

	/* bail early if requested clk rate is not changed */
	if (rate == cl->prev)
		return 0;

	d_vpr_p("Scaling clock %s to %llu, prev %llu\n", cl->name, rate, cl->prev);

	if (core->capabilities[MMRM].value) {
		/* set clock rate to mmrm driver */
		client = cl->mmrm_client;
		memset(&client_data, 0, sizeof(client_data));
		client_data.num_hw_blocks = 1;
		rc = mmrm_client_set_value(client, &client_data, rate);
		if (rc) {
			d_vpr_e("%s: Failed to set mmrm clock rate %llu %s: %d\n",
				__func__, rate, cl->name, rc);
			return rc;
		}
	} else {
		/* set clock rate to clock driver */
		rc = clk_set_rate(cl->clk, rate);
		if (rc) {
			d_vpr_e("%s: Failed to set clock rate %llu %s: %d\n",
				__func__, rate, cl->name, rc);
			return rc;
		}
	}
	cl->prev = rate;
	return rc;
}
#else
static int __set_clk_rate(struct msm_vidc_core *core, struct clock_info *cl,
			  u64 rate)
{
	int rc = 0;

	/* not registered */
	if (!core || !cl || !core->capabilities) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/*
	 * This conversion is necessary since we are scaling clock values based on
	 * the branch clock. However, mmrm driver expects source clock to be registered
	 * and used for scaling.
	 * TODO: Remove this scaling if using source clock instead of branch clock.
	 */
	rate = rate * MSM_VIDC_CLOCK_SOURCE_SCALING_RATIO;

	/* bail early if requested clk rate is not changed */
	if (rate == cl->prev)
		return 0;

	d_vpr_p("Scaling clock %s to %llu, prev %llu\n", cl->name, rate, cl->prev);

	rc = clk_set_rate(cl->clk, rate);
	if (rc) {
		d_vpr_e("%s: Failed to set clock rate %llu %s: %d\n",
			__func__, rate, cl->name, rc);
		return rc;
	}

	cl->prev = rate;

	return rc;
}
#endif

static int __set_clocks(struct msm_vidc_core *core, u64 freq)
{
	int rc = 0;
	struct clock_info *cl;

	venus_hfi_for_each_clock(core, cl) {
		if (cl->has_scaling) {
			rc = __set_clk_rate(core, cl, freq);
			if (rc)
				return rc;
		}
	}

	return 0;
}

static int __disable_unprepare_clock(struct msm_vidc_core *core,
				     const char *clk_name)
{
	int rc = 0;
	struct clock_info *cl;
	bool found;

	if (!core || !clk_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_clock(core, cl) {
		if (!cl->clk) {
			d_vpr_e("%s: invalid clock %s\n", __func__, cl->name);
			return -EINVAL;
		}
		if (strcmp(cl->name, clk_name))
			continue;
		found = true;
		clk_disable_unprepare(cl->clk);
		if (cl->has_scaling)
			__set_clk_rate(core, cl, 0);
		cl->prev = 0;
		d_vpr_h("%s: clock %s disable unprepared\n", __func__, cl->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: clock %s not found\n", __func__, clk_name);
		return -EINVAL;
	}

	return rc;
}

static int __prepare_enable_clock(struct msm_vidc_core *core,
				  const char *clk_name)
{
	int rc = 0;
	struct clock_info *cl;
	bool found;
	u64 rate = 0;

	if (!core || !clk_name) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	found = false;
	venus_hfi_for_each_clock(core, cl) {
		if (!cl->clk) {
			d_vpr_e("%s: invalid clock\n", __func__);
			return -EINVAL;
		}
		if (strcmp(cl->name, clk_name))
			continue;
		found = true;
		/*
		 * For the clocks we control, set the rate prior to preparing
		 * them.  Since we don't really have a load at this point, scale
		 * it to the lowest frequency possible
		 */
		if (cl->has_scaling) {
			rate = clk_round_rate(cl->clk, 0);
			/**
			 * source clock is already multipled with scaling ratio and __set_clk_rate
			 * attempts to multiply again. So divide scaling ratio before calling
			 * __set_clk_rate.
			 */
			rate = rate / MSM_VIDC_CLOCK_SOURCE_SCALING_RATIO;
			__set_clk_rate(core, cl, rate);
		}

		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			d_vpr_e("%s: failed to enable clock %s\n",
				__func__, cl->name);
			return rc;
		}
		if (!__clk_is_enabled(cl->clk)) {
			d_vpr_e("%s: clock %s not enabled\n",
				__func__, cl->name);
			clk_disable_unprepare(cl->clk);
			if (cl->has_scaling)
				__set_clk_rate(core, cl, 0);
			return -EINVAL;
		}
		d_vpr_h("%s: clock %s prepare enabled\n", __func__, cl->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: clock %s not found\n", __func__, clk_name);
		return -EINVAL;
	}

	return rc;
}

static int __init_resources(struct msm_vidc_core *core)
{
	int i, rc = 0;

	rc = __init_regulators(core);
	if (rc) {
		d_vpr_e("Failed to get all regulators\n");
		return -ENODEV;
	}

	rc = __init_clocks(core);
	if (rc) {
		d_vpr_e("Failed to init clocks\n");
		rc = -ENODEV;
		goto err_init_clocks;
	}

	rc = __register_mmrm(core);
	if (rc) {
		d_vpr_e("Failed to register mmrm\n");
		rc = -ENODEV;
		goto err_init_mmrm;
	}

	for (i = 0; i < core->dt->reset_set.count; i++) {
		rc = __handle_reset_clk(core, i, INIT);
		if (rc) {
			d_vpr_e("Failed to init reset clocks\n");
			rc = -ENODEV;
			goto err_init_reset_clk;
		}
	}

	rc = __init_bus(core);
	if (rc) {
		d_vpr_e("Failed to init bus: %d\n", rc);
		goto err_init_bus;
	}

	rc = __init_subcaches(core);
	if (rc)
		d_vpr_e("Failed to init subcaches: %d\n", rc);

	return rc;

err_init_reset_clk:
err_init_bus:
	__deregister_mmrm(core);
err_init_mmrm:
	__deinit_clocks(core);
err_init_clocks:
	__deinit_regulators(core);
	return rc;
}

static void __deinit_resources(struct msm_vidc_core *core)
{
	__deinit_subcaches(core);
	__deinit_bus(core);
	__deregister_mmrm(core);
	__deinit_clocks(core);
	__deinit_regulators(core);
}

static const struct msm_vidc_resources_ops res_ops = {
	.get = __init_resources,
	.put = __deinit_resources,
	.reset_bridge = __reset_ahb2axi_bridge,
	.gdsc_on = __enable_regulator,
	.gdsc_off = __disable_regulator,
	.gdsc_hw_ctrl = __hand_off_regulators,
	.gdsc_sw_ctrl = __acquire_regulators,
	.llcc = llcc_enable,
	.set_bw = set_bw,
	.set_clks = __set_clocks,
	.clk_enable = __prepare_enable_clock,
	.clk_disable = __disable_unprepare_clock,
};

const struct msm_vidc_resources_ops *get_resources_ops(void)
{
	return &res_ops;
}
