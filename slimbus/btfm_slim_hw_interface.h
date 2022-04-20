// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LINUX_BTFM_SLIM_HW_INTERFACE_H
#define __LINUX_BTFM_SLIM_HW_INTERFACE_H

#if IS_ENABLED(CONFIG_BTFM_SLIM)
#define BTFMSLIM_DEV_NAME "btfmslim_slave"
#else
#define BTFMSLIM_DEV_NAME "btfmslim"
#endif

// Todo protect with flags
int btfm_slim_register_hw_ep(struct btfmslim *btfm_slim);
void btfm_slim_unregister_hwep(void);
#endif /*__LINUX_BTFM_SLIM_HW_INTERFACE_H*/
