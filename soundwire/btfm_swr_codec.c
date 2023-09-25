// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "btfm_swr.h"

static int bt_soc_enable_status;
int btfm_feedback_ch_setting;

static int btfm_swr_codec_write(struct snd_soc_component *codec,
			unsigned int reg, unsigned int value)
{
	BTFMSWR_DBG("");
	return 0;
}

static unsigned int btfm_swr_codec_read(struct snd_soc_component *codec,
				unsigned int reg)
{
	BTFMSWR_DBG("");
	return 0;
}

static int btfm_soc_status_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSWR_DBG("");
	ucontrol->value.integer.value[0] = bt_soc_enable_status;
	return 1;
}

static int btfm_soc_status_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSWR_DBG("");
	return 1;
}

static int btfm_get_feedback_ch_setting(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSWR_DBG("");
	ucontrol->value.integer.value[0] = btfm_feedback_ch_setting;
	return 1;
}

static int btfm_put_feedback_ch_setting(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	BTFMSWR_DBG("");
	btfm_feedback_ch_setting = ucontrol->value.integer.value[0];
	return 1;
}

static const struct snd_kcontrol_new status_controls[] = {
	SOC_SINGLE_EXT("BT SOC status", 0, 0, 1, 0,
			btfm_soc_status_get,
			btfm_soc_status_put),
	SOC_SINGLE_EXT("BT set feedback channel", 0, 0, 1, 0,
	btfm_get_feedback_ch_setting,
	btfm_put_feedback_ch_setting)
};


static int btfm_swr_codec_probe(struct snd_soc_component *codec)
{
	BTFMSWR_DBG("");
	snd_soc_add_component_controls(codec, status_controls,
				   ARRAY_SIZE(status_controls));
	return 0;
}

static void btfm_swr_codec_remove(struct snd_soc_component *codec)
{
	BTFMSWR_DBG("");
}

static int btfm_swr_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = -1;

	BTFMSWR_INFO("substream = %s  stream = %d dai->name = %s",
		 substream->name, substream->stream, dai->name);
	ret = btfm_swr_hw_init();
	return ret;
}

static void btfm_swr_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0;
	u8 port_type;

	BTFMSWR_INFO("dai->name: %s, dai->id: %d, dai->rate: %d", dai->name,
		dai->id, dai->rate);

	switch (dai->id) {
	case FMAUDIO_TX:
		port_type = FM_AUDIO_TX1;
		break;
	case BTAUDIO_TX:
		port_type = BT_AUDIO_TX1;
		break;
	case BTAUDIO_RX:
		port_type = BT_AUDIO_RX1;
		break;
	case BTAUDIO_A2DP_SINK_TX:
		port_type = BT_AUDIO_TX2;
		break;
	case BTFM_NUM_CODEC_DAIS:
	default:
		BTFMSWR_ERR("dai->id is invalid:%d", dai->id);
		return;
	}

	ret = btfm_swr_disable_port(pbtfmswr->p_dai_port->port_info[dai->id].port,
					pbtfmswr->num_channels, port_type);

}

static int btfm_swr_dai_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	pbtfmswr->bps = params_width(params);
	pbtfmswr->direction = substream->stream;
	pbtfmswr->num_channels = params_channels(params);

	BTFMSWR_INFO("dai->name = %s dai id %x rate %d bps %d num_ch %d",
		dai->name, dai->id, params_rate(params), params_width(params),
		params_channels(params));
	return 0;
}

static int btfm_swr_dai_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int ret = -EINVAL;
	u8 port_type;

	bt_soc_enable_status = 0;
	BTFMSWR_INFO("dai->name: %s, dai->id: %d, dai->rate: %d direction: %d", dai->name,
		dai->id, dai->rate, pbtfmswr->direction);

	/* save sample rate */
	pbtfmswr->sample_rate = dai->rate;

	switch (dai->id) {
	case FMAUDIO_TX:
		port_type = FM_AUDIO_TX1;
		break;
	case BTAUDIO_TX:
		port_type = BT_AUDIO_TX1;
		break;
	case BTAUDIO_RX:
		port_type = BT_AUDIO_RX1;
		break;
	case BTAUDIO_A2DP_SINK_TX:
		port_type = BT_AUDIO_TX2;
		break;
	case BTFM_NUM_CODEC_DAIS:
	default:
		BTFMSWR_ERR("dai->id is invalid:%d", dai->id);
		return -EINVAL;
	}

	ret = btfm_swr_enable_port(pbtfmswr->p_dai_port->port_info[dai->id].port,
					pbtfmswr->num_channels, dai->rate, port_type);

	/* save the enable channel status */
	if (ret == 0)
		bt_soc_enable_status = 1;

	if (ret == -EISCONN) {
		BTFMSWR_ERR("channel opened without closing, returning success");
		ret = 0;
	}

	return ret;
}

/* This function will be called once during boot up */
static int btfm_swr_dai_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	BTFMSWR_DBG("");
	return 0;
}

static int btfm_swr_dai_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
{
	*rx_slot = 0;
	*tx_slot = 0;
	*rx_num = 0;
	*tx_num = 0;

	switch (dai->id) {
	case FMAUDIO_TX:
	case BTAUDIO_TX:
	case BTAUDIO_A2DP_SINK_TX:
		*tx_num = pbtfmswr->num_channels;
		*tx_slot = pbtfmswr->num_channels == 2 ? TWO_CHANNEL_MASK :
							ONE_CHANNEL_MASK;
		break;
	case BTAUDIO_RX:
		*rx_num = pbtfmswr->num_channels;
		*rx_slot = pbtfmswr->num_channels == 2 ? TWO_CHANNEL_MASK :
							ONE_CHANNEL_MASK;
		break;

	default:
		BTFMSWR_ERR("Unsupported DAI %d", dai->id);
		return -EINVAL;
	}

	return 0;
}
static struct snd_soc_dai_ops btfmswr_dai_ops = {
	.startup = btfm_swr_dai_startup,
	.shutdown = btfm_swr_dai_shutdown,
	.hw_params = btfm_swr_dai_hw_params,
	.prepare = btfm_swr_dai_prepare,
	.set_channel_map = btfm_swr_dai_set_channel_map,
	.get_channel_map = btfm_swr_dai_get_channel_map,
};

static struct snd_soc_dai_driver btfmswr_dai[] = {
	{	/* FM Audio data multiple channel  : FM -> lpass */
		.name = "btfm_fm_swr_tx",
		.id = FMAUDIO_TX,
		.capture = {
			.stream_name = "FM TX Capture",
			.rates = SNDRV_PCM_RATE_48000, /* 48 KHz */
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &btfmswr_dai_ops,
	},
	{	/* Bluetooth SCO voice uplink: bt -> lpass */
		.name = "btfm_bt_sco_swr_tx",
		.id = BTAUDIO_TX,
		.capture = {
			.stream_name = "SCO TX Capture",
			/* 8/16/44.1/48/88.2/96/192 Khz */
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000
				| SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmswr_dai_ops,
	},
	{	/* Bluetooth SCO voice downlink: lpass -> bt or A2DP Playback */
		.name = "btfm_bt_sco_a2dp_swr_rx",
		.id = BTAUDIO_RX,
		.playback = {
			.stream_name = "SCO A2DP RX Playback",
			/* 8/16/44.1/48/88.2/96/192 Khz */
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000
				| SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmswr_dai_ops,
	},
	{	/* Bluetooth A2DP sink: bt -> lpass */
		.name = "btfm_a2dp_sink_swr_tx",
		.id = BTAUDIO_A2DP_SINK_TX,
		.capture = {
			.stream_name = "A2DP sink TX Capture",
			/* 8/16/44.1/48/88.2/96/192 Khz */
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000
				| SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmswr_dai_ops,
	},
};

static const struct snd_soc_component_driver btfmswr_codec = {
	.probe	= btfm_swr_codec_probe,
	.remove	= btfm_swr_codec_remove,
	.read	= btfm_swr_codec_read,
	.write	= btfm_swr_codec_write,
};

int btfm_swr_register_codec(struct btfmswr *btfm_swr)
{
	int ret = 0;
	struct device *dev = btfm_swr->dev;

	BTFMSWR_DBG("");
	dev_err(dev, "\n");

	/* Register Codec driver */
	ret = snd_soc_register_component(dev, &btfmswr_codec,
		btfmswr_dai, ARRAY_SIZE(btfmswr_dai));
	if (ret)
		BTFMSWR_ERR("failed to register codec (%d)", ret);
	return ret;
}

void btfm_swr_unregister_codec(struct device *dev)
{
	BTFMSWR_DBG("");
	/* Unregister Codec driver */
	snd_soc_unregister_component(dev);
}


MODULE_DESCRIPTION("BTFM SoundWire Codec driver");
MODULE_LICENSE("GPL v2");
