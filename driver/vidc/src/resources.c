// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */
/* Copyright (c) 2022. Qualcomm Innovation Center, Inc. All rights reserved. */

#include <linux/sort.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/interconnect.h>
#include <linux/soc/qcom/llcc-qcom.h>
#ifdef CONFIG_MSM_MMRM
#include <linux/soc/qcom/msm_mmrm.h>
#endif

#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_power.h"
#include "msm_vidc_platform.h"
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

static void devm_llcc_release(struct device *dev, void *res)
{
	d_vpr_h("%s()\n", __func__);
	llcc_slice_putd(*(struct llcc_slice_desc **)res);
}

static struct llcc_slice_desc *devm_llcc_get(struct device *dev, u32 id)
{
	struct llcc_slice_desc **ptr, *llcc;

	ptr = devres_alloc(devm_llcc_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	llcc = llcc_slice_getd(id);
	if (!IS_ERR(llcc)) {
		*ptr = llcc;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return llcc;
}

#ifdef CONFIG_MSM_MMRM
static void devm_mmrm_release(struct device *dev, void *res)
{
	d_vpr_h("%s()\n", __func__);
	mmrm_client_deregister(*(struct mmrm_client **)res);
}

static struct mmrm_client *devm_mmrm_get(struct device *dev, struct mmrm_client_desc *desc)
{
	struct mmrm_client **ptr, *mmrm;

	ptr = devres_alloc(devm_mmrm_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mmrm = mmrm_client_register(desc);
	if (!IS_ERR(mmrm)) {
		*ptr = mmrm;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return mmrm;
}
#endif

/* A comparator to compare loads (needed later on) */
static inline int cmp(const void *a, const void *b)
{
	/* want to sort in reverse so flip the comparison */
	return ((struct freq_table *)b)->freq -
		((struct freq_table *)a)->freq;
}

static int __init_register_base(struct msm_vidc_core *core)
{
	struct msm_vidc_resource *res;

	if (!core || !core->pdev || !core->resource) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	res = core->resource;

	res->register_base_addr = devm_platform_ioremap_resource(core->pdev, 0);
	if (IS_ERR(res->register_base_addr)) {
		d_vpr_e("%s: map reg addr failed %ld\n",
			__func__, PTR_ERR(res->register_base_addr));
		return -EINVAL;
	}
	d_vpr_h("%s: reg_base %#x\n", __func__, res->register_base_addr);

	return 0;
}

static int __init_irq(struct msm_vidc_core *core)
{
	struct msm_vidc_resource *res;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0))
	struct resource *kres;
#endif
	int rc = 0;

	if (!core || !core->pdev || !core->resource) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	res = core->resource;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0))
	res->irq = platform_get_irq(core->pdev, 0);
#else
	kres = platform_get_resource(core->pdev, IORESOURCE_IRQ, 0);
	res->irq = kres ? kres->start : -1;
#endif
	if (res->irq < 0)
		d_vpr_e("%s: get irq failed, %d\n", __func__, res->irq);

	d_vpr_h("%s: irq %d\n", __func__, res->irq);

	rc = devm_request_threaded_irq(&core->pdev->dev, res->irq, venus_hfi_isr,
			venus_hfi_isr_handler, IRQF_TRIGGER_HIGH, "msm-vidc", core);
	if (rc) {
		d_vpr_e("%s: Failed to allocate venus IRQ\n", __func__);
		return rc;
	}
	disable_irq_nosync(res->irq);

	return rc;
}

static int __init_bus(struct msm_vidc_core *core)
{
	const struct bw_table *bus_tbl;
	struct bus_set *interconnects;
	struct bus_info *binfo = NULL;
	u32 bus_count = 0, cnt = 0;
	int rc = 0;

	if (!core || !core->resource || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	interconnects = &core->resource->bus_set;

	bus_tbl = core->platform->data.bw_tbl;
	bus_count = core->platform->data.bw_tbl_size;

	if (!bus_tbl || !bus_count) {
		d_vpr_e("%s: invalid bus tbl %#x or count %d\n",
			__func__, bus_tbl, bus_count);
		return -EINVAL;
	}

	/* allocate bus_set */
	interconnects->bus_tbl = devm_kzalloc(&core->pdev->dev,
			sizeof(*interconnects->bus_tbl) * bus_count, GFP_KERNEL);
	if (!interconnects->bus_tbl) {
		d_vpr_e("%s: failed to alloc memory for bus table\n", __func__);
		return -ENOMEM;
	}
	interconnects->count = bus_count;

	/* populate bus field from platform data */
	for (cnt = 0; cnt < interconnects->count; cnt++) {
		interconnects->bus_tbl[cnt].name = bus_tbl[cnt].name;
		interconnects->bus_tbl[cnt].min_kbps = bus_tbl[cnt].min_kbps;
		interconnects->bus_tbl[cnt].max_kbps = bus_tbl[cnt].max_kbps;
	}

	/* print bus fields */
	venus_hfi_for_each_bus(core, binfo) {
		d_vpr_h("%s: name %s min_kbps %u max_kbps %u\n",
			__func__, binfo->name, binfo->min_kbps, binfo->max_kbps);
	}

	/* get interconnect handle */
	venus_hfi_for_each_bus(core, binfo) {
		if (!strcmp(binfo->name, "venus-llcc")) {
			if (msm_vidc_syscache_disable) {
				d_vpr_h("%s: skipping LLC bus init: %s\n", __func__,
					binfo->name);
				continue;
			}
		}
		binfo->icc = devm_of_icc_get(&core->pdev->dev, binfo->name);
		if (IS_ERR_OR_NULL(binfo->icc)) {
			d_vpr_e("%s: failed to get bus: %s\n", __func__, binfo->name);
			rc = PTR_ERR(binfo->icc) ?
				PTR_ERR(binfo->icc) : -EBADHANDLE;
			binfo->icc = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_regulators(struct msm_vidc_core *core)
{
	const struct regulator_table *regulator_tbl;
	struct regulator_set *regulators;
	struct regulator_info *rinfo = NULL;
	u32 regulator_count = 0, cnt = 0;
	int rc = 0;

	if (!core || !core->resource || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	regulators = &core->resource->regulator_set;

	/* skip init if regulators not supported */
	if (!is_regulator_supported(core)) {
		d_vpr_h("%s: regulators are not available in database\n", __func__);
		return 0;
	}

	regulator_tbl = core->platform->data.regulator_tbl;
	regulator_count = core->platform->data.regulator_tbl_size;

	if (!regulator_tbl || !regulator_count) {
		d_vpr_e("%s: invalid regulator tbl %#x or count %d\n",
			__func__, regulator_tbl, regulator_count);
		return -EINVAL;
	}

	/* allocate regulator_set */
	regulators->regulator_tbl = devm_kzalloc(&core->pdev->dev,
			sizeof(*regulators->regulator_tbl) * regulator_count, GFP_KERNEL);
	if (!regulators->regulator_tbl) {
		d_vpr_e("%s: failed to alloc memory for regulator table\n", __func__);
		return -ENOMEM;
	}
	regulators->count = regulator_count;

	/* populate regulator fields */
	for (cnt = 0; cnt < regulators->count; cnt++) {
		regulators->regulator_tbl[cnt].name = regulator_tbl[cnt].name;
		regulators->regulator_tbl[cnt].hw_power_collapse = regulator_tbl[cnt].hw_trigger;
	}

	/* print regulator fields */
	venus_hfi_for_each_regulator(core, rinfo) {
		d_vpr_h("%s: name %s hw_power_collapse %d\n",
			__func__, rinfo->name, rinfo->hw_power_collapse);
	}

	/* get regulator handle */
	venus_hfi_for_each_regulator(core, rinfo) {
		rinfo->regulator = devm_regulator_get(&core->pdev->dev, rinfo->name);
		if (IS_ERR_OR_NULL(rinfo->regulator)) {
			rc = PTR_ERR(rinfo->regulator) ?
				PTR_ERR(rinfo->regulator) : -EBADHANDLE;
			d_vpr_e("%s: failed to get regulator: %s\n", __func__, rinfo->name);
			rinfo->regulator = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_clocks(struct msm_vidc_core *core)
{
	const struct clk_table *clk_tbl;
	struct clock_set *clocks;
	struct clock_info *cinfo = NULL;
	u32 clk_count = 0, cnt = 0;
	int rc = 0;

	if (!core || !core->resource || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	clocks = &core->resource->clock_set;

	clk_tbl = core->platform->data.clk_tbl;
	clk_count = core->platform->data.clk_tbl_size;

	if (!clk_tbl || !clk_count) {
		d_vpr_e("%s: invalid clock tbl %#x or count %d\n",
			__func__, clk_tbl, clk_count);
		return -EINVAL;
	}

	/* allocate clock_set */
	clocks->clock_tbl = devm_kzalloc(&core->pdev->dev,
			sizeof(*clocks->clock_tbl) * clk_count, GFP_KERNEL);
	if (!clocks->clock_tbl) {
		d_vpr_e("%s: failed to alloc memory for clock table\n", __func__);
		return -ENOMEM;
	}
	clocks->count = clk_count;

	/* populate clock field from platform data */
	for (cnt = 0; cnt < clocks->count; cnt++) {
		clocks->clock_tbl[cnt].name = clk_tbl[cnt].name;
		clocks->clock_tbl[cnt].clk_id = clk_tbl[cnt].clk_id;
		clocks->clock_tbl[cnt].has_scaling = clk_tbl[cnt].scaling;
	}

	/* print clock fields */
	venus_hfi_for_each_clock(core, cinfo) {
		d_vpr_h("%s: clock name %s clock id %#x scaling %d\n",
			__func__, cinfo->name, cinfo->clk_id, cinfo->has_scaling);
	}

	/* get clock handle */
	venus_hfi_for_each_clock(core, cinfo) {
		cinfo->clk = devm_clk_get(&core->pdev->dev, cinfo->name);
		if (IS_ERR_OR_NULL(cinfo->clk)) {
			d_vpr_e("%s: failed to get clock: %s\n", __func__, cinfo->name);
			rc = PTR_ERR(cinfo->clk) ?
				PTR_ERR(cinfo->clk) : -EINVAL;
			cinfo->clk = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_reset_clocks(struct msm_vidc_core *core)
{
	const struct clk_rst_table *rst_tbl;
	struct reset_set *rsts;
	struct reset_info *rinfo = NULL;
	u32 rst_count = 0, cnt = 0;
	int rc = 0;

	if (!core || !core->resource || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	rsts = &core->resource->reset_set;

	rst_tbl = core->platform->data.clk_rst_tbl;
	rst_count = core->platform->data.clk_rst_tbl_size;

	if (!rst_tbl || !rst_count) {
		d_vpr_e("%s: invalid reset tbl %#x or count %d\n",
			__func__, rst_tbl, rst_count);
		return -EINVAL;
	}

	/* allocate reset_set */
	rsts->reset_tbl = devm_kzalloc(&core->pdev->dev,
			sizeof(*rsts->reset_tbl) * rst_count, GFP_KERNEL);
	if (!rsts->reset_tbl) {
		d_vpr_e("%s: failed to alloc memory for reset table\n", __func__);
		return -ENOMEM;
	}
	rsts->count = rst_count;

	/* populate clock field from platform data */
	for (cnt = 0; cnt < rsts->count; cnt++)
		rsts->reset_tbl[cnt].name = rst_tbl[cnt].name;

	/* print reset clock fields */
	venus_hfi_for_each_reset_clock(core, rinfo) {
		d_vpr_h("%s: reset clk %s\n", __func__, rinfo->name);
	}

	/* get reset clock handle */
	venus_hfi_for_each_reset_clock(core, rinfo) {
		rinfo->rst = devm_reset_control_get(&core->pdev->dev, rinfo->name);
		if (IS_ERR_OR_NULL(rinfo->rst)) {
			d_vpr_e("%s: failed to get reset clock: %s\n", __func__, rinfo->name);
			rc = PTR_ERR(rinfo->rst) ?
				PTR_ERR(rinfo->rst) : -EINVAL;
			rinfo->rst = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_subcaches(struct msm_vidc_core *core)
{
	const struct subcache_table *llcc_tbl;
	struct subcache_set *caches;
	struct subcache_info *sinfo = NULL;
	u32 llcc_count = 0, cnt = 0;
	int rc = 0;

	if (!core || !core->resource || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	caches = &core->resource->subcache_set;

	/* skip init if subcache not available */
	if (!is_sys_cache_present(core))
		return 0;

	llcc_tbl = core->platform->data.subcache_tbl;
	llcc_count = core->platform->data.subcache_tbl_size;

	if (!llcc_tbl || !llcc_count) {
		d_vpr_e("%s: invalid llcc tbl %#x or count %d\n",
			__func__, llcc_tbl, llcc_count);
		return -EINVAL;
	}

	/* allocate clock_set */
	caches->subcache_tbl = devm_kzalloc(&core->pdev->dev,
			sizeof(*caches->subcache_tbl) * llcc_count, GFP_KERNEL);
	if (!caches->subcache_tbl) {
		d_vpr_e("%s: failed to alloc memory for subcache table\n", __func__);
		return -ENOMEM;
	}
	caches->count = llcc_count;

	/* populate subcache fields from platform data */
	for (cnt = 0; cnt < caches->count; cnt++) {
		caches->subcache_tbl[cnt].name = llcc_tbl[cnt].name;
		caches->subcache_tbl[cnt].llcc_id = llcc_tbl[cnt].llcc_id;
	}

	/* print subcache fields */
	venus_hfi_for_each_subcache(core, sinfo) {
		d_vpr_h("%s: name %s subcache id %d\n",
			__func__, sinfo->name, sinfo->llcc_id);
	}

	/* get subcache/llcc handle */
	venus_hfi_for_each_subcache(core, sinfo) {
		sinfo->subcache = devm_llcc_get(&core->pdev->dev, sinfo->llcc_id);
		if (IS_ERR_OR_NULL(sinfo->subcache)) {
			d_vpr_e("%s: failed to get subcache: %d\n", __func__, sinfo->llcc_id);
			rc = PTR_ERR(sinfo->subcache) ?
				PTR_ERR(sinfo->subcache) : -EBADHANDLE;
			sinfo->subcache = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_freq_table(struct msm_vidc_core *core)
{
	struct freq_table *freq_tbl;
	struct freq_set *clks;
	u32 freq_count = 0, cnt = 0;
	int rc = 0;

	if (!core || !core->resource || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	clks = &core->resource->freq_set;

	freq_tbl = core->platform->data.freq_tbl;
	freq_count = core->platform->data.freq_tbl_size;

	if (!freq_tbl || !freq_count) {
		d_vpr_e("%s: invalid freq tbl %#x or count %d\n",
			__func__, freq_tbl, freq_count);
		return -EINVAL;
	}

	/* allocate freq_set */
	clks->freq_tbl = devm_kzalloc(&core->pdev->dev,
			sizeof(*clks->freq_tbl) * freq_count, GFP_KERNEL);
	if (!clks->freq_tbl) {
		d_vpr_e("%s: failed to alloc memory for freq table\n", __func__);
		return -ENOMEM;
	}
	clks->count = freq_count;

	/* populate freq field from platform data */
	for (cnt = 0; cnt < clks->count; cnt++)
		clks->freq_tbl[cnt].freq = freq_tbl[cnt].freq;

	/* sort freq table */
	sort(clks->freq_tbl, clks->count, sizeof(*clks->freq_tbl), cmp, NULL);

	/* print freq field freq_set */
	d_vpr_h("%s: updated freq table\n", __func__);
	for (cnt = 0; cnt < clks->count; cnt++)
		d_vpr_h("%s:\t %lu\n", __func__, clks->freq_tbl[cnt].freq);

	return rc;
}

static int __init_context_banks(struct msm_vidc_core *core)
{
	const struct context_bank_table *cb_tbl;
	struct context_bank_set *cbs;
	struct context_bank_info *cbinfo = NULL;
	u32 cb_count = 0, cnt = 0;
	int rc = 0;

	if (!core || !core->resource || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	cbs = &core->resource->context_bank_set;

	cb_tbl = core->platform->data.context_bank_tbl;
	cb_count = core->platform->data.context_bank_tbl_size;

	if (!cb_tbl || !cb_count) {
		d_vpr_e("%s: invalid context bank tbl %#x or count %d\n",
			__func__, cb_tbl, cb_count);
		return -EINVAL;
	}

	/* allocate context_bank table */
	cbs->context_bank_tbl = devm_kzalloc(&core->pdev->dev,
			sizeof(*cbs->context_bank_tbl) * cb_count, GFP_KERNEL);
	if (!cbs->context_bank_tbl) {
		d_vpr_e("%s: failed to alloc memory for context_bank table\n", __func__);
		return -ENOMEM;
	}
	cbs->count = cb_count;

	/**
	 * populate context bank field from platform data except
	 * dev & domain which are assigned as part of context bank
	 * probe sequence
	 */
	for (cnt = 0; cnt < cbs->count; cnt++) {
		cbs->context_bank_tbl[cnt].name = cb_tbl[cnt].name;
		cbs->context_bank_tbl[cnt].addr_range.start = cb_tbl[cnt].start;
		cbs->context_bank_tbl[cnt].addr_range.size = cb_tbl[cnt].size;
		cbs->context_bank_tbl[cnt].secure = cb_tbl[cnt].secure;
		cbs->context_bank_tbl[cnt].dma_coherant = cb_tbl[cnt].dma_coherant;
		cbs->context_bank_tbl[cnt].region = cb_tbl[cnt].region;
		cbs->context_bank_tbl[cnt].dma_mask = cb_tbl[cnt].dma_mask;
	}

	/* print context_bank fiels */
	venus_hfi_for_each_context_bank(core, cbinfo) {
		d_vpr_h("%s: name %s addr start %#x size %#x secure %d "
			"coherant %d region %d dma_mask %llu\n",
			__func__, cbinfo->name, cbinfo->addr_range.start,
			cbinfo->addr_range.size, cbinfo->secure,
			cbinfo->dma_coherant, cbinfo->region, cbinfo->dma_mask);
	}

	return rc;
}

#ifdef CONFIG_MSM_MMRM
static int __register_mmrm(struct msm_vidc_core *core)
{
	int rc = 0;
	struct clock_info *cl;

	if (!core || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* skip if platform does not support mmrm */
	if (!is_mmrm_supported(core)) {
		d_vpr_h("%s: MMRM not supported\n", __func__);
		return 0;
	}

	/* get mmrm handle for each clock sources */
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
			return PTR_ERR(cl->clk) ? PTR_ERR(cl->clk) : -EINVAL;
		}

		desc.client_type = MMRM_CLIENT_CLOCK;
		desc.client_info.desc.client_domain = MMRM_CLIENT_DOMAIN_VIDEO;
		desc.client_info.desc.client_id = cl->clk_id;
		strscpy(name, cl->name, sizeof(desc.client_info.desc.name));
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

		cl->mmrm_client = devm_mmrm_get(&core->pdev->dev, &desc);
		if (!cl->mmrm_client) {
			d_vpr_e("%s: Failed to register clk(%s): %d\n",
				__func__, cl->name, rc);
			return -EINVAL;
		}
	}

	return rc;
}
#else
static int __register_mmrm(struct msm_vidc_core *core)
{
	return 0;
}
#endif



static int __acquire_regulator(struct msm_vidc_core *core,
			       struct regulator_info *rinfo)
{
	int rc = 0;

	if (!core || !rinfo) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (rinfo->hw_power_collapse) {
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

	if (rinfo->hw_power_collapse) {
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

static int __disable_subcaches(struct msm_vidc_core *core)
{
	struct subcache_info *sinfo;
	int rc = 0;

	if (msm_vidc_syscache_disable || !is_sys_cache_present(core))
		return 0;

	/* De-activate subcaches */
	venus_hfi_for_each_subcache_reverse(core, sinfo) {
		if (!sinfo->isactive)
			continue;

		d_vpr_h("%s: De-activate subcache %s\n", __func__, sinfo->name);
		rc = llcc_slice_deactivate(sinfo->subcache);
		if (rc) {
			d_vpr_e("Failed to de-activate %s: %d\n",
				sinfo->name, rc);
		}
		sinfo->isactive = false;
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

	if (!bus->icc) {
		d_vpr_e("%s: invalid bus\n", __func__);
		return -EINVAL;
	}

	d_vpr_p("Voting bus %s to ab %lu kBps\n", bus->name, bw_kbps);

	rc = icc_set_bw(bus->icc, bw_kbps, 0);
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
		if (bus && bus->icc) {
			type = get_type_frm_name(bus->name);

			if (type == DDR) {
				bw_kbps = bw_ddr;
				bw_prev = core->power.bw_ddr;
			} else if (type == LLCC) {
				bw_kbps = bw_llcc;
				bw_prev = core->power.bw_llcc;
			} else {
				bw_kbps = bus->max_kbps;
				bw_prev = core->power.bw_ddr ?
						bw_kbps : 0;
			}

			/* ensure freq is within limits */
			bw_kbps = clamp_t(typeof(bw_kbps), bw_kbps,
						 bus->min_kbps, bus->max_kbps);

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
	if (!core || !cl || !core->platform) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (is_mmrm_supported(core) && !cl->mmrm_client) {
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

	if (is_mmrm_supported(core)) {
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
	int rc = 0;

	rc = __init_register_base(core);
	if (rc)
		return rc;

	rc = __init_irq(core);
	if (rc)
		return rc;

	rc = __init_bus(core);
	if (rc)
		return rc;

	rc = __init_regulators(core);
	if (rc)
		return rc;

	rc = __init_clocks(core);
	if (rc)
		return rc;

	rc = __init_reset_clocks(core);
	if (rc)
		return rc;

	rc = __init_subcaches(core);
	if (rc)
		return rc;

	rc = __init_freq_table(core);
	if (rc)
		return rc;

	rc = __init_context_banks(core);
	if (rc)
		return rc;

	rc = __register_mmrm(core);
	if (rc)
		return rc;

	return rc;
}

static int __reset_control_deassert(struct msm_vidc_core *core)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		rc = reset_control_deassert(rcinfo->rst);
		if (rc) {
			d_vpr_e("%s: deassert reset control failed. rc = %d\n", __func__, rc);
			continue;
		}
		d_vpr_h("%s: deassert reset control %s\n", __func__, rcinfo->name);
	}

	return rc;
}

static int __reset_control_assert(struct msm_vidc_core *core)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0, cnt = 0;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		if (!rcinfo->rst) {
			d_vpr_e("%s: invalid reset clock %s\n",
				__func__, rcinfo->name);
			return -EINVAL;
		}
		rc = reset_control_assert(rcinfo->rst);
		if (rc) {
			d_vpr_e("%s: failed to assert reset control %s, rc = %d\n",
				__func__, rcinfo->name, rc);
			goto deassert_reset_control;
		}
		cnt++;
		d_vpr_h("%s: assert reset control %s, count %d\n", __func__, rcinfo->name, cnt);

		usleep_range(1000, 1100);
	}

	return rc;
deassert_reset_control:
	venus_hfi_for_each_reset_clock_reverse_continue(core, rcinfo, cnt) {
		d_vpr_e("%s: deassert reset control %s\n", __func__, rcinfo->name);
		reset_control_deassert(rcinfo->rst);
	}

	return rc;
}

static int __reset_ahb2axi_bridge(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = __reset_control_assert(core);
	if (rc)
		return rc;

	rc = __reset_control_deassert(core);
	if (rc)
		return rc;

	return rc;
}

static const struct msm_vidc_resources_ops res_ops = {
	.init = __init_resources,
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
