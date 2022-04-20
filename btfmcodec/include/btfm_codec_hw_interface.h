// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LINUX_BTFM_CODEC_HW_INTERFACE_H
#define __LINUX_BTFM_CODEC_HW_INTERFACE_H

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

/* This flag is set to indicate btfm codec driver is
 * responsible to configure master.
 */
#define BTADV_AUDIO_MASTER_CONFIG	0
#define DEVICE_NAME_MAX_LEN	64

struct hwep_comp_drv {
	int  (*hwep_probe)  (struct snd_soc_component *component);
	void (*hwep_remove) (struct snd_soc_component *component);
	unsigned int  (*hwep_read)(struct snd_soc_component *component, unsigned int reg);
	int  (*hwep_write)(struct snd_soc_component *componentm, unsigned int reg, unsigned int value);
};

struct hwep_dai_ops {
	int (*hwep_startup)(void *);
	void (*hwep_shutdown)(void *, int);
	int (*hwep_hw_params)(void *, uint32_t, uint32_t);
	int (*hwep_prepare)(void *, uint32_t, uint32_t, int);
	int (*hwep_set_channel_map)(void *, unsigned int, unsigned int *,
				unsigned int, unsigned int *);
	int (*hwep_get_channel_map)(void *, unsigned int *, unsigned int *,
				unsigned int *, unsigned int *, int);
};

struct hwep_dai_driver {
	const char *dai_name;
	unsigned int id;
	struct snd_soc_pcm_stream capture;
	struct snd_soc_pcm_stream playback;
        struct hwep_dai_ops *dai_ops;
};

struct hwep_data {
	struct device *dev;
	char driver_name [DEVICE_NAME_MAX_LEN];
        struct hwep_comp_drv *drv;
        struct hwep_dai_driver *dai_drv;
	int num_dai;
	unsigned long flags;
};

int btfmcodec_register_hw_ep(struct hwep_data *);
int btfmcodec_unregister_hw_ep(char *);
// ToDo below.
/*
#if IS_ENABLED(CONFIG_SLIM_BTFM_CODEC_DRV)
int btfmcodec_register_hw_ep(struct hwep_data *);
int btfmcodec_unregister_hw_ep(char *);
#else
static inline int btfmcodec_register_hw_ep(struct hwep_data *hwep_info)
{
	return -EOPNOTSUPP;
}

static inline int btfmcodec_unregister_hw_ep(char *dev_name)
{
	return -EOPNOTSUPP;
}
#endif
*/
#endif /*__LINUX_BTFM_CODEC_HW_INTERFACE_H */
