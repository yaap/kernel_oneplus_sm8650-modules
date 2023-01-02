// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include "btfm_codec.h"
#include "btfm_codec_interface.h"

static struct snd_soc_dai_driver *btfmcodec_dai_info;

static int btfmcodec_codec_probe(struct snd_soc_component *codec)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(codec);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_data *hwep_info = btfmcodec->hwep_info;
	BTFMCODEC_DBG("");

	// ToDo: check weather probe has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (hwep_info->drv && hwep_info->drv->hwep_probe) {
		return hwep_info->drv->hwep_probe(codec);
	}

	// ToDo to add mixer control.
	return 0;
}

static void btfmcodec_codec_remove(struct snd_soc_component *codec)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(codec);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_data *hwep_info = btfmcodec->hwep_info;
	BTFMCODEC_DBG("");

	// ToDo: check whether remove has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (hwep_info->drv && hwep_info->drv->hwep_remove) {
		hwep_info->drv->hwep_remove(codec);
	}
}

static int btfmcodec_codec_write(struct snd_soc_component *codec,
			unsigned int reg, unsigned int value)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(codec);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_data *hwep_info = btfmcodec->hwep_info;
	BTFMCODEC_DBG("");

	// ToDo: check whether remove has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (hwep_info->drv && hwep_info->drv->hwep_remove) {
		return hwep_info->drv->hwep_write(codec, reg, value);
	}

	return 0;
}

static unsigned int btfmcodec_codec_read(struct snd_soc_component *codec,
				unsigned int reg)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(codec);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_data *hwep_info = btfmcodec->hwep_info;
	BTFMCODEC_DBG("");

	// ToDo: check whether remove has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (hwep_info->drv && hwep_info->drv->hwep_read) {
		return hwep_info->drv->hwep_read(codec, reg);
	}

	return 0;
}

static const struct snd_soc_component_driver btfmcodec_codec_component = {
	.probe	= btfmcodec_codec_probe,
	.remove	= btfmcodec_codec_remove,
	.read	= btfmcodec_codec_read,
	.write	= btfmcodec_codec_write,
};


static inline void * btfmcodec_get_dai_drvdata(struct hwep_data *hwep_info)
{
	if (!hwep_info || !hwep_info->dai_drv) return NULL;
	return hwep_info->dai_drv;
}

static int btfmcodec_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(dai->component);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_dai_driver *dai_drv = (struct hwep_dai_driver *)btfmcodec_get_dai_drvdata(btfmcodec->hwep_info);

	BTFMCODEC_DBG("substream = %s  stream = %d dai->name = %s",
		 substream->name, substream->stream, dai->name);

	// ToDo: check whether statup has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (dai && dai_drv->dai_ops && dai_drv->dai_ops->hwep_startup) {
		return dai_drv->dai_ops->hwep_startup((void *)btfmcodec->hwep_info);
	} else {
		return -1;
	}

	return 0;
}

static void btfmcodec_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(dai->component);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_dai_driver *dai_drv = (struct hwep_dai_driver *)btfmcodec_get_dai_drvdata(btfmcodec->hwep_info);

	BTFMCODEC_DBG("dai->name: %s, dai->id: %d, dai->rate: %d", dai->name,
		dai->id, dai->rate);

	// ToDo: check whether statup has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (dai && dai_drv->dai_ops && dai_drv->dai_ops->hwep_shutdown) {
		dai_drv->dai_ops->hwep_shutdown((void *)btfmcodec->hwep_info, dai->id);
	}
}

static int btfmcodec_dai_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(dai->component);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_dai_driver *dai_drv = (struct hwep_dai_driver *)btfmcodec_get_dai_drvdata(btfmcodec->hwep_info);
	uint32_t bps = params_width(params);
	uint32_t direction = substream->stream;

	BTFMCODEC_DBG("dai->name = %s DAI-ID %x rate %d bps %d num_ch %d",
		dai->name, dai->id, params_rate(params), params_width(params),
		params_channels(params));
	// ToDo: check whether hw_params has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (dai_drv && dai_drv->dai_ops && dai_drv->dai_ops->hwep_hw_params) {
		return dai_drv->dai_ops->hwep_hw_params((void *)btfmcodec->hwep_info, bps, direction);
	} else {
		return -1;
	}

	return 0;
}

static int btfmcodec_dai_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(dai->component);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_dai_driver *dai_drv = (struct hwep_dai_driver *)btfmcodec_get_dai_drvdata(btfmcodec->hwep_info);
	uint32_t sampling_rate = dai->rate;
	uint32_t direction = substream->stream;
	int id = dai->id;
	int ret;


	BTFMCODEC_INFO("dai->name: %s, dai->id: %d, dai->rate: %d direction: %d", dai->name,
		id, sampling_rate, direction);
	// ToDo: check whether hw_params has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (dai_drv && dai_drv->dai_ops && dai_drv->dai_ops->hwep_prepare) {
		ret = dai_drv->dai_ops->hwep_prepare((void *)btfmcodec->hwep_info, sampling_rate,
							direction, id);
		if (ret == 0 && test_bit(BTADV_AUDIO_MASTER_CONFIG, &btfmcodec->hwep_info->flags)) {
			BTFMCODEC_DBG("configuring master now");
		}
	} else {
		return -1;
	}

	return 0;
	return 0;
}

static int btfmcodec_dai_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(dai->component);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_dai_driver *dai_drv = (struct hwep_dai_driver *)btfmcodec_get_dai_drvdata(btfmcodec->hwep_info);

	BTFMCODEC_DBG("");
	// ToDo: check whether hw_params has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (dai_drv && dai_drv->dai_ops && dai_drv->dai_ops->hwep_set_channel_map) {
		return dai_drv->dai_ops->hwep_set_channel_map((void *)btfmcodec->hwep_info, tx_num,
								tx_slot, rx_num, rx_slot);
	} else {
		return -1;
	}

	return 0;
}

static int btfmcodec_dai_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
{
	struct btfmcodec_data *btfmcodec = snd_soc_component_get_drvdata(dai->component);
	struct btfmcodec_state_machine states = btfmcodec->states;
	struct hwep_dai_driver *dai_drv = (struct hwep_dai_driver *)btfmcodec_get_dai_drvdata(btfmcodec->hwep_info);

	BTFMCODEC_DBG("");
	// ToDo: check whether hw_params has to allowed when state if different
	if (states.current_state != IDLE) {
		BTFMCODEC_WARN("Received probe when state is :%s", coverttostring(states.current_state));
	} else if (dai_drv && dai_drv->dai_ops && dai_drv->dai_ops->hwep_get_channel_map) {
		return dai_drv->dai_ops->hwep_get_channel_map((void *)btfmcodec->hwep_info, tx_num,
								tx_slot, rx_num, rx_slot, dai->id);
	} else {
		return -1;
	}

	return 0;
}

static struct snd_soc_dai_ops btfmcodec_dai_ops = {
	.startup = btfmcodec_dai_startup,
	.shutdown = btfmcodec_dai_shutdown,
	.hw_params = btfmcodec_dai_hw_params,
	.prepare = btfmcodec_dai_prepare,
	.set_channel_map = btfmcodec_dai_set_channel_map,
	.get_channel_map = btfmcodec_dai_get_channel_map,
};

int btfm_register_codec(struct hwep_data *hwep_info)
{
	struct btfmcodec_data *btfmcodec;
	struct device *dev;
	struct hwep_dai_driver *dai_drv;
	int i, ret;

	btfmcodec = btfm_get_btfmcodec();
	dev = &btfmcodec->dev;
	btfmcodec_dai_info = kzalloc((sizeof(struct snd_soc_dai_driver) * hwep_info->num_dai), GFP_KERNEL);
	if (!btfmcodec_dai_info) {
		BTFMCODEC_ERR("failed to allocate memory");
		return -ENOMEM;
	}

	for (i = 0; i < hwep_info->num_dai; i++) {
		dai_drv = &hwep_info->dai_drv[i];
		btfmcodec_dai_info[i].name = dai_drv->dai_name;
		btfmcodec_dai_info[i].id = dai_drv->id;
		btfmcodec_dai_info[i].capture = dai_drv->capture;
		btfmcodec_dai_info[i].playback = dai_drv->playback;
		btfmcodec_dai_info[i].ops = &btfmcodec_dai_ops;
	}

	BTFMCODEC_INFO("Adding %d dai support to codec", hwep_info->num_dai);
	BTFMCODEC_INFO("slim bus driver name:%s", dev->driver->name);
	ret = snd_soc_register_component(dev, &btfmcodec_codec_component,
					btfmcodec_dai_info, hwep_info->num_dai);

	return ret;
}

void btfm_unregister_codec(void)
{
	struct btfmcodec_data *btfmcodec;

	btfmcodec = btfm_get_btfmcodec();
	snd_soc_unregister_component(&btfmcodec->dev);
}
