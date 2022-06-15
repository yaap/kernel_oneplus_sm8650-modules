/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2022, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_VIDC_RESOURCES_H_
#define _MSM_VIDC_RESOURCES_H_

struct msm_vidc_core;

struct msm_vidc_resources_ops {
	int (*get)(struct msm_vidc_core *core);
	void (*put)(struct msm_vidc_core *core);

	int (*reset_bridge)(struct msm_vidc_core *core);

	int (*gdsc_on)(struct msm_vidc_core *core, const char *name);
	int (*gdsc_off)(struct msm_vidc_core *core, const char *name);
	int (*gdsc_hw_ctrl)(struct msm_vidc_core *core);
	int (*gdsc_sw_ctrl)(struct msm_vidc_core *core);

	int (*llcc)(struct msm_vidc_core *core, bool enable);
	int (*set_bw)(struct msm_vidc_core *core, unsigned long bw_ddr,
		      unsigned long bw_llcc);
	int (*set_clks)(struct msm_vidc_core *core, u64 rate);

	int (*clk_disable)(struct msm_vidc_core *core, const char *name);
	int (*clk_enable)(struct msm_vidc_core *core, const char *name);

	int (*set_regs)(struct msm_vidc_core *core);
};

const struct msm_vidc_resources_ops *get_resources_ops(void);

#endif
