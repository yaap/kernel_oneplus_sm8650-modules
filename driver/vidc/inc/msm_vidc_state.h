/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_STATE_H_
#define _MSM_VIDC_STATE_H_

#include "msm_vidc_internal.h"

struct msm_vidc_core;
enum msm_vidc_state;
enum msm_vidc_sub_state;

#define FOREACH_CORE_STATE(CORE_STATE) {               \
	CORE_STATE(CORE_DEINIT)                        \
	CORE_STATE(CORE_INIT_WAIT)                     \
	CORE_STATE(CORE_INIT)                          \
	CORE_STATE(CORE_ERROR)                         \
}

#define FOREACH_EVENT(EVENT) {                         \
	EVENT(S_FMT)                                   \
	EVENT(REQBUFS)                                 \
	EVENT(S_CTRL)                                  \
	EVENT(STREAMON)                                \
	EVENT(STREAMOFF)                               \
	EVENT(CMD_START)                               \
	EVENT(CMD_STOP)                                \
	EVENT(BUF_QUEUE)                               \
}

enum msm_vidc_core_state FOREACH_CORE_STATE(GENERATE_MSM_VIDC_ENUM);

enum msm_vidc_core_sub_state {
	CORE_SUBSTATE_NONE                   = 0x0,
	CORE_SUBSTATE_POWER_ENABLE           = BIT(0),
	CORE_SUBSTATE_GDSC_HANDOFF           = BIT(1),
	CORE_SUBSTATE_PM_SUSPEND             = BIT(2),
	CORE_SUBSTATE_FW_PWR_CTRL            = BIT(3),
	CORE_SUBSTATE_PAGE_FAULT             = BIT(4),
	CORE_SUBSTATE_CPU_WATCHDOG           = BIT(5),
	CORE_SUBSTATE_VIDEO_UNRESPONSIVE     = BIT(6),
	CORE_SUBSTATE_MAX                    = BIT(7),
};

enum msm_vidc_core_event_type {
	CORE_EVENT_NONE                      = BIT(0),
	CORE_EVENT_UPDATE_SUB_STATE          = BIT(1),
};

enum msm_vidc_event FOREACH_EVENT(GENERATE_MSM_VIDC_ENUM);

/* core statemachine functions */
enum msm_vidc_allow msm_vidc_allow_core_state_change(
	struct msm_vidc_core *core, enum msm_vidc_core_state req_state);
int msm_vidc_update_core_state(struct msm_vidc_core *core,
	enum msm_vidc_core_state request_state, const char *func);
bool core_in_valid_state(struct msm_vidc_core *core);
bool is_core_state(struct msm_vidc_core *core, enum msm_vidc_core_state state);
bool is_core_sub_state(struct msm_vidc_core *core,
	enum msm_vidc_core_sub_state sub_state);
const char *core_state_name(enum msm_vidc_core_state state);
const char *core_sub_state_name(enum msm_vidc_core_sub_state sub_state);

/* inst statemachine functions */
int msm_vidc_update_state(struct msm_vidc_inst *inst,
	enum msm_vidc_state request_state, const char *func);
enum msm_vidc_allow msm_vidc_allow_state_change(
	struct msm_vidc_inst *inst,
	enum msm_vidc_state req_state);

#endif // _MSM_VIDC_STATE_H_
