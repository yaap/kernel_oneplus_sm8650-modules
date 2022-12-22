// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "msm_vidc_driver.h"
#include "msm_vidc_state.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_core.h"

bool core_in_valid_state(struct msm_vidc_core *core)
{
	return (core->state == MSM_VIDC_CORE_INIT ||
		core->state == MSM_VIDC_CORE_INIT_WAIT);
}

bool is_core_state(struct msm_vidc_core *core, enum msm_vidc_core_state state)
{
	return core->state == state;
}

static const char * const core_state_name_arr[] =
	FOREACH_CORE_STATE(GENERATE_STRING);

const char *core_state_name(enum msm_vidc_core_state state)
{
	const char *name = "UNKNOWN STATE";

	if (state >= ARRAY_SIZE(core_state_name_arr))
		goto exit;

	name = core_state_name_arr[state];

exit:
	return name;
}

static int __strict_check(struct msm_vidc_core *core, const char *function)
{
	bool fatal = !mutex_is_locked(&core->lock);

	WARN_ON(fatal);

	if (fatal)
		d_vpr_e("%s: strict check failed\n", function);

	return fatal ? -EINVAL : 0;
}

static int msm_vidc_core_deinit_state(struct msm_vidc_core *core,
	enum msm_vidc_core_event_type type,
	struct msm_vidc_event_data *data)
{
	if (!core || !data) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	d_vpr_e("%s: unexpected core event type %u\n", __func__, type);
	return -EINVAL;
}

static int msm_vidc_core_init_wait_state(struct msm_vidc_core *core,
	enum msm_vidc_core_event_type type,
	struct msm_vidc_event_data *data)
{
	int rc = 0;

	if (!core || !data) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	switch (type) {
	case CORE_EVENT_UPDATE_SUB_STATE:
	{
		u32 req_sub_state;
		u32 allow_mask = -1;

		req_sub_state = data->edata.uval;

		/* none of the requested substate supported */
		if (!(req_sub_state & allow_mask)) {
			d_vpr_e("%s: invalid substate update request %#x\n",
				__func__, req_sub_state);
			return -EINVAL;
		}

		/* update core substate */
		core->sub_state |= req_sub_state & allow_mask;
		return rc;
	}
	default: {
		d_vpr_e("%s: unexpected core event type %u\n",
			__func__, type);
		return -EINVAL;
	}
	}

	return rc;
}

static int msm_vidc_core_init_state(struct msm_vidc_core *core,
	enum msm_vidc_core_event_type type,
	struct msm_vidc_event_data *data)
{
	int rc = 0;

	if (!core || !data) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	switch (type) {
	case CORE_EVENT_UPDATE_SUB_STATE:
	{
		u32 req_sub_state;
		u32 allow_mask = -1;

		req_sub_state = data->edata.uval;

		/* none of the requested substate supported */
		if (!(req_sub_state & allow_mask)) {
			d_vpr_e("%s: invalid substate update request %#x\n",
				__func__, req_sub_state);
			return -EINVAL;
		}

		/* update core substate */
		core->sub_state |= req_sub_state & allow_mask;
		return rc;
	}
	default: {
		d_vpr_e("%s: unexpected core event type %u\n",
			__func__, type);
		return -EINVAL;
	}
	}

	return rc;
}

static int msm_vidc_core_error_state(struct msm_vidc_core *core,
	enum msm_vidc_core_event_type type,
	struct msm_vidc_event_data *data)
{
	int rc = 0;

	if (!core || !data) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	switch (type) {
	case CORE_EVENT_UPDATE_SUB_STATE:
	{
		u32 req_sub_state;
		u32 allow_mask = -1;

		req_sub_state = data->edata.uval;

		/* none of the requested substate supported */
		if (!(req_sub_state & allow_mask)) {
			d_vpr_e("%s: invalid substate update request %#x\n",
				__func__, req_sub_state);
			return -EINVAL;
		}

		/* update core substate */
		core->sub_state |= req_sub_state & allow_mask;
		return rc;
	}
	default: {
		d_vpr_e("%s: unexpected core event type %u\n",
			__func__, type);
		return -EINVAL;
	}
	}

	return rc;
}

struct msm_vidc_core_state_handle *msm_vidc_get_core_state_handle(
	enum msm_vidc_core_state req_state)
{
	int cnt;
	struct msm_vidc_core_state_handle *core_state_handle = NULL;
	static struct msm_vidc_core_state_handle state_handle[] = {
		{MSM_VIDC_CORE_DEINIT,      msm_vidc_core_deinit_state      },
		{MSM_VIDC_CORE_INIT_WAIT,   msm_vidc_core_init_wait_state   },
		{MSM_VIDC_CORE_INIT,        msm_vidc_core_init_state        },
		{MSM_VIDC_CORE_ERROR,       msm_vidc_core_error_state       },
	};

	for (cnt = 0; cnt < ARRAY_SIZE(state_handle); cnt++) {
		if (state_handle[cnt].state == req_state) {
			core_state_handle = &state_handle[cnt];
			break;
		}
	}

	/* if req_state does not exist in the table */
	if (cnt == ARRAY_SIZE(state_handle)) {
		d_vpr_e("%s: invalid core state \"%s\" requested\n",
			__func__, core_state_name(req_state));
		return core_state_handle;
	}

	return core_state_handle;
}

int msm_vidc_update_core_state(struct msm_vidc_core *core,
	enum msm_vidc_core_state request_state, const char *func)
{
	struct msm_vidc_core_state_handle *state_handle = NULL;
	int rc = 0;

	/* get core state handler for requested state */
	state_handle = msm_vidc_get_core_state_handle(request_state);
	if (!state_handle)
		return -EINVAL;

	d_vpr_h("%s: core state changed to %s from %s\n", func,
		core_state_name(state_handle->state), core_state_name(core->state));

	/* finally update core state and handler */
	core->state = state_handle->state;
	core->state_handle = state_handle->handle;

	return rc;
}

struct msm_vidc_core_state_allow {
	enum msm_vidc_core_state   from;
	enum msm_vidc_core_state   to;
	enum msm_vidc_allow        allow;
};

enum msm_vidc_allow msm_vidc_allow_core_state_change(
	struct msm_vidc_core *core,
	enum msm_vidc_core_state req_state)
{
	int cnt;
	enum msm_vidc_allow allow = MSM_VIDC_DISALLOW;
	static struct msm_vidc_core_state_allow state[] = {
		/* from, to, allow */
		{MSM_VIDC_CORE_DEINIT,      MSM_VIDC_CORE_DEINIT,      MSM_VIDC_IGNORE    },
		{MSM_VIDC_CORE_DEINIT,      MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_ALLOW     },
		{MSM_VIDC_CORE_DEINIT,      MSM_VIDC_CORE_INIT,        MSM_VIDC_DISALLOW  },
		{MSM_VIDC_CORE_DEINIT,      MSM_VIDC_CORE_ERROR,       MSM_VIDC_IGNORE    },
		{MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_CORE_DEINIT,      MSM_VIDC_DISALLOW  },
		{MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_IGNORE    },
		{MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_CORE_INIT,        MSM_VIDC_ALLOW     },
		{MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_CORE_ERROR,       MSM_VIDC_ALLOW     },
		{MSM_VIDC_CORE_INIT,        MSM_VIDC_CORE_DEINIT,      MSM_VIDC_ALLOW     },
		{MSM_VIDC_CORE_INIT,        MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_DISALLOW  },
		{MSM_VIDC_CORE_INIT,        MSM_VIDC_CORE_INIT,        MSM_VIDC_IGNORE    },
		{MSM_VIDC_CORE_INIT,        MSM_VIDC_CORE_ERROR,       MSM_VIDC_ALLOW     },
		{MSM_VIDC_CORE_ERROR,       MSM_VIDC_CORE_DEINIT,      MSM_VIDC_ALLOW     },
		{MSM_VIDC_CORE_ERROR,       MSM_VIDC_CORE_INIT_WAIT,   MSM_VIDC_IGNORE    },
		{MSM_VIDC_CORE_ERROR,       MSM_VIDC_CORE_INIT,        MSM_VIDC_IGNORE    },
		{MSM_VIDC_CORE_ERROR,       MSM_VIDC_CORE_ERROR,       MSM_VIDC_IGNORE    },
	};

	for (cnt = 0; cnt < ARRAY_SIZE(state); cnt++) {
		if (state[cnt].from == core->state && state[cnt].to == req_state) {
			allow = state[cnt].allow;
			break;
		}
	}

	return allow;
}

int msm_vidc_change_core_state(struct msm_vidc_core *core,
	enum msm_vidc_core_state request_state, const char *func)
{
	enum msm_vidc_allow allow;
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* core must be locked */
	rc = __strict_check(core, func);
	if (rc) {
		d_vpr_e("%s(): core was not locked\n", func);
		return rc;
	}

	/* current and requested state is same */
	if (core->state == request_state)
		return 0;

	/* check if requested state movement is allowed */
	allow = msm_vidc_allow_core_state_change(core, request_state);
	if (allow == MSM_VIDC_IGNORE) {
		d_vpr_h("%s: %s core state change %s -> %s\n", func,
			allow_name(allow), core_state_name(core->state),
			core_state_name(request_state));
		return 0;
	} else if (allow == MSM_VIDC_DISALLOW) {
		d_vpr_e("%s: %s core state change %s -> %s\n", func,
			allow_name(allow), core_state_name(core->state),
			core_state_name(request_state));
		return -EINVAL;
	}

	/* go ahead and update core state */
	rc = msm_vidc_update_core_state(core, request_state, func);
	if (rc)
		return rc;

	return rc;
}

bool is_core_sub_state(struct msm_vidc_core *core,
	enum msm_vidc_core_sub_state sub_state)
{
	return !!(core->sub_state & sub_state);
}

const char *core_sub_state_name(enum msm_vidc_core_sub_state sub_state)
{
	switch (sub_state) {
	case CORE_SUBSTATE_NONE:                 return "NONE ";
	case CORE_SUBSTATE_GDSC_HANDOFF:         return "GDSC_HANDOFF ";
	case CORE_SUBSTATE_PM_SUSPEND:           return "PM_SUSPEND ";
	case CORE_SUBSTATE_FW_PWR_CTRL:          return "FW_PWR_CTRL ";
	case CORE_SUBSTATE_POWER_ENABLE:         return "POWER_ENABLE ";
	case CORE_SUBSTATE_PAGE_FAULT:           return "PAGE_FAULT ";
	case CORE_SUBSTATE_CPU_WATCHDOG:         return "CPU_WATCHDOG ";
	case CORE_SUBSTATE_VIDEO_UNRESPONSIVE:   return "VIDEO_UNRESPONSIVE ";
	case CORE_SUBSTATE_MAX:                  return "MAX ";
	}

	return "UNKNOWN ";
}

static int prepare_core_sub_state_name(enum msm_vidc_core_sub_state sub_state,
	char *buf, u32 size)
{
	int i = 0;

	if (!buf || !size)
		return -EINVAL;

	strscpy(buf, "\0", size);
	if (sub_state == CORE_SUBSTATE_NONE) {
		strscpy(buf, "CORE_SUBSTATE_NONE", size);
		return 0;
	}

	for (i = 0; BIT(i) < CORE_SUBSTATE_MAX; i++) {
		if (sub_state & BIT(i))
			strlcat(buf, core_sub_state_name(BIT(i)), size);
	}

	return 0;
}

static int msm_vidc_update_core_sub_state(struct msm_vidc_core *core,
	enum msm_vidc_core_sub_state sub_state, const char *func)
{
	struct msm_vidc_event_data data;
	char sub_state_name[MAX_NAME_LENGTH];
	int ret = 0, rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* no substate update */
	if (!sub_state)
		return 0;

	/* invoke update core substate event */
	memset(&data, 0, sizeof(struct msm_vidc_event_data));
	data.edata.uval = sub_state;
	rc = core->state_handle(core, CORE_EVENT_UPDATE_SUB_STATE, &data);
	if (rc) {
		ret = prepare_core_sub_state_name(sub_state,
			 sub_state_name, sizeof(sub_state_name) - 1);
		if (!ret)
			d_vpr_e("%s: state %s, requested invalid core substate %s\n",
				func, core_state_name(core->state), sub_state_name);
		return rc;
	}

	return rc;
}

int msm_vidc_change_core_sub_state(struct msm_vidc_core *core,
		enum msm_vidc_core_sub_state clear_sub_state,
		enum msm_vidc_core_sub_state set_sub_state, const char *func)
{
	int rc = 0;
	enum msm_vidc_core_sub_state prev_sub_state;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* core must be locked */
	rc = __strict_check(core, func);
	if (rc) {
		d_vpr_e("%s(): core was not locked\n", func);
		return rc;
	}

	/* sanitize core state handler */
	if (!core->state_handle) {
		d_vpr_e("%s: invalid core state handle\n", __func__);
		return -EINVAL;
	}

	/* final value will not change */
	if (clear_sub_state == set_sub_state)
		return 0;

	/* sanitize clear & set value */
	if (set_sub_state > CORE_SUBSTATE_MAX ||
		clear_sub_state > CORE_SUBSTATE_MAX) {
		d_vpr_e("%s: invalid sub states. clear %#x or set %#x\n",
			func, clear_sub_state, set_sub_state);
		return -EINVAL;
	}

	prev_sub_state = core->sub_state;

	/* set sub state */
	rc = msm_vidc_update_core_sub_state(core, set_sub_state, func);
	if (rc)
		return rc;

	/* check if all core substates updated */
	if ((core->sub_state & set_sub_state) != set_sub_state)
		d_vpr_e("%s: all substates not updated %#x, expected %#x\n",
			func, core->sub_state & set_sub_state, set_sub_state);

	/* clear sub state */
	core->sub_state &= ~clear_sub_state;

	/* print substates only when there is a change */
	if (core->sub_state != prev_sub_state) {
		rc = prepare_core_sub_state_name(core->sub_state, core->sub_state_name,
			sizeof(core->sub_state_name) - 1);
		if (!rc)
			d_vpr_h("%s: core sub state changed to %s\n", func, core->sub_state_name);
	}

	return 0;
}
