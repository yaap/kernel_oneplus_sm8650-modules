// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LINUX_BTFM_CODEC_H
#define __LINUX_BTFM_CODEC_H

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/printk.h>
#include <linux/cdev.h>
#include <linux/skbuff.h>
#include "btfm_codec_hw_interface.h"

#define BTFMCODEC_DBG(fmt, arg...)  pr_err("%s: " fmt "\n", __func__, ## arg)
#define BTFMCODEC_INFO(fmt, arg...) pr_err("%s: " fmt "\n", __func__, ## arg)
#define BTFMCODEC_ERR(fmt, arg...)  pr_err("%s: " fmt "\n", __func__, ## arg)
#define BTFMCODEC_WARN(fmt, arg...) pr_warn("%s: " fmt "\n", __func__, ## arg)

#define DEVICE_NAME_MAX_LEN	64

enum btfmcodec_states {
	/*Default state of btfm codec driver */
	IDLE = 0,
	/* When BT is active transport */
	BT_Connected = 1,
	/* Waiting for BT bearer indication after configuring HW ports */
	BT_Connecting = 2,
	/* When BTADV_AUDIO is active transport */
	BTADV_AUDIO_Connected = 3,
	/* Waiting for BTADV_AUDIO bearer switch indications */
	BTADV_AUDIO_Connecting = 4
};

char *coverttostring(enum btfmcodec_states);
struct btfmcodec_state_machine {
	enum btfmcodec_states prev_state;
	enum btfmcodec_states current_state;
	enum btfmcodec_states next_state;
};

struct btfmcodec_char_device {
	struct cdev cdev;
	refcount_t active_clients;
	struct mutex lock;
	int reuse_minor;
	char dev_name[DEVICE_NAME_MAX_LEN];
	struct sk_buff_head rxq;
	struct work_struct rx_work;
	wait_queue_head_t readq;
	spinlock_t tx_queue_lock;
	struct sk_buff_head txq;
	void *btfmcodec;
};

struct btfmcodec_data {
	struct device dev;
	struct btfmcodec_state_machine states;
	struct btfmcodec_char_device *btfmcodec_dev;
	struct hwep_data *hwep_info;
};

int btfmcodec_dev_enqueue_pkt(struct btfmcodec_char_device *, uint8_t *, int);
struct btfmcodec_data *btfm_get_btfmcodec(void);
#endif /*__LINUX_BTFM_CODEC_H */
