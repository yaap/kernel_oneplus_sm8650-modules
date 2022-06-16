// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/iopoll.h>

#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_dt.h"
#include "msm_vidc_variant.h"

static void __fatal_error(bool fatal)
{
	WARN_ON(fatal);
}

static int __strict_check(struct msm_vidc_core *core, const char *function)
{
	bool fatal = !mutex_is_locked(&core->lock);

	__fatal_error(fatal);

	if (fatal)
		d_vpr_e("%s: strict check failed\n", function);

	return fatal ? -EINVAL : 0;
}

int __write_register(struct msm_vidc_core *core, u32 reg, u32 value)
{
	u32 hwiosymaddr = reg;
	u8 *base_addr;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = __strict_check(core, __func__);
	if (rc)
		return rc;

	if (!core->power_enabled) {
		d_vpr_e("HFI Write register failed : Power is OFF\n");
		return -EINVAL;
	}

	base_addr = core->register_base_addr;
	d_vpr_l("regwrite(%pK + %#x) = %#x\n", base_addr, hwiosymaddr, value);
	base_addr += hwiosymaddr;
	writel_relaxed(value, base_addr);

	/*
	 * Memory barrier to make sure value is written into the register.
	 */
	wmb();

	return rc;
}

/*
 * Argument mask is used to specify which bits to update. In case mask is 0x11,
 * only bits 0 & 4 will be updated with corresponding bits from value. To update
 * entire register with value, set mask = 0xFFFFFFFF.
 */
int __write_register_masked(struct msm_vidc_core *core, u32 reg, u32 value,
			    u32 mask)
{
	u32 prev_val, new_val;
	u8 *base_addr;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = __strict_check(core, __func__);
	if (rc)
		return rc;

	if (!core->power_enabled) {
		d_vpr_e("%s: register write failed, power is off\n",
			__func__);
		return -EINVAL;
	}

	base_addr = core->register_base_addr;
	base_addr += reg;

	prev_val = readl_relaxed(base_addr);
	/*
	 * Memory barrier to ensure register read is correct
	 */
	rmb();

	new_val = (prev_val & ~mask) | (value & mask);
	d_vpr_l(
		"Base addr: %pK, writing to: %#x, previous-value: %#x, value: %#x, mask: %#x, new-value: %#x...\n",
		base_addr, reg, prev_val, value, mask, new_val);
	writel_relaxed(new_val, base_addr);
	/*
	 * Memory barrier to make sure value is written into the register.
	 */
	wmb();

	return rc;
}

int __read_register(struct msm_vidc_core *core, u32 reg, u32 *value)
{
	int rc = 0;
	u8 *base_addr;

	if (!core || !value) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!core->power_enabled) {
		d_vpr_e("HFI Read register failed : Power is OFF\n");
		return -EINVAL;
	}

	base_addr = core->register_base_addr;

	*value = readl_relaxed(base_addr + reg);
	/*
	 * Memory barrier to make sure value is read correctly from the
	 * register.
	 */
	rmb();
	d_vpr_l("regread(%pK + %#x) = %#x\n", base_addr, reg, *value);

	return rc;
}

int __read_register_with_poll_timeout(struct msm_vidc_core *core, u32 reg,
				      u32 mask, u32 exp_val, u32 sleep_us,
				      u32 timeout_us)
{
	int rc = 0;
	u32 val = 0;
	u8 *addr;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!core->power_enabled) {
		d_vpr_e("%s failed: Power is OFF\n", __func__);
		return -EINVAL;
	}

	addr = (u8 *)core->register_base_addr + reg;

	rc = readl_relaxed_poll_timeout(addr, val, ((val & mask) == exp_val), sleep_us, timeout_us);
	/*
	 * Memory barrier to make sure value is read correctly from the
	 * register.
	 */
	rmb();
	d_vpr_l(
		"regread(%pK + %#x) = %#x. rc %d, mask %#x, exp_val %#x, cond %u, sleep %u, timeout %u\n",
		core->register_base_addr, reg, val, rc, mask, exp_val,
		((val & mask) == exp_val), sleep_us, timeout_us);

	return rc;
}

int __set_registers(struct msm_vidc_core *core)
{
	struct reg_set *reg_set;
	int i, rc = 0;

	if (!core || !core->dt) {
		d_vpr_e("core resources null, cannot set registers\n");
		return -EINVAL;
	}

	reg_set = &core->dt->reg_set;
	for (i = 0; i < reg_set->count; i++) {
		rc = __write_register_masked(core, reg_set->reg_tbl[i].reg,
					     reg_set->reg_tbl[i].value,
					     reg_set->reg_tbl[i].mask);
		if (rc)
			return rc;
	}

	return rc;
}
